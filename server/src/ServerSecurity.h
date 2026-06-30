#pragma once

/*
  File: server/src/ServerSecurity.h
  Purpose: Demo password hashing and token helpers for the local API server.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "ServerCommon.h"

static QString hashPassword(const QString& pass)
{
  return QString(QCryptographicHash::hash(pass.toUtf8(), QCryptographicHash::Sha256).toHex());
}

// Demo authorization token in the "ok:<username>" format.
static QString makeToken(const QString& username)
{
  return "ok:" + username;
}

static QString tokenUsername(const QString& authHeader)
{
  // Expected header format: "Bearer ok:<username>".
  const QString h = authHeader.trimmed();
  if (!h.startsWith("Bearer ", Qt::CaseInsensitive)) return {};

  const QString tok = h.mid(QString("Bearer ").size()).trimmed();
  if (!tok.startsWith("ok:")) return {};

  return tok.mid(3);
}

static bool isAdminUser(const QString& username)
{
  QSqlDatabase db = QSqlDatabase::database();
  if (!db.isOpen()) return false;

  QSqlQuery q(db);
  q.prepare("SELECT is_admin FROM users WHERE username=:u;");
  q.bindValue(":u", username);

  if (!q.exec() || !q.next()) return false;
  return (q.value(0).toInt() != 0);
}
