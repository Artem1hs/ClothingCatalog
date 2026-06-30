#pragma once

/**
 * @file ServerDatabase.h
 * @brief SQLite schema creation, migrations, demo administrator creation, and product import.
 */
#include "ServerCommon.h"
#include <QDir>
#include <QFileInfo>
#include <QStringList>

/**
 * @brief Opens or creates the SQLite database next to the server executable.
 */
static bool openServerDb()
{
  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
  // The database file name can be changed here if needed.
  db.setDatabaseName(QCoreApplication::applicationDirPath() + "/clothing.db");
  if (!db.open()) {
    qWarning() << "DB open error:" << db.lastError().text();
    return false;
  }
  return true;
}

/**
 * @brief Creates the server database schema when required.
 * @param err Optional error output.
 * @return True when all schema operations succeed.
 */
static bool ensureTables(QString* err)
{
  QSqlDatabase db = QSqlDatabase::database();
  QSqlQuery q(db);

  auto execOr = [&](const QString& sql)->bool{
    if (!q.exec(sql)) {
      if (err) *err = q.lastError().text();
      return false;
    }
    return true;
  };

  if (!execOr("CREATE TABLE IF NOT EXISTS users("
       "username TEXT PRIMARY KEY,"
       "password_hash TEXT NOT NULL,"
       "is_admin INTEGER DEFAULT 0"
       ");")) return false;

  if (!execOr("CREATE TABLE IF NOT EXISTS products("
       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
       "name TEXT NOT NULL,"
       "category TEXT,"
       "color TEXT,"
       "size TEXT,"
       "price REAL,"
       "image_path TEXT,"
       "image_path2 TEXT,"
       "image_path3 TEXT,"
       "material TEXT,"
       "season TEXT,"
       "style_tag TEXT"
       ");")) return false;

  auto ensureCol = [&](const QString& col) {
    QSqlQuery q2(db);
    q2.exec("PRAGMA table_info(products);");
    bool found = false;
    while (q2.next()) if (q2.value(1).toString() == col) { found = true; break; }
    if (!found) {
      QSqlQuery q3(db);
      q3.exec("ALTER TABLE products ADD COLUMN " + col + " TEXT;");
    }
  };

  ensureCol("image_path2");
  ensureCol("image_path3");


  if (!execOr("CREATE TABLE IF NOT EXISTS orders("
       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
       "username TEXT,"
       "created_at TEXT,"
       "total REAL DEFAULT 0,"
       "status TEXT DEFAULT 'оформлен',"
       "payment_method TEXT,"
       "items_count INTEGER DEFAULT 0,"
       "address TEXT"
       ");")) return false;

  if (!execOr("CREATE TABLE IF NOT EXISTS order_items("
       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
       "order_id INTEGER,"
       "product_id INTEGER,"
       "quantity INTEGER,"
       "price REAL"
       ");")) return false;

  if (!execOr("CREATE TABLE IF NOT EXISTS phone_codes("
       "username TEXT PRIMARY KEY,"
       "code TEXT NOT NULL,"
       "expires_at INTEGER NOT NULL"
       ");")) return false;


  return true;
}

/**
 * @brief Adds missing user profile columns for older local databases.
 */
static bool ensureUsersColumns(QString* err)
{
  QSqlDatabase db = QSqlDatabase::database();
  QSqlQuery q(db);

  auto hasCol = [&](const QString& col)->bool{
    QSqlQuery t(db);
    if (!t.exec("PRAGMA table_info(users);")) return false;
    while (t.next()) {
      if (t.value(1).toString() == col) return true;
    }
    return false;
  };

  auto addCol = [&](const QString& col, const QString& type)->bool{
    if (hasCol(col)) return true;
    if (!q.exec(QString("ALTER TABLE users ADD COLUMN %1 %2;").arg(col, type))) {
      if (err) *err = q.lastError().text();
      return false;
    }
    return true;
  };

  if (!addCol("first_name", "TEXT")) return false;
  if (!addCol("last_name","TEXT")) return false;
  if (!addCol("email",  "TEXT")) return false;
  if (!addCol("phone",  "TEXT")) return false;
  if (!addCol("phone_verified", "INTEGER DEFAULT 0")) return false;

  return true;
}

