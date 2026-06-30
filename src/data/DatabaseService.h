#pragma once

/*
  File: src/data/DatabaseService.h
  Purpose: Desktop-side SQLite helpers, product loading, image path resolution, and order/cart persistence.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "../models/DataModels.h"
#include "../api/ApiClient.h"

static bool ensureOrdersSchema(QSqlDatabase db);
static bool ensureLookbookProjectsSchema(QSqlDatabase db);

// Creates application tables if they are missing. Safe to call on every startup.
inline bool ensureTables(QSqlDatabase &db)
{
  if (!db.isOpen())
    return false;

  QSqlQuery q(db);

  if (!q.exec(
     "CREATE TABLE IF NOT EXISTS products ("
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
     ");"))
  {
    qDebug() << "Create products table error:" << q.lastError().text();
    return false;
  }

  auto ensureCol = [&](const QString& col) {
    QSqlQuery q2(db);
    q2.exec("PRAGMA table_info(products);");
    bool found = false;
    while (q2.next()) {
      if (q2.value(1).toString() == col) { found = true; break; }
    }
    if (!found) {
      QSqlQuery q3(db);
      q3.exec("ALTER TABLE products ADD COLUMN " + col + " TEXT;");
    }
  };

  ensureCol("image_path2");
  ensureCol("image_path3");


  if (!q.exec(
     "CREATE TABLE IF NOT EXISTS users ("
     "username TEXT PRIMARY KEY,"
     "password_hash TEXT NOT NULL,"
     "is_admin INTEGER DEFAULT 0"
     ");"))
  {
    qDebug() << "Create users table error:" << q.lastError().text();
    return false;
  }

  if (!q.exec(
     "CREATE TABLE IF NOT EXISTS user_profiles ("
     "username TEXT PRIMARY KEY,"
     "gender TEXT,"
     "height INTEGER,"
     "style TEXT,"
     "budget TEXT,"
     "favorite_colors TEXT"
     ");"))
  {
    qDebug() << "Create user_profiles table error:" << q.lastError().text();
    return false;
  }

  if (!q.exec(
     "CREATE TABLE IF NOT EXISTS orders ("
     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
     "username TEXT,"
     "created_at TEXT,"
     "total REAL,"
     "status TEXT DEFAULT 'формируется',"
     "payment_method TEXT,"
     "items_count INTEGER DEFAULT 0,"
     "address TEXT"
     ");"))
  {
    qDebug() << "Create orders table error:" << q.lastError().text();
    return false;
  }

  if (!q.exec(
     "CREATE TABLE IF NOT EXISTS order_items ("
     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
     "order_id INTEGER,"
     "product_id INTEGER,"
     "quantity INTEGER,"
     "price REAL"
     ");"))
  {
    qDebug() << "Create order_items table error:" << q.lastError().text();
    return false;
  }

  // Lookbook: project records and items inside each project.
  if (!q.exec(
     "CREATE TABLE IF NOT EXISTS lookbook_projects ("
     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
     "username TEXT NOT NULL,"
     "title TEXT NOT NULL,"
     "created_at TEXT"
     ");"))
  {
    qDebug() << "Create lookbook_projects table error:" << q.lastError().text();
    return false;
  }

  // position: 0=headwear, 1=top, 2=bottom, 3=shoes.
  if (!q.exec(
     "CREATE TABLE IF NOT EXISTS lookbook_items ("
     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
     "project_id INTEGER NOT NULL,"
     "position INTEGER NOT NULL,"
     "product_id INTEGER NOT NULL"
     ");"))
  {
    qDebug() << "Create lookbook_items table error:" << q.lastError().text();
    return false;
  }

  if (!ensureLookbookProjectsSchema(db))
    return false;

  if (!ensureOrdersSchema(db))
    return false;

  return true;
}


static bool ensureLookbookProjectsSchema(QSqlDatabase db)
{
  QSet<QString> cols;
  QSqlQuery q(db);
  if (q.exec("PRAGMA table_info(lookbook_projects);")) {
    while (q.next())
      cols.insert(q.value(1).toString());
  }

  auto addCol = [&](const QString &name, const QString &ddl) {
    if (cols.contains(name))
      return true;
    QSqlQuery a(db);
    if (!a.exec(ddl)) {
      qDebug() << "ALTER lookbook_projects add column failed" << name << a.lastError().text();
      return false;
    }
    cols.insert(name);
    return true;
  };

  if (!addCol("cover_path", "ALTER TABLE lookbook_projects ADD COLUMN cover_path TEXT;"))
    return false;
  if (!addCol("description", "ALTER TABLE lookbook_projects ADD COLUMN description TEXT;"))
    return false;
  if (!addCol("access_level", "ALTER TABLE lookbook_projects ADD COLUMN access_level TEXT DEFAULT 'private';"))
    return false;
  if (!addCol("comments_enabled", "ALTER TABLE lookbook_projects ADD COLUMN comments_enabled INTEGER DEFAULT 1;"))
    return false;
  if (!addCol("ratings_enabled", "ALTER TABLE lookbook_projects ADD COLUMN ratings_enabled INTEGER DEFAULT 1;"))
    return false;
  if (!addCol("currency", "ALTER TABLE lookbook_projects ADD COLUMN currency TEXT DEFAULT 'RUB';"))
    return false;
  if (!addCol("show_prices", "ALTER TABLE lookbook_projects ADD COLUMN show_prices INTEGER DEFAULT 1;"))
    return false;
  if (!addCol("affiliate_enabled", "ALTER TABLE lookbook_projects ADD COLUMN affiliate_enabled INTEGER DEFAULT 0;"))
    return false;
  if (!addCol("share_token", "ALTER TABLE lookbook_projects ADD COLUMN share_token TEXT;"))
    return false;

  return true;
}


static bool ensureOrdersSchema(QSqlDatabase db)
{
  // Schema migration: add missing columns to the orders table.
  QSet<QString> cols;
  QSqlQuery q(db);
  if (q.exec("PRAGMA table_info(orders);")) {
    while (q.next()) {
      cols.insert(q.value(1).toString()); // column name
    }
  }

  auto addCol = [&](const QString &name, const QString &ddl) {
    if (cols.contains(name))
      return true;
    QSqlQuery a(db);
    if (!a.exec(ddl)) {
      qDebug() << "ALTER orders add column failed" << name << a.lastError().text();
      return false;
    }
    cols.insert(name);
    return true;
  };

  // SQLite supports adding columns through ALTER TABLE ... ADD COLUMN.
  if (!addCol("status", "ALTER TABLE orders ADD COLUMN status TEXT DEFAULT 'формируется';"))
    return false;
  if (!addCol("payment_method", "ALTER TABLE orders ADD COLUMN payment_method TEXT;"))
    return false;
  if (!addCol("items_count", "ALTER TABLE orders ADD COLUMN items_count INTEGER DEFAULT 0;"))
    return false;
  if (!addCol("address", "ALTER TABLE orders ADD COLUMN address TEXT;"))
    return false;

  // ===== Project tables =====
  if (!q.exec(
     "CREATE TABLE IF NOT EXISTS projects ("
     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
     "username TEXT NOT NULL,"
     "title TEXT NOT NULL,"
     "created_at TEXT NOT NULL DEFAULT (datetime('now','localtime'))"
     ");"))
    return false;

  if (!q.exec(
     "CREATE TABLE IF NOT EXISTS project_items ("
     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
     "project_id INTEGER NOT NULL,"
     "product_id INTEGER NOT NULL,"
     "qty INTEGER NOT NULL DEFAULT 1,"
     "FOREIGN KEY(project_id) REFERENCES projects(id) ON DELETE CASCADE"
     ");"))
    return false;

  // Indexes speed up queries and prevent duplicate products inside one project.
  q.exec("CREATE INDEX IF NOT EXISTS idx_projects_user ON projects(username);");
  q.exec("CREATE INDEX IF NOT EXISTS idx_project_items_project ON project_items(project_id);");
  q.exec("CREATE UNIQUE INDEX IF NOT EXISTS uq_project_item ON project_items(project_id, product_id);");

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

static bool removeLegacyDemoProducts(QSqlDatabase &db)
{
  for (const QString &name : legacyDemoProductNames()) {
    QSqlQuery del(db);
    del.prepare("DELETE FROM products WHERE name=?");
    del.addBindValue(name);
    if (!del.exec()) {
      qDebug() << "Delete legacy product error:" << del.lastError().text();
      return false;
    }
  }
  return true;
}

// Finds the product image directory in common development and deployment locations.
static bool findProductsImageDir(QDir *outDir)
{
  const QString appDir = QCoreApplication::applicationDirPath();
  const QString cwd = QDir::currentPath();
  const QStringList candidates = {
    QDir(appDir).absoluteFilePath("image"),
    QDir(appDir).absoluteFilePath("../image"),
    QDir(appDir).absoluteFilePath("../../image"),
    QDir(appDir).absoluteFilePath("../../../image"),
    QDir(appDir).absoluteFilePath("../../../../image"),
    QDir(appDir).absoluteFilePath("../../../../../image"),
    QDir(cwd).absoluteFilePath("image"),
    QDir(cwd).absoluteFilePath("../image"),
    QDir(cwd).absoluteFilePath("../../image"),
    QDir(cwd).absoluteFilePath("../../../image")
  };

  const QStringList filters = {"*.png", "*.jpg", "*.jpeg", "*.webp"};
  for (const QString &path : candidates) {
    QDir dir(path);
    if (dir.exists() && !dir.entryList(filters, QDir::Files, QDir::Name).isEmpty()) {
      if (outDir)
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

// Imports product rows from image files so the catalog can be rebuilt from assets.
static bool ensureImageFolderProducts(QSqlDatabase &db)
{
  QDir imageDir;
  if (!findProductsImageDir(&imageDir)) {
    qDebug() << "Products image folder not found";
    return true;
  }

  const QStringList files = imageDir.entryList({"*.png", "*.jpg", "*.jpeg", "*.webp"}, QDir::Files, QDir::Name);
  for (const QString &file : files) {
    const QString category = productCategoryFromImage(file);
    const QString name = productNameFromImage(file);

    QSqlQuery exists(db);
    exists.prepare("SELECT COUNT(*) FROM products WHERE image_path=? OR name=?");
    exists.addBindValue(file);
    exists.addBindValue(name);
    if (!exists.exec() || !exists.next()) {
      qDebug() << "Check image product error:" << exists.lastError().text();
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
        qDebug() << "Update image product error:" << upd.lastError().text();
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
      qDebug() << "Insert image product error:" << ins.lastError().text();
      return false;
    }
  }

  return true;
}


inline bool openDatabase()
{
  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
  QString dbPath = QCoreApplication::applicationDirPath() + "/clothing.db";
  qDebug() << "DB path =" << dbPath;
  db.setDatabaseName(dbPath);
  if (!db.open()) {
    QMessageBox::critical(nullptr, "Ошибка БД",
              "Не удалось открыть базу данных:\n" + db.lastError().text());
    return false;
  }

  
  // Enable SQLite foreign keys.
  { QSqlQuery fk(db); fk.exec("PRAGMA foreign_keys = ON;"); }

if (!ensureTables(db))
    return false;

  if (!removeLegacyDemoProducts(db))
    return false;
  if (!ensureImageFolderProducts(db))
    return false;

  QSqlQuery q(db);
  if (!q.exec("SELECT COUNT(*) FROM users WHERE username='admin';")) {
    qDebug() << "Count admin error:" << q.lastError().text();
    return false;
  }

  int adminCount = 0;
  if (q.next()) adminCount = q.value(0).toInt();

  if (adminCount == 0) {
    QCryptographicHash h(QCryptographicHash::Sha256);
    h.addData("admin");
    QString hash = QString::fromLatin1(h.result().toHex());

    QSqlQuery ins(db);
    ins.prepare("INSERT INTO users(username, password_hash, is_admin) "
         "VALUES('admin', :h, 1)");
    ins.bindValue(":h", hash);
    if (!ins.exec()) {
      qDebug() << "Insert admin error:" << ins.lastError().text();
    }
  }

  return true;
}

inline QVector<Product> loadProductsFromLocalDb()
{
  QVector<Product> list;
  QSqlDatabase db = QSqlDatabase::database();
  if (!db.isOpen())
    return list;

  QSqlQuery q(db);
  if (!q.exec(
     "SELECT id, name, category, color, size, price, "
     "image_path, image_path2, image_path3, "
     "material, season, style_tag "
     "FROM products ORDER BY id ASC;"))
  {
    qWarning() << "SELECT local products error:" << q.lastError().text();
    return list;
  }

  while (q.next()) {
    Product p;
    p.id = q.value(0).toInt();
    p.name = q.value(1).toString();
    p.category = q.value(2).toString();
    p.color = q.value(3).toString();
    p.size = q.value(4).toString();
    p.price = q.value(5).toDouble();
    p.imagePath = q.value(6).toString();
    p.imagePath2 = q.value(7).toString();
    p.imagePath3 = q.value(8).toString();
    p.material = q.value(9).toString();
    p.season = q.value(10).toString();
    p.styleTag = q.value(11).toString();
    list.push_back(p);
  }

  return list;
}

inline QVector<Product> loadProductsFromDb()
{
  QString err;
  QVector<Product> tmp;
  if (!ApiClient::instance().getProducts(&tmp, &err)) {
    qDebug() << "Load products HTTP error:" << err;
    return loadProductsFromLocalDb();
  }

  return tmp;
}


inline QString hashPassword(const QString &password)
{
  QByteArray ba = QCryptographicHash::hash(password.toUtf8(),
                       QCryptographicHash::Sha256);
  return QString::fromLatin1(ba.toHex());
}


// ------------------ Image path resolution ------------------

// Resolves database image values to real files for both Qt Creator and deployed builds.
inline QString resolveImagePath(const QString &stored)
{
  QString raw = stored.trimmed();
  if (raw.isEmpty())
    return QString();

  raw.replace('\\', '/');

  // 1) Absolute path from the admin panel or database.
  if (QFileInfo(raw).isAbsolute() && QFileInfo::exists(raw))
    return QFileInfo(raw).absoluteFilePath();

  // Remove extra prefixes to avoid paths like image/image/file.png.
  QString fileName = raw;
  while (fileName.startsWith("./"))
    fileName.remove(0, 2);
  if (fileName.startsWith("image/", Qt::CaseInsensitive))
    fileName = fileName.mid(QString("image/").size());
  if (fileName.startsWith("images/", Qt::CaseInsensitive))
    fileName = fileName.mid(QString("images/").size());

  const QString appDir = QCoreApplication::applicationDirPath();
  const QString cwd = QDir::currentPath();

  const QStringList candidates = {
    // Main build/deployment case: the image folder is placed next to the executable.
    QDir(appDir).absoluteFilePath("image/" + fileName),
    QDir(appDir).absoluteFilePath("images/" + fileName),

    // Fallbacks for running from Qt Creator or from a non-standard working directory.
    QDir(appDir).absoluteFilePath("../image/" + fileName),
    QDir(appDir).absoluteFilePath("../images/" + fileName),
    QDir(appDir).absoluteFilePath("../../image/" + fileName),
    QDir(appDir).absoluteFilePath("../../images/" + fileName),
    QDir(appDir).absoluteFilePath("../../../image/" + fileName),
    QDir(appDir).absoluteFilePath("../../../images/" + fileName),
    QDir(appDir).absoluteFilePath("../../../../image/" + fileName),
    QDir(appDir).absoluteFilePath("../../../../images/" + fileName),

    // Handle cases where the working directory differs from the executable directory.
    QDir(cwd).absoluteFilePath("image/" + fileName),
    QDir(cwd).absoluteFilePath("images/" + fileName),
    QDir(cwd).absoluteFilePath("../image/" + fileName),
    QDir(cwd).absoluteFilePath("../images/" + fileName),
    QDir(cwd).absoluteFilePath("../../image/" + fileName),
    QDir(cwd).absoluteFilePath("../../images/" + fileName),

    // Handle database values that already contain a relative subfolder.
    QDir(appDir).absoluteFilePath(raw),
    QDir(cwd).absoluteFilePath(raw)
  };

  for (const QString &path : candidates) {
    if (QFileInfo::exists(path))
      return QFileInfo(path).absoluteFilePath();
  }

  qDebug() << "Image not found. stored=" << stored
       << "normalized=" << fileName
       << "appDir=" << appDir
       << "cwd=" << cwd;

  // Return the expected path to simplify diagnostics.
  return QDir(appDir).absoluteFilePath("image/" + fileName);
}

// ================== Cart ==================
