cmake_minimum_required(VERSION 3.20)

project(ClothingCatalog VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# If Qt is not found automatically, set the local path to your Qt installation:
# set(CMAKE_PREFIX_PATH "C:/Qt/6.9.3/msvc2022_64")

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Sql Network WebSockets HttpServer)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Sql Network WebSockets HttpServer)

# The desktop client and the local server are built from the same repository.
# Keeping both targets together makes the demo easier to run from Qt Creator.
set(APP_SOURCES
  main.cpp

  src/app/AppCommon.h
  src/app/ServerLauncher.h
  src/models/DataModels.h
  src/api/ApiClient.h
  src/style/AppTheme.h
  src/data/DatabaseService.h
  src/cart/CartManager.h
  src/services/ProductLoader.h
  src/payment/LocalPayServer.h
  src/dialogs/ProductDetailsDialog.h
  src/dialogs/PaymentDialog.h
  src/dialogs/CartDialog.h
  src/dialogs/RecommendationsDialog.h
  src/dialogs/UserProfileDialog.h
  src/dialogs/MyOrdersDialog.h
  src/dialogs/QuickRecommendDialog.h
  src/dialogs/ProductEditDialog.h
  src/dialogs/AdminDialog.h
  src/widgets/ProductCardFrame.h
  src/widgets/CustomTitleBar.h
  src/widgets/RoundCloseButton.h
  src/windows/CatalogWindow.h
  src/auth/AuthDialogs.h
  resources.qrc
)

if (WIN32)
  list(APPEND APP_SOURCES app_icon.rc)
endif()

add_executable(${PROJECT_NAME}
  ${APP_SOURCES}
)

if (WIN32)
  set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::WebSockets
)

# Enable Qt automatic processing for resources, UI files, and the meta-object compiler.
set_target_properties(${PROJECT_NAME} PROPERTIES
  AUTOMOC ON
  AUTORCC ON
  AUTOUIC ON
)

# Force UTF-8 source encoding when building with MSVC.
if (MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()



# -------------------- Local API server target (Qt HTTP + WebSocket) --------------------
# The server target is a separate executable because the client communicates over HTTP.
# This mirrors a real client/server architecture while staying fully local.
set(SERVER_SOURCES
  server/server_main.cpp
  server/src/ServerCommon.h
  server/src/ServerSecurity.h
  server/src/ServerDatabase.h
  server/src/ServerResponses.h
  server/src/SmtpService.h
)

add_executable(ClothingServer
  ${SERVER_SOURCES}
)

if (WIN32)
  set_target_properties(ClothingServer PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_link_libraries(ClothingServer
  PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::WebSockets
    Qt${QT_VERSION_MAJOR}::HttpServer
)

set_target_properties(ClothingServer PROPERTIES
  AUTOMOC ON
)

if (MSVC)
  target_compile_options(ClothingServer PRIVATE /utf-8)
endif()

if (MINGW)
  target_link_libraries(ClothingServer PRIVATE ws2_32 crypt32 bcrypt)
endif()


# Finalize Qt 6 executables after all target properties are configured.
if (QT_VERSION_MAJOR EQUAL 6)
  qt_finalize_executable(${PROJECT_NAME})
  qt_finalize_executable(ClothingServer)
endif()


option(BUILD_QML_DEMO "Build standalone Qt Quick/QML demo" OFF)
if (BUILD_QML_DEMO)
  add_subdirectory(qml-demo)
endif()
