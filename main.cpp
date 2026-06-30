/*
  File: main.cpp
  Purpose: Application entry point. Starts the local API server, runs the login loop, opens the main catalog window, and shuts the server down cleanly.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "src/app/AppCommon.h"

#include "src/models/DataModels.h"
#include "src/api/ApiClient.h"
#include "src/style/AppTheme.h"
#include "src/data/DatabaseService.h"
#include "src/cart/CartManager.h"
#include "src/dialogs/ProductDetailsDialog.h"
#include "src/services/ProductLoader.h"
#include "src/payment/LocalPayServer.h"
#include "src/dialogs/PaymentDialog.h"
#include "src/dialogs/CartDialog.h"
#include "src/dialogs/RecommendationsDialog.h"
#include "src/dialogs/UserProfileDialog.h"
#include "src/dialogs/MyOrdersDialog.h"
#include "src/dialogs/QuickRecommendDialog.h"
#include "src/dialogs/ProductEditDialog.h"
#include "src/dialogs/AdminDialog.h"
#include "src/widgets/ProductCardFrame.h"
#include "src/windows/CatalogWindow.h"
#include "src/auth/AuthDialogs.h"
#include "src/app/ServerLauncher.h"

int main(int argc, char *argv[])
{
  // QApplication owns the GUI event loop and must be created before any widgets.
  QApplication app(argc, argv);

  // Start the local API server before showing the client UI.
  QProcess* serverProc = startLocalServer();

  // Point the desktop client to the local API server.
  ApiClient::instance().setBaseUrl(QUrl("http://127.0.0.1:8080"));

  app.setWindowIcon(QIcon("icon.ico"));
  setupLightTheme(app);

  QFile styleFile(":/style/minimal.qss");
  if (styleFile.open(QFile::ReadOnly)) {
    app.setStyleSheet(styleFile.readAll());
  } else {
    qDebug() << "Не удалось открыть :/style/minimal.qss";
  }

  // A custom exit code is used to return from the main window back to the login dialog.
  const int LogoutCode = 1234;

  while (true) {
    // The login dialog is modal. If the user cancels it, the application exits.
    LoginDialog login;
    if (login.exec() != QDialog::Accepted)
      break;

    if (!QSqlDatabase::contains("qt_sql_default_connection") || !QSqlDatabase::database().isOpen()) {
      if (!openDatabase())
        return 1;
    }

    // The main window receives the authenticated user name and role.
    CatalogWindow w(login.username(), login.isAdmin());


    QObject::connect(&w, &CatalogWindow::logoutRequested,
             [&app, LogoutCode]() {
               app.exit(LogoutCode);
             });

    w.resize(1300, 800);

    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeo = screen->availableGeometry();
    QRect winGeo = w.frameGeometry();
    winGeo.moveCenter(screenGeo.center());
    w.move(winGeo.topLeft());

    w.show();

    int code = app.exec();
    if (code == LogoutCode)
      continue;
    break;
  }

  // Stop the local API server process before exiting the application.
  if (serverProc) {
    serverProc->terminate();
    serverProc->waitForFinished(1000);
    serverProc->kill();
    serverProc->deleteLater();
  }

  return 0;
}

