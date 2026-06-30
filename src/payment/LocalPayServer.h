#pragma once

/*
  File: src/payment/LocalPayServer.h
  Purpose: Tiny local HTTP server that simulates payment success or failure callbacks.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "../app/AppCommon.h"

class LocalPayServer : public QObject {
  Q_OBJECT
public:
  enum Status { PENDING, PAID, FAILED };

  explicit LocalPayServer(QObject *parent = nullptr) : QObject(parent) {
    connect(&m_srv, &QTcpServer::newConnection, this, &LocalPayServer::onNewConn);
  }

  bool start(quint16 port = 8787) {
    if (m_srv.isListening()) return true;
    return m_srv.listen(QHostAddress::AnyIPv4, port);
  }

  quint16 port() const { return m_srv.serverPort(); }

  QString lanIp() const {
    // Get the first available IPv4 address from the local network.
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const auto &iface : ifaces) {
      if (!(iface.flags() & QNetworkInterface::IsUp) ||
        (iface.flags() & QNetworkInterface::IsLoopBack))
        continue;
      for (const auto &addr : iface.addressEntries()) {
        if (addr.ip().protocol() == QAbstractSocket::IPv4Protocol) {
          auto ip = addr.ip().toString();
          if (ip.startsWith("169.254.")) continue; // skip the autoconfiguration address
          return ip;
        }
      }
    }
    return "127.0.0.1";
  }

  // Create a payment for the specified amount in kopecks.
  QString createPayment(int amountCents) {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_pay[id] = { amountCents, PENDING };
    return id;
  }

  // Get the payment status.
  Status status(const QString &id) const {
    return m_pay.contains(id) ? m_pay[id].st : FAILED;
  }

signals:
  void paymentChanged(const QString &id);

private:
  struct PayRec { int amountCents = 0; Status st = PENDING; };
  QTcpServer m_srv;
  QHash<QString, PayRec> m_pay;

  // Build an HTTP response.
  static QByteArray httpResponse(const QByteArray &body,
                  const QByteArray &contentType = "text/html; charset=utf-8",
                  int code = 200)
  {
    QByteArray head = "HTTP/1.1 " + QByteArray::number(code) + " OK\r\n";
    head += "Content-Type: " + contentType + "\r\n";
    head += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    head += "Connection: close\r\n\r\n";
    return head + body;
  }

  // Handle an incoming HTTP request.
  void onNewConn() {
    while (m_srv.hasPendingConnections()) {
      QTcpSocket *s = m_srv.nextPendingConnection();
      connect(s, &QTcpSocket::readyRead, this, [this, s]() {
        QByteArray req = s->readAll();
        // First request line, for example: "GET /path?x=y HTTP/1.1".
        int eol = req.indexOf("\r\n");
        if (eol <= 0) { s->disconnectFromHost(); return; }
        QByteArray firstLine = req.left(eol);
        QList<QByteArray> parts = firstLine.split(' ');
        if (parts.size() < 2) { s->disconnectFromHost(); return; }
        QByteArray target = parts[1];

        // Parse the URL.
        QUrl url;
        url.setScheme("http");
        url.setHost("local");
        url.setPath(QString::fromUtf8(target));
        // Parse query parameters manually for a relative URL.
        QByteArray path = target;
        QByteArray query;
        int qpos = target.indexOf('?');
        if (qpos >= 0) {
          path = target.left(qpos);
          query = target.mid(qpos + 1);
        }
        QUrlQuery q(QString::fromUtf8(query));

        if (path == "/") {
          // Root server page used for a simple health check.
          QByteArray body = "<h3>LocalPayServer работает</h3>";
          s->write(httpResponse(body));
          s->disconnectFromHost();
          return;
        }

        if (path == "/pay") {
          QString id = q.queryItemValue("id");
          QString act = q.queryItemValue("act"); // act can be "ok" or "fail"

          if (!m_pay.contains(id)) {
            s->write(httpResponse("<h3>Неизвестный paymentId</h3>", "text/html; charset=utf-8", 404));
            s->disconnectFromHost();
            return;
          }

          // Handle the payment action.
          if (act == "ok") m_pay[id].st = PAID;
          if (act == "fail") m_pay[id].st = FAILED;
          emit paymentChanged(id);

          // Build the HTML response page.
          int rub = m_pay[id].amountCents / 100;
          QByteArray body =
           "<html><body style='font-family:Segoe UI;padding:18px'>"
           "<h2>СберPay (тест)</h2>"
           "<p>Сумма: <b>" + QByteArray::number(rub) + " ₽</b></p>"
                         "<p>Платёж: <code>" + id.toUtf8() + "</code></p>"
                   "<a href='/pay?id=" + id.toUtf8() + "&act=ok'>Оплатить</a><br><br>"
                   "<a href='/pay?id=" + id.toUtf8() + "&act=fail'>Отмена</a>"
                   "<p style='color:#666;margin-top:16px'>После нажатия вернитесь в приложение.</p>"
                   "</body></html>";
          s->write(httpResponse(body));
          s->disconnectFromHost();
          return;
        }

        if (path == "/status") {
          QString id = q.queryItemValue("id");
          QJsonObject o;
          o["id"] = id;
          if (!m_pay.contains(id)) {
            o["status"] = "UNKNOWN";
          } else {
            auto st = m_pay[id].st;
            o["status"] = (st == PENDING ? "PENDING" : st == PAID ? "PAID" : "FAILED");
          }
          QByteArray body = QJsonDocument(o).toJson(QJsonDocument::Compact);
          s->write(httpResponse(body, "application/json; charset=utf-8"));
          s->disconnectFromHost();
          return;
        }

        // If the request is not recognized.
        s->write(httpResponse("<h3>404 Not Found</h3>", "text/html; charset=utf-8", 404));
        s->disconnectFromHost();
      });
    }
  }
};

// Global pointer to the single payment server instance used by the app.
static LocalPayServer *g_paySrv = nullptr;
