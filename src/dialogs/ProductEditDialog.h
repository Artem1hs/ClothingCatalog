#pragma once

/*
  File: src/dialogs/ProductEditDialog.h
  Purpose: Product create/edit dialog used from the administrator panel.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "../models/DataModels.h"

class ProductEditDialog : public QDialog
{
public:
  ProductEditDialog(const Product &product,
           QWidget *parent = nullptr)
    : QDialog(parent)
    , m_product(product)
  {
    setWindowTitle("Изменить товар");
    resize(520, 420);

    QVBoxLayout *root = new QVBoxLayout(this);
    QFormLayout *form = new QFormLayout();

    m_name = new QLineEdit(m_product.name, this);

    m_category = new QComboBox(this);
    m_category->addItems({"Футболка", "Лонгслив", "Майка", "Топ", "Рубашка", "Блузка", "Худи", "Свитшот", "Свитер", "Кардиган", "Жакет", "Пиджак", "Куртка", "Пальто", "Тренч", "Брюки", "Джинсы", "Юбка", "Шорты", "Платье", "Костюм", "Обувь", "Кроссовки", "Кеды", "Ботинки", "Лоферы", "Сумка", "Ремень", "Шарф", "Головной убор", "Шапка", "Кепка", "Аксессуары"});
    m_category->setEditable(true);
    int idxC = m_category->findText(m_product.category);
    if (idxC >= 0) m_category->setCurrentIndex(idxC);

    m_color = new QLineEdit(m_product.color, this);

    m_size = new QComboBox(this);
    m_size->addItems({"XXS", "XS", "S", "M", "L", "XL", "XXL", "3XL", "one size", "35", "36", "37", "38", "39", "40", "41", "42", "43", "44", "45", "46"});
    m_size->setEditable(true);
    int idxS = m_size->findText(m_product.size);
    if (idxS >= 0) m_size->setCurrentIndex(idxS);

    m_price = new QDoubleSpinBox(this);
    m_price->setRange(0.0, 999999.0);
    m_price->setDecimals(2);
    m_price->setValue(m_product.price);

    m_image1 = new QLineEdit(m_product.imagePath, this);
    m_image2 = new QLineEdit(m_product.imagePath2, this);
    m_image3 = new QLineEdit(m_product.imagePath3, this);

    m_image1->setPlaceholderText("фото 1, напр. tshirt1.png");
    m_image2->setPlaceholderText("фото 2 (опционально)");
    m_image3->setPlaceholderText("фото 3 (опционально)");

    m_material = new QLineEdit(m_product.material, this);

    // Product season.
    m_season = new QComboBox(this);
    m_season->addItems({"Весна", "Лето", "Осень", "Зима", "Демисезон", "Всесезонный", "Праздник", "Капсула"});
    m_season->setEditable(true);
    int sIdx = m_season->findText(m_product.season);
    if (sIdx >= 0) m_season->setCurrentIndex(sIdx);

    // Product style.
    m_style = new QComboBox(this);
    m_style->addItems({"Кэжуал", "Минималистичный", "Базовый", "Спортивный", "Деловой", "Streetwear", "Smart Casual", "Oversize", "Гранж", "Винтаж", "Классика", "Романтический", "Бохо", "Офисный", "Casual", "Sport", "Business", "Street", "Basic", "Minimal", "Grunge", "Vintage", "Classic", "Romantic", "Boho", "Office"});
    m_style->setEditable(true);
    int stIdx = m_style->findText(m_product.styleTag);
    if (stIdx < 0) {
      const QString tag = m_product.styleTag.trimmed().toLower();
      if (tag == "casual") stIdx = m_style->findText("Кэжуал");
      else if (tag == "sport") stIdx = m_style->findText("Спортивный");
      else if (tag == "business") stIdx = m_style->findText("Деловой");
      else if (tag == "street" || tag == "streetwear") stIdx = m_style->findText("Streetwear");
      else if (tag == "basic") stIdx = m_style->findText("Базовый");
      else if (tag == "minimal" || tag == "minimalism") stIdx = m_style->findText("Минималистичный");
      else if (tag == "smart casual") stIdx = m_style->findText("Smart Casual");
      else if (tag == "oversize") stIdx = m_style->findText("Oversize");
      else if (tag == "grunge") stIdx = m_style->findText("Гранж");
      else if (tag == "vintage") stIdx = m_style->findText("Винтаж");
      else if (tag == "classic") stIdx = m_style->findText("Классика");
      else if (tag == "romantic") stIdx = m_style->findText("Романтический");
      else if (tag == "boho") stIdx = m_style->findText("Бохо");
      else if (tag == "office") stIdx = m_style->findText("Офисный");
    }
    if (stIdx >= 0) m_style->setCurrentIndex(stIdx);

    form->addRow("Название:", m_name);
    form->addRow("Категория:", m_category);
    form->addRow("Цвет:", m_color);
    form->addRow("Размер:", m_size);
    form->addRow("Цена, ₽:", m_price);
    form->addRow("Фото 1:", m_image1);
    form->addRow("Фото 2:", m_image2);
    form->addRow("Фото 3:", m_image3);
    form->addRow("Материал:", m_material);
    form->addRow("Сезон:", m_season);
    form->addRow("Стиль:", m_style);

    root->addLayout(form);

    QHBoxLayout *btns = new QHBoxLayout();
    QPushButton *saveBtn = new QPushButton("Сохранить", this);
    QPushButton *cancelBtn = new QPushButton("Отмена", this);
    btns->addStretch();
    btns->addWidget(saveBtn);
    btns->addWidget(cancelBtn);
    root->addLayout(btns);

    connect(saveBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  }

  Product editedProduct() const
  {
    Product p = m_product;
    p.name   = m_name->text().trimmed();
    p.category = m_category->currentText();
    p.color   = m_color->text().trimmed();
    p.size   = m_size->currentText();
    p.price   = m_price->value();
    p.imagePath = m_image1->text().trimmed();
    p.imagePath2 = m_image2->text().trimmed();
    p.imagePath3 = m_image3->text().trimmed();
    p.material = m_material->text().trimmed();
    p.season  = m_season->currentText();
    p.styleTag = m_style->currentText();
    return p;
  }

private:
  Product    m_product;
  QLineEdit   *m_name   = nullptr;
  QComboBox   *m_category = nullptr;
  QLineEdit   *m_color   = nullptr;
  QComboBox   *m_size   = nullptr;
  QDoubleSpinBox *m_price  = nullptr;
  QLineEdit *m_image1 = nullptr;
  QLineEdit *m_image2 = nullptr;
  QLineEdit *m_image3 = nullptr;
  QLineEdit *m_material = nullptr;
  QComboBox *m_season = nullptr;
  QComboBox *m_style = nullptr;

};

