#pragma once

/*
  File: src/dialogs/RecommendationsDialog.h
  Purpose: Recommendation results dialog.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "ProductDetailsDialog.h"
#include "../data/DatabaseService.h"

class RecommendationsDialog : public QDialog
{
public:
  RecommendationsDialog(const QString &username,
             const QVector<Product> &products,
             CartManager &cart,
             QWidget *parent = nullptr)
    : QDialog(parent)
    , m_username(username)
    , m_products(products)
    , m_cart(cart)
  {
    setWindowTitle("Рекомендуемые товары");
    resize(900, 600);

    QVBoxLayout *root = new QVBoxLayout(this);

    QLabel *title = new QLabel("Подборка для: " + m_username, this);
    QFont f = title->font();
    f.setPointSize(13);
    f.setBold(true);
    title->setFont(f);
    root->addWidget(title);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    root->addWidget(scroll);

    QWidget *container = new QWidget(scroll);
    scroll->setWidget(container);

    m_gridLayout = new QGridLayout(container);
    container->setLayout(m_gridLayout);

    rebuildGrid(m_products);
  }

private:
  QString m_username;
  QVector<Product> m_products;
  CartManager &m_cart;

  QWidget *m_gridWidget = nullptr;
  QGridLayout *m_gridLayout = nullptr;

  QWidget *createProductCard(const Product &p)
  {
    QFrame *card = new QFrame();
    card->setObjectName("ProductCard");
    card->setFrameShape(QFrame::StyledPanel);
    card->setMinimumSize(220, 220);

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(6);

    // ---------- Photo ----------
    QLabel *image = new QLabel(card);
    image->setAlignment(Qt::AlignCenter);
    image->setStyleSheet("border:none; background:transparent;");
    image->setMinimumHeight(90);

    if (!p.imagePath.isEmpty()) {
      QString fullPath = resolveImagePath(p.imagePath);
      QPixmap pix(fullPath);
      if (!pix.isNull()) {
        image->setPixmap(pix.scaled(160, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        image->setText("");
      } else {
        image->setText("[фото]");
      }
    } else {
      image->setText("[фото]");
    }
    layout->addWidget(image);

    // ---------- Name ----------
    QLabel *name = new QLabel(p.name, card);
    name->setWordWrap(true);
    QFont nf = name->font();
    nf.setBold(true);
    name->setFont(nf);
    layout->addWidget(name);

    // ---------- Category | Color | Size ----------
    QLabel *info = new QLabel(
      QString("%1 | %2 | %3")
        .arg(p.category)
        .arg(p.color)
        .arg(p.size),
      card);
    info->setWordWrap(true);
    info->setStyleSheet("color:#666666; font-size:11px;");
    layout->addWidget(info);

    // ---------- Material | Season | Style ----------
    auto safe = [](const QString &s) -> QString {
      return s.trimmed().isEmpty() ? "—" : s.trimmed();
    };

    QLabel *meta = new QLabel(
      QString("%1 | %2 | %3")
        .arg(safe(p.material))
        .arg(safe(p.season))
        .arg(safe(p.styleTag)),
      card);
    meta->setWordWrap(true);
    meta->setStyleSheet("color:#8C8F95; font-size:11px;");
    layout->addWidget(meta);

    // ---------- Price ----------
    QLabel *price = new QLabel(
      QString("%1 ₽").arg(QString::number(p.price, 'f', 2)),
      card);
    QFont pf = price->font();
    pf.setBold(true);
    price->setFont(pf);
    layout->addWidget(price);

    // ---------- Button ----------
    QPushButton *addBtn = new QPushButton("Добавить в корзину", card);
    layout->addWidget(addBtn);

    QObject::connect(addBtn, &QPushButton::clicked,
             card, [this, p]() {
               m_cart.addProduct(p, 1);
               QMessageBox::information(nullptr, "Корзина",
                          "Товар добавлен в корзину.");
             });

    return card;
  }


  void rebuildGrid(const QVector<Product> &list)
  {
    const int columns = 3;
    int row = 0, col = 0;

    for (const Product &p : list) {
      m_gridLayout->addWidget(createProductCard(p), row, col);
      if (++col == columns) { col = 0; ++row; }
    }

    if (list.isEmpty()) {
      QLabel *lbl = new QLabel("Ничего не найдено.");
      lbl->setAlignment(Qt::AlignCenter);
      m_gridLayout->addWidget(lbl, row, 0, 1, columns);
    }
  }
};
