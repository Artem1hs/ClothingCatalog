/*
  File: server/server_main.cpp
  Purpose: Local backend executable. Defines HTTP routes, initializes SQLite tables, and sends WebSocket notifications about order changes.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "src/ServerCommon.h"
#include "src/ServerSecurity.h"
#include "src/ServerDatabase.h"
#include "src/ServerResponses.h"
#include "src/SmtpService.h"

int main(int argc, char *argv[])
{
  QCoreApplication a(argc, argv);

  // Open or create the local SQLite database used by the API server.
  if (!openServerDb())
    return 1;

  QString initErr;
  if (!ensureTables(&initErr)) {
    qWarning() << "Schema ensure error:" << initErr;
    return 1;
  }

  if (!ensureUsersColumns(&initErr)) {
    qWarning() << "Users columns ensure error:" << initErr;
    return 1;
  }

  if (!ensureAdminUser(&initErr)) {
    qWarning() << "Admin ensure error:" << initErr;
    return 1;
  }

  if (!ensureImageFolderProducts(&initErr)) {
    qWarning() << "Image products ensure error:" << initErr;
    return 1;
  }

  // ---- WebSocket server for order status updates ----
  // WebSocket clients are desktop windows that need real-time order updates.
  QWebSocketServer wsServer(QStringLiteral("ClothingWS"), QWebSocketServer::NonSecureMode);
  if (!wsServer.listen(QHostAddress::AnyIPv4, 8081)) {
    qWarning() << "WS listen error on 8081";
    return 1;
  }

  QSet<QWebSocket*> wsClients;
  QObject::connect(&wsServer, &QWebSocketServer::newConnection, [&]() {
    QWebSocket* sock = wsServer.nextPendingConnection();
    wsClients.insert(sock);
    QObject::connect(sock, &QWebSocket::disconnected, [&]() {
      wsClients.remove(sock);
      sock->deleteLater();
    });
  });

  // Broadcast compact JSON messages to all connected desktop clients.
  auto broadcast = [&](const QJsonObject& msg){
    const QByteArray payload = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    const QString text = QString::fromUtf8(payload);
    for (auto* c : wsClients) c->sendTextMessage(text);
  };

  // ---- HTTP API server ----
  // QHttpServer keeps the API self-contained without an external web framework.
  QHttpServer http;

  http.route("/api/ping", []() {
    return QJsonObject{{"ok", true}};
  });

  // POST /api/auth/register
  http.route("/api/auth/register", QHttpServerRequest::Method::Post,
        [&](const QHttpServerRequest& req) -> QHttpServerResponse
        {
          QJsonParseError pe;
          const QJsonDocument doc = QJsonDocument::fromJson(req.body(), &pe);
          if (pe.error != QJsonParseError::NoError || !doc.isObject())
            return jsonError("bad_json", "Invalid JSON body", QHttpServerResponse::StatusCode::BadRequest);

          const QJsonObject o = doc.object();

          const QString first = o.value("first_name").toString().trimmed();
          const QString last = o.value("last_name").toString().trimmed();
          const QString email = o.value("email").toString().trimmed().toLower();
          const QString phone = o.value("phone").toString().trimmed();
          const QString pass = o.value("password").toString();

          if (first.isEmpty() || last.isEmpty() || email.isEmpty() || phone.isEmpty() || pass.isEmpty())
            return jsonError("bad_input", "first_name/last_name/email/phone/password required",
                    QHttpServerResponse::StatusCode::BadRequest);

          // For compatibility, the email is also used as the username.
          const QString u = email;

          QSqlDatabase db = QSqlDatabase::database();
          QSqlQuery q(db);

          // If the user already exists, check whether the phone number was confirmed.
          // If the phone is not confirmed yet, allow issuing a new confirmation code.
          q.prepare("SELECT phone_verified FROM users WHERE username=:u;");
          q.bindValue(":u", u);
          if (!q.exec())
            return jsonError("db", q.lastError().text(), QHttpServerResponse::StatusCode::InternalServerError);

          if (q.next()) {
            const bool verified = (q.value(0).toInt() != 0);
            if (verified) {
              return jsonError("exists", "User already exists", QHttpServerResponse::StatusCode::Conflict);
            }

            // The user exists, but the phone number is still not confirmed.
            // Refresh contact data before issuing a new code.
            QSqlQuery upd(db);
            upd.prepare("UPDATE users SET first_name=:fn, last_name=:ln, email=:em, phone=:ph, password_hash=:h "
                 "WHERE username=:u;");
            upd.bindValue(":fn", first);
            upd.bindValue(":ln", last);
            upd.bindValue(":em", email);
            upd.bindValue(":ph", phone);
            upd.bindValue(":h", hashPassword(pass));
            upd.bindValue(":u", u);
            if (!upd.exec())
              return jsonError("db", upd.lastError().text(), QHttpServerResponse::StatusCode::InternalServerError);

          } else {
            // Create a new user record.
            QSqlQuery ins(db);
            ins.prepare("INSERT INTO users(username, password_hash, is_admin, first_name, last_name, email, phone, phone_verified) "
                 "VALUES(:u,:h,0,:fn,:ln,:em,:ph,0);");
            ins.bindValue(":u", u);
            ins.bindValue(":h", hashPassword(pass));
            ins.bindValue(":fn", first);
            ins.bindValue(":ln", last);
            ins.bindValue(":em", email);
            ins.bindValue(":ph", phone);

            if (!ins.exec())
              return jsonError("db", ins.lastError().text(), QHttpServerResponse::StatusCode::InternalServerError);
          }

          // Generate a phone confirmation code.
          const QString code = QString("%1").arg(QRandomGenerator::global()->bounded(0, 1000000), 6, 10, QChar('0'));
          const qint64 exp = QDateTime::currentSecsSinceEpoch() + 5 * 60; // 5 minutes

          QSqlQuery pc(db);
          pc.prepare("INSERT OR REPLACE INTO phone_codes(username, code, expires_at) VALUES(:u,:c,:e);");
          pc.bindValue(":u", u);
          pc.bindValue(":c", code);
          pc.bindValue(":e", exp);
          pc.exec(); // A failed code update should not interrupt registration.

          // Send a welcome email when SMTP is configured.
          QString mailErr;
          smtpSendGmailThanks(email, &mailErr); // A failed email send should not interrupt registration.

          return QHttpServerResponse(QJsonObject{{"ok", true}, {"phone_code", code}});
        });

  // POST /api/auth/login
  http.route("/api/auth/login", QHttpServerRequest::Method::Post,
        [&](const QHttpServerRequest& req) -> QHttpServerResponse
  {
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(req.body(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
      return jsonError("bad_json", "Invalid JSON body", QHttpServerResponse::StatusCode::BadRequest);

    const QJsonObject o = doc.object();
    const QString u = o.value("username").toString().trimmed();
    const QString p = o.value("password").toString();

    if (u.isEmpty() || p.isEmpty())
      return jsonError("bad_input", "username/password required", QHttpServerResponse::StatusCode::BadRequest);

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("SELECT password_hash, is_admin FROM users WHERE username=:u;");
    q.bindValue(":u", u);

    if (!q.exec() || !q.next())
      return jsonError("user_not_found", "User not found", QHttpServerResponse::StatusCode::Unauthorized);

    const QString dbHash = q.value(0).toString();
    const bool isAdmin = (q.value(1).toInt() != 0);

    if (dbHash != hashPassword(p))
      return jsonError("bad_password", "Wrong password", QHttpServerResponse::StatusCode::Unauthorized);

    return QHttpServerResponse(QJsonObject{
      {"token", makeToken(u)},
      {"username", u},
      {"is_admin", isAdmin}
    });
  });

  // POST /api/auth/confirm_phone { "email":"...", "code":"123456" }
  http.route("/api/auth/confirm_phone", QHttpServerRequest::Method::Post,
        [&](const QHttpServerRequest& req) -> QHttpServerResponse
        {
          QJsonParseError pe;
          const QJsonDocument doc = QJsonDocument::fromJson(req.body(), &pe);
          if (pe.error != QJsonParseError::NoError || !doc.isObject())
            return jsonError("bad_json", "Invalid JSON body", QHttpServerResponse::StatusCode::BadRequest);

          const QJsonObject o = doc.object();
          const QString u = o.value("email").toString().trimmed().toLower();
          const QString code = o.value("code").toString().trimmed();

          if (u.isEmpty() || code.isEmpty())
            return jsonError("bad_input", "email and code required", QHttpServerResponse::StatusCode::BadRequest);

          QSqlDatabase db = QSqlDatabase::database();
          QSqlQuery q(db);

          q.prepare("SELECT code, expires_at FROM phone_codes WHERE username=:u;");
          q.bindValue(":u", u);
          if (!q.exec() || !q.next())
            return jsonError("not_found", "No code", QHttpServerResponse::StatusCode::NotFound);

          const QString dbCode = q.value(0).toString();
          const qint64 exp = q.value(1).toLongLong();

          if (QDateTime::currentSecsSinceEpoch() > exp)
            return jsonError("expired", "Code expired", QHttpServerResponse::StatusCode::BadRequest);

          if (dbCode != code)
            return jsonError("wrong_code", "Wrong code", QHttpServerResponse::StatusCode::BadRequest);

          QSqlQuery uupd(db);
          uupd.prepare("UPDATE users SET phone_verified=1 WHERE username=:u;");
          uupd.bindValue(":u", u);
          uupd.exec();

          QSqlQuery del(db);
          del.prepare("DELETE FROM phone_codes WHERE username=:u;");
          del.bindValue(":u", u);
          del.exec();

          return QHttpServerResponse(QJsonObject{{"ok", true}});
        });

  // GET /api/users/me (requires a Bearer token)
  http.route("/api/users/me", QHttpServerRequest::Method::Get,
        [&](const QHttpServerRequest& req) -> QHttpServerResponse
        {
          const QString u = tokenUsername(QString::fromUtf8(req.value("Authorization")));
          if (u.isEmpty())
            return jsonError("unauthorized", "Missing/invalid Authorization header",
                    QHttpServerResponse::StatusCode::Unauthorized);

          QSqlDatabase db = QSqlDatabase::database();
          QSqlQuery q(db);
          q.prepare("SELECT first_name, last_name, email, phone, phone_verified, is_admin "
              "FROM users WHERE username=:u;");
          q.bindValue(":u", u);

          if (!q.exec() || !q.next())
            return jsonError("user_not_found", "User not found",
                    QHttpServerResponse::StatusCode::NotFound);

          return QHttpServerResponse(QJsonObject{
            {"username", u},
            {"first_name", q.value(0).toString()},
            {"last_name", q.value(1).toString()},
            {"email",   q.value(2).toString()},
            {"phone",   q.value(3).toString()},
            {"phone_verified", q.value(4).toInt() != 0},
            {"is_admin",  q.value(5).toInt() != 0}
          });
        });




  // POST /api/users/me/update (saves profile contact data)
  http.route("/api/users/me/update", QHttpServerRequest::Method::Post,
        [&](const QHttpServerRequest& req) -> QHttpServerResponse
        {
          const QString u = tokenUsername(QString::fromUtf8(req.value("Authorization")));
          if (u.isEmpty())
            return jsonError("unauthorized", "Missing/invalid Authorization header",
                    QHttpServerResponse::StatusCode::Unauthorized);

          QJsonParseError pe;
          const QJsonDocument doc = QJsonDocument::fromJson(req.body(), &pe);
          if (pe.error != QJsonParseError::NoError || !doc.isObject())
            return jsonError("bad_json", "Invalid JSON body", QHttpServerResponse::StatusCode::BadRequest);

          const QJsonObject o = doc.object();
          const QString first = o.value("first_name").toString().trimmed();
          const QString last = o.value("last_name").toString().trimmed();
          const QString email = o.value("email").toString().trimmed().toLower();
          const QString phone = o.value("phone").toString().trimmed();

          if (first.isEmpty() || last.isEmpty() || email.isEmpty())
            return jsonError("bad_input", "first_name/last_name/email required",
                    QHttpServerResponse::StatusCode::BadRequest);

          QSqlDatabase db = QSqlDatabase::database();
          QSqlQuery q(db);
          q.prepare("UPDATE users SET first_name=:fn, last_name=:ln, email=:em, phone=:ph WHERE username=:u;");
          q.bindValue(":fn", first);
          q.bindValue(":ln", last);
          q.bindValue(":em", email);
          q.bindValue(":ph", phone);
          q.bindValue(":u", u);

          if (!q.exec())
            return jsonError("db", q.lastError().text(), QHttpServerResponse::StatusCode::InternalServerError);

          return QHttpServerResponse(QJsonObject{
            {"ok", true},
            {"username", u},
            {"first_name", first},
            {"last_name", last},
            {"email", email},
            {"phone", phone}
          });
        });

  // POST /api/users/password (changes the user password)
  http.route("/api/users/password", QHttpServerRequest::Method::Post,
        [&](const QHttpServerRequest& req) -> QHttpServerResponse
        {
          const QString u = tokenUsername(QString::fromUtf8(req.value("Authorization")));
          if (u.isEmpty())
            return jsonError("unauthorized", "Missing/invalid Authorization header",
                    QHttpServerResponse::StatusCode::Unauthorized);

          QJsonParseError pe;
          const QJsonDocument doc = QJsonDocument::fromJson(req.body(), &pe);
          if (pe.error != QJsonParseError::NoError || !doc.isObject())
            return jsonError("bad_json", "Invalid JSON body", QHttpServerResponse::StatusCode::BadRequest);

          const QJsonObject o = doc.object();
          const QString current = o.value("current_password").toString();
          const QString next = o.value("new_password").toString();

          if (current.isEmpty() || next.size() < 6)
            return jsonError("bad_input", "current_password and new_password >= 6 required",
                    QHttpServerResponse::StatusCode::BadRequest);

          QSqlDatabase db = QSqlDatabase::database();
          QSqlQuery q(db);
          q.prepare("SELECT password_hash FROM users WHERE username=:u;");
          q.bindValue(":u", u);
          if (!q.exec() || !q.next())
            return jsonError("user_not_found", "User not found", QHttpServerResponse::StatusCode::NotFound);

          if (q.value(0).toString() != hashPassword(current))
            return jsonError("bad_password", "Wrong current password", QHttpServerResponse::StatusCode::Unauthorized);

          QSqlQuery upd(db);
          upd.prepare("UPDATE users SET password_hash=:h WHERE username=:u;");
          upd.bindValue(":h", hashPassword(next));
          upd.bindValue(":u", u);
          if (!upd.exec())
            return jsonError("db", upd.lastError().text(), QHttpServerResponse::StatusCode::InternalServerError);

          return QHttpServerResponse(QJsonObject{{"ok", true}});
        });

  // GET /api/products
  http.route("/api/products", QHttpServerRequest::Method::Get,
        [&](const QHttpServerRequest&) -> QHttpServerResponse
        {
          QSqlDatabase db = QSqlDatabase::database();
          QSqlQuery q(db);

          if (!q.exec("SELECT id, name, category, color, size, price, "
               "image_path, image_path2, image_path3, "
               "material, season, style_tag "
               "FROM products ORDER BY id ASC;"))
          {
            return jsonError("db", q.lastError().text(),
                    QHttpServerResponse::StatusCode::InternalServerError);
          }

          QJsonArray arr;
          while (q.next()) {
            arr.push_back(QJsonObject{
              {"id", q.value(0).toInt()},
              {"name", q.value(1).toString()},
              {"category", q.value(2).toString()},
              {"color", q.value(3).toString()},
              {"size", q.value(4).toString()},
              {"price", q.value(5).toDouble()},
              {"image_path", q.value(6).toString()},
              {"image_path2", q.value(7).toString()},
              {"image_path3", q.value(8).toString()},
              {"material",  q.value(9).toString()},
              {"season",   q.value(10).toString()},
              {"style_tag",  q.value(11).toString()}
            });
          }
          return QHttpServerResponse(QJsonObject{{"products", arr}});
        });

  // POST /api/admin/products (admin)
  http.route("/api/admin/products", QHttpServerRequest::Method::Post,
        [&](const QHttpServerRequest& req) -> QHttpServerResponse
  {
          QString adminUser;
          std::optional<QHttpServerResponse> err;
          if (!requireAdmin(req, &adminUser, &err))
            return std::move(*err);


    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(req.body(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
      return jsonError("bad_json", "Invalid JSON body", QHttpServerResponse::StatusCode::BadRequest);

    const QJsonObject o = doc.object();

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("INSERT INTO products(name, category, color, size, price, image_path, image_path2, image_path3, material, season, style_tag) "
        "VALUES(:name,:cat,:color,:size,:price,:img1,:img2,:img3,:mat,:season,:tag);");

    q.bindValue(":name",  o.value("name").toString());
    q.bindValue(":cat",  o.value("category").toString());
    q.bindValue(":color", o.value("color").toString());
    q.bindValue(":size",  o.value("size").toString());
    q.bindValue(":price", o.value("price").toDouble());
    q.bindValue(":img1",  o.value("image_path").toString());
    q.bindValue(":img2",  o.value("image_path2").toString());
    q.bindValue(":img3",  o.value("image_path3").toString());
    q.bindValue(":mat",  o.value("material").toString());
    q.bindValue(":season", o.value("season").toString());
    q.bindValue(":tag",  o.value("style_tag").toString());

    if (!q.exec())
      return jsonError("db", q.lastError().text(), QHttpServerResponse::StatusCode::InternalServerError);

    const int newId = q.lastInsertId().toInt();
    broadcast(QJsonObject{{"type","products_changed"}, {"by", adminUser}, {"product_id", newId}});

    return QHttpServerResponse(QJsonObject{{"ok", true}, {"id", newId}});
  });

  // GET /api/orders (user)
  http.route("/api/orders", QHttpServerRequest::Method::Get,
        [&](const QHttpServerRequest& req) -> QHttpServerResponse
  {
    const QString u = tokenUsername(QString::fromUtf8(req.value("Authorization")));
    if (u.isEmpty())
      return jsonError("unauthorized", "Missing/invalid Authorization header",
               QHttpServerResponse::StatusCode::Unauthorized);

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("SELECT id, created_at, total, status, payment_method, items_count, address "
        "FROM orders WHERE username=:u ORDER BY id DESC;");
    q.bindValue(":u", u);

    if (!q.exec())
      return jsonError("db", q.lastError().text(), QHttpServerResponse::StatusCode::InternalServerError);

    QJsonArray arr;
    while (q.next()) {
      arr.push_back(QJsonObject{
        {"id", q.value(0).toInt()},
        {"created_at", q.value(1).toString()},
        {"total", q.value(2).toDouble()},
        {"status", q.value(3).toString()},
        {"payment_method", q.value(4).toString()},
        {"items_count", q.value(5).toInt()},
        {"address", q.value(6).toString()}
      });
    }
    return QHttpServerResponse(QJsonObject{{"orders", arr}});
  });

  // GET /api/admin/orders (admin)
  http.route("/api/admin/orders", QHttpServerRequest::Method::Get,
        [&](const QHttpServerRequest& req) -> QHttpServerResponse
  {
          QString adminUser;
          std::optional<QHttpServerResponse> err;
          if (!requireAdmin(req, &adminUser, &err))
            return std::move(*err);



    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    if (!q.exec("SELECT id, username, created_at, total, status, payment_method, items_count, address "
         "FROM orders ORDER BY id DESC;"))
      return jsonError("db", q.lastError().text(), QHttpServerResponse::StatusCode::InternalServerError);

    QJsonArray arr;
    while (q.next()) {
      arr.push_back(QJsonObject{
        {"id", q.value(0).toInt()},
        {"username", q.value(1).toString()},
        {"created_at", q.value(2).toString()},
        {"total", q.value(3).toDouble()},
        {"status", q.value(4).toString()},
        {"payment_method", q.value(5).toString()},
        {"items_count", q.value(6).toInt()},
        {"address", q.value(7).toString()}
      });
    }
    return QHttpServerResponse(QJsonObject{{"orders", arr}});
  });

  // POST /api/admin/orders/status (admin) body example: { "order_id": 1, "status": "in_progress" }
  http.route("/api/admin/orders/status", QHttpServerRequest::Method::Post,
        [&](const QHttpServerRequest& req) -> QHttpServerResponse
  {
          QString adminUser;
          std::optional<QHttpServerResponse> err;
          if (!requireAdmin(req, &adminUser, &err))
            return std::move(*err);

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(req.body(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
      return jsonError("bad_json", "Invalid JSON body", QHttpServerResponse::StatusCode::BadRequest);

    const QJsonObject o = doc.object();
    const int orderId = o.value("order_id").toInt();
    const QString status = o.value("status").toString();

    if (orderId <= 0 || status.isEmpty())
      return jsonError("bad_input", "order_id and status required", QHttpServerResponse::StatusCode::BadRequest);

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("UPDATE orders SET status=:s WHERE id=:id;");
    q.bindValue(":s", status);
    q.bindValue(":id", orderId);

    if (!q.exec())
      return jsonError("db", q.lastError().text(), QHttpServerResponse::StatusCode::InternalServerError);

    broadcast(QJsonObject{{"type","order_status_changed"}, {"by", adminUser}, {"order_id", orderId}, {"status", status}});
    return QHttpServerResponse(QJsonObject{{"ok", true}});
  });

  // QtHttpServer::listen() returns the selected port; 0 means that listening failed.
  // ---- Start the HTTP server through QTcpServer ----
  QTcpServer* tcp = new QTcpServer(&a);

  if (!tcp->listen(QHostAddress::AnyIPv4, 8080)) {
    qWarning() << "TCP listen error on 8080:" << tcp->errorString();
    return 1;
  }

  if (!http.bind(tcp)) {
    qWarning() << "HTTP bind() failed";
    return 1;
  }


  qInfo() << "Server started.";
  qInfo() << "HTTP: 0.0.0.0:8080 WS: 0.0.0.0:8081";
  return a.exec();
}