/**
 * @brief Creates or updates the local demonstration administrator account.
 */
static bool ensureAdminUser(QString* err)
{
  QSqlDatabase db = QSqlDatabase::database();

  const QString adminHash = QString(
    QCryptographicHash::hash("admin", QCryptographicHash::Sha256).toHex()
    );

  QSqlQuery q(db);
  q.prepare("SELECT COUNT(*) FROM users WHERE username='admin';");

  if (!q.exec()) {
    if (err) *err = q.lastError().text();
    return false;
  }

  int count = 0;
  if (q.next())
    count = q.value(0).toInt();

  if (count == 0) {
    QSqlQuery ins(db);
    ins.prepare(
     "INSERT INTO users(username, password_hash, is_admin) "
     "VALUES('admin', :h, 1);"
      );
    ins.bindValue(":h", adminHash);

    if (!ins.exec()) {
      if (err) *err = ins.lastError().text();
      return false;
    }
  } else {
    QSqlQuery upd(db);
    upd.prepare(
     "UPDATE users SET password_hash=:h, is_admin=1 "
     "WHERE username='admin';"
      );
    upd.bindValue(":h", adminHash);

    if (!upd.exec()) {
      if (err) *err = upd.lastError().text();
      return false;
    }
  }

  return true;
}

static QStringList legacyDemoProductNames()
{
  return {
   "Чёрная бейсболка Basic",
   "Бежевая бейсболка Basic",
   "Молочная бейсболка Basic",
   "Карамельная бейсболка Basic",
   "Песочная панама летняя",
   "Чёрная футболка с логотипом",
   "Белая базовая футболка",
   "Красная футболка с принтом",
   "Синяя оверсайз футболка",
   "Бежевая худи минималистичная",
   "Чёрная классическая худи",
   "Молочный свитшот",
   "Графитовые брюки"
  };
}

static bool removeLegacyDemoProducts(QString* err)
{
  QSqlDatabase db = QSqlDatabase::database();
  for (const QString &name : legacyDemoProductNames()) {
    QSqlQuery del(db);
    del.prepare("DELETE FROM products WHERE name=?");
    del.addBindValue(name);
    if (!del.exec()) {
      if (err) *err = del.lastError().text();
      return false;
    }
  }
  return true;
}

/**
 * @brief Finds a product image folder near the executable or in source-tree fallback paths.
 */
static bool findProductsImageDir(QDir *outDir)
{
  const QString base = QCoreApplication::applicationDirPath();
  const QStringList candidates = {
    base + "/image",
    base + "/../image",
    base + "/../../image",
    base + "/../../../image",
    base + "/../../../../image",
    base + "/../../../../../image"
  };

  for (const QString &candidate : candidates) {
    QDir dir(candidate);
    if (dir.exists() && !dir.entryList({"*.png", "*.jpg", "*.jpeg", "*.webp"}, QDir::Files).isEmpty()) {
      *outDir = dir;
      return true;
    }
  }

  return false;
}

static int productNumberFromFile(const QString &fileName)
{
  QString digits;
  const QString stem = QFileInfo(fileName).completeBaseName();
  for (const QChar ch : stem) {
    if (ch.isDigit())
      digits += ch;
  }
  return digits.toInt();
}

