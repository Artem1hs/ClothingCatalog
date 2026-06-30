#pragma once

/*
  File: src/services/ProductLoader.h
  Purpose: Legacy product-loading helper kept for compatibility with earlier UI code.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "../models/DataModels.h"

inline void loadProducts()
{
  QSqlDatabase db = QSqlDatabase::database();
  if (!db.isOpen()) {
    qWarning("DB is not open");
    return;
  }
  QSqlQuery q(db);
  if (!q.exec("SELECT id, name, category, color, size, price, "
       "image_path, image_path2, image_path3, "
       "material, season, style_tag "
       "FROM products;"))
  {
    qWarning() << "SELECT products error:" << q.lastError().text();
    return;
  }


  while (q.next()) {
    Product p;
    p.id     = q.value(0).toInt();
    p.name    = q.value(1).toString();
    p.category  = q.value(2).toString();
    p.color   = q.value(3).toString();
    p.size    = q.value(4).toString();
    p.price   = q.value(5).toDouble();
    p.imagePath = q.value(6).toString();
    p.imagePath2 = q.value(7).toString();
    p.imagePath3 = q.value(8).toString();
    p.material  = q.value(9).toString();
    p.season   = q.value(10).toString();
    p.styleTag  = q.value(11).toString();

  }

  // The UI can be refreshed after products are loaded if needed.
  // rebuildGrid(m_products);
}



