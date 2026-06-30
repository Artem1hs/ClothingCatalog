#pragma once

/*
  File: src/dialogs/AdminDialog.h
  Purpose: Administrator dialog for editing products and changing order statuses.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "ProductEditDialog.h"
#include "../models/DataModels.h"
#include "../data/DatabaseService.h"
#include <QSignalBlocker>

class AdminDialog : public QDialog
{
public:
  AdminDialog(QWidget *parent = nullptr)
    : QDialog(parent)
  {
    setWindowTitle("Администрирование товаров и заказов");

    resize(1000, 800);
    setMinimumSize(1000, 800);
    setSizeGripEnabled(true);

    QVBoxLayout *root = new QVBoxLayout(this);

    QTabWidget *tabs = new QTabWidget(this);
    root->addWidget(tabs);

    QWidget *productsPage = new QWidget(this);
    QVBoxLayout *prodLayout = new QVBoxLayout(productsPage);

    QLabel *title = new QLabel(
     "Управление товарами (картинки из папки image рядом с .exe)", this);
    QFont f = title->font();
    f.setPointSize(11);
    f.setBold(true);
    title->setFont(f);
    prodLayout->addWidget(title);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(12);
    QStringList headers;
    headers << "ID" << "Название" << "Категория" << "Цвет"
        << "Размер" << "Цена, ₽"
        << "Фото 1" << "Фото 2" << "Фото 3"
        << "Материал" << "Сезон" << "Стиль";
    m_table->setHorizontalHeaderLabels(headers);
    m_table->setHorizontalHeaderLabels(headers);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    prodLayout->addWidget(m_table);

    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableWidget::customContextMenuRequested,
        this, &AdminDialog::onTableContextMenu);

    QFormLayout *form = new QFormLayout();

    m_nameEdit = new QLineEdit(this);

    m_categoryBox = new QComboBox(this);
    m_categoryBox->addItems({"Футболка", "Лонгслив", "Майка", "Топ", "Рубашка", "Блузка", "Худи", "Свитшот", "Свитер", "Кардиган", "Жакет", "Пиджак", "Куртка", "Пальто", "Тренч", "Брюки", "Джинсы", "Юбка", "Шорты", "Платье", "Костюм", "Обувь", "Кроссовки", "Кеды", "Ботинки", "Лоферы", "Сумка", "Ремень", "Шарф", "Головной убор", "Шапка", "Кепка", "Аксессуары"});
    m_categoryBox->setEditable(true);

    m_colorEdit = new QLineEdit(this);
    m_colorEdit->setPlaceholderText("Например: чёрный");

    m_sizeBox = new QComboBox(this);
    m_sizeBox->addItems({"XXS", "XS", "S", "M", "L", "XL", "XXL", "3XL", "one size", "35", "36", "37", "38", "39", "40", "41", "42", "43", "44", "45", "46"});
    m_sizeBox->setEditable(true);

    m_priceSpin = new QDoubleSpinBox(this);
    m_priceSpin->setMinimum(0.0);
    m_priceSpin->setMaximum(999999.0);
    m_priceSpin->setDecimals(2);
    m_priceSpin->setSingleStep(100.0);

    m_imagePathEdit = new QLineEdit(this);
    m_imagePathEdit->setPlaceholderText("имя файла, напр. tshirt1.png");

    m_imagePathEdit2 = new QLineEdit(this);
    m_imagePathEdit2->setPlaceholderText("2-е фото (опционально), напр. tshirt1_2.png");

    m_imagePathEdit3 = new QLineEdit(this);
    m_imagePathEdit3->setPlaceholderText("3-е фото (опционально), напр. tshirt1_3.png");


    auto makeImgRow = [&](QLineEdit *edit, const QString &title) -> QWidget* {
      QPushButton *btn = new QPushButton("Обзор...", this);
      connect(btn, &QPushButton::clicked, this, [this, edit, title]() {
        QString base = QCoreApplication::applicationDirPath() + "/image";
        QString file = QFileDialog::getOpenFileName(
          this, title, base,
         "Изображения (*.png *.jpg *.jpeg *.bmp)");
        if (!file.isEmpty()) {
          QFileInfo fi(file);
          edit->setText(fi.fileName()); // only the file name is stored
        }
      });

      QWidget *row = new QWidget(this);
      QHBoxLayout *lay = new QHBoxLayout(row);
      lay->setContentsMargins(0,0,0,0);
      lay->addWidget(edit);
      lay->addWidget(btn);
      return row;
    };

    QWidget *imgRowWidget1 = makeImgRow(m_imagePathEdit,"Выберите 1-е фото");
    QWidget *imgRowWidget2 = makeImgRow(m_imagePathEdit2, "Выберите 2-е фото");
    QWidget *imgRowWidget3 = makeImgRow(m_imagePathEdit3, "Выберите 3-е фото");


    form->addRow("Название:", m_nameEdit);
    form->addRow("Категория:", m_categoryBox);
    form->addRow("Цвет:", m_colorEdit);
    form->addRow("Размер:", m_sizeBox);
    form->addRow("Цена, ₽:", m_priceSpin);
    form->addRow("Фото 1:", imgRowWidget1);
    form->addRow("Фото 2:", imgRowWidget2);
    form->addRow("Фото 3:", imgRowWidget3);

    // Product fields: material, season, and style.
    m_materialEdit = new QLineEdit(this);
    m_materialEdit->setPlaceholderText("например: хлопок, шерсть, кожа");

    m_seasonBox = new QComboBox(this);
    m_seasonBox->addItems({"Весна", "Лето", "Осень", "Зима", "Демисезон", "Всесезонный", "Праздник", "Капсула"});
    m_seasonBox->setEditable(true);

    m_styleBox = new QComboBox(this);
    m_styleBox->addItems({"Кэжуал", "Минималистичный", "Базовый", "Спортивный", "Деловой", "Streetwear", "Smart Casual", "Oversize", "Гранж", "Винтаж", "Классика", "Романтический", "Бохо", "Офисный", "Casual", "Sport", "Business", "Street", "Basic", "Minimal", "Grunge", "Vintage", "Classic", "Romantic", "Boho", "Office"});
    m_styleBox->setEditable(true);

    // Add fields to the form layout.
    form->addRow("Материал:", m_materialEdit);
    form->addRow("Сезон:", m_seasonBox);
    form->addRow("Стиль:", m_styleBox);

    prodLayout->addLayout(form);

    m_status = new QLabel(this);
    m_status->setStyleSheet("color:#388e3c;");
    prodLayout->addWidget(m_status);

    QHBoxLayout *btnsProd = new QHBoxLayout();
    QPushButton *addBtn    = new QPushButton("Добавить товар", this);
    QPushButton *duplicateBtn = new QPushButton("Дублировать выбранный", this);
    QPushButton *fillBtn   = new QPushButton("Заполнить из выбранного", this);
    QPushButton *clearBtn   = new QPushButton("Очистить форму", this);
    QPushButton *deleteBtn  = new QPushButton("Удалить выбранный", this);
    btnsProd->addWidget(addBtn);
    btnsProd->addWidget(duplicateBtn);
    btnsProd->addWidget(fillBtn);
    btnsProd->addWidget(clearBtn);
    btnsProd->addWidget(deleteBtn);
    btnsProd->addStretch();
    prodLayout->addLayout(btnsProd);

    connect(addBtn,    &QPushButton::clicked, this, &AdminDialog::onAdd);
    connect(duplicateBtn, &QPushButton::clicked, this, &AdminDialog::onDuplicate);
    connect(fillBtn,   &QPushButton::clicked, this, &AdminDialog::fillFormFromSelected);
    connect(clearBtn,   &QPushButton::clicked, this, &AdminDialog::clearProductForm);
    connect(deleteBtn,  &QPushButton::clicked, this, &AdminDialog::onDelete);

    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
      editRow(row);
    });

    tabs->addTab(productsPage, "Товары");

    QWidget *ordersPage = new QWidget(this);
    QVBoxLayout *ordersLayout = new QVBoxLayout(ordersPage);

    QLabel *ordTitle = new QLabel("Все заказы", this);
    QFont f2 = ordTitle->font();
    f2.setPointSize(12);
    f2.setBold(true);
    ordTitle->setFont(f2);
    ordersLayout->addWidget(ordTitle);

    m_ordersTable = new QTableWidget(this);
    m_ordersTable->setColumnCount(7);
    QStringList oh;
    oh << "№ заказа" << "Пользователь" << "Дата" << "Кол-во" << "Оплата" << "Сумма, ₽" << "Статус";
    m_ordersTable->setHorizontalHeaderLabels(oh);
    m_ordersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_ordersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_ordersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ordersTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ordersLayout->addWidget(m_ordersTable);

    tabs->addTab(ordersPage, "Заказы");

    QHBoxLayout *btns = new QHBoxLayout();
    QPushButton *closeBtn = new QPushButton("Закрыть", this);
    btns->addStretch();
    btns->addWidget(closeBtn);
    root->addLayout(btns);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    refreshProductsTable();
    refreshOrdersTable();
  }

private:
  QTableWidget *m_table    = nullptr;
  QTableWidget *m_ordersTable = nullptr;
  QLineEdit *m_materialEdit = nullptr;
  QComboBox *m_seasonBox = nullptr;
  QComboBox *m_styleBox = nullptr;
  QLineEdit  *m_nameEdit   = nullptr;
  QComboBox  *m_categoryBox  = nullptr;
  QLineEdit  *m_colorEdit   = nullptr;
  QComboBox  *m_sizeBox    = nullptr;
  QDoubleSpinBox *m_priceSpin  = nullptr;
  QLineEdit  *m_imagePathEdit = nullptr;
  QLineEdit  *m_imagePathEdit2 = nullptr;
  QLineEdit  *m_imagePathEdit3 = nullptr;


  QLabel *m_status = nullptr;

  static QString normalizeOrderStatus(QString s)
  {
    s = s.trimmed().toLower();
    s.replace("ё", "е");
    return s;
  }

  static QStringList orderStatusVisual(const QString &status)
  {
    const QString n = normalizeOrderStatus(status);
    if (n == "оформлен") return {"Оформлен", "#EFE7DF", "#6B5444", "#D8C9BE"};
    if (n == "оплачен") return {"Оплачен", "#E5F4E8", "#2E7D46", "#B7DFC0"};
    if (n.contains("формир")) return {"Формируется", "#FFF1D7", "#9A641F", "#F0D09B"};
    if (n == "собран") return {"Собран", "#EAE6FF", "#5B4BB2", "#C9C1F4"};
    if (n.contains("ожидает отправ")) return {"Ожидает отправки", "#E5EDFF", "#325EA8", "#BFD0F5"};
    if (n.contains("в пути") || n.contains("отправ")) return {"В пути", "#DCEEFF", "#2D6FA8", "#BBD8F3"};
    if (n.contains("достав")) return {"Доставлен", "#DDF4E3", "#2E7D46", "#B7DFC0"};
    if (n.contains("отмен")) return {"Отменён", "#F9DCDD", "#B64B53", "#E8B5BB"};
    if (n.contains("возврат")) return {"↩ Возврат", "#F4E1F4", "#8C4A86", "#D8B7D8"};
    return {"◔ В обработке", "#EEE7E1", "#5F5750", "#D8CBC0"};
  }

  static void applyStatusComboStyle(QComboBox *box)
  {
    if (!box)
      return;
    const QStringList st = orderStatusVisual(box->currentText());
    box->setStyleSheet(QString(
     "QComboBox{background:%1;color:%2;border:1px solid %3;border-radius:10px;min-height:30px;padding:0 10px;font-weight:800;}"
     "QComboBox::drop-down{border:none;width:22px;}"
     "QComboBox QAbstractItemView{background:white;color:#241F1A;selection-background-color:#EFE7DF;selection-color:#241F1A;}"
    ).arg(st.value(1), st.value(2), st.value(3)));
  }

  void refreshProductsTable()
  {
    m_table->setRowCount(0);
    QVector<Product> products = loadProductsFromDb();
    m_table->setRowCount(products.size());
    for (int i = 0; i < products.size(); ++i) {
      const Product &p = products[i];
      m_table->setItem(i, 0, new QTableWidgetItem(QString::number(p.id)));
      m_table->setItem(i, 1, new QTableWidgetItem(p.name));
      m_table->setItem(i, 2, new QTableWidgetItem(p.category));
      m_table->setItem(i, 3, new QTableWidgetItem(p.color));
      m_table->setItem(i, 4, new QTableWidgetItem(p.size));
      m_table->setItem(i, 5, new QTableWidgetItem(QString::number(p.price, 'f', 2)));
      m_table->setItem(i, 6, new QTableWidgetItem(p.imagePath));
      m_table->setItem(i, 7, new QTableWidgetItem(p.imagePath2));
      m_table->setItem(i, 8, new QTableWidgetItem(p.imagePath3));
      m_table->setItem(i, 9, new QTableWidgetItem(p.material));
      m_table->setItem(i,10, new QTableWidgetItem(p.season));
      m_table->setItem(i,11, new QTableWidgetItem(p.styleTag));
    }
  }

  void refreshOrdersTable()
  {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen())
      return;

    m_ordersTable->setRowCount(0);

    QSqlQuery q(db);
    if (!q.exec("SELECT id, username, created_at, items_count, payment_method, total, status "
         "FROM orders ORDER BY id DESC;"))
      return;

    m_ordersTable->setRowCount(0);

    int row = 0;
    while (q.next()) {
      const int orderId = q.value(0).toInt();

      m_ordersTable->insertRow(row);
      m_ordersTable->setItem(row, 0, new QTableWidgetItem(QString::number(orderId)));
      m_ordersTable->setItem(row, 1, new QTableWidgetItem(q.value(1).toString())); // user
      m_ordersTable->setItem(row, 2, new QTableWidgetItem(q.value(2).toString())); // date
      m_ordersTable->setItem(row, 3, new QTableWidgetItem(q.value(3).toString())); // quantity
      m_ordersTable->setItem(row, 4, new QTableWidgetItem(q.value(4).toString())); // payment
      m_ordersTable->setItem(row, 5, new QTableWidgetItem(
                    QString::number(q.value(5).toDouble(),'f',2))); // total amount

      // The order status can be edited in the administrator panel.
      QComboBox *st = new QComboBox(m_ordersTable);
      st->addItems({"оформлен", "оплачен", "формируется", "собран", "собран и ожидает отправки", "в пути", "доставлен", "отменён", "возврат"});
      const QString cur = q.value(6).toString();
      int idx = st->findText(cur);
      if (idx >= 0) st->setCurrentIndex(idx);
      else st->setCurrentText(cur.trimmed().isEmpty() ? QString("оформлен") : cur);
      applyStatusComboStyle(st);
      m_ordersTable->setCellWidget(row, 6, st);

      connect(st, &QComboBox::currentTextChanged, this, [this, st, orderId](const QString &newStatus) {
        applyStatusComboStyle(st);
        QSqlDatabase db = QSqlDatabase::database();
        if (!db.isOpen()) return;
        QSqlQuery u(db);
        u.prepare("UPDATE orders SET status=:s WHERE id=:id;");
        u.bindValue(":s", newStatus);
        u.bindValue(":id", orderId);
        if (!u.exec()) {
          QMessageBox::warning(this, "Админка", "Не удалось обновить статус: " + u.lastError().text());
        }
      });

      ++row;
    }
  }

void onDuplicate()
  {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen())
      return;

    const int row = m_table->currentRow();
    if (row < 0) {
      QMessageBox::information(this, "Админка", "Выберите товар для дублирования.");
      return;
    }

    Product p;
    p.name    = m_table->item(row, 1)->text() + " (копия)";
    p.category  = m_table->item(row, 2)->text();
    p.color   = m_table->item(row, 3)->text();
    p.size    = m_table->item(row, 4)->text();
    p.price   = m_table->item(row, 5)->text().toDouble();
    p.imagePath = m_table->item(row, 6)->text();
    p.imagePath2 = m_table->item(row, 7)->text();
    p.imagePath3 = m_table->item(row, 8)->text();
    p.material  = m_table->item(row, 9)->text();
    p.season   = m_table->item(row,10)->text();
    p.styleTag  = m_table->item(row,11)->text();

    QSqlQuery q(db);
    q.prepare(
     "INSERT INTO products(name, category, color, size, price, "
     "image_path, image_path2, image_path3, material, season, style_tag) "
     "VALUES(:n, :c, :col, :s, :p, :img1, :img2, :img3, :mat, :season, :style);"
    );
    q.bindValue(":n", p.name);
    q.bindValue(":c", p.category);
    q.bindValue(":col", p.color);
    q.bindValue(":s", p.size);
    q.bindValue(":p", p.price);
    q.bindValue(":img1", p.imagePath);
    q.bindValue(":img2", p.imagePath2);
    q.bindValue(":img3", p.imagePath3);
    q.bindValue(":mat", p.material);
    q.bindValue(":season", p.season);
    q.bindValue(":style", p.styleTag);

    if (!q.exec()) {
      QMessageBox::warning(this, "Админка", "Не удалось дублировать товар: " + q.lastError().text());
      return;
    }

    m_status->setStyleSheet("color:#388e3c;");
    m_status->setText("Товар продублирован.");
    refreshProductsTable();
  }

  void clearProductForm()
  {
    m_nameEdit->clear();
    if (m_categoryBox) m_categoryBox->setCurrentIndex(0);
    m_colorEdit->clear();
    if (m_sizeBox) m_sizeBox->setCurrentIndex(0);
    m_priceSpin->setValue(0.0);
    m_imagePathEdit->clear();
    m_imagePathEdit2->clear();
    m_imagePathEdit3->clear();
    m_materialEdit->clear();
    if (m_seasonBox) m_seasonBox->setCurrentIndex(0);
    if (m_styleBox) m_styleBox->setCurrentIndex(0);
    m_nameEdit->setFocus();
  }

  void fillFormFromSelected()
  {
    const int row = m_table->currentRow();
    if (row < 0)
      return;

    m_nameEdit->setText(m_table->item(row, 1)->text());

    const QString category = m_table->item(row, 2)->text();
    if (m_categoryBox->findText(category) < 0)
      m_categoryBox->addItem(category);
    m_categoryBox->setCurrentText(category);

    m_colorEdit->setText(m_table->item(row, 3)->text());

    const QString size = m_table->item(row, 4)->text();
    if (m_sizeBox->findText(size) < 0)
      m_sizeBox->addItem(size);
    m_sizeBox->setCurrentText(size);

    m_priceSpin->setValue(m_table->item(row, 5)->text().toDouble());
    m_imagePathEdit->setText(m_table->item(row, 6)->text());
    m_imagePathEdit2->setText(m_table->item(row, 7)->text());
    m_imagePathEdit3->setText(m_table->item(row, 8)->text());
    m_materialEdit->setText(m_table->item(row, 9)->text());

    const QString season = m_table->item(row, 10)->text();
    if (m_seasonBox->findText(season) < 0)
      m_seasonBox->addItem(season);
    m_seasonBox->setCurrentText(season);

    const QString style = m_table->item(row, 11)->text();
    if (m_styleBox->findText(style) < 0)
      m_styleBox->addItem(style);
    m_styleBox->setCurrentText(style);

    m_status->setStyleSheet("color:#6f4b37;");
    m_status->setText("Данные выбранного товара перенесены в форму. Можно создать похожий товар.");
  }

    void onAdd()
  {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen())
      return;

    QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
      m_status->setStyleSheet("color:#d32f2f;");
      m_status->setText("Название не может быть пустым.");
      return;
    }

    QSqlQuery q(db);
    q.prepare(
     "INSERT INTO products(name, category, color, size, price, "
     "image_path, image_path2, image_path3, "
     "material, season, style_tag) "
     "VALUES(:n, :c, :col, :s, :p, :img1, :img2, :img3, :mat, :season, :style)"
      );
    q.bindValue(":n",  name);
    q.bindValue(":c",  m_categoryBox->currentText());
    q.bindValue(":col", m_colorEdit->text().trimmed());
    q.bindValue(":s",  m_sizeBox->currentText());
    q.bindValue(":p",  m_priceSpin->value());
    q.bindValue(":img1", m_imagePathEdit->text().trimmed());
    q.bindValue(":img2", m_imagePathEdit2->text().trimmed());
    q.bindValue(":img3", m_imagePathEdit3->text().trimmed());
    q.bindValue(":mat", m_materialEdit->text().trimmed());
    q.bindValue(":season", m_seasonBox->currentText());
    q.bindValue(":style", m_styleBox->currentText());
    if (!q.exec()) {
      m_status->setStyleSheet("color:#d32f2f;");
      m_status->setText("Ошибка добавления: " + q.lastError().text());
      return;
    }

    m_status->setStyleSheet("color:#388e3c;");
    m_status->setText("Товар добавлен.");

    clearProductForm();


    refreshProductsTable();
  }

  void onDelete()
  {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen())
      return;
    int row = m_table->currentRow();
    if (row < 0)
      return;

    int id = m_table->item(row, 0)->text().toInt();

    if (QMessageBox::question(this, "Удаление",
                "Удалить выбранный товар?") != QMessageBox::Yes)
      return;

    QSqlQuery q(db);
    q.prepare("DELETE FROM products WHERE id=:id");
    q.bindValue(":id", id);
    q.exec();

    refreshProductsTable();
  }

  void onTableContextMenu(const QPoint &pos)
  {
    int row = m_table->rowAt(pos.y());
    if (row < 0)
      return;

    QMenu menu(this);
    QAction *editAct  = menu.addAction("Изменить товар");
    QAction *fillAct  = menu.addAction("Заполнить форму из товара");
    QAction *dupAct  = menu.addAction("Дублировать товар");
    QAction *delAct  = menu.addAction("Удалить товар");
    QAction *chosen  = menu.exec(m_table->viewport()->mapToGlobal(pos));
    if (!chosen)
      return;

    m_table->setCurrentCell(row, 0);
    if (chosen == delAct) {
      onDelete();
    } else if (chosen == editAct) {
      editRow(row);
    } else if (chosen == fillAct) {
      fillFormFromSelected();
    } else if (chosen == dupAct) {
      onDuplicate();
    }
  }

  void editRow(int row)
  {
    Product p;
    p.id    = m_table->item(row, 0)->text().toInt();
    p.name   = m_table->item(row, 1)->text();
    p.category = m_table->item(row, 2)->text();
    p.color   = m_table->item(row, 3)->text();
    p.size   = m_table->item(row, 4)->text();
    p.price   = m_table->item(row, 5)->text().toDouble();
    p.imagePath = m_table->item(row, 6)->text();
    p.imagePath2 = m_table->item(row, 7)->text();
    p.imagePath3 = m_table->item(row, 8)->text();
    p.material  = m_table->item(row, 9)->text();
    p.season   = m_table->item(row,10)->text();
    p.styleTag  = m_table->item(row,11)->text();


    ProductEditDialog dlg(p, this);
    if (dlg.exec() != QDialog::Accepted)
      return;

    Product np = dlg.editedProduct();

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen())
      return;

    QSqlQuery q(db);
    q.prepare(
     "UPDATE products "
     "SET name=:n, category=:c, color=:col, size=:s, price=:p, "
     " image_path=:img1, image_path2=:img2, image_path3=:img3, "
     " material=:mat, season=:season, style_tag=:style "
     "WHERE id=:id;"
      );
    q.bindValue(":n",  np.name);
    q.bindValue(":c",  np.category);
    q.bindValue(":col", np.color);
    q.bindValue(":s",  np.size);
    q.bindValue(":p",  np.price);
    q.bindValue(":img1", np.imagePath);
    q.bindValue(":img2", np.imagePath2);
    q.bindValue(":img3", np.imagePath3);
    q.bindValue(":mat", np.material);
    q.bindValue(":season", np.season);
    q.bindValue(":style", np.styleTag);
    q.bindValue(":id", np.id);
    if (!q.exec()) {
      QMessageBox::warning(this, "Изменение товара",
                "Ошибка изменения: " + q.lastError().text());
    }

    refreshProductsTable();
  }
};