static QString productCategoryFromImage(const QString &fileName)
{
  const QString f = fileName.toLower();
  if (f.contains("sneakers") || f.contains("keds") || f.contains("shoe")) return "Обувь";
  if (f.contains("cap") || f.contains("panama")) return "Головной убор";
  if (f.contains("tshirt")) return "Футболка";
  if (f.contains("hoodie")) return "Худи";
  if (f.contains("sweatshirt")) return "Свитшот";
  if (f.contains("longsleeve")) return "Лонгслив";
  if (f.contains("shirt")) return "Рубашка";
  if (f.contains("bomber") || f.contains("denimjacket")) return "Куртка";
  if (f.contains("sweater")) return "Свитер";
  if (f.contains("cardigan")) return "Кардиган";
  if (f.contains("blazer")) return "Пиджак";
  if (f.contains("shorts")) return "Шорты";
  if (f.contains("skirt")) return "Юбка";
  if (f.contains("pants") || f.contains("joggers") || f.contains("jeans") || f.contains("chinos") || f.contains("cargo")) return "Брюки";
  return "Товар";
}

static QString productNameFromImage(const QString &fileName)
{
  const QString category = productCategoryFromImage(fileName);
  const QString f = fileName.toLower();
  const QString stem = QFileInfo(fileName).completeBaseName().toLower();
  const int n = productNumberFromFile(fileName);

  if (stem == "tshirt14") return "Футболка базовая";
  if (stem == "tshirt15") return "Футболка модная";
  if (stem == "tshirt16") return "Футболка оверсайз";
  if (stem == "tshirt17") return "Футболка streetwear";
  if (stem == "tshirt18") return "Футболка минималистичная";
  if (stem == "tshirt19") return "Футболка винтажная";
  if (stem == "tshirt20") return "Футболка спортивная";
  if (stem == "tshirt21") return "Футболка графическая";
  if (stem == "tshirt22") return "Футболка с маленьким принтом";
  if (stem == "tshirt23") return "Футболка с большим принтом";
  if (stem == "tshirt24") return "Футболка с надписью";
  if (stem == "tshirt25") return "Футболка relaxed fit";
  if (stem == "tshirt26") return "Футболка boxy fit";
  if (stem == "tshirt27") return "Футболка с вышивкой";

  if (stem == "hoodie3") return "Базовое худи";
  if (stem == "hoodie4") return "Модное худи";
  if (stem == "hoodie5") return "Оверсайз худи";
  if (stem == "hoodie6") return "Streetwear худи";
  if (stem == "hoodie7") return "Минималистичное худи";
  if (stem == "hoodie8") return "Худи с принтом";
  if (stem == "hoodie9") return "Худи с маленьким логотипом";
  if (stem == "hoodie10") return "Худи на молнии";

  if (stem == "sweatshirt2") return "Свитшот";
  if (stem == "longsleeve1") return "Лонгслив";
  if (stem == "shirt1") return "Рубашка";
  if (stem == "bomber1") return "Бомбер";
  if (stem == "denimjacket1") return "Джинсовая куртка";
  if (stem == "sweater1") return "Свитер";
  if (stem == "cardigan1") return "Кардиган";
  if (stem == "blazer1") return "Пиджак";

  if (stem == "jeans1") return "Прямые джинсы";
  if (stem == "jeans2") return "Широкие джинсы";
  if (stem == "cargo1") return "Cargo pants";
  if (stem == "sportpants1") return "Спортивные брюки";
  if (stem == "pants2") return "Базовые брюки";
  if (stem == "chinos1") return "Чиносы";
  if (stem == "joggers1") return "Джоггеры";
  if (stem == "shorts1") return "Шорты";
  if (stem == "skirtmini1") return "Юбка mini";
  if (stem == "skirtmidi1") return "Юбка midi";

  if (stem == "sneakers1") return "Базовые кроссовки";
  if (stem == "sneakers2") return "Модные кроссовки";
  if (stem == "sneakers3") return "Streetwear sneakers";
  if (stem == "keds_white1") return "Белые кеды";
  if (stem == "keds_black1") return "Чёрные кеды";

  if (category == "Головной убор") {
    if (f.contains("panama")) return "Панама Sand";
    if (f.contains("black")) return "Кепка Black";
    if (f.contains("beige")) return "Кепка Beige";
    if (f.contains("milk")) return "Кепка Milk";
    if (f.contains("caramel")) return "Кепка Caramel";
  }

  if (category == "Брюки") {
    QString type = "Брюки";
    if (f.contains("sportpants")) type = "Спортивные брюки";
    else if (f.contains("joggers")) type = "Джоггеры";
    else if (f.contains("jeans")) type = "Джинсы";
    else if (f.contains("chinos")) type = "Чиносы";
    else if (f.contains("cargo")) type = "Карго";
    if (n > 0)
      return QString("%1 %2").arg(type).arg(n);
    return type;
  }

  if (category == "Обувь")
    return f.contains("keds") ? "Кеды" : "Кроссовки";

  if (n > 0)
    return QString("%1 %2").arg(category).arg(n);

  QString displayStem = QFileInfo(fileName).completeBaseName();
  displayStem.replace('_', ' ');
  return category + " " + displayStem;
}

