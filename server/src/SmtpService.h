#pragma once

/*
  File: server/src/SmtpService.h
  Purpose: Minimal SMTP helper used to send an optional registration email when SMTP variables are configured.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "ServerCommon.h"

static bool smtpSendGmailThanks(const QString& toEmail, QString* err)
{
  const QString smtpUser = qEnvironmentVariable("SMTP_USER"); // SMTP account address.
  const QString smtpPass = qEnvironmentVariable("SMTP_PASS"); // SMTP application password.
  if (smtpUser.isEmpty() || smtpPass.isEmpty()) {
    if (err) *err = "SMTP_USER/SMTP_PASS не заданы";
    return false;
  }

  auto readAllWait = [&](QSslSocket& s)->QByteArray {
    s.waitForReadyRead(5000);
    return s.readAll();
  };

  auto sendLine = [&](QSslSocket& s, const QByteArray& line)->bool{
    s.write(line + "\r\n");
    return s.waitForBytesWritten(5000);
  };

  auto expectCode = [&](const QByteArray& resp, const QByteArray& code)->bool{
    return resp.startsWith(code);
  };

  QSslSocket sock;
  sock.connectToHost("smtp.gmail.com", 587);
  if (!sock.waitForConnected(5000)) {
    if (err) *err = "Не удалось подключиться к smtp.gmail.com:587";
    return false;
  }

  QByteArray r = readAllWait(sock);
  if (!expectCode(r, "220")) { if (err) *err = "SMTP: no 220 greeting"; return false; }

  sendLine(sock, "EHLO ClothingServer");
  r = readAllWait(sock);
  if (!expectCode(r, "250")) { if (err) *err = "SMTP: EHLO failed"; return false; }

  sendLine(sock, "STARTTLS");
  r = readAllWait(sock);
  if (!expectCode(r, "220")) { if (err) *err = "SMTP: STARTTLS failed"; return false; }

  sock.startClientEncryption();
  if (!sock.waitForEncrypted(5000)) { if (err) *err = "SMTP: TLS encrypt failed"; return false; }

  sendLine(sock, "EHLO ClothingServer");
  r = readAllWait(sock);
  if (!expectCode(r, "250")) { if (err) *err = "SMTP: EHLO after TLS failed"; return false; }

  // Authenticate on the SMTP server.
  sendLine(sock, "AUTH LOGIN");
  r = readAllWait(sock);
  if (!expectCode(r, "334")) { if (err) *err = "SMTP: AUTH LOGIN not accepted"; return false; }

  sendLine(sock, smtpUser.toUtf8().toBase64());
  r = readAllWait(sock);
  if (!expectCode(r, "334")) { if (err) *err = "SMTP: username rejected"; return false; }

  sendLine(sock, smtpPass.toUtf8().toBase64());
  r = readAllWait(sock);
  if (!expectCode(r, "235")) { if (err) *err = "SMTP: password rejected"; return false; }

  // Send the SMTP MAIL FROM command.
  sendLine(sock, QByteArray("MAIL FROM:<") + smtpUser.toUtf8() + ">");
  r = readAllWait(sock);
  if (!expectCode(r, "250")) { if (err) *err = "SMTP: MAIL FROM failed"; return false; }

  sendLine(sock, QByteArray("RCPT TO:<") + toEmail.toUtf8() + ">");
  r = readAllWait(sock);
  if (!expectCode(r, "250") && !expectCode(r, "251")) { if (err) *err = "SMTP: RCPT TO failed"; return false; }

  sendLine(sock, "DATA");
  r = readAllWait(sock);
  if (!expectCode(r, "354")) { if (err) *err = "SMTP: DATA not accepted"; return false; }

  const QString subject = "Спасибо за регистрацию";
  const QString body = "Спасибо за регистрацию!\r\nДобро пожаловать в приложение.\r\n";

  QByteArray msg;
  msg += "From: <" + smtpUser.toUtf8() + ">\r\n";
  msg += "To: <" + toEmail.toUtf8() + ">\r\n";
  msg += "Subject: " + subject.toUtf8() + "\r\n";
  msg += "Content-Type: text/plain; charset=utf-8\r\n";
  msg += "\r\n";
  msg += body.toUtf8();
  msg += "\r\n.\r\n";

  sock.write(msg);
  if (!sock.waitForBytesWritten(5000)) { if (err) *err = "SMTP: write msg failed"; return false; }

  r = readAllWait(sock);
  if (!expectCode(r, "250")) { if (err) *err = "SMTP: message not accepted"; return false; }

  sendLine(sock, "QUIT");
  sock.disconnectFromHost();
  return true;
}
