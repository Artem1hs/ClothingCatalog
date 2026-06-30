# ClothingCatalog

ClothingCatalog is a desktop C++/Qt application for a clothing store prototype. It includes registration, login, a product catalog, filtering, cart, checkout, order history, recommendations, lookbook projects, and an administrator panel.

The application uses a local Qt-based API server and stores data in an SQLite database. The repository also includes a standalone QML demo, CI configuration, and Doxygen configuration.

## Features

- user registration and login;
- phone confirmation demo flow;
- product catalog with images;
- product filtering by category, size, color, and season;
- cart and checkout flow;
- demo QR/card payment screens;
- user order history;
- administrator panel for products and orders;
- product recommendations based on profile and budget;
- lookbook builder with outfit slots;
- local HTTP API server based on Qt HttpServer;
- WebSocket notifications for order status changes;
- SQLite database storage.

## Technologies

- C++17;
- Qt 6;
- Qt Widgets;
- Qt SQL;
- Qt Network;
- Qt WebSockets;
- Qt HttpServer;
- Qt Quick/QML demo;
- SQLite;
- CMake;
- GitHub Actions CI;
- Doxygen.

## Project structure

```text
ClothingCatalog/
├── .github/workflows/   # CI configuration
├── assets/              # Interface images used by the home/projects screens
├── image/               # Product images loaded into the catalog database
├── qml-demo/            # Standalone Qt Quick/QML demo
├── server/              # Local API server source code
├── src/                 # Desktop application source code
├── CMakeLists.txt       # Build configuration for app and local server
├── Doxyfile             # Doxygen documentation configuration
├── main.cpp             # Desktop application entry point
├── minimal.qss          # Qt stylesheet
└── resources.qrc        # Qt resource list
```

## Main modules

- `main.cpp` starts the local API server, shows the login dialog, opens the main catalog window, and shuts the server down on exit.
- `server/server_main.cpp` contains the HTTP API routes and the WebSocket notification server.
- `src/api/ApiClient.h` is the desktop client's wrapper around local HTTP API calls.
- `src/data/DatabaseService.h` contains local SQLite helpers and image path resolution.
- `server/src/ServerDatabase.h` creates and updates server tables and imports products from the `image/` folder.
- `src/windows/CatalogWindow.h` contains the main application window, catalog layout, navigation, lookbook UI, and embedded cart panel.
- `src/dialogs/` contains modal windows for authentication, cart, payments, orders, profile editing, product editing, and recommendations.

## QML demo

The `qml-demo/` folder contains a standalone Qt Quick demo screen. It is separated from the main Qt Widgets application and can be used to show basic QML layout, properties, reusable visual blocks, and a desktop dashboard interface.

Build the QML demo separately:

```bash
cmake -S qml-demo -B build-qml-demo
cmake --build build-qml-demo
```

Or enable it from the root CMake project:

```bash
cmake -S . -B build -DBUILD_QML_DEMO=ON
cmake --build build
```

## Demo administrator account

An administrator account is created automatically when the database is initialized:

```text
login: admin
password: admin
```

This account is intended only for local demonstration. Do not use it in production.

## Build

Open the project in Qt Creator or build it with CMake:

```bash
cmake -S . -B build
cmake --build build
```

The project builds two executables:

- `ClothingCatalog` — desktop client;
- `ClothingServer` — local API server used by the desktop client.

For running the application on another Windows computer, use `windeployqt` after building so that the required Qt libraries are copied next to the executable files.

## CI/CD

The repository includes a GitHub Actions workflow in `.github/workflows/cmake.yml`. The workflow installs Qt, configures the project with CMake, builds the desktop client and local server, and uploads build artifacts.

## Doxygen

Generate HTML documentation with:

```bash
doxygen Doxyfile
```

The generated documentation is written to `docs/html`.

## Environment variables

SMTP settings and demo payment requisites are read from environment variables. The example file is `.env.example`.

```text
SMTP_USER=example@gmail.com
SMTP_PASS=app_password_here
PAYMENT_REQUISITE_NUMBER=70000000000
PAYMENT_BANK_CODE=100000000000
```

Never commit a real `.env` file or real passwords to GitHub.

## Release package

A release archive should include the source code, assets, product images, QML demo, CI configuration, Doxygen configuration, and example environment file. It should not include build folders, local databases, executables, DLL files, logs, temporary files, or secret configuration files.