static QString productColorFromImage(const QString &fileName)
{
  const QString f = fileName.toLower();
  if (f.contains("black")) return "Чёрный";
  if (f.contains("white")) return "Белый";
  if (f.contains("beige")) return "Бежевый";
  if (f.contains("milk")) return "Молочный";
  if (f.contains("caramel")) return "Карамельный";
  if (f.contains("sand")) return "Песочный";

  const QStringList palette = {"Чёрный", "Белый", "Синий", "Красный", "Зелёный", "Графит", "Молочный", "Бежевый"};
  const int n = qMax(1, productNumberFromFile(fileName));
  return palette[(n - 1) % palette.size()];
}

static QString productSizeFromImage(const QString &fileName)
{
  const QString category = productCategoryFromImage(fileName);
  if (category == "Головной убор")
    return "one size";
  if (category == "Обувь") {
    const QString stem = QFileInfo(fileName).completeBaseName().toLower();
    if (stem == "sneakers1") return "41";
    if (stem == "sneakers2") return "42";
    if (stem == "sneakers3") return "43";
    if (stem == "keds_white1") return "40";
    if (stem == "keds_black1") return "41";
    const QStringList shoeSizes = {"39", "40", "41", "42", "43"};
    const int n = qMax(1, productNumberFromFile(fileName));
    return shoeSizes[(n - 1) % shoeSizes.size()];
  }

  const QStringList sizes = {"S", "M", "L", "XL"};
  const int n = qMax(1, productNumberFromFile(fileName));
  return sizes[(n - 1) % sizes.size()];
}

static double productPriceFromImage(const QString &fileName)
{
  const QString category = productCategoryFromImage(fileName);
  const int n = qMax(1, productNumberFromFile(fileName));
  if (category == "Футболка") return 1990 + (n % 6) * 180;
  if (category == "Худи") return 4290 + (n % 5) * 250;
  if (category == "Свитшот") return 3790;
  if (category == "Лонгслив") return 2990;
  if (category == "Рубашка") return 3490;
  if (category == "Куртка") return 6990;
  if (category == "Свитер" || category == "Кардиган") return 4590;
  if (category == "Пиджак") return 7490;
  if (category == "Брюки" || category == "Юбка" || category == "Шорты") return 5290;
  if (category == "Обувь") return fileName.toLower().contains("keds") ? 3990 + (n % 2) * 250 : 5490 + (n % 3) * 400;
  if (category == "Головной убор") return 1590 + (n % 3) * 150;
  return 2490;
}

static QString productMaterialFromImage(const QString &fileName)
{
  const QString category = productCategoryFromImage(fileName);
  const QString f = fileName.toLower();
  if (category == "Обувь") return f.contains("keds") ? "Текстиль" : "Текстиль, экокожа";
  if (category == "Худи" || category == "Свитшот" || category == "Свитер" || category == "Кардиган" || category == "Лонгслив") return "Трикотаж";
  if (category == "Куртка") return f.contains("denim") ? "Деним" : "Полиэстер";
  if (category == "Пиджак") return "Костюмная ткань";
  if (category == "Брюки" || category == "Юбка" || category == "Шорты") return "Хлопок";
  return "Хлопок";
}

