#pragma once

/**
 * @file ApiClient.h
 * @brief Local HTTP API client used by the Qt Widgets application.
 */
#include "../models/DataModels.h"

/**
 * @brief Singleton wrapper around local API requests.
 *
 * The class stores the current session token, prepares authenticated JSON requests,
 * and converts server responses into application data models.
 */
class ApiClient : public QObject {
  Q_OBJECT
public:
  /**
   * @brief Returns the shared API client instance.
   */
  static ApiClient& instance() {
    static ApiClient inst;
    return inst;
  }

  void setBaseUrl(const QUrl& url) { m_baseUrl = url; }
  QUrl baseUrl() const { return m_baseUrl; }

  QString token() const { return m_token; }
  QString username() const { return m_username; }
  bool isAdmin() const { return m_isAdmin; }

  /**
   * @brief Sends credentials to the API server and stores the returned session token.
   * @param username User login.
   * @param password User password.
   * @param err Optional error output.
   * @return True when authentication succeeds.
   */
  bool login(const QString& username, const QString& password, QString* err = nullptr) {
    QJsonObject req{
      {"username", username},
      {"password", password}
    };
    QJsonObject resp;
    int http = 0;
    if (!postJson("/api/auth/login", req, &resp, &http, err)) return false;

    // Expected response: { token, username, is_admin }
    m_token = resp.value("token").toString();
    m_username = resp.value("username").toString(username);
    m_isAdmin = resp.value("is_admin").toBool(false);

    if (m_token.isEmpty()) {
      if (err) *err = "Сервер не вернул token.";
      return false;
    }
    return true;
  }

  /**
   * @brief Creates a user and returns a demo phone confirmation code.
   */
  bool registerUserEx(const QString& first,
            const QString& last,
            const QString& email,
            const QString& phone,
            const QString& password,
            QString* outPhoneCode,
            QString* err = nullptr)
  {
    QJsonObject req{
      {"first_name", first},
      {"last_name", last},
      {"email", email},
      {"phone", phone},
      {"password", password}
    };
    QJsonObject resp;
    int http = 0;
    if (!postJson("/api/auth/register", req, &resp, &http, err)) return false;
    if (outPhoneCode) *outPhoneCode = resp.value("phone_code").toString();
    return true;
  }

  bool confirmPhone(const QString& email, const QString& code, QString* err = nullptr)
  {
    QJsonObject req{
      {"email", email},
      {"code", code}
    };
    QJsonObject resp;
    int http = 0;
    return postJson("/api/auth/confirm_phone", req, &resp, &http, err);
  }

  bool getMe(QJsonObject* out, QString* err = nullptr)
  {
    QJsonObject resp;
    int http = 0;
    if (!getJson("/api/users/me", &resp, &http, err)) return false;
    if (out) *out = resp;
    return true;
  }

  bool updateMe(const QString& first,
         const QString& last,
         const QString& email,
         const QString& phone,
         QJsonObject* out = nullptr,
         QString* err = nullptr)
  {
    QJsonObject req{
      {"first_name", first},
      {"last_name", last},
      {"email", email},
      {"phone", phone}
    };
    QJsonObject resp;
    int http = 0;
    if (!postJson("/api/users/me/update", req, &resp, &http, err)) return false;
    if (out) *out = resp;
    return true;
  }

  bool changePassword(const QString& currentPassword,
            const QString& newPassword,
            QString* err = nullptr)
  {
    QJsonObject req{
      {"current_password", currentPassword},
      {"new_password", newPassword}
    };
    QJsonObject resp;
    int http = 0;
    return postJson("/api/users/password", req, &resp, &http, err);
  }

  /**
   * @brief Downloads product data and converts JSON objects to Product structures.
   * @param out Output product list.
   * @param err Optional error output.
   * @return True when the request succeeds and the response is valid.
   */
  bool getProducts(QVector<Product>* out, QString* err = nullptr) {
    QJsonObject resp;
    int http = 0;
    if (!getJson("/api/products", &resp, &http, err)) return false;

    // Expected response: { products: [ {...}, {...} ] }
    QJsonArray arr = resp.value("products").toArray();
    QVector<Product> list;
    list.reserve(arr.size());

    for (const auto& v : arr) {
      QJsonObject o = v.toObject();
      Product p;
      p.id    = o.value("id").toInt();
      p.name   = o.value("name").toString();
      p.category = o.value("category").toString();
      p.color   = o.value("color").toString();
      p.size   = o.value("size").toString();
      p.price   = o.value("price").toDouble();
      p.imagePath = o.value("image_path").toString();
      p.imagePath2 = o.value("image_path2").toString();
      p.imagePath3 = o.value("image_path3").toString();
      p.material = o.value("material").toString();
      p.season  = o.value("season").toString();
      p.styleTag = o.value("style_tag").toString();
      list.push_back(p);
    }

    *out = std::move(list);
    return true;
  }

private:
  ApiClient() {
    m_mgr = new QNetworkAccessManager(this);
  }

  /**
   * @brief Builds an authenticated JSON request for the configured local API path.
   */
  QNetworkRequest makeRequest(const QString& path) const {
    QUrl url = m_baseUrl;
    url.setPath(path);
    QNetworkRequest r(url);
    r.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    if (!m_token.isEmpty()) {
      r.setRawHeader("Authorization", QByteArray("Bearer ") + m_token.toUtf8());
    }
    return r;
  }

  bool getJson(const QString& path, QJsonObject* out, int* httpCode, QString* err) {
    QNetworkRequest req = makeRequest(path);
    QNetworkReply* rep = m_mgr->get(req);
    return waitJson(rep, out, httpCode, err);
  }

  bool postJson(const QString& path, const QJsonObject& body, QJsonObject* out, int* httpCode, QString* err) {
    QNetworkRequest req = makeRequest(path);
    QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QNetworkReply* rep = m_mgr->post(req, data);
    return waitJson(rep, out, httpCode, err);
  }

  /**
   * @brief Waits for a network reply and parses its JSON object body.
   */
  bool waitJson(QNetworkReply* rep, QJsonObject* out, int* httpCode, QString* err) {
    QEventLoop loop;
    connect(rep, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (httpCode)
      *httpCode = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    QByteArray body = rep->readAll();

    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
    QJsonObject obj;
    if (pe.error == QJsonParseError::NoError && doc.isObject())
      obj = doc.object();

    if (rep->error() != QNetworkReply::NoError) {
      if (err) {
        QString serverMsg = obj.value("message").toString().trimmed();
        if (!serverMsg.isEmpty())
          *err = serverMsg;
        else
          *err = "Ошибка запроса: " + rep->errorString();
      }
      rep->deleteLater();
      return false;
    }

    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
      if (err)
        *err = "Сервер вернул не-JSON или не-object: " + QString::fromUtf8(body.left(200));
      rep->deleteLater();
      return false;
    }

    if (out)
      *out = obj;

    rep->deleteLater();
    return true;
  }

private:
  QNetworkAccessManager* m_mgr = nullptr;
  QUrl m_baseUrl = QUrl("http://127.0.0.1:8080"); // Local API server address.
  QString m_token;
  QString m_username;
  bool m_isAdmin = false;
};


// ================== Theme and QSS helper functions ==================
