#pragma once

/*
  File: src/dialogs/CartDialog.h
  Purpose: Cart, checkout, payment, confirmation, and QR-code dialogs.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "../widgets/RoundCloseButton.h"
#include "../cart/CartManager.h"
#include "../data/DatabaseService.h"

namespace CartUi
{
inline QString money(double value)
{
  QLocale locale(QLocale::Russian, QLocale::Russia);
  return locale.toString(value, 'f', 0) + " ₽";
}

inline QString money2(double value)
{
  QLocale locale(QLocale::Russian, QLocale::Russia);
  return locale.toString(value, 'f', 2) + " ₽";
}

inline QString safeText(const QString &value, const QString &fallback = "—")
{
  const QString t = value.trimmed();
  return t.isEmpty() ? fallback : t;
}

inline QLabel *makeTitleLabel(const QString &text, QWidget *parent = nullptr)
{
  QLabel *label = new QLabel(text, parent);
  label->setStyleSheet("color:#202226;font-size:22px;font-weight:800;background:transparent;");
  return label;
}

inline void addShadow(QWidget *widget, int blur = 28, int y = 8, int alpha = 34)
{
  auto *shadow = new QGraphicsDropShadowEffect(widget);
  shadow->setBlurRadius(blur);
  shadow->setOffset(0, y);
  shadow->setColor(QColor(40, 34, 28, alpha));
  widget->setGraphicsEffect(shadow);
}

inline QString baseButtonStyle(const QString &bg, const QString &hover, const QString &pressed,
               const QString &fg = "#FFFFFF", const QString &border = "transparent")
{
  return QString(
   "QPushButton{background:%1;color:%4;border:1px solid %5;border-radius:18px;"
   "padding:9px 18px;font-size:14px;font-weight:800;}"
   "QPushButton:hover{background:%2;}"
   "QPushButton:pressed{background:%3;}"
   "QPushButton:disabled{background:#E5E0DA;color:#AAA39B;border-color:#E5E0DA;}"
  ).arg(bg, hover, pressed, fg, border);
}

inline QString actionButtonStyle(const QString &bg, const QString &hover, const QString &pressed,
                 const QString &fg = "#FFFFFF", const QString &border = "transparent")
{
  return QString(
   "QPushButton{background:%1;color:%4;border:1px solid %5;border-radius:18px;"
   "padding:9px 9px;font-size:13px;font-weight:800;}"
   "QPushButton:hover{background:%2;}"
   "QPushButton:pressed{background:%3;}"
   "QPushButton:disabled{background:#E5E0DA;color:#AAA39B;border-color:#E5E0DA;}"
  ).arg(bg, hover, pressed, fg, border);
}

inline QPixmap roundedPixmap(const QPixmap &source, const QSize &size, int radius)
{
  if (source.isNull())
    return QPixmap();

  QPixmap scaled = source.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
  QPixmap cropped(size);
  cropped.fill(Qt::transparent);

  QPainter cropPainter(&cropped);
  cropPainter.setRenderHint(QPainter::Antialiasing, true);
  const int x = (scaled.width() - size.width()) / 2;
  const int y = (scaled.height() - size.height()) / 2;
  QPainterPath path;
  path.addRoundedRect(cropped.rect(), radius, radius);
  cropPainter.setClipPath(path);
  cropPainter.drawPixmap(-x, -y, scaled);
  return cropped;
}

inline int gfMultiply(int x, int y)
{
  int z = 0;
  for (int i = 0; i < 8; ++i) {
    if ((y & 1) != 0)
      z ^= x;
    y >>= 1;
    x <<= 1;
    if ((x & 0x100) != 0)
      x ^= 0x11D;
  }
  return z & 0xFF;
}

inline QVector<int> reedSolomonDivisor(int degree)
{
  QVector<int> result(degree, 0);
  result[degree - 1] = 1;

  int root = 1;
  for (int i = 0; i < degree; ++i) {
    for (int j = 0; j < degree; ++j) {
      result[j] = gfMultiply(result[j], root);
      if (j + 1 < degree)
        result[j] ^= result[j + 1];
    }
    root = gfMultiply(root, 0x02);
  }
  return result;
}

// Generates a minimal QR image without requiring any external QR library.
inline QPixmap generateQrCodePixmap(const QString &payload, int moduleSize = 9, int quietZone = 4)
{
  // Small generator for a real QR Code Version 1-L.
  // It is enough for a short demo payload up to 17 bytes,
  // for example: CCPAY:12422. The generated code is readable by a camera.
  QByteArray data = payload.toLatin1();
  if (data.size() > 17)
    data = data.left(17);

  constexpr int size = 21;
  constexpr int dataCodewords = 19;
  constexpr int eccCodewords = 7;

  QVector<QVector<int>> modules(size, QVector<int>(size, 0));
  QVector<QVector<int>> isFunction(size, QVector<int>(size, 0));

  auto setFunction = [&](int x, int y, bool black) {
    if (x < 0 || y < 0 || x >= size || y >= size)
      return;
    modules[y][x] = black ? 1 : 0;
    isFunction[y][x] = 1;
  };

  auto drawFinder = [&](int x, int y) {
    for (int dy = -1; dy <= 7; ++dy) {
      for (int dx = -1; dx <= 7; ++dx) {
        const int xx = x + dx;
        const int yy = y + dy;
        if (xx < 0 || yy < 0 || xx >= size || yy >= size)
          continue;

        bool black = false;
        if (dx >= 0 && dx <= 6 && dy >= 0 && dy <= 6) {
          black = dx == 0 || dx == 6 || dy == 0 || dy == 6 ||
              (dx >= 2 && dx <= 4 && dy >= 2 && dy <= 4);
        }
        setFunction(xx, yy, black);
      }
    }
  };

  drawFinder(0, 0);
  drawFinder(size - 7, 0);
  drawFinder(0, size - 7);

  for (int i = 8; i < size - 8; ++i) {
    setFunction(i, 6, i % 2 == 0);
    setFunction(6, i, i % 2 == 0);
  }

  // Reserve format areas before placing data bits.
  for (int i = 0; i <= 5; ++i)
    setFunction(8, i, false);
  setFunction(8, 7, false);
  setFunction(8, 8, false);
  setFunction(7, 8, false);
  for (int i = 9; i < 15; ++i)
    setFunction(14 - i, 8, false);
  for (int i = 0; i < 8; ++i)
    setFunction(size - 1 - i, 8, false);
  for (int i = 8; i < 15; ++i)
    setFunction(8, size - 15 + i, false);
  setFunction(8, size - 8, true);

  QVector<int> bits;
  bits.reserve(dataCodewords * 8);
  auto appendBits = [&](int value, int count) {
    for (int i = count - 1; i >= 0; --i)
      bits.append((value >> i) & 1);
  };

  appendBits(0x4, 4);         // byte mode
  appendBits(data.size(), 8);     // character count for versions 1-9
  for (unsigned char ch : data)
    appendBits(ch, 8);

  const int capacityBits = dataCodewords * 8;
  const int terminator = qMin(4, capacityBits - bits.size());
  for (int i = 0; i < terminator; ++i)
    bits.append(0);
  while ((bits.size() % 8) != 0)
    bits.append(0);

  QVector<int> dataWords;
  dataWords.reserve(dataCodewords);
  for (int i = 0; i < bits.size(); i += 8) {
    int value = 0;
    for (int j = 0; j < 8; ++j)
      value = (value << 1) | bits[i + j];
    dataWords.append(value);
  }

  for (int padIndex = 0; dataWords.size() < dataCodewords; ++padIndex)
    dataWords.append((padIndex % 2 == 0) ? 0xEC : 0x11);

  const QVector<int> divisor = reedSolomonDivisor(eccCodewords);
  QVector<int> remainder(eccCodewords, 0);
  for (int word : dataWords) {
    const int factor = word ^ remainder.first();
    remainder.removeFirst();
    remainder.append(0);
    for (int i = 0; i < eccCodewords; ++i)
      remainder[i] ^= gfMultiply(divisor[i], factor);
  }

  QVector<int> allWords = dataWords;
  allWords += remainder;

  QVector<int> allBits;
  allBits.reserve(allWords.size() * 8);
  for (int word : allWords) {
    for (int i = 7; i >= 0; --i)
      allBits.append((word >> i) & 1);
  }

  int bitIndex = 0;
  bool upward = true;
  for (int right = size - 1; right > 0; right -= 2) {
    if (right == 6)
      --right;
    for (int vert = 0; vert < size; ++vert) {
      const int y = upward ? (size - 1 - vert) : vert;
      for (int dx = 0; dx < 2; ++dx) {
        const int x = right - dx;
        if (isFunction[y][x])
          continue;
        int bit = bitIndex < allBits.size() ? allBits[bitIndex] : 0;
        if (((x + y) & 1) == 0) // mask 0
          bit ^= 1;
        modules[y][x] = bit;
        ++bitIndex;
      }
    }
    upward = !upward;
  }

  const int errorCorrectionL = 1;
  const int mask = 0;
  const int formatData = (errorCorrectionL << 3) | mask;
  int rem = formatData << 10;
  constexpr int formatGenerator = 0x537;
  for (int i = 14; i >= 10; --i) {
    if (((rem >> i) & 1) != 0)
      rem ^= formatGenerator << (i - 10);
  }
  const int formatBits = ((formatData << 10) | (rem & 0x3FF)) ^ 0x5412;
  auto getBit = [&](int value, int index) { return ((value >> index) & 1) != 0; };

  for (int i = 0; i <= 5; ++i)
    setFunction(8, i, getBit(formatBits, i));
  setFunction(8, 7, getBit(formatBits, 6));
  setFunction(8, 8, getBit(formatBits, 7));
  setFunction(7, 8, getBit(formatBits, 8));
  for (int i = 9; i < 15; ++i)
    setFunction(14 - i, 8, getBit(formatBits, i));
  for (int i = 0; i < 8; ++i)
    setFunction(size - 1 - i, 8, getBit(formatBits, i));
  for (int i = 8; i < 15; ++i)
    setFunction(8, size - 15 + i, getBit(formatBits, i));
  setFunction(8, size - 8, true);

  const int imageSize = (size + quietZone * 2) * moduleSize;
  QImage image(imageSize, imageSize, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::white);

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setPen(Qt::NoPen);
  painter.setBrush(Qt::black);
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      if (modules[y][x]) {
        painter.drawRect((x + quietZone) * moduleSize,
                 (y + quietZone) * moduleSize,
                 moduleSize,
                 moduleSize);
      }
    }
  }
  painter.end();

  return QPixmap::fromImage(image);
}

}

class CartNoticeDialog : public QDialog
{
public:
  CartNoticeDialog(const QString &title,
           const QString &message,
           const QString &accentText,
           const QString &secondaryText,
           QWidget *parent = nullptr)
    : QDialog(parent)
  {
    setModal(true);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, true);
    resize(430, 220);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(0);

    QFrame *shell = new QFrame(this);
    shell->setObjectName("CartNoticeShell");
    shell->setStyleSheet(
     "QFrame#CartNoticeShell{background:#FFFFFF;border:1px solid #E7DFD7;border-radius:20px;}"
    );
    CartUi::addShadow(shell, 30, 8, 42);
    outer->addWidget(shell);

    auto *lay = new QVBoxLayout(shell);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    QWidget *top = new QWidget(shell);
    top->setFixedHeight(58);
    top->setStyleSheet("background:transparent;border-bottom:1px solid #EFE7DF;");
    auto *topLay = new QHBoxLayout(top);
    topLay->setContentsMargins(20, 0, 14, 0);
    topLay->setSpacing(10);

    QLabel *icon = new QLabel("ℹ", top);
    icon->setFixedSize(30, 30);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet("color:#D46D56;font-size:22px;font-weight:900;background:transparent;");
    topLay->addWidget(icon);

    QLabel *titleLbl = new QLabel(title, top);
    titleLbl->setStyleSheet("color:#202226;font-size:16px;font-weight:800;background:transparent;");
    topLay->addWidget(titleLbl, 1);

    QPushButton *closeBtn = new RoundCloseButton(top);
    closeBtn->setFixedSize(32, 32);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
     "QPushButton{background:transparent;color:#202226;border:none;border-radius:16px;font-size:22px;font-weight:600;}"
     "QPushButton:hover{background:#F2ECE6;}"
    );
    topLay->addWidget(closeBtn);
    lay->addWidget(top);

    QLabel *msg = new QLabel(message, shell);
    msg->setWordWrap(true);
    msg->setAlignment(Qt::AlignCenter);
    msg->setStyleSheet("color:#4D4A47;font-size:14px;line-height:145%;padding:22px 28px;background:transparent;");
    lay->addWidget(msg, 1);

    auto *buttons = new QHBoxLayout();
    buttons->setContentsMargins(28, 0, 28, 22);
    buttons->setSpacing(10);
    buttons->addStretch();

    if (!secondaryText.isEmpty()) {
      QPushButton *secondary = new QPushButton(secondaryText, shell);
      secondary->setCursor(Qt::PointingHandCursor);
      secondary->setFixedHeight(38);
      secondary->setStyleSheet(CartUi::baseButtonStyle("#FFFFFF", "#F8F3EE", "#F0E6DD", "#42464D", "#E1D8D0"));
      buttons->addWidget(secondary);
      connect(secondary, &QPushButton::clicked, this, &QDialog::reject);
    }

    QPushButton *ok = new QPushButton(accentText, shell);
    ok->setCursor(Qt::PointingHandCursor);
    ok->setFixedHeight(38);
    ok->setStyleSheet(CartUi::baseButtonStyle("#E36550", "#EC735E", "#D84E3E"));
    buttons->addWidget(ok);
    lay->addLayout(buttons);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
  }
};

class CartClearConfirmDialog : public QDialog
{
public:
  explicit CartClearConfirmDialog(QWidget *parent = nullptr)
    : QDialog(parent)
  {
    setModal(true);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, true);
    resize(520, 270);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(0);

    QFrame *shell = new QFrame(this);
    shell->setObjectName("ClearCartShell");
    shell->setStyleSheet(
     "QFrame#ClearCartShell{background:#FFFFFF;border:1px solid #E7DFD7;border-radius:22px;}"
    );
    CartUi::addShadow(shell, 34, 10, 45);
    outer->addWidget(shell);

    auto *main = new QVBoxLayout(shell);
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);

    QWidget *top = new QWidget(shell);
    top->setFixedHeight(60);
    top->setStyleSheet("background:transparent;border-bottom:1px solid #EFE7DF;");
    auto *topLay = new QHBoxLayout(top);
    topLay->setContentsMargins(22, 0, 16, 0);
    topLay->setSpacing(10);

    QLabel *warn = new QLabel("!", top);
    warn->setFixedSize(32, 32);
    warn->setAlignment(Qt::AlignCenter);
    warn->setStyleSheet("color:#3E4146;font-size:22px;background:transparent;");
    topLay->addWidget(warn);

    QLabel *title = new QLabel("Очистить корзину", top);
    title->setStyleSheet("color:#202226;font-size:16px;font-weight:800;background:transparent;");
    topLay->addWidget(title, 1);

    QPushButton *x = new RoundCloseButton(top);
    x->setFixedSize(32, 32);
    x->setCursor(Qt::PointingHandCursor);
    x->setStyleSheet(
     "QPushButton{background:transparent;color:#202226;border:none;border-radius:16px;font-size:22px;font-weight:600;}"
     "QPushButton:hover{background:#F2ECE6;}"
    );
    topLay->addWidget(x);
    main->addWidget(top);

    QWidget *content = new QWidget(shell);
    auto *contentLay = new QHBoxLayout(content);
    contentLay->setContentsMargins(28, 24, 28, 18);
    contentLay->setSpacing(24);

    QLabel *trash = new QLabel("", content);
    trash->setAlignment(Qt::AlignCenter);
    trash->setFixedSize(92, 92);
    trash->setStyleSheet(
     "background:#F6F1EC;border-radius:46px;color:#5C5854;font-size:38px;font-weight:800;"
    );
    contentLay->addWidget(trash);

    QLabel *text = new QLabel(content);
    text->setWordWrap(true);
    text->setText("<b>Вы уверены, что хотите очистить корзину?</b><br><br>Все товары будут удалены без возможности восстановления.");
    text->setStyleSheet("color:#403D39;font-size:14px;line-height:145%;background:transparent;");
    contentLay->addWidget(text, 1);
    main->addWidget(content, 1);

    auto *buttons = new QHBoxLayout();
    buttons->setContentsMargins(28, 0, 28, 24);
    buttons->setSpacing(12);
    buttons->addStretch();

    QPushButton *cancel = new QPushButton("Отмена", shell);
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setFixedSize(150, 40);
    cancel->setStyleSheet(CartUi::baseButtonStyle("#FFFFFF", "#F8F3EE", "#F0E6DD", "#22252A", "#DDD4CC"));
    buttons->addWidget(cancel);

    QPushButton *clear = new QPushButton("Очистить", shell);
    clear->setCursor(Qt::PointingHandCursor);
    clear->setFixedSize(150, 40);
    clear->setStyleSheet(CartUi::baseButtonStyle("#E13F3D", "#EA5652", "#C9302E"));
    buttons->addWidget(clear);
    main->addLayout(buttons);

    connect(x, &QPushButton::clicked, this, &QDialog::reject);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(clear, &QPushButton::clicked, this, &QDialog::accept);
  }
};


class CartAddCardDialog : public QDialog
{
public:
  explicit CartAddCardDialog(QWidget *parent = nullptr)
    : QDialog(parent)
  {
    setModal(true);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, true);
    resize(500, 390);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(0);

    QFrame *shell = new QFrame(this);
    shell->setObjectName("AddCardShell");
    shell->setStyleSheet(
     "QFrame#AddCardShell{background:#FFFFFF;border:1px solid #E7DFD7;border-radius:22px;}"
     "QLineEdit{background:#FFFFFF;color:#202226;border:1px solid #DED6CE;border-radius:9px;padding:8px 10px;font-size:13px;}"
     "QLineEdit:focus{border:1px solid #E36550;}"
    );
    CartUi::addShadow(shell, 34, 10, 45);
    outer->addWidget(shell);

    auto *main = new QVBoxLayout(shell);
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);

    QWidget *top = new QWidget(shell);
    top->setFixedHeight(60);
    top->setStyleSheet("background:transparent;border-bottom:1px solid #EFE7DF;");
    auto *topLay = new QHBoxLayout(top);
    topLay->setContentsMargins(22, 0, 16, 0);
    topLay->setSpacing(10);

    QLabel *cardIcon = new QLabel("", top);
    cardIcon->setFixedSize(32, 32);
    cardIcon->setAlignment(Qt::AlignCenter);
    cardIcon->setStyleSheet("color:#202226;font-size:22px;background:transparent;");
    topLay->addWidget(cardIcon);

    QLabel *title = new QLabel("Добавить карту", top);
    title->setStyleSheet("color:#202226;font-size:16px;font-weight:800;background:transparent;");
    topLay->addWidget(title, 1);

    QPushButton *x = new RoundCloseButton(top);
    x->setFixedSize(32, 32);
    x->setCursor(Qt::PointingHandCursor);
    x->setStyleSheet(
     "QPushButton{background:transparent;color:#202226;border:none;border-radius:16px;font-size:22px;font-weight:600;}"
     "QPushButton:hover{background:#F2ECE6;}"
    );
    topLay->addWidget(x);
    main->addWidget(top);

    QWidget *body = new QWidget(shell);
    auto *form = new QVBoxLayout(body);
    form->setContentsMargins(26, 20, 26, 8);
    form->setSpacing(8);

    QLabel *hint = new QLabel("Это демонстрационная карта для показа интерфейса. Можно вводить тестовые данные — оплата остаётся моковой.", body);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#6A6763;font-size:12px;background:transparent;line-height:140%;");
    form->addWidget(hint);

    auto addEdit = [&](const QString &label, QLineEdit **target, const QString &placeholder) {
      QLabel *l = new QLabel(label, body);
      l->setStyleSheet("color:#5C5955;font-size:12px;font-weight:700;background:transparent;");
      form->addWidget(l);
      *target = new QLineEdit(body);
      (*target)->setPlaceholderText(placeholder);
      form->addWidget(*target);
    };

    addEdit("Название карты", &m_titleEdit, "Например: Моя тестовая карта");
    addEdit("Номер карты", &m_numberEdit, "0000 0000 0000 0000");

    QWidget *row = new QWidget(body);
    row->setStyleSheet("background:transparent;");
    auto *rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->setSpacing(10);

    QWidget *dateBox = new QWidget(row);
    auto *dateLay = new QVBoxLayout(dateBox);
    dateLay->setContentsMargins(0, 0, 0, 0);
    dateLay->setSpacing(6);
    QLabel *dateLbl = new QLabel("Срок", dateBox);
    dateLbl->setStyleSheet("color:#5C5955;font-size:12px;font-weight:700;background:transparent;");
    dateLay->addWidget(dateLbl);
    m_expiryEdit = new QLineEdit(dateBox);
    m_expiryEdit->setPlaceholderText("12/30");
    dateLay->addWidget(m_expiryEdit);
    rowLay->addWidget(dateBox, 1);

    QWidget *cvvBox = new QWidget(row);
    auto *cvvLay = new QVBoxLayout(cvvBox);
    cvvLay->setContentsMargins(0, 0, 0, 0);
    cvvLay->setSpacing(6);
    QLabel *cvvLbl = new QLabel("CVV", cvvBox);
    cvvLbl->setStyleSheet("color:#5C5955;font-size:12px;font-weight:700;background:transparent;");
    cvvLay->addWidget(cvvLbl);
    m_cvvEdit = new QLineEdit(cvvBox);
    m_cvvEdit->setPlaceholderText("000");
    m_cvvEdit->setEchoMode(QLineEdit::Password);
    cvvLay->addWidget(m_cvvEdit);
    rowLay->addWidget(cvvBox, 1);
    form->addWidget(row);

    m_errorLabel = new QLabel(body);
    m_errorLabel->setStyleSheet("color:#D9483B;font-size:12px;background:transparent;");
    form->addWidget(m_errorLabel);

    main->addWidget(body, 1);

    auto *buttons = new QHBoxLayout();
    buttons->setContentsMargins(26, 8, 26, 24);
    buttons->setSpacing(12);
    buttons->addStretch();

    QPushButton *cancel = new QPushButton("Отмена", shell);
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setFixedSize(140, 40);
    cancel->setStyleSheet(CartUi::baseButtonStyle("#FFFFFF", "#F8F3EE", "#F0E6DD", "#22252A", "#DDD4CC"));
    buttons->addWidget(cancel);

    QPushButton *save = new QPushButton("Добавить", shell);
    save->setCursor(Qt::PointingHandCursor);
    save->setFixedSize(150, 40);
    save->setStyleSheet(CartUi::baseButtonStyle("#E36550", "#EC735E", "#D84E3E"));
    buttons->addWidget(save);
    main->addLayout(buttons);

    m_titleEdit->setText("Тестовая карта");
    m_expiryEdit->setText("12/30");
    m_cvvEdit->setText("000");

    connect(m_numberEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
      const QString formatted = formatCardNumber(text);
      if (formatted != text) {
        QSignalBlocker blocker(m_numberEdit);
        m_numberEdit->setText(formatted);
      }
    });

    connect(m_expiryEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
      const QString formatted = formatExpiry(text);
      if (formatted != text) {
        QSignalBlocker blocker(m_expiryEdit);
        m_expiryEdit->setText(formatted);
      }
    });

    connect(m_cvvEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
      QString digits = onlyDigits(text).left(4);
      if (digits != text) {
        QSignalBlocker blocker(m_cvvEdit);
        m_cvvEdit->setText(digits);
      }
    });

    connect(x, &QPushButton::clicked, this, &QDialog::reject);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, [this]() { tryAccept(); });
  }

  QString cardTitle() const
  {
    const QString title = m_titleEdit->text().trimmed();
    return title.isEmpty() ? QString("Банковская карта") : title;
  }

  QString maskedCard() const
  {
    const QString digits = onlyDigits(m_numberEdit->text());
    const QString last4 = digits.size() >= 4 ? digits.right(4) : QString("0000");
    return QString("**** **** **** %1").arg(last4);
  }

private:
  QLineEdit *m_titleEdit = nullptr;
  QLineEdit *m_numberEdit = nullptr;
  QLineEdit *m_expiryEdit = nullptr;
  QLineEdit *m_cvvEdit = nullptr;
  QLabel *m_errorLabel = nullptr;

  static QString onlyDigits(const QString &text)
  {
    QString digits;
    digits.reserve(text.size());
    for (const QChar ch : text) {
      if (ch.isDigit())
        digits.append(ch);
    }
    return digits;
  }

  static QString formatCardNumber(const QString &text)
  {
    const QString digits = onlyDigits(text).left(16);
    QString out;
    for (int i = 0; i < digits.size(); ++i) {
      if (i > 0 && i % 4 == 0)
        out.append(' ');
      out.append(digits.at(i));
    }
    return out;
  }

  static QString formatExpiry(const QString &text)
  {
    const QString digits = onlyDigits(text).left(4);
    if (digits.size() <= 2)
      return digits;
    return digits.left(2) + "/" + digits.mid(2);
  }

  void tryAccept()
  {
    const QString digits = onlyDigits(m_numberEdit->text());
    const QString expiry = m_expiryEdit->text().trimmed();
    const QString cvv = onlyDigits(m_cvvEdit->text());

    if (digits.size() < 12) {
      m_errorLabel->setText("Введите тестовый номер карты: минимум 12 цифр.");
      return;
    }
    if (expiry.size() != 5) {
      m_errorLabel->setText("Укажите срок в формате ММ/ГГ.");
      return;
    }
    if (cvv.size() < 3) {
      m_errorLabel->setText("Введите тестовый CVV: 3 или 4 цифры.");
      return;
    }

    accept();
  }
};



class CartQrPaymentDialog : public QDialog
{
public:
  CartQrPaymentDialog(double total, QWidget *parent = nullptr)
    : QDialog(parent)
    , m_payload(QString("CCPAY:%1").arg(qRound(total)))
  {
    setModal(true);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, true);
    resize(430, 500);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(0);

    QFrame *shell = new QFrame(this);
    shell->setObjectName("QrPaymentShell");
    shell->setStyleSheet(
     "QFrame#QrPaymentShell{background:#FFFFFF;border:1px solid #E7DFD7;border-radius:22px;}"
    );
    CartUi::addShadow(shell, 34, 10, 45);
    outer->addWidget(shell);

    auto *main = new QVBoxLayout(shell);
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);

    QWidget *top = new QWidget(shell);
    top->setFixedHeight(60);
    top->setStyleSheet("background:transparent;border-bottom:1px solid #EFE7DF;");
    auto *topLay = new QHBoxLayout(top);
    topLay->setContentsMargins(22, 0, 16, 0);
    topLay->setSpacing(10);

    QLabel *qrIcon = new QLabel("▦", top);
    qrIcon->setFixedSize(32, 32);
    qrIcon->setAlignment(Qt::AlignCenter);
    qrIcon->setStyleSheet("color:#202226;font-size:22px;font-weight:900;background:transparent;");
    topLay->addWidget(qrIcon);

    QLabel *title = new QLabel("Оплата по QR-коду", top);
    title->setStyleSheet("color:#202226;font-size:16px;font-weight:800;background:transparent;");
    topLay->addWidget(title, 1);

    QPushButton *x = new RoundCloseButton(top);
    x->setFixedSize(32, 32);
    x->setCursor(Qt::PointingHandCursor);
    x->setStyleSheet(
     "QPushButton{background:transparent;color:#202226;border:none;border-radius:16px;font-size:22px;font-weight:600;}"
     "QPushButton:hover{background:#F2ECE6;}"
    );
    topLay->addWidget(x);
    main->addWidget(top);

    QWidget *body = new QWidget(shell);
    auto *bodyLay = new QVBoxLayout(body);
    bodyLay->setContentsMargins(28, 20, 28, 18);
    bodyLay->setSpacing(12);
    bodyLay->setAlignment(Qt::AlignHCenter);

    QLabel *amount = new QLabel(QString("Сумма к оплате: <b>%1</b>").arg(CartUi::money2(total)), body);
    amount->setAlignment(Qt::AlignCenter);
    amount->setStyleSheet("color:#202226;font-size:15px;background:transparent;");
    bodyLay->addWidget(amount);

    QLabel *hint = new QLabel("Отсканируйте QR-код камерой телефона. Это тестовый QR для демонстрации мок-оплаты.", body);
    hint->setWordWrap(true);
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("color:#6A6763;font-size:12px;background:transparent;line-height:140%;");
    bodyLay->addWidget(hint);

    QLabel *qr = new QLabel(body);
    qr->setAlignment(Qt::AlignCenter);
    qr->setFixedSize(286, 286);
    qr->setPixmap(CartUi::generateQrCodePixmap(m_payload, 9, 5));
    qr->setStyleSheet("background:#FFFFFF;border:1px solid #E7DFD7;border-radius:16px;padding:12px;");
    bodyLay->addWidget(qr, 0, Qt::AlignHCenter);

    QLabel *payload = new QLabel(QString("Код платежа: <b>%1</b>").arg(m_payload), body);
    payload->setAlignment(Qt::AlignCenter);
    payload->setTextInteractionFlags(Qt::TextSelectableByMouse);
    payload->setStyleSheet("color:#4D4A47;font-size:12px;background:transparent;");
    bodyLay->addWidget(payload);

    main->addWidget(body, 1);

    auto *buttons = new QHBoxLayout();
    buttons->setContentsMargins(28, 0, 28, 24);
    buttons->setSpacing(12);
    buttons->addStretch();

    QPushButton *copy = new QPushButton("Скопировать код", shell);
    copy->setCursor(Qt::PointingHandCursor);
    copy->setFixedSize(160, 40);
    copy->setStyleSheet(CartUi::baseButtonStyle("#FFFFFF", "#F8F3EE", "#F0E6DD", "#22252A", "#DDD4CC"));
    buttons->addWidget(copy);

    QPushButton *ready = new QPushButton("Готово", shell);
    ready->setCursor(Qt::PointingHandCursor);
    ready->setFixedSize(130, 40);
    ready->setStyleSheet(CartUi::baseButtonStyle("#E36550", "#EC735E", "#D84E3E"));
    buttons->addWidget(ready);
    main->addLayout(buttons);

    connect(x, &QPushButton::clicked, this, &QDialog::accept);
    connect(ready, &QPushButton::clicked, this, &QDialog::accept);
    connect(copy, &QPushButton::clicked, this, [this, copy]() {
      QApplication::clipboard()->setText(m_payload);
      copy->setText("Код скопирован");
      QTimer::singleShot(1200, copy, [copy]() { copy->setText("Скопировать код"); });
    });
  }

private:
  QString m_payload;
};

// Checkout dialog collects delivery data and selected payment method.
class CartCheckoutDialog : public QDialog
{
public:
  CartCheckoutDialog(const QString &username,
            double subtotal,
            double delivery,
            double tax,
            double total,
            QWidget *parent = nullptr)
    : QDialog(parent)
    , m_subtotal(subtotal)
    , m_delivery(delivery)
    , m_tax(tax)
    , m_total(total)
  {
    setModal(true);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, true);
    resize(820, 540);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(0);

    QFrame *shell = new QFrame(this);
    shell->setObjectName("CheckoutShell");
    shell->setStyleSheet(
     "QFrame#CheckoutShell{background:#FFFFFF;border:1px solid #E7DFD7;border-radius:22px;}"
     "QLineEdit{background:#FFFFFF;color:#202226;border:1px solid #DED6CE;border-radius:9px;padding:7px 10px;font-size:13px;}"
     "QLineEdit:focus{border:1px solid #E36550;}"
     "QPushButton#PaymentOption{background:#FFFFFF;color:#33363B;border:2px solid #DDD4CC;border-radius:12px;"
     "padding:7px 10px;text-align:left;font-size:12px;font-weight:700;}"
     "QPushButton#PaymentOption:hover{background:#FFFBF7;border-color:#D9B7A8;}"
     "QPushButton#PaymentOption:checked{background:#FFF2ED;border:2px solid #E36550;color:#202226;}"
     "QPushButton#AddPaymentMethod{background:#FFFFFF;color:#D46D56;border:1px dashed #D46D56;border-radius:11px;"
     "padding:7px 10px;text-align:center;font-size:12px;font-weight:800;}"
     "QPushButton#AddPaymentMethod:hover{background:#FFF2ED;}"
    );
    CartUi::addShadow(shell, 34, 10, 45);
    outer->addWidget(shell);

    auto *main = new QVBoxLayout(shell);
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);

    QWidget *top = new QWidget(shell);
    top->setFixedHeight(60);
    top->setStyleSheet("background:transparent;border-bottom:1px solid #EFE7DF;");
    auto *topLay = new QHBoxLayout(top);
    topLay->setContentsMargins(24, 0, 16, 0);
    topLay->setSpacing(12);

    QLabel *cartIcon = new QLabel("", top);
    cartIcon->setFixedSize(30, 30);
    cartIcon->setAlignment(Qt::AlignCenter);
    cartIcon->setStyleSheet("color:#202226;font-size:22px;background:transparent;");
    topLay->addWidget(cartIcon);

    QLabel *title = new QLabel("Оформление заказа", top);
    title->setStyleSheet("color:#202226;font-size:16px;font-weight:800;background:transparent;");
    topLay->addWidget(title, 1);

    QPushButton *x = new RoundCloseButton(top);
    x->setFixedSize(32, 32);
    x->setCursor(Qt::PointingHandCursor);
    x->setStyleSheet(
     "QPushButton{background:transparent;color:#202226;border:none;border-radius:16px;font-size:22px;font-weight:600;}"
     "QPushButton:hover{background:#F2ECE6;}"
    );
    topLay->addWidget(x);
    main->addWidget(top);

    QWidget *body = new QWidget(shell);
    auto *bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(24, 16, 24, 10);
    bodyLay->setSpacing(16);

    // Delivery
    QWidget *deliveryBox = new QWidget(body);
    auto *deliveryLay = new QVBoxLayout(deliveryBox);
    deliveryLay->setContentsMargins(0, 0, 0, 0);
    deliveryLay->setSpacing(8);

    QLabel *deliveryTitle = new QLabel("Доставка", deliveryBox);
    deliveryTitle->setStyleSheet("color:#202226;font-size:14px;font-weight:800;background:transparent;");
    deliveryLay->addWidget(deliveryTitle);

    auto addField = [&](const QString &label, QLineEdit **target, const QString &placeholder, const QString &value = QString()) {
      QLabel *l = new QLabel(label, deliveryBox);
      l->setStyleSheet("color:#6A6763;font-size:11px;background:transparent;");
      deliveryLay->addWidget(l);
      *target = new QLineEdit(deliveryBox);
      (*target)->setPlaceholderText(placeholder);
      (*target)->setText(value);
      deliveryLay->addWidget(*target);
    };

    addField("Получатель", &m_recipientEdit, "ФИО получателя", username == "admin" ? QString() : username);
    addField("Адрес доставки", &m_addressEdit, "Город, улица, дом, квартира");
    addField("Телефон", &m_phoneEdit, "+7 (___) ___-__-__");
    if (m_recipientEdit)
      m_recipientEdit->setMaxLength(80);
    if (m_phoneEdit) {
      m_phoneEdit->setMaxLength(24);
      m_phoneEdit->setInputMethodHints(Qt::ImhDialableCharactersOnly);
    }
    deliveryLay->addStretch();
    bodyLay->addWidget(deliveryBox, 11);

    QFrame *line1 = new QFrame(body);
    line1->setFrameShape(QFrame::VLine);
    line1->setStyleSheet("color:#E8DFD7;background:#E8DFD7;max-width:1px;");
    bodyLay->addWidget(line1);

    // Payment method
    QWidget *paymentBox = new QWidget(body);
    auto *paymentLay = new QVBoxLayout(paymentBox);
    m_paymentLay = paymentLay;
    paymentLay->setContentsMargins(0, 0, 0, 0);
    paymentLay->setSpacing(7);

    QLabel *paymentTitle = new QLabel("Способ оплаты", paymentBox);
    paymentTitle->setStyleSheet("color:#202226;font-size:14px;font-weight:800;background:transparent;");
    paymentLay->addWidget(paymentTitle);

    QLabel *paymentHint = new QLabel("Выбранный вариант подсвечивается рамкой.", paymentBox);
    paymentHint->setWordWrap(true);
    paymentHint->setStyleSheet("color:#77716B;font-size:10px;background:transparent;");
    paymentLay->addWidget(paymentHint);

    m_paymentGroup = new QButtonGroup(this);
    m_paymentGroup->setExclusive(true);

    m_cardPaymentBtn = makePaymentOption("Банковская карта", "**** **** **** 4242", paymentBox);
    m_cashPaymentBtn = makePaymentOption("Оплата при получении", "Без предоплаты", paymentBox);
    m_eMoneyPaymentBtn = makePaymentOption("Электронные деньги", "ЮMoney / WebMoney", paymentBox);
    m_qrPaymentBtn = makePaymentOption("Оплата по QR-коду", "СБП / тестовый QR", paymentBox);

    m_paymentGroup->addButton(m_cardPaymentBtn, 0);
    m_paymentGroup->addButton(m_cashPaymentBtn, 1);
    m_paymentGroup->addButton(m_eMoneyPaymentBtn, 2);
    m_paymentGroup->addButton(m_qrPaymentBtn, 3);
    m_cardPaymentBtn->setChecked(true);

    paymentLay->addWidget(m_cardPaymentBtn);
    paymentLay->addWidget(m_cashPaymentBtn);
    paymentLay->addWidget(m_eMoneyPaymentBtn);

    m_addPaymentBtn = new QPushButton("+ Добавить способ оплаты", paymentBox);
    m_addPaymentBtn->setObjectName("AddPaymentMethod");
    m_addPaymentBtn->setCursor(Qt::PointingHandCursor);
    m_addPaymentBtn->setFixedHeight(34);
    paymentLay->addWidget(m_addPaymentBtn);

    paymentLay->addWidget(m_qrPaymentBtn);
    paymentLay->addStretch();
    bodyLay->addWidget(paymentBox, 9);

    connect(m_addPaymentBtn, &QPushButton::clicked, this, [this]() {
      CartAddCardDialog dlg(this);
      if (dlg.exec() != QDialog::Accepted)
        return;

      const QString title = dlg.cardTitle();
      const QString masked = dlg.maskedCard();
      m_addedCardText = QString("%1 %2").arg(title, masked);

      if (!m_addedCardBtn) {
        m_addedCardBtn = makePaymentOption(title, masked, this);
        m_paymentGroup->addButton(m_addedCardBtn, 4);
        const int insertIndex = m_paymentLay ? m_paymentLay->indexOf(m_addPaymentBtn) : -1;
        if (m_paymentLay)
          m_paymentLay->insertWidget(insertIndex >= 0 ? insertIndex : 0, m_addedCardBtn);
      } else {
        m_addedCardBtn->setText(QString("%1\n%2").arg(title, masked));
      }

      m_addedCardBtn->setChecked(true);
      if (m_errorLabel) {
        m_errorLabel->setStyleSheet("color:#2E8B57;font-size:12px;padding:0 28px;background:transparent;");
        m_errorLabel->setText(QString("Добавлена карта для показа: %1.").arg(masked));
      }
    });

    connect(m_qrPaymentBtn, &QPushButton::clicked, this, [this]() {
      CartQrPaymentDialog dlg(m_total, this);
      dlg.exec();
    });

    QFrame *line2 = new QFrame(body);
    line2->setFrameShape(QFrame::VLine);
    line2->setStyleSheet("color:#E8DFD7;background:#E8DFD7;max-width:1px;");
    bodyLay->addWidget(line2);

    // Totals
    QWidget *summaryBox = new QWidget(body);
    auto *summaryLay = new QVBoxLayout(summaryBox);
    summaryLay->setContentsMargins(0, 0, 0, 0);
    summaryLay->setSpacing(9);

    QLabel *summaryTitle = new QLabel("Ваш заказ", summaryBox);
    summaryTitle->setStyleSheet("color:#202226;font-size:14px;font-weight:800;background:transparent;");
    summaryLay->addWidget(summaryTitle);
    summaryLay->addWidget(makeSummaryRow("Товаров на сумму", CartUi::money(m_subtotal), summaryBox));
    summaryLay->addWidget(makeSummaryRow("Доставка", CartUi::money(m_delivery), summaryBox));
    summaryLay->addWidget(makeSummaryRow("Налог (НДС 20%)", CartUi::money(m_tax), summaryBox));

    QFrame *divider = new QFrame(summaryBox);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet("color:#E8DFD7;background:#E8DFD7;max-height:1px;");
    summaryLay->addWidget(divider);

    QLabel *grand = new QLabel(QString("<b>Итого к оплате</b><br><span style='font-size:18px;font-weight:900;'>%1</span>").arg(CartUi::money2(m_total)), summaryBox);
    grand->setStyleSheet("color:#15171A;background:transparent;");
    summaryLay->addWidget(grand);
    summaryLay->addStretch();
    bodyLay->addWidget(summaryBox, 7);

    main->addWidget(body, 1);

    m_errorLabel = new QLabel(shell);
    m_errorLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_errorLabel->setStyleSheet("color:#D9483B;font-size:12px;padding:0 28px;background:transparent;");
    main->addWidget(m_errorLabel);

    auto *buttons = new QHBoxLayout();
    buttons->setContentsMargins(24, 8, 24, 24);
    buttons->setSpacing(12);
    buttons->addStretch();

    QPushButton *cancel = new QPushButton("Отмена", shell);
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setFixedSize(140, 40);
    cancel->setStyleSheet(CartUi::baseButtonStyle("#FFFFFF", "#F8F3EE", "#F0E6DD", "#22252A", "#DDD4CC"));
    buttons->addWidget(cancel);

    QPushButton *confirm = new QPushButton("Подтвердить", shell);
    confirm->setCursor(Qt::PointingHandCursor);
    confirm->setFixedSize(160, 40);
    confirm->setStyleSheet(CartUi::baseButtonStyle("#E36550", "#EC735E", "#D84E3E"));
    buttons->addWidget(confirm);
    main->addLayout(buttons);

    connect(x, &QPushButton::clicked, this, &QDialog::reject);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(confirm, &QPushButton::clicked, this, [this]() { tryAccept(); });
  }

  QString address() const
  {
    return QString("Получатель: %1\nАдрес: %2\nТелефон: %3")
      .arg(m_recipientEdit->text().trimmed(),
         m_addressEdit->text().trimmed(),
         m_phoneEdit->text().trimmed());
  }

  QString paymentMethod() const
  {
    const int id = m_paymentGroup ? m_paymentGroup->checkedId() : 0;
    switch (id) {
    case 1:
      return "Оплата при получении";
    case 2:
      return "Электронные деньги (тест)";
    case 3:
      return "Оплата по QR-коду (тест)";
    case 4:
      return m_addedCardText.isEmpty() ? QString("Добавленная карта (тест)") : m_addedCardText + " (тест)";
    case 0:
    default:
      return "Банковская карта (тест)";
    }
  }

private:
  double m_subtotal = 0.0;
  double m_delivery = 0.0;
  double m_tax = 0.0;
  double m_total = 0.0;

  QLineEdit *m_recipientEdit = nullptr;
  QLineEdit *m_addressEdit = nullptr;
  QLineEdit *m_phoneEdit = nullptr;
  QButtonGroup *m_paymentGroup = nullptr;
  QVBoxLayout *m_paymentLay = nullptr;
  QPushButton *m_cardPaymentBtn = nullptr;
  QPushButton *m_cashPaymentBtn = nullptr;
  QPushButton *m_eMoneyPaymentBtn = nullptr;
  QPushButton *m_qrPaymentBtn = nullptr;
  QPushButton *m_addPaymentBtn = nullptr;
  QPushButton *m_addedCardBtn = nullptr;
  QLabel *m_errorLabel = nullptr;
  QString m_addedCardText;

  static QString onlyDigits(const QString &text)
  {
    QString digits;
    digits.reserve(text.size());
    for (const QChar ch : text) {
      if (ch.isDigit())
        digits.append(ch);
    }
    return digits;
  }

  static bool isValidRecipientName(const QString &text)
  {
    const QString normalized = text.simplified();
    const QStringList parts = normalized.split(' ', Qt::SkipEmptyParts);
    if (parts.size() < 2)
      return false;

    for (const QString &part : parts) {
      if (part.size() < 2)
        return false;
      for (const QChar ch : part) {
        if (!ch.isLetter() && ch != '-')
          return false;
      }
    }
    return true;
  }

  static bool isValidCheckoutPhone(const QString &text)
  {
    const QString phone = text.trimmed();
    const QString digits = onlyDigits(phone);
    if (digits.size() < 10 || digits.size() > 15)
      return false;
    if (phone.count('+') > 1)
      return false;
    if (phone.contains('+') && !phone.startsWith('+'))
      return false;

    for (const QChar ch : phone) {
      if (!ch.isDigit() && ch != '+' && ch != ' ' && ch != '(' && ch != ')' && ch != '-')
        return false;
    }
    return true;
  }

  QPushButton *makePaymentOption(const QString &title, const QString &subtitle, QWidget *parent)
  {
    QPushButton *button = new QPushButton(parent);
    button->setObjectName("PaymentOption");
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setText(subtitle.isEmpty() ? title : QString("%1\n%2").arg(title, subtitle));
    button->setFixedHeight(subtitle.isEmpty() ? 40 : 48);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return button;
  }

  QWidget *makeSummaryRow(const QString &left, const QString &right, QWidget *parent)
  {
    QWidget *w = new QWidget(parent);
    w->setStyleSheet("background:transparent;");
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(8);

    QLabel *a = new QLabel(left, w);
    a->setStyleSheet("color:#5C5A57;font-size:12px;background:transparent;");
    QLabel *b = new QLabel(right, w);
    b->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    b->setStyleSheet("color:#202226;font-size:12px;font-weight:700;background:transparent;");
    l->addWidget(a, 1);
    l->addWidget(b);
    return w;
  }

  void tryAccept()
  {
    const QString recipient = m_recipientEdit->text().trimmed();
    const QString address = m_addressEdit->text().trimmed();
    const QString phone = m_phoneEdit->text().trimmed();

    m_errorLabel->setStyleSheet("color:#D9483B;font-size:12px;padding:0 28px;background:transparent;");
    if (!isValidRecipientName(recipient)) {
      m_errorLabel->setText("Введите ФИО: минимум имя и фамилию, только буквы и дефис.");
      m_recipientEdit->setFocus();
      return;
    }
    if (address.size() < 10) {
      m_errorLabel->setText("Введите полный адрес доставки.");
      return;
    }
    if (!isValidCheckoutPhone(phone)) {
      m_errorLabel->setText("Введите корректный телефон: 10-15 цифр, можно с +7.");
      m_phoneEdit->setFocus();
      return;
    }

    accept();
  }
};


class CartPaymentProcessingDialog : public QDialog
{
public:
  explicit CartPaymentProcessingDialog(QWidget *parent = nullptr)
    : QDialog(parent)
  {
    setModal(true);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, true);
    resize(430, 220);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(0);

    QFrame *shell = new QFrame(this);
    shell->setObjectName("PaymentProcessingShell");
    shell->setStyleSheet(
     "QFrame#PaymentProcessingShell{background:#FFFFFF;border:1px solid #E7DFD7;border-radius:22px;}"
    );
    CartUi::addShadow(shell, 34, 10, 45);
    outer->addWidget(shell);

    auto *main = new QVBoxLayout(shell);
    main->setContentsMargins(28, 24, 28, 24);
    main->setSpacing(12);
    main->setAlignment(Qt::AlignCenter);

    m_icon = new QLabel("⌛", shell);
    m_icon->setFixedSize(78, 78);
    m_icon->setAlignment(Qt::AlignCenter);
    m_icon->setStyleSheet(
     "background:#FFF2ED;color:#D46D56;border:1px solid #F0D1C7;border-radius:39px;"
     "font-size:34px;font-weight:900;"
    );
    main->addWidget(m_icon, 0, Qt::AlignHCenter);

    m_status = new QLabel("Проводится оплата", shell);
    m_status->setAlignment(Qt::AlignCenter);
    m_status->setStyleSheet("color:#202226;font-size:18px;font-weight:900;background:transparent;");
    main->addWidget(m_status);

    m_hint = new QLabel("Пожалуйста, подождите 5 секунд. Это тестовая мок-оплата.", shell);
    m_hint->setAlignment(Qt::AlignCenter);
    m_hint->setWordWrap(true);
    m_hint->setStyleSheet("color:#6A6763;font-size:12px;background:transparent;");
    main->addWidget(m_hint);

    auto *pulse = new QTimer(this);
    pulse->setInterval(500);
    connect(pulse, &QTimer::timeout, this, [this]() {
      m_dotStep = (m_dotStep + 1) % 4;
      m_status->setText("Проводится оплата" + QString(m_dotStep, '.'));
    });
    pulse->start();

    QTimer::singleShot(5000, this, [this, pulse]() {
      pulse->stop();
      m_icon->setText("Готово");
      m_icon->setStyleSheet(
       "background:#EEF8F1;color:#2E8B57;border:1px solid #CDEAD6;border-radius:39px;"
       "font-size:38px;font-weight:900;"
      );
      m_status->setText("Оплата успешно прошла");
      m_hint->setText("Заказ можно сохранять и передавать в раздел «Мои заказы».");
      QTimer::singleShot(850, this, [this]() { accept(); });
    });
  }

private:
  QLabel *m_icon = nullptr;
  QLabel *m_status = nullptr;
  QLabel *m_hint = nullptr;
  int m_dotStep = 0;
};

// Full cart window with item controls, totals, checkout, and clear-cart actions.
class CartDialog : public QDialog
{
public:
  CartDialog(const QString &username, CartManager &cart, QWidget *parent = nullptr)
    : QDialog(parent)
    , m_username(username)
    , m_cart(cart)
  {
    setWindowTitle("Корзина");
    setModal(true);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFixedSize(840, 550);

    setStyleSheet(
     "QDialog{background:transparent;color:#202226;}"
     "QFrame#CartShell{background:#FFFFFF;border:1px solid #E1D8D0;border-radius:18px;}"
     "QFrame#CartTable{background:#FFFFFF;border:1px solid #DAD2CB;border-radius:14px;}"
     "QFrame#CartHeader{background:#FBFAF8;border:none;border-bottom:1px solid #E5DED7;border-top-left-radius:14px;border-top-right-radius:14px;}"
     "QFrame#CartRow{background:#FFFFFF;border:none;border-bottom:1px solid #EEE7E0;}"
     "QFrame#CartRow:hover{background:#FFFBF7;}"
     "QLabel{background:transparent;}"
     "QScrollArea{background:transparent;border:none;}"
     "QScrollArea > QWidget > QWidget{background:transparent;}"
     "QPushButton#QtyBtn{background:#FFFFFF;color:#202226;border:1px solid #D8D1CA;border-radius:8px;font-size:17px;font-weight:700;padding:0;}"
     "QPushButton#QtyBtn:hover{background:#F6EFE8;border-color:#CFC4BA;}"
     "QPushButton#DeleteBtn{background:transparent;color:#E13F3D;border:none;border-radius:15px;font-size:22px;font-weight:800;padding:0;}"
     "QPushButton#DeleteBtn:hover{background:#FFF1EF;}"
    );

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(0);

    QFrame *shell = new QFrame(this);
    shell->setObjectName("CartShell");
    CartUi::addShadow(shell, 28, 7, 34);
    outer->addWidget(shell);

    auto *main = new QVBoxLayout(shell);
    main->setContentsMargins(16, 12, 16, 14);
    main->setSpacing(8);

    // Top row
    QWidget *topBar = new QWidget(shell);
    auto *topLay = new QHBoxLayout(topBar);
    topLay->setContentsMargins(0, 0, 0, 4);
    topLay->setSpacing(12);

    QLabel *cartIcon = new QLabel("", topBar);
    cartIcon->setFixedSize(34, 34);
    cartIcon->setAlignment(Qt::AlignCenter);
    cartIcon->setStyleSheet("font-size:28px;color:#202226;background:transparent;");
    topLay->addWidget(cartIcon);

    QLabel *title = CartUi::makeTitleLabel("Корзина", topBar);
    topLay->addWidget(title);
    topLay->addStretch();

    QLabel *user = new QLabel(QString("Пользователь: <b>%1</b>").arg(m_username), topBar);
    user->setStyleSheet("color:#202226;font-size:14px;background:transparent;");
    topLay->addWidget(user);

    QLabel *avatar = new QLabel("●", topBar);
    avatar->setFixedSize(34, 34);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet("background:#F2D7CE;color:#D46D56;border-radius:17px;font-size:26px;font-weight:900;");
    topLay->addWidget(avatar);

    QPushButton *closeBtn = new RoundCloseButton(topBar);
    closeBtn->setFixedSize(32, 32);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setToolTip("Закрыть корзину");
    closeBtn->setStyleSheet(
     "QPushButton{background:transparent;color:#202226;border:none;border-radius:16px;font-size:22px;font-weight:600;padding:0;}"
     "QPushButton:hover{background:#F2ECE6;}"
     "QPushButton:pressed{background:#E7DFD7;}"
    );
    topLay->addWidget(closeBtn);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    main->addWidget(topBar);

    QFrame *line = new QFrame(shell);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("background:#E7DFD7;color:#E7DFD7;max-height:1px;");
    main->addWidget(line);

    QLabel *welcome = new QLabel(QString("Добро пожаловать, <b>%1</b>! Просмотр и оформление твоей корзины.").arg(m_username), shell);
    welcome->setStyleSheet("color:#33363B;font-size:14px;background:transparent;");
    main->addWidget(welcome);

    // Cart table
    QFrame *table = new QFrame(shell);
    table->setObjectName("CartTable");
    auto *tableLay = new QVBoxLayout(table);
    tableLay->setContentsMargins(0, 0, 0, 0);
    tableLay->setSpacing(0);

    tableLay->addWidget(createHeader(table));

    m_scroll = new QScrollArea(table);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setMinimumHeight(245);
    m_scroll->setMaximumHeight(268);

    QWidget *rowsHost = new QWidget(m_scroll);
    rowsHost->setStyleSheet("background:transparent;");
    m_rowsLayout = new QVBoxLayout(rowsHost);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(0);
    m_scroll->setWidget(rowsHost);
    tableLay->addWidget(m_scroll, 1);
    main->addWidget(table, 1);

    // Bottom area: buttons and totals
    QWidget *bottom = new QWidget(shell);
    auto *bottomLay = new QHBoxLayout(bottom);
    bottomLay->setContentsMargins(0, 2, 0, 0);
    bottomLay->setSpacing(10);

    m_continueBtn = new QPushButton("← Продолжить покупки", bottom);
    m_clearBtn = new QPushButton("Очистить корзину", bottom);
    m_orderBtn = new QPushButton("Оформить заказ", bottom);

    m_continueBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_orderBtn->setCursor(Qt::PointingHandCursor);
    m_continueBtn->setFixedSize(214, 44);
    m_clearBtn->setFixedSize(166, 44);
    m_orderBtn->setFixedSize(174, 44);
    m_continueBtn->setStyleSheet(CartUi::actionButtonStyle("#858B90", "#959BA0", "#737A80"));
    m_clearBtn->setStyleSheet(CartUi::actionButtonStyle("#858B90", "#959BA0", "#737A80"));
    m_orderBtn->setStyleSheet(CartUi::actionButtonStyle("#D46D56", "#E07862", "#BD5946"));

    QWidget *buttonsBox = new QWidget(bottom);
    auto *buttonsLay = new QHBoxLayout(buttonsBox);
    buttonsLay->setContentsMargins(0, 0, 0, 0);
    buttonsLay->setSpacing(10);
    buttonsLay->addWidget(m_continueBtn);
    buttonsLay->addWidget(m_clearBtn);
    buttonsLay->addWidget(m_orderBtn);
    buttonsLay->addStretch();
    bottomLay->addWidget(buttonsBox, 1, Qt::AlignBottom);

    QWidget *summary = createSummary(bottom);
    bottomLay->addWidget(summary, 0, Qt::AlignRight | Qt::AlignBottom);
    main->addWidget(bottom);

    connect(m_continueBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_clearBtn, &QPushButton::clicked, this, &CartDialog::onClear);
    connect(m_orderBtn, &QPushButton::clicked, this, &CartDialog::onCreateOrder);

    reload();
  }

protected:
  void mousePressEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton && event->position().y() <= 74) {
      m_dragging = true;
      m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
      event->accept();
      return;
    }
    QDialog::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override
  {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
      move(event->globalPosition().toPoint() - m_dragPosition);
      event->accept();
      return;
    }
    QDialog::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton)
      m_dragging = false;
    QDialog::mouseReleaseEvent(event);
  }

private:
  QString    m_username;
  CartManager &m_cart;
  QScrollArea *m_scroll = nullptr;
  QVBoxLayout *m_rowsLayout = nullptr;
  QLabel    *m_subtotalLabel = nullptr;
  QLabel    *m_deliveryLabel = nullptr;
  QLabel    *m_taxLabel = nullptr;
  QLabel    *m_grandTotalLabel = nullptr;
  QPushButton *m_continueBtn = nullptr;
  QPushButton *m_clearBtn = nullptr;
  QPushButton *m_orderBtn = nullptr;
  bool     m_dragging = false;
  QPoint    m_dragPosition;

  static constexpr double DeliveryPrice = 350.0;
  static constexpr double VatRate = 0.20;

  double subtotal() const { return m_cart.total(); }
  double delivery() const { return m_cart.items().isEmpty() ? 0.0 : DeliveryPrice; }
  double tax() const { return subtotal() * VatRate; }
  double grandTotal() const { return subtotal() + delivery() + tax(); }

  QFrame *createHeader(QWidget *parent)
  {
    QFrame *header = new QFrame(parent);
    header->setObjectName("CartHeader");
    header->setFixedHeight(40);
    auto *lay = new QHBoxLayout(header);
    lay->setContentsMargins(18, 0, 18, 0);
    lay->setSpacing(10);

    addHeaderLabel(lay, "Товар", 1, Qt::AlignLeft | Qt::AlignVCenter);
    addHeaderLabel(lay, "Цена", 90, Qt::AlignCenter);
    addHeaderLabel(lay, "Кол-во", 118, Qt::AlignCenter);
    addHeaderLabel(lay, "Сумма", 100, Qt::AlignCenter);
    addHeaderLabel(lay, "Действие", 82, Qt::AlignCenter);
    return header;
  }

  void addHeaderLabel(QHBoxLayout *lay, const QString &text, int widthOrStretch, Qt::Alignment align)
  {
    QLabel *lbl = new QLabel(text);
    lbl->setAlignment(align);
    lbl->setStyleSheet("color:#202226;font-size:13px;font-weight:800;background:transparent;");
    if (widthOrStretch == 1) {
      lay->addWidget(lbl, 1);
    } else {
      lbl->setFixedWidth(widthOrStretch);
      lay->addWidget(lbl);
    }
  }

  QWidget *createSummary(QWidget *parent)
  {
    QWidget *summary = new QWidget(parent);
    summary->setFixedWidth(196);
    summary->setStyleSheet("background:transparent;");
    auto *lay = new QVBoxLayout(summary);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(5);

    m_subtotalLabel = new QLabel(summary);
    m_deliveryLabel = new QLabel(summary);
    m_taxLabel = new QLabel(summary);
    m_grandTotalLabel = new QLabel(summary);

    lay->addWidget(m_subtotalLabel);
    lay->addWidget(m_deliveryLabel);
    lay->addWidget(m_taxLabel);

    QFrame *divider = new QFrame(summary);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet("background:#DCD4CC;color:#DCD4CC;max-height:1px;");
    lay->addWidget(divider);
    lay->addWidget(m_grandTotalLabel);
    return summary;
  }

  void setSummaryRow(QLabel *label, const QString &left, const QString &right, bool important = false)
  {
    if (important) {
      label->setText(QString("<div style='font-size:17px;font-weight:900;color:#111315;'>%1<br><span style='font-size:21px;'>%2</span></div>").arg(left, right));
    } else {
      label->setText(QString("<table width='100%' cellspacing='0' cellpadding='0'><tr>"
                 "<td style='font-size:13px;color:#202226;'>%1</td>"
                 "<td align='right' style='font-size:13px;color:#202226;font-weight:700;'>%2</td>"
                 "</tr></table>").arg(left, right));
    }
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setStyleSheet("background:transparent;");
  }

  void clearRows()
  {
    while (QLayoutItem *item = m_rowsLayout->takeAt(0)) {
      if (QWidget *w = item->widget())
        w->deleteLater();
      delete item;
    }
  }

  void reload()
  {
    clearRows();

    const QVector<CartItem> &items = m_cart.items();
    if (items.isEmpty()) {
      m_rowsLayout->addWidget(createEmptyState(m_scroll));
    } else {
      for (const CartItem &ci : items)
        m_rowsLayout->addWidget(createRow(ci, m_scroll));
    }
    m_rowsLayout->addStretch();

    setSummaryRow(m_subtotalLabel, "Subtotal:", CartUi::money(subtotal()));
    setSummaryRow(m_deliveryLabel, "Доставка:", CartUi::money(delivery()));
    setSummaryRow(m_taxLabel, "Налог (НДС 20%):", CartUi::money(tax()));
    setSummaryRow(m_grandTotalLabel, "Итого к оплате:", CartUi::money2(grandTotal()), true);

    const bool hasItems = !items.isEmpty();
    m_clearBtn->setEnabled(hasItems);
    m_orderBtn->setEnabled(hasItems);
  }

  QWidget *createEmptyState(QWidget *parent)
  {
    QWidget *empty = new QWidget(parent);
    empty->setMinimumHeight(230);
    empty->setStyleSheet("background:#FFFFFF;");
    auto *lay = new QVBoxLayout(empty);
    lay->setAlignment(Qt::AlignCenter);
    lay->setSpacing(8);

    QLabel *icon = new QLabel("", empty);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet("color:#C4B8AE;font-size:46px;background:transparent;");
    lay->addWidget(icon);

    QLabel *title = new QLabel("Корзина пуста", empty);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("color:#202226;font-size:17px;font-weight:800;background:transparent;");
    lay->addWidget(title);

    QLabel *hint = new QLabel("Добавьте товары из каталога, и они появятся здесь с фото и характеристиками.", empty);
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#77716B;font-size:13px;background:transparent;");
    lay->addWidget(hint);
    return empty;
  }

  QWidget *createRow(const CartItem &ci, QWidget *parent)
  {
    const Product &p = ci.product;
    const int productId = p.id;

    QFrame *row = new QFrame(parent);
    row->setObjectName("CartRow");
    row->setFixedHeight(82);
    auto *lay = new QHBoxLayout(row);
    lay->setContentsMargins(18, 8, 18, 8);
    lay->setSpacing(10);

    QLabel *image = new QLabel(row);
    image->setFixedSize(58, 58);
    image->setAlignment(Qt::AlignCenter);
    image->setStyleSheet("background:#F2F0EE;border:1px solid #E7DFD7;border-radius:9px;color:#9B938C;font-size:11px;");

    QPixmap px(resolveImagePath(p.imagePath));
    if (!px.isNull()) {
      image->setPixmap(CartUi::roundedPixmap(px, QSize(58, 58), 9));
    } else {
      image->setText("нет\nфото");
    }
    lay->addWidget(image);

    QWidget *info = new QWidget(row);
    info->setStyleSheet("background:transparent;");
    auto *infoLay = new QVBoxLayout(info);
    infoLay->setContentsMargins(0, 0, 0, 0);
    infoLay->setSpacing(3);

    QLabel *name = new QLabel(CartUi::safeText(p.name, "Товар"), info);
    name->setStyleSheet("color:#15171A;font-size:14px;font-weight:900;background:transparent;");
    name->setWordWrap(false);
    infoLay->addWidget(name);

    QLabel *details1 = new QLabel(QString("Цвет: %1 • Размер: %2 • Категория: %3")
                   .arg(CartUi::safeText(p.color), CartUi::safeText(p.size), CartUi::safeText(p.category)), info);
    details1->setStyleSheet("color:#5E5A56;font-size:12px;background:transparent;");
    infoLay->addWidget(details1);

    QLabel *details2 = new QLabel(QString("Сезон: %1 • Материал: %2 • Стиль: %3")
                   .arg(CartUi::safeText(p.season), CartUi::safeText(p.material), CartUi::safeText(p.styleTag)), info);
    details2->setStyleSheet("color:#5E5A56;font-size:12px;background:transparent;");
    infoLay->addWidget(details2);
    lay->addWidget(info, 1);

    QLabel *price = new QLabel(CartUi::money(p.price), row);
    price->setAlignment(Qt::AlignCenter);
    price->setFixedWidth(90);
    price->setStyleSheet("color:#202226;font-size:14px;font-weight:700;background:transparent;");
    lay->addWidget(price);

    QWidget *qty = createQtyControl(productId, ci.quantity, row);
    qty->setFixedWidth(118);
    lay->addWidget(qty);

    QLabel *sum = new QLabel(CartUi::money(p.price * ci.quantity), row);
    sum->setAlignment(Qt::AlignCenter);
    sum->setFixedWidth(100);
    sum->setStyleSheet("color:#202226;font-size:14px;font-weight:800;background:transparent;");
    lay->addWidget(sum);

    QPushButton *del = new QPushButton("", row);
    del->setObjectName("DeleteBtn");
    del->setCursor(Qt::PointingHandCursor);
    del->setFixedSize(82, 34);
    lay->addWidget(del);
    connect(del, &QPushButton::clicked, this, [this, productId]() {
      m_cart.removeProduct(productId);
      reload();
    });

    return row;
  }

  QWidget *createQtyControl(int productId, int quantity, QWidget *parent)
  {
    QWidget *host = new QWidget(parent);
    host->setStyleSheet("background:transparent;");
    auto *lay = new QHBoxLayout(host);
    lay->setContentsMargins(3, 0, 3, 0);
    lay->setSpacing(0);
    lay->setAlignment(Qt::AlignCenter);

    QPushButton *minus = new QPushButton("−", host);
    QLabel *value = new QLabel(QString::number(quantity), host);
    QPushButton *plus = new QPushButton("+", host);

    for (QPushButton *btn : {minus, plus}) {
      btn->setObjectName("QtyBtn");
      btn->setCursor(Qt::PointingHandCursor);
      btn->setFixedSize(34, 34);
    }

    value->setFixedSize(42, 34);
    value->setAlignment(Qt::AlignCenter);
    value->setStyleSheet("background:#FFFFFF;color:#15171A;border-top:1px solid #D8D1CA;border-bottom:1px solid #D8D1CA;font-size:14px;font-weight:800;");

    lay->addWidget(minus);
    lay->addWidget(value);
    lay->addWidget(plus);

    connect(minus, &QPushButton::clicked, this, [this, productId]() {
      m_cart.changeQuantity(productId, -1);
      reload();
    });
    connect(plus, &QPushButton::clicked, this, [this, productId]() {
      m_cart.changeQuantity(productId, +1);
      reload();
    });
    return host;
  }

  void onClear()
  {
    if (m_cart.items().isEmpty())
      return;

    CartClearConfirmDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
      m_cart.clear();
      reload();
    }
  }

  void onCreateOrder()
  {
    QVector<CartItem> items = m_cart.items();
    if (items.isEmpty()) {
      CartNoticeDialog dlg("Оформление заказа", "Корзина пуста. Сначала добавьте товары из каталога.", "Понятно", QString(), this);
      dlg.exec();
      return;
    }

    const double sub = subtotal();
    const double deliv = delivery();
    const double taxValue = tax();
    const double total = grandTotal();

    CartCheckoutDialog checkout(m_username, sub, deliv, taxValue, total, this);
    if (checkout.exec() != QDialog::Accepted)
      return;

    const QString selectedPaymentMethod = checkout.paymentMethod();
    const QString orderStatus = selectedPaymentMethod == "Оплата при получении" ? "оформлен" : "оплачен";

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
      if (!openDatabase()) {
        CartNoticeDialog dlg("Оформление заказа", "База данных недоступна. Заказ не сохранён.", "Понятно", QString(), this);
        dlg.exec();
        return;
      }
      db = QSqlDatabase::database();
    }

    CartPaymentProcessingDialog processing(this);
    if (processing.exec() != QDialog::Accepted)
      return;

    QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    QSqlQuery q(db);
    if (!q.exec("BEGIN TRANSACTION;")) {
      CartNoticeDialog dlg("Оформление заказа", "Не удалось начать транзакцию сохранения заказа.", "Понятно", QString(), this);
      dlg.exec();
      return;
    }

    bool ok = true;

    int itemsCount = 0;
    for (const CartItem &ci : items)
      itemsCount += ci.quantity;

    q.prepare("INSERT INTO orders(username, created_at, total, status, payment_method, items_count, address) "
        "VALUES(:u, :d, :t, :s, :pm, :cnt, :addr);");
    q.bindValue(":u", m_username);
    q.bindValue(":d", now);
    q.bindValue(":t", total);
    q.bindValue(":s", orderStatus);
    q.bindValue(":pm", selectedPaymentMethod);
    q.bindValue(":cnt", itemsCount);
    q.bindValue(":addr", checkout.address());

    if (!q.exec()) {
      qDebug() << "Insert order error:" << q.lastError().text();
      ok = false;
    }

    int orderId = 0;
    if (ok) {
      if (!q.exec("SELECT last_insert_rowid();")) {
        qDebug() << "last_insert_rowid error:" << q.lastError().text();
        ok = false;
      } else if (q.next()) {
        orderId = q.value(0).toInt();
      } else {
        ok = false;
      }
    }

    if (ok) {
      QSqlQuery qi(db);
      qi.prepare("INSERT INTO order_items(order_id, product_id, quantity, price) "
           "VALUES(:o, :p, :q, :r);");

      for (const CartItem &ci : items) {
        qi.bindValue(":o", orderId);
        qi.bindValue(":p", ci.product.id);
        qi.bindValue(":q", ci.quantity);
        qi.bindValue(":r", ci.product.price);

        if (!qi.exec()) {
          qDebug() << "Insert order_item error:" << qi.lastError().text();
          ok = false;
          break;
        }
        qi.finish();
      }
    }

    if (ok) {
      q.exec("COMMIT;");
      m_cart.clear();
      reload();

      CartNoticeDialog dlg("Оформление заказа", "Оплата успешно прошла. Заказ сохранён в разделе «Мои заказы».", "Готово", QString(), this);
      dlg.exec();
      accept();
    } else {
      q.exec("ROLLBACK;");
      CartNoticeDialog dlg("Оформление заказа", "Ошибка при сохранении заказа. Попробуйте ещё раз.", "Понятно", QString(), this);
      dlg.exec();
    }
  }
};
