#pragma once

/*
  File: server/src/ServerCommon.h
  Purpose: Shared server-side Qt includes used by the local API server headers.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include <optional>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QSslSocket>
#include <QRandomGenerator>
#include <utility>
#include <QDebug>
#include <QDateTime>
#include <QTcpServer>


#include <QtHttpServer/QHttpServer>
#include <QtHttpServer/QHttpServerRequest>
#include <QtHttpServer/QHttpServerResponse>
#include <QWebSocketServer>
#include <QWebSocket>
