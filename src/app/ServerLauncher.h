#pragma once

/*
  File: src/app/ServerLauncher.h
  Purpose: Helper for starting ClothingServer.exe next to the desktop application executable.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "AppCommon.h"

static bool isPortOpen(quint16 port)
{
  QTcpSocket s;
  s.connectToHost(QHostAddress::LocalHost, port);
  return s.waitForConnected(150);
}

static QProcess* startLocalServer()
{
  if (isPortOpen(8080))
    return nullptr; // The server is already running.

  const QString dir = QCoreApplication::applicationDirPath();
  const QString serverExe = dir + "/ClothingServer.exe";

  if (!QFileInfo::exists(serverExe)) {
    QMessageBox::critical(nullptr, "Ошибка",
              "Не найден ClothingServer.exe рядом с приложением:\n" + serverExe);
    return nullptr;
  }

  auto *p = new QProcess;
  p->setWorkingDirectory(dir);
  p->setProgram(serverExe);
  p->start();

  // Wait until the local server opens port 8080.
  for (int i = 0; i < 50; ++i) { // about 5 seconds
    if (isPortOpen(8080))
      return p;
    QThread::msleep(100);
  }

  QMessageBox::critical(nullptr, "Ошибка", "Сервер не поднялся (порт 8080 не открылся).");
  p->kill();
  p->deleteLater();
  return nullptr;
}

// ================== Entry point helpers ==================
