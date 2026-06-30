#pragma once

/*
  File: server/src/ServerResponses.h
  Purpose: Small helpers for JSON API responses and administrator access checks.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "ServerSecurity.h"

static QHttpServerResponse jsonError(const QString& code,
                  const QString& msg,
                  QHttpServerResponse::StatusCode st)
{
  return QHttpServerResponse(QJsonObject{{"error", code}, {"message", msg}}, st);
}

static bool requireAdmin(const QHttpServerRequest& req,
             QString* outUser,
             std::optional<QHttpServerResponse>* outErr)
{
  const QString auth = QString::fromUtf8(req.value("Authorization"));
  const QString u = tokenUsername(auth);

  if (outUser) *outUser = u;

  if (u.isEmpty()) {
    if (outErr) {
      *outErr = jsonError("unauthorized",
               "Missing/invalid Authorization header",
                QHttpServerResponse::StatusCode::Unauthorized);
    }
    return false;
  }

  if (!isAdminUser(u)) {
    if (outErr) {
      *outErr = jsonError("forbidden",
               "Admin only",
                QHttpServerResponse::StatusCode::Forbidden);
    }
    return false;
  }

  return true;
}
