#pragma once

/*
  File: src/dialogs/QuickRecommendDialog.h
  Purpose: Quick recommendation dialog for collecting simple user preferences.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "../widgets/RoundCloseButton.h"
#include "../models/DataModels.h"

#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

class QuickRecommendDialog : public QDialog
{
public:
  explicit QuickRecommendDialog(const QVector<Product> &products, QWidget *parent = nullptr)
    : QDialog(parent)
    , m_products(products)
  {
    setWindowTitle("Быстрый подбор одежды");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setModal(true);
    resize(790, 640);
    setMinimumSize(750, 600);

    setStyleSheet(
     "QDialog{background:#00000000;}"
     "QFrame#RootCard{background:rgba(246,241,236,238);border:1px solid rgba(224,214,205,190);border-radius:18px;}"
     "QFrame#SoftPanel{background:rgba(255,255,255,110);border:1px solid rgba(190,175,164,120);border-radius:12px;}"
     "QLabel{background:transparent;border:none;color:#14110F;}"
     "QLabel#Title{font-size:18px;font-weight:900;}"
     "QLabel#SectionTitle{font-size:16px;font-weight:800;}"
     "QLabel#FieldLabel{font-size:13px;font-weight:700;}"
     "QLabel#FoundLabel{background:rgba(255,255,255,170);border:1px solid rgba(205,194,184,150);border-radius:9px;padding:6px 8px;font-size:11px;font-weight:800;}"
     "QComboBox,QSpinBox,QLineEdit{background:rgba(255,255,255,165);border:1px solid rgba(175,160,150,135);border-radius:10px;min-height:34px;max-height:34px;padding:0 11px;font-size:13px;color:#17120F;}"
     "QComboBox:focus,QSpinBox:focus,QLineEdit:focus{border:1px solid #A05D45;}"
     "QComboBox::drop-down{border:none;width:28px;}"
     "QComboBox::down-arrow{image:none;border:none;}"
     "QSpinBox::up-button,QSpinBox::down-button{width:0;border:none;}"
     "QPushButton#CloseX{background:transparent;border:none;color:#111;font-size:24px;font-weight:400;}"
     "QPushButton#CloseX:hover{background:rgba(255,255,255,110);border-radius:16px;}"
     "QToolButton#Chip{background:rgba(255,255,255,165);border:1px solid rgba(175,160,150,130);border-radius:9px;min-height:28px;max-height:28px;padding:0 8px;font-size:11px;color:#17120F;}"
     "QToolButton#Chip:checked{background:rgba(139,96,72,210);border:1px solid #8B6048;color:white;}"
     "QToolButton#ColorDot{border:2px solid rgba(255,255,255,210);border-radius:14px;min-width:28px;max-width:28px;min-height:28px;max-height:28px;}"
     "QToolButton#ColorDot:checked{border:3px solid #6F4B37;}"
      
     "QSpinBox#WeightSpin{background:rgba(255,255,255,165);border:1px solid rgba(175,160,150,135);border-radius:10px;min-width:150px;max-width:150px;min-height:34px;max-height:34px;font-size:13px;padding:0 10px;}"
     "QSpinBox#BudgetSpin{background:transparent;border:none;min-width:82px;max-width:82px;min-height:32px;max-height:32px;font-size:13px;}"
     "QFrame#BudgetFrame{background:rgba(255,255,255,165);border:1px solid rgba(160,95,75,120);border-radius:10px;}"
     "QPushButton#PrimaryBtn{background:#A96A50;color:white;border:none;border-radius:10px;min-height:42px;max-height:42px;padding:0 18px;font-size:15px;font-weight:900;}"
     "QPushButton#PrimaryBtn:hover{background:#965A42;}"
     "QPushButton#SecondaryBtn{background:#A87968;color:white;border:none;border-radius:10px;min-height:42px;max-height:42px;padding:0 18px;font-size:15px;font-weight:800;}"
     "QPushButton#SecondaryBtn:hover{background:#936B5C;}"
     "QLabel#PreviewBox{background:rgba(255,255,255,170);border:1px solid rgba(205,194,184,145);border-radius:8px;color:#7B7068;}"
    );

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    QFrame *rootCard = new QFrame(this);
    rootCard->setObjectName("RootCard");
    outer->addWidget(rootCard);

    QVBoxLayout *root = new QVBoxLayout(rootCard);
    root->setContentsMargins(16, 12, 16, 14);
    root->setSpacing(7);

    QHBoxLayout *header = new QHBoxLayout();
    QLabel *title = new QLabel("Быстрый подбор одежды", rootCard);
    title->setObjectName("Title");
    header->addWidget(title);
    header->addStretch();

    QPushButton *closeX = new RoundCloseButton(rootCard);
    closeX->setObjectName("CloseX");
    closeX->setCursor(Qt::PointingHandCursor);
    closeX->setFixedSize(32, 32);
    header->addWidget(closeX);
    root->addLayout(header);

    QLabel *mainTitle = new QLabel("Основные параметры", rootCard);
    mainTitle->setObjectName("SectionTitle");
    root->addWidget(mainTitle);

    QGridLayout *form = new QGridLayout();
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);

    m_genderBox = new QComboBox(rootCard);
    m_genderBox->addItems({"Не важно", "Мужской, Унисекс", "Женский"});

    m_weightSpin = new QSpinBox(rootCard);
    m_weightSpin->setObjectName("WeightSpin");
    m_weightSpin->setRange(40, 200);
    m_weightSpin->setValue(70);
    m_weightSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_weightSpin->setAlignment(Qt::AlignCenter);

    m_weightSpin->setSuffix(" кг");

    QWidget *weightWrap = new QWidget(rootCard);
    QHBoxLayout *weightLay = new QHBoxLayout(weightWrap);
    weightLay->setContentsMargins(0, 0, 0, 0);
    weightLay->setSpacing(0);
    weightLay->addWidget(m_weightSpin, 0, Qt::AlignLeft);

    m_sizeBox = new QComboBox(rootCard);
    m_sizeBox->addItems({"Не важно", "XS", "S", "M", "L", "XL", "XXL", "one size", "S, M, L",
              "35", "36", "37", "38", "39", "40", "41", "42", "43", "44", "45", "46"});

    m_heightSpin = new QSpinBox(rootCard);
    m_heightSpin->setObjectName("WeightSpin");
    m_heightSpin->setRange(0, 230);
    m_heightSpin->setValue(0);
    m_heightSpin->setSpecialValueText("Не указан");
    m_heightSpin->setSuffix(" см");
    m_heightSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_heightSpin->setAlignment(Qt::AlignCenter);

    form->addWidget(fieldLabel("Пол:", rootCard), 0, 0);
    form->addWidget(m_genderBox, 0, 1);
    form->addWidget(fieldLabel("Вес (кг):", rootCard), 0, 2);
    form->addWidget(weightWrap, 0, 3);

    form->addWidget(fieldLabel("Размер:", rootCard), 1, 0);
    form->addWidget(m_sizeBox, 1, 1);
    form->addWidget(fieldLabel("Рост (см):", rootCard), 1, 2);
    form->addWidget(m_heightSpin, 1, 3);

    QWidget *styleWrap = new QWidget(rootCard);
    QGridLayout *styleLay = new QGridLayout(styleWrap);
    styleLay->setContentsMargins(0, 0, 0, 0);
    styleLay->setHorizontalSpacing(6);
    styleLay->setVerticalSpacing(6);

    auto addStyleChip = [&](const QString &value, const QString &text, int row, int col, bool checked = false) {
      QToolButton *chip = new QToolButton(styleWrap);
      chip->setObjectName("Chip");
      chip->setText(text);
      chip->setCheckable(true);
      chip->setChecked(checked);
      chip->setCursor(Qt::PointingHandCursor);
      chip->setProperty("value", value);
      styleLay->addWidget(chip, row, col);
      m_styleChips << chip;
    };

    // Existing catalog styles plus extra styles for future products.
    addStyleChip("Кэжуал", "Кэжуал", 0, 0);
    addStyleChip("Минимализм", "Минимализм", 0, 1);
    addStyleChip("Базовый", "Базовый", 0, 2);
    addStyleChip("Спортивный", "Спорт", 0, 3);
    addStyleChip("Деловой", "Деловой", 1, 0);
    addStyleChip("Streetwear", "Streetwear", 1, 1);
    addStyleChip("Smart Casual", "Smart Casual", 1, 2);
    addStyleChip("Oversize", "Oversize", 1, 3);
    addStyleChip("Гранж", "Гранж", 2, 0);
    addStyleChip("Винтаж", "Винтаж", 2, 1);
    addStyleChip("Классика", "Классика", 2, 2);
    addStyleChip("Романтический", "Романтика", 2, 3);
    addStyleChip("Бохо", "Бохо", 3, 0);
    addStyleChip("Офисный", "Офисный", 3, 1);

    form->addWidget(fieldLabel("Стиль:", rootCard), 2, 0);
    form->addWidget(styleWrap, 2, 1, 1, 3);

    root->addLayout(form);

    QLabel *detailsTitle = new QLabel("Дополнительные детали", rootCard);
    detailsTitle->setObjectName("SectionTitle");
    root->addWidget(detailsTitle);

    QGridLayout *details = new QGridLayout();
    details->setHorizontalSpacing(12);
    details->setVerticalSpacing(8);

    QWidget *typesWrap = new QWidget(rootCard);
    QHBoxLayout *typesLay = new QHBoxLayout(typesWrap);
    typesLay->setContentsMargins(0, 0, 0, 0);
    typesLay->setSpacing(6);
    addChip(typesLay, m_categoryChips, "Футболки", "Футболки");
    addChip(typesLay, m_categoryChips, "Брюки", "Брюки");
    addChip(typesLay, m_categoryChips, "Куртки", "Куртки");
    addChip(typesLay, m_categoryChips, "Рубашки", "Рубашки");
    addChip(typesLay, m_categoryChips, "Обувь", "Обувь");
    typesLay->addStretch();

    details->addWidget(fieldLabel("Тип одежды:", rootCard), 0, 0);
    details->addWidget(typesWrap, 0, 1, 1, 3);

    m_colorsEdit = new QLineEdit(rootCard);
    m_colorsEdit->setPlaceholderText("Например: чёрный, синий");
    details->addWidget(fieldLabel("Любимые цвета:", rootCard), 1, 0);
    details->addWidget(m_colorsEdit, 1, 1, 1, 3);

    QWidget *colorRow = new QWidget(rootCard);
    QHBoxLayout *colorLay = new QHBoxLayout(colorRow);
    colorLay->setContentsMargins(0, 0, 0, 0);
    colorLay->setSpacing(6);
    addColorDot(colorLay, "чёрный", "#1D2228");
    addColorDot(colorLay, "синий", "#123C78");
    addColorDot(colorLay, "зелёный", "#28583A");
    addColorDot(colorLay, "бордовый", "#6D1F32");
    addColorDot(colorLay, "молочный", "#E7D8B8");
    addColorDot(colorLay, "красный", "#9B1023");
    addColorDot(colorLay, "коричневый", "#8C3730");
    addColorDot(colorLay, "темно-синий", "#173766");
    addColorDot(colorLay, "изумрудный", "#285A4C");
    addColorDot(colorLay, "графит", "#1E2C2D");
    addColorDot(colorLay, "алый", "#AA102A");
    addColorDot(colorLay, "терракотовый", "#B76455");
    colorLay->addStretch();
    details->addWidget(colorRow, 2, 1, 1, 3);

    QFrame *budgetFrame = new QFrame(rootCard);
    budgetFrame->setObjectName("BudgetFrame");
    QHBoxLayout *budgetLay = new QHBoxLayout(budgetFrame);
    budgetLay->setContentsMargins(10, 0, 10, 0);
    budgetLay->setSpacing(6);

    QLabel *fromLbl = new QLabel("От", budgetFrame);
    fromLbl->setStyleSheet("font-size:13px;font-weight:700;");
    QLabel *toLbl = new QLabel("До", budgetFrame);
    toLbl->setStyleSheet("font-size:13px;font-weight:700;");

    m_budgetMinSpin = new QSpinBox(budgetFrame);
    m_budgetMinSpin->setObjectName("BudgetSpin");
    m_budgetMinSpin->setRange(0, 1000000);
    m_budgetMinSpin->setSingleStep(500);
    m_budgetMinSpin->setValue(0);
    m_budgetMinSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);

    m_budgetMaxSpin = new QSpinBox(budgetFrame);
    m_budgetMaxSpin->setObjectName("BudgetSpin");
    m_budgetMaxSpin->setRange(0, 1000000);
    m_budgetMaxSpin->setSingleStep(500);
    m_budgetMaxSpin->setValue(0);
    m_budgetMaxSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);

    m_currencyBox = new QComboBox(budgetFrame);
    m_currencyBox->addItems({"₽, $, €", "₽", "$", "€"});
    m_currencyBox->setFixedWidth(92);

    budgetLay->addWidget(fromLbl);
    budgetLay->addWidget(m_budgetMinSpin);
    budgetLay->addWidget(toLbl);
    budgetLay->addWidget(m_budgetMaxSpin);
    budgetLay->addStretch();
    budgetLay->addWidget(m_currencyBox);

    details->addWidget(fieldLabel("Бюджет, ₽:", rootCard), 3, 0);
    details->addWidget(budgetFrame, 3, 1, 1, 3);

    root->addLayout(details);

    QHBoxLayout *bottom = new QHBoxLayout();
    bottom->setSpacing(8);

    m_foundLabel = new QLabel(rootCard);
    m_foundLabel->setObjectName("FoundLabel");
    bottom->addWidget(m_foundLabel);

    for (int i = 0; i < 4; ++i) {
      QLabel *preview = new QLabel(rootCard);
      preview->setObjectName("PreviewBox");
      preview->setAlignment(Qt::AlignCenter);
      preview->setFixedSize(42, 42);
      preview->setText("фото");
      m_previews << preview;
      bottom->addWidget(preview);
    }

    bottom->addStretch();

    m_okBtn = new QPushButton(rootCard);
    m_okBtn->setObjectName("PrimaryBtn");
    m_okBtn->setCursor(Qt::PointingHandCursor);
    bottom->addWidget(m_okBtn);

    QPushButton *cancelBtn = new QPushButton("Отмена ", rootCard);
    cancelBtn->setObjectName("SecondaryBtn");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    bottom->addWidget(cancelBtn);

    root->addLayout(bottom);

    connect(closeX, &QPushButton::clicked, this, &QDialog::reject);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_okBtn, &QPushButton::clicked, this, &QDialog::accept);

    auto wireUpdate = [this](QObject *obj) {
      if (auto box = qobject_cast<QComboBox*>(obj)) {
        connect(box, &QComboBox::currentTextChanged, this, [this]() { refreshPreview(); });
      } else if (auto spin = qobject_cast<QSpinBox*>(obj)) {
        connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, [this]() { refreshPreview(); });
      } else if (auto edit = qobject_cast<QLineEdit*>(obj)) {
        connect(edit, &QLineEdit::textChanged, this, [this]() { refreshPreview(); });
      }
    };

    wireUpdate(m_genderBox);
    wireUpdate(m_weightSpin);
    wireUpdate(m_sizeBox);
    wireUpdate(m_heightSpin);
    wireUpdate(m_colorsEdit);
    wireUpdate(m_budgetMinSpin);
    wireUpdate(m_budgetMaxSpin);
    wireUpdate(m_currencyBox);

    for (QToolButton *chip : m_styleChips)
      connect(chip, &QToolButton::clicked, this, [this]() { refreshPreview(); });
    for (QToolButton *chip : m_categoryChips)
      connect(chip, &QToolButton::clicked, this, [this]() { refreshPreview(); });
    for (QToolButton *chip : m_colorDots)
      connect(chip, &QToolButton::clicked, this, [this]() { refreshPreview(); });

    refreshPreview();
  }

  explicit QuickRecommendDialog(QWidget *parent = nullptr)
    : QuickRecommendDialog(QVector<Product>(), parent)
  {
  }

  void applyProfile(const UserProfile &profile)
  {
    const QString gender = norm(profile.gender);
    if (gender.contains("жен")) {
      m_genderBox->setCurrentText("Женский");
    } else if (gender.contains("муж") || gender.contains("уни")) {
      m_genderBox->setCurrentText("Мужской, Унисекс");
    }

    if (profile.height > 0)
      m_heightSpin->setValue(profile.height);

    const QString styleText = norm(profile.style);
    const QStringList styleParts = splitList(profile.style);
    const bool hasStyle = !styleText.isEmpty() && styleText != "не выбран" && styleText != "не важно";
    for (QToolButton *chip : m_styleChips) {
      if (!chip)
        continue;

      const QString value = norm(chip->property("value").toString());
      const QString label = norm(chip->text());
      bool checked = false;
      if (hasStyle) {
        checked = styleText.contains(value) || value.contains(styleText) ||
             styleText.contains(label) || label.contains(styleText);
        for (const QString &part : styleParts) {
          if (checked)
            break;
          checked = part.contains(value) || value.contains(part) ||
               part.contains(label) || label.contains(part);
        }
      }
      chip->setChecked(checked);
    }

    const QString colorsText = profile.favoriteColors.trimmed();
    if (m_colorsEdit)
      m_colorsEdit->setText(colorsText);

    const QStringList colorParts = splitList(colorsText);
    for (QToolButton *dot : m_colorDots) {
      if (!dot)
        continue;

      const QString value = norm(dot->property("value").toString());
      bool checked = false;
      for (const QString &part : colorParts) {
        checked = part.contains(value) || value.contains(part);
        if (checked)
          break;
      }
      dot->setChecked(checked);
    }

    const QString budget = norm(profile.budget);
    if (!budget.isEmpty() && budget != "не указан") {
      const QRegularExpression budgetRange("(\\d[\\d\\s.,]*)\\D+(\\d[\\d\\s.,]*)");
      const auto match = budgetRange.match(profile.budget);
      auto moneyToInt = [](QString raw) {
        raw.remove(QRegularExpression("[^0-9]"));
        return raw.toInt();
      };

      if (match.hasMatch()) {
        m_budgetMinSpin->setValue(moneyToInt(match.captured(1)));
        m_budgetMaxSpin->setValue(moneyToInt(match.captured(2)));
      } else if (budget.contains("низ")) {
        m_budgetMinSpin->setValue(0);
        m_budgetMaxSpin->setValue(2500);
      } else if (budget.contains("сред")) {
        m_budgetMinSpin->setValue(2500);
        m_budgetMaxSpin->setValue(6000);
      } else if (budget.contains("высок")) {
        m_budgetMinSpin->setValue(6000);
        m_budgetMaxSpin->setValue(1000000);
      }
      m_currencyBox->setCurrentText("₽");
    }

    refreshPreview();
  }

  RecommendCriteria criteria() const
  {
    RecommendCriteria c;
    c.gender = m_genderBox->currentText();
    c.weight = m_weightSpin->value();
    c.size = m_sizeBox->currentText();
    c.height = m_heightSpin->value();
    c.style = checkedValues(m_styleChips).join(", ");
    c.category = checkedValues(m_categoryChips).join(", ");
    c.colors = selectedColorsText();
    c.minBudget = m_budgetMinSpin->value();
    c.maxBudget = m_budgetMaxSpin->value();
    c.currency = m_currencyBox->currentText();
    return c;
  }

private:
  QVector<Product> m_products;

  QComboBox *m_genderBox = nullptr;
  QSpinBox *m_weightSpin = nullptr;
  QComboBox *m_sizeBox = nullptr;
  QSpinBox *m_heightSpin = nullptr;
  QLineEdit *m_colorsEdit = nullptr;
  QSpinBox *m_budgetMinSpin = nullptr;
  QSpinBox *m_budgetMaxSpin = nullptr;
  QComboBox *m_currencyBox = nullptr;
  QLabel *m_foundLabel = nullptr;
  QPushButton *m_okBtn = nullptr;

  QList<QToolButton*> m_styleChips;
  QList<QToolButton*> m_categoryChips;
  QList<QToolButton*> m_colorDots;
  QList<QLabel*> m_previews;

  QLabel *fieldLabel(const QString &text, QWidget *parent) const
  {
    QLabel *label = new QLabel(text, parent);
    label->setObjectName("FieldLabel");
    return label;
  }

  void addChip(QHBoxLayout *layout, QList<QToolButton*> &list, const QString &value, const QString &text, bool checked = false)
  {
    QToolButton *chip = new QToolButton(this);
    chip->setObjectName("Chip");
    chip->setText(text);
    chip->setCheckable(true);
    chip->setChecked(checked);
    chip->setCursor(Qt::PointingHandCursor);
    chip->setProperty("value", value);
    layout->addWidget(chip);
    list << chip;
  }

  void addColorDot(QHBoxLayout *layout, const QString &name, const QString &hex, bool checked = false)
  {
    QToolButton *dot = new QToolButton(this);
    dot->setObjectName("ColorDot");
    dot->setCheckable(true);
    dot->setChecked(checked);
    dot->setCursor(Qt::PointingHandCursor);
    dot->setToolTip(name);
    dot->setProperty("value", name);
    dot->setStyleSheet(QString(
     "QToolButton#ColorDot{background:%1;border:2px solid rgba(255,255,255,220);border-radius:14px;min-width:28px;max-width:28px;min-height:28px;max-height:28px;}"
     "QToolButton#ColorDot:checked{border:3px solid #6F4B37;}"
     "QToolButton#ColorDot:hover{border:3px solid #A05D45;}"
    ).arg(hex));
    layout->addWidget(dot);
    m_colorDots << dot;
  }

  QStringList checkedValues(const QList<QToolButton*> &buttons) const
  {
    QStringList out;
    for (QToolButton *btn : buttons) {
      if (btn && btn->isChecked())
        out << btn->property("value").toString();
    }
    return out;
  }

  QString selectedColorsText() const
  {
    QStringList colors = checkedValues(m_colorDots);

    QString extra = m_colorsEdit ? m_colorsEdit->text() : QString();
    extra.replace(";", ",");
    extra.replace("/", ",");
    extra.replace("|", ",");
    const QStringList raw = extra.split(",", Qt::SkipEmptyParts);
    for (QString c : raw) {
      c = c.trimmed();
      if (c.isEmpty())
        continue;

      bool exists = false;
      for (const QString &already : colors) {
        if (already.compare(c, Qt::CaseInsensitive) == 0) {
          exists = true;
          break;
        }
      }

      if (!exists)
        colors << c;
    }

    return colors.join(", ");
  }

  static QString norm(QString s)
  {
    s = s.toLower().trimmed();
    s.replace("ё", "е");
    return s.simplified();
  }

  static QStringList splitList(QString input)
  {
    input.replace(";", ",");
    input.replace("|", ",");
    input.replace("/", ",");
    QStringList raw = input.split(",", Qt::SkipEmptyParts);
    QStringList out;
    for (QString v : raw) {
      v = norm(v);
      if (!v.isEmpty())
        out << v;
    }
    return out;
  }

  static bool colorMatches(const Product &p, const QStringList &colors)
  {
    if (colors.isEmpty())
      return true;

    const QString productColor = norm(p.color);
    for (const QString &color : colors) {
      const QString c = norm(color);
      if (productColor == c || productColor.contains(c) || c.contains(productColor))
        return true;
    }
    return false;
  }

  static bool categoryMatches(const Product &p, const QStringList &categories)
  {
    if (categories.isEmpty())
      return true;

    const QString cat = norm(p.category);
    const QString name = norm(p.name);

    for (const QString &raw : categories) {
      const QString c = norm(raw);
      if (c.contains("футбол") && (cat.contains("футбол") || name.contains("футбол"))) return true;
      if (c.contains("брюк") && (cat.contains("брюк") || cat.contains("джинс") || name.contains("брюк") || name.contains("джинс"))) return true;
      if (c.contains("куртк") && (cat.contains("куртк") || cat.contains("пальто") || cat.contains("тренч") || name.contains("куртк") || name.contains("пальто") || name.contains("тренч"))) return true;
      if (c.contains("худи") && (cat.contains("худи") || cat.contains("свитшот") || name.contains("худи") || name.contains("свитшот"))) return true;
      if (c.contains("рубаш") && (cat.contains("рубаш") || name.contains("рубаш"))) return true;
      if (c.contains("обув") && (cat.contains("обув") || cat.contains("кед") || cat.contains("крос") || cat.contains("бот") || name.contains("кед") || name.contains("крос") || name.contains("бот"))) return true;
      if (cat == c)
        return true;
    }

    return false;
  }

  static bool hasDigit(const QString &text)
  {
    for (const QChar ch : text) {
      if (ch.isDigit())
        return true;
    }
    return false;
  }

  static bool isFootwearProduct(const Product &p)
  {
    const QString hay = norm(p.name + " " + p.category);
    return hay.contains("обув") || hay.contains("кед") || hay.contains("крос") ||
        hay.contains("сникер") || hay.contains("ботин") || hay.contains("shoe");
  }

  static bool sizeMatches(const Product &p, const QString &sizeText)
  {
    QString s = norm(sizeText);
    if (s.isEmpty() || s == "не важно")
      return true;

    const bool requestedShoeSize = hasDigit(s);
    if (isFootwearProduct(p)) {
      if (!requestedShoeSize)
        return true;
      return p.size.compare(sizeText, Qt::CaseInsensitive) == 0;
    }

    if (requestedShoeSize)
      return p.size.compare(sizeText, Qt::CaseInsensitive) == 0;

    if (s.contains("s, m, l"))
      return p.size.compare("S", Qt::CaseInsensitive) == 0 ||
          p.size.compare("M", Qt::CaseInsensitive) == 0 ||
          p.size.compare("L", Qt::CaseInsensitive) == 0 ||
          p.size.compare("one size", Qt::CaseInsensitive) == 0;

    return p.size.compare(sizeText, Qt::CaseInsensitive) == 0 ||
        p.size.compare("one size", Qt::CaseInsensitive) == 0;
  }

  static bool styleMatches(const Product &p, const QStringList &styles)
  {
    if (styles.isEmpty())
      return true;

    const QString hay = norm(p.name + " " + p.category + " " + p.styleTag + " " + p.season + " " + p.color);

    for (const QString &styleRaw : styles) {
      const QString s = norm(styleRaw);

      if (s.contains("кэжуал") || s.contains("casual")) {
        if (hay.contains("casual") || hay.contains("кэж") || hay.contains("повсед") ||
          hay.contains("футбол") || hay.contains("худи") || hay.contains("джинс"))
          return true;
      }

      if (s.contains("минимал") || s.contains("minimal")) {
        if (hay.contains("minimal") || hay.contains("миним") || hay.contains("basic") ||
          hay.contains("баз") || hay.contains("беж") || hay.contains("бел") || hay.contains("чер"))
          return true;
      }

      if (s.contains("баз") || s.contains("basic")) {
        if (hay.contains("basic") || hay.contains("баз") || hay.contains("футбол") ||
          hay.contains("рубаш") || hay.contains("джинс") || hay.contains("бел") || hay.contains("чер"))
          return true;
      }

      if (s.contains("sport") || s.contains("спорт")) {
        if (hay.contains("sport") || hay.contains("спорт") || hay.contains("кед") ||
          hay.contains("крос") || hay.contains("худи") || hay.contains("свитшот"))
          return true;
      }

      if (s.contains("business") || s.contains("делов") || s.contains("офис") || s.contains("office")) {
        if (hay.contains("business") || hay.contains("делов") || hay.contains("office") ||
          hay.contains("офис") || hay.contains("рубаш") || hay.contains("брюк") ||
          hay.contains("пальто") || hay.contains("тренч"))
          return true;
      }

      if (s.contains("street") || s.contains("стрит")) {
        if (hay.contains("street") || hay.contains("стрит") || hay.contains("худи") ||
          hay.contains("овер") || hay.contains("футбол") || hay.contains("кед"))
          return true;
      }

      if (s.contains("smart")) {
        if (hay.contains("smart") || hay.contains("casual") || hay.contains("рубаш") ||
          hay.contains("брюк") || hay.contains("тренч") || hay.contains("пальто"))
          return true;
      }

      if (s.contains("oversize") || s.contains("овер")) {
        if (hay.contains("oversize") || hay.contains("овер") || hay.contains("худи") ||
          hay.contains("рубаш") || hay.contains("свитшот"))
          return true;
      }

      if (s.contains("гранж") || s.contains("grunge")) {
        if (hay.contains("grunge") || hay.contains("гранж") || hay.contains("чер") ||
          hay.contains("графит") || hay.contains("худи") || hay.contains("овер"))
          return true;
      }

      if (s.contains("винтаж") || s.contains("vintage") || s.contains("ретро")) {
        if (hay.contains("vintage") || hay.contains("винтаж") || hay.contains("ретро") ||
          hay.contains("террак") || hay.contains("карамел") || hay.contains("олив") ||
          hay.contains("70"))
          return true;
      }

      if (s.contains("класс") || s.contains("classic")) {
        if (hay.contains("classic") || hay.contains("класс") || hay.contains("рубаш") ||
          hay.contains("брюк") || hay.contains("пальто") || hay.contains("бел") || hay.contains("чер"))
          return true;
      }

      if (s.contains("роман") || s.contains("romantic")) {
        if (hay.contains("romantic") || hay.contains("роман") || hay.contains("плать") ||
          hay.contains("юб") || hay.contains("топ") || hay.contains("персик") || hay.contains("молоч"))
          return true;
      }

      if (s.contains("бохо") || s.contains("boho")) {
        if (hay.contains("boho") || hay.contains("бохо") || hay.contains("лен") ||
          hay.contains("песоч") || hay.contains("олив") || hay.contains("рубаш"))
          return true;
      }

      if (s.contains("лисо")) {
        if (hay.contains("casual") || hay.contains("basic") || hay.contains("футбол") ||
          hay.contains("рубаш") || hay.contains("брюк"))
          return true;
      }

      if (hay.contains(s))
        return true;
    }

    return false;
  }

  bool productMatches(const Product &p) const
  {
    const RecommendCriteria c = criteria();

    const QStringList categories = splitList(c.category);
    const QStringList colors = splitList(c.colors);
    const QStringList styles = splitList(c.style);

    if (!categories.isEmpty() && !categoryMatches(p, categories))
      return false;

    if (!styles.isEmpty() && !styleMatches(p, styles))
      return false;

    if (!colors.isEmpty() && !colorMatches(p, colors))
      return false;

    if (!sizeMatches(p, c.size))
      return false;

    if (c.minBudget > 0.0 && p.price < c.minBudget)
      return false;

    if (c.maxBudget > 0.0 && p.price > c.maxBudget)
      return false;

    // Weight and height are not strict filters; they only improve size relevance.
    return true;
  }

  QString resolveImagePathLocal(const QString &rawPath) const
  {
    if (rawPath.trimmed().isEmpty())
      return QString();

    QFileInfo direct(rawPath);
    if (direct.exists())
      return direct.absoluteFilePath();

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
      appDir + "/" + rawPath,
      appDir + "/image/" + rawPath,
      appDir + "/images/" + rawPath,
      QDir::currentPath() + "/" + rawPath,
      QDir::currentPath() + "/image/" + rawPath,
      QDir::currentPath() + "/images/" + rawPath
    };

    for (const QString &candidate : candidates) {
      QFileInfo info(candidate);
      if (info.exists())
        return info.absoluteFilePath();
    }

    return rawPath;
  }

  void refreshPreview()
  {
    if (!m_foundLabel || !m_okBtn)
      return;

    QVector<Product> matched;
    for (const Product &p : m_products) {
      if (productMatches(p))
        matched.push_back(p);
    }

    m_foundLabel->setText(QString("Найдено товаров: %1").arg(matched.size()));
    m_okBtn->setText(QString("Подобрать (%1) ").arg(matched.size()));
    m_okBtn->setEnabled(!matched.isEmpty());

    for (int i = 0; i < m_previews.size(); ++i) {
      QLabel *preview = m_previews[i];
      preview->setPixmap(QPixmap());
      preview->setText("фото");

      if (i >= matched.size())
        continue;

      const Product &p = matched[i];
      QPixmap pix(resolveImagePathLocal(p.imagePath));
      if (!pix.isNull()) {
        preview->setText(QString());
        preview->setPixmap(pix.scaled(preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
      }
    }
  }
};