static QString productSeasonFromImage(const QString &fileName)
{
  const QString category = productCategoryFromImage(fileName);
  const QString f = fileName.toLower();
  if (category == "Обувь") return "Демисезон";
  if (category == "Футболка" || category == "Шорты" || f.contains("panama")) return "Лето";
  if (category == "Худи" || category == "Свитшот" || category == "Свитер" || category == "Кардиган" || category == "Куртка") return "Осень";
  return "Демисезон";
}

static QString productStyleFromImage(const QString &fileName)
{
  const QString f = fileName.toLower();
  const QString stem = QFileInfo(fileName).completeBaseName().toLower();
  if (f.contains("streetwear") || stem == "tshirt17" || stem == "hoodie6" || stem == "sneakers3") return "streetwear";
  if (f.contains("minimal") || stem == "tshirt18" || stem == "hoodie7") return "minimal basic";
  if (f.contains("oversize") || stem == "tshirt16" || stem == "hoodie5" || stem == "tshirt26") return "oversize casual";
  if (f.contains("vintage") || stem == "tshirt19") return "vintage";
  if (stem == "tshirt20" || f.contains("sport") || f.contains("sneakers")) return "sport casual";
  if (f.contains("keds") || f.contains("basic") || stem == "tshirt14" || stem == "hoodie3") return "casual basic";
  return "casual";
}

/**
 * @brief Synchronizes catalog products with available files in the image folder.
 */
static bool ensureImageFolderProducts(QString* err)
{
  if (!removeLegacyDemoProducts(err))
    return false;

  QDir imageDir;
  if (!findProductsImageDir(&imageDir)) {
    qWarning() << "Products image folder not found";
    return true;
  }

  QSqlDatabase db = QSqlDatabase::database();
  const QStringList files = imageDir.entryList({"*.png", "*.jpg", "*.jpeg", "*.webp"}, QDir::Files, QDir::Name);

  for (const QString &file : files) {
    const QString category = productCategoryFromImage(file);
    const QString name = productNameFromImage(file);

    QSqlQuery exists(db);
    exists.prepare("SELECT COUNT(*) FROM products WHERE image_path=? OR name=?");
    exists.addBindValue(file);
    exists.addBindValue(name);
    if (!exists.exec() || !exists.next()) {
      if (err) *err = exists.lastError().text();
      return false;
    }
    if (exists.value(0).toInt() > 0) {
      QSqlQuery upd(db);
      upd.prepare("UPDATE products SET name=?, category=?, color=?, size=?, price=?, material=?, season=?, style_tag=? "
           "WHERE image_path=? OR name=?");
      upd.addBindValue(name);
      upd.addBindValue(category);
      upd.addBindValue(productColorFromImage(file));
      upd.addBindValue(productSizeFromImage(file));
      upd.addBindValue(productPriceFromImage(file));
      upd.addBindValue(productMaterialFromImage(file));
      upd.addBindValue(productSeasonFromImage(file));
      upd.addBindValue(productStyleFromImage(file));
      upd.addBindValue(file);
      upd.addBindValue(name);
      if (!upd.exec()) {
        if (err) *err = upd.lastError().text();
        return false;
      }
      continue;
    }

    QSqlQuery ins(db);
    ins.prepare("INSERT INTO products(name, category, color, size, price, image_path, image_path2, image_path3, material, season, style_tag) "
         "VALUES(?,?,?,?,?,?,?,?,?,?,?)");
    ins.addBindValue(name);
    ins.addBindValue(category);
    ins.addBindValue(productColorFromImage(file));
    ins.addBindValue(productSizeFromImage(file));
    ins.addBindValue(productPriceFromImage(file));
    ins.addBindValue(file);
    ins.addBindValue(QString());
    ins.addBindValue(QString());
    ins.addBindValue(productMaterialFromImage(file));
    ins.addBindValue(productSeasonFromImage(file));
    ins.addBindValue(productStyleFromImage(file));

    if (!ins.exec()) {
      if (err) *err = ins.lastError().text();
      return false;
    }
  }

  return true;
}
