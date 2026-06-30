#pragma once

/*
  File: src/dialogs/ProductDetailsDialog.h
  Purpose: Product details window with image preview and add-to-cart controls.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "../widgets/RoundCloseButton.h"
#include "../cart/CartManager.h"
#include "../data/DatabaseService.h"
#include <QButtonGroup>
#include <QColor>
#include <QDialog>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

class ProductDetailsDialog : public QDialog
{
public:
  ProductDetailsDialog(const Product &p, CartManager &cart, QWidget *parent = nullptr)
    : QDialog(parent), m_product(p), m_cart(cart)
  {
    setWindowTitle("Информация о товаре");
    setModal(true);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, true);
    resize(780, 610);
    setMinimumSize(740, 580);

    QStringList imgs;
    if (!p.imagePath.isEmpty()) imgs << p.imagePath;
    if (!p.imagePath2.isEmpty()) imgs << p.imagePath2;
    if (!p.imagePath3.isEmpty()) imgs << p.imagePath3;

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(0);

    QFrame *shell = new QFrame(this);
    shell->setObjectName("ProductDetailsShell");
    shell->setStyleSheet(
     "QFrame#ProductDetailsShell{background:#F8F5F1;border:1px solid #E5DBD2;border-radius:26px;}"
    );
    auto *shadow = new QGraphicsDropShadowEffect(shell);
    shadow->setBlurRadius(34);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(60, 44, 30, 45));
    shell->setGraphicsEffect(shadow);
    outer->addWidget(shell);

    QVBoxLayout *shellLay = new QVBoxLayout(shell);
    shellLay->setContentsMargins(14, 14, 14, 14);
    shellLay->setSpacing(10);

    QWidget *titleBar = new QWidget(shell);
    titleBar->setObjectName("ProductDetailsTitleBar");
    titleBar->setStyleSheet("QWidget#ProductDetailsTitleBar{background:transparent;border:none;}");
    QHBoxLayout *titleLay = new QHBoxLayout(titleBar);
    titleLay->setContentsMargins(4, 0, 4, 0);
    titleLay->setSpacing(8);

    QLabel *titleText = new QLabel("Информация о товаре", titleBar);
    titleText->setStyleSheet("color:#25232A;font-size:18px;font-weight:800;");
    titleLay->addWidget(titleText);
    titleLay->addStretch();

    QPushButton *closeBtn = new RoundCloseButton(titleBar);
    closeBtn->setFixedSize(38, 38);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
     "QPushButton{background:#FFFFFF;border:1px solid #E2D8CF;border-radius:19px;color:#4F4036;font-size:22px;font-weight:700;}"
     "QPushButton:hover{background:#F1E7DE;}"
     "QPushButton:pressed{background:#E7D9CC;}"
    );
    titleLay->addWidget(closeBtn);
    shellLay->addWidget(titleBar);

    QWidget *content = new QWidget(shell);
    content->setObjectName("ProductDetailsContent");
    content->setStyleSheet("QWidget#ProductDetailsContent{background:transparent;}");
    QHBoxLayout *contentLay = new QHBoxLayout(content);
    contentLay->setContentsMargins(6, 0, 6, 0);
    contentLay->setSpacing(18);
    shellLay->addWidget(content, 1);

    QWidget *leftCol = new QWidget(content);
    QVBoxLayout *leftLay = new QVBoxLayout(leftCol);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->setSpacing(14);

    m_imageStack = new QStackedWidget(leftCol);
    m_imageStack->setObjectName("ProductImageStack");
    m_imageStack->setMinimumSize(340, 390);
    m_imageStack->setStyleSheet(
     "QStackedWidget#ProductImageStack{background:#FFFFFF;border:1px solid #E6DDD5;border-radius:22px;}"
    );

    if (imgs.isEmpty()) {
      QLabel *lab = createImageLabel(QString());
      lab->setText("[нет фото]");
      m_imageStack->addWidget(lab);
    } else {
      for (const QString &rel : imgs)
        m_imageStack->addWidget(createImageLabel(rel));
    }
    leftLay->addWidget(m_imageStack, 1);

    QHBoxLayout *navLay = new QHBoxLayout();
    navLay->setContentsMargins(0, 0, 0, 0);
    navLay->setSpacing(10);
    navLay->addStretch();

    m_prevBtn = new QToolButton(leftCol);
    m_nextBtn = new QToolButton(leftCol);
    m_prevBtn->setText("‹");
    m_nextBtn->setText("›");
    const QString navBtnStyle =
     "QToolButton{background:#FFFFFF;border:1px solid #E4DAD2;border-radius:14px;color:#54453B;font-size:22px;font-weight:700;padding:0 2px;}"
     "QToolButton:hover{background:#F1E7DE;}"
     "QToolButton:pressed{background:#E6D8CB;}"
     "QToolButton:disabled{color:#B9ADA2;background:#FBF9F7;}";
    for (QToolButton *btn : {m_prevBtn, m_nextBtn}) {
      btn->setCursor(Qt::PointingHandCursor);
      btn->setFixedSize(34, 34);
      btn->setStyleSheet(navBtnStyle);
    }

    navLay->addWidget(m_prevBtn);
    QWidget *dotsHost = new QWidget(leftCol);
    QHBoxLayout *dotsLay = new QHBoxLayout(dotsHost);
    dotsLay->setContentsMargins(0, 0, 0, 0);
    dotsLay->setSpacing(8);
    for (int i = 0; i < m_imageStack->count(); ++i) {
      QLabel *dot = new QLabel(dotsHost);
      dot->setFixedSize(10, 10);
      dot->setStyleSheet("background:#D6C9BE;border-radius:5px;");
      m_dots.push_back(dot);
      dotsLay->addWidget(dot);
    }
    navLay->addWidget(dotsHost);
    navLay->addWidget(m_nextBtn);
    navLay->addStretch();
    leftLay->addLayout(navLay);

    QPushButton *addBtn = new QPushButton("Добавить в корзину", leftCol);
    addBtn->setMinimumHeight(50);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(
     "QPushButton{background:#B46A42;color:white;border:none;border-radius:18px;padding:14px 24px;font-size:16px;font-weight:800;text-align:left;}"
     "QPushButton:hover{background:#A95E37;}"
     "QPushButton:pressed{background:#9C5631;}"
    );
    leftLay->addWidget(addBtn);
    contentLay->addWidget(leftCol, 10);

    QWidget *rightCol = new QWidget(content);
    QVBoxLayout *rightLay = new QVBoxLayout(rightCol);
    rightLay->setContentsMargins(0, 10, 0, 0);
    rightLay->setSpacing(14);
    contentLay->addWidget(rightCol, 9);

    QLabel *nameLabel = new QLabel(p.name.toUpper(), rightCol);
    nameLabel->setWordWrap(true);
    nameLabel->setStyleSheet("color:#23222A;font-size:22px;font-weight:900;line-height:1.15em;");
    rightLay->addWidget(nameLabel);

    QLabel *skuLabel = new QLabel(QString("Артикул SKU: %1").arg(makeSku(p)), rightCol);
    skuLabel->setStyleSheet("color:#49454A;font-size:14px;");
    rightLay->addWidget(skuLabel);

    QLabel *stockLabel = new QLabel("В наличии", rightCol);
    stockLabel->setStyleSheet("color:#2E7D32;font-size:15px;font-weight:700;");
    rightLay->addWidget(stockLabel);

    QWidget *specsCard = new QWidget(rightCol);
    specsCard->setStyleSheet(
     "QWidget{background:transparent;border:none;}"
     "QLabel{background:transparent;border:none;}"
    );
    QVBoxLayout *specsLay = new QVBoxLayout(specsCard);
    specsLay->setContentsMargins(0, 2, 0, 2);
    specsLay->setSpacing(11);
    specsLay->addWidget(makeInfoRow("material", "Материал", safeText(p.material), specsCard));
    specsLay->addWidget(makeInfoRow("season", "Сезон", safeText(p.season), specsCard));
    specsLay->addWidget(makeInfoRow("style", "Стиль", safeText(p.styleTag), specsCard));
    specsLay->addWidget(makeInfoRow("category", "Категория", safeText(p.category), specsCard));
    specsLay->addWidget(makeInfoRow("color", "Цвет", safeText(p.color), specsCard));
    rightLay->addWidget(specsCard);

    QLabel *sizeTitle = new QLabel("Выберите размер:", rightCol);
    sizeTitle->setStyleSheet("color:#25222A;font-size:16px;font-weight:800;");
    rightLay->addWidget(sizeTitle);

    QWidget *sizeHost = new QWidget(rightCol);
    QHBoxLayout *sizeLay = new QHBoxLayout(sizeHost);
    sizeLay->setContentsMargins(0, 0, 0, 0);
    sizeLay->setSpacing(12);
    m_sizeGroup = new QButtonGroup(this);
    m_sizeGroup->setExclusive(true);

    QStringList sizes = parseSizes(p.size);
    if (sizes.isEmpty())
      sizes << safeText(p.size);
    if (sizes.size() == 1 && sizes.first() == "—")
      sizes = QStringList{ "UNI" };

    for (int i = 0; i < sizes.size(); ++i) {
      QPushButton *sizeBtn = new QPushButton(sizes[i], sizeHost);
      sizeBtn->setCheckable(true);
      sizeBtn->setCursor(Qt::PointingHandCursor);
      const int sizeBtnWidth = qMax(60, qMin(112, 26 + static_cast<int>(sizes[i].length()) * 10));
      sizeBtn->setFixedSize(sizeBtnWidth, 46);
      sizeBtn->setStyleSheet(
       "QPushButton{background:#FFFFFF;border:1px solid #DECFC3;border-radius:16px;color:#2A2521;font-size:15px;font-weight:700;}"
       "QPushButton:hover{background:#F6EEE8;}"
       "QPushButton:checked{background:#FFFFFF;border:2px solid #6D4B4B;color:#2A2521;}"
      );
      if (i == 0) sizeBtn->setChecked(true);
      m_sizeGroup->addButton(sizeBtn);
      sizeLay->addWidget(sizeBtn);
    }
    sizeLay->addStretch();
    rightLay->addWidget(sizeHost);

    QWidget *priceCard = new QWidget(rightCol);
    priceCard->setStyleSheet(
     "QWidget{background:#FFFFFF;border:1px solid #E8DDD4;border-radius:22px;}"
     "QLabel{background:transparent;border:none;}"
    );
    QGridLayout *priceLay = new QGridLayout(priceCard);
    priceLay->setContentsMargins(20, 18, 20, 18);
    priceLay->setHorizontalSpacing(10);
    priceLay->setVerticalSpacing(8);

    QLabel *oldPrice = new QLabel(QString("<span style='color:#5B4D47;font-size:15px;text-decoration:line-through;'>%1 ₽</span>")
                    .arg(QString::number(previousPrice(p.price), 'f', 2)), priceCard);
    oldPrice->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    priceLay->addWidget(oldPrice, 0, 0, 1, 1);

    QLabel *badge = new QLabel("-20%", priceCard);
    badge->setAlignment(Qt::AlignCenter);
    badge->setFixedSize(64, 38);
    badge->setStyleSheet("background:#C7764F;color:white;border-radius:12px;font-size:14px;font-weight:800;");
    priceLay->addWidget(badge, 0, 1, 1, 1, Qt::AlignRight);

    QLabel *newPrice = new QLabel(QString("%1 ₽").arg(QString::number(p.price, 'f', 2)), priceCard);
    newPrice->setStyleSheet("color:#4F2F3A;font-size:24px;font-weight:900;");
    priceLay->addWidget(newPrice, 1, 0, 1, 2);
    rightLay->addWidget(priceCard);

    rightLay->addStretch();

    QPushButton *bottomCloseBtn = new QPushButton("Закрыть", rightCol);
    bottomCloseBtn->setMinimumHeight(50);
    bottomCloseBtn->setCursor(Qt::PointingHandCursor);
    bottomCloseBtn->setStyleSheet(
     "QPushButton{background:#5A4250;color:white;border:none;border-radius:18px;padding:14px 24px;font-size:16px;font-weight:800;}"
     "QPushButton:hover{background:#503946;}"
     "QPushButton:pressed{background:#47313D;}"
    );
    rightLay->addWidget(bottomCloseBtn);

    updateImageState();

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(bottomCloseBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(addBtn, &QPushButton::clicked, this, &ProductDetailsDialog::onAddToCart);
    connect(m_prevBtn, &QToolButton::clicked, this, [this]() {
      const int n = m_imageStack->count();
      if (n <= 1) return;
      int i = m_imageStack->currentIndex();
      m_imageStack->setCurrentIndex((i - 1 + n) % n);
      updateImageState();
    });
    connect(m_nextBtn, &QToolButton::clicked, this, [this]() {
      const int n = m_imageStack->count();
      if (n <= 1) return;
      int i = m_imageStack->currentIndex();
      m_imageStack->setCurrentIndex((i + 1) % n);
      updateImageState();
    });
  }

protected:
  void mousePressEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton) {
      m_dragging = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      m_dragStart = event->globalPosition().toPoint() - frameGeometry().topLeft();
#else
      m_dragStart = event->globalPos() - frameGeometry().topLeft();
#endif
      event->accept();
      return;
    }
    QDialog::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override
  {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      move(event->globalPosition().toPoint() - m_dragStart);
#else
      move(event->globalPos() - m_dragStart);
#endif
      event->accept();
      return;
    }
    QDialog::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent *event) override
  {
    m_dragging = false;
    QDialog::mouseReleaseEvent(event);
  }

private:
  Product m_product;
  CartManager &m_cart;
  QStackedWidget *m_imageStack = nullptr;
  QToolButton *m_prevBtn = nullptr;
  QToolButton *m_nextBtn = nullptr;
  QVector<QLabel*> m_dots;
  QButtonGroup *m_sizeGroup = nullptr;
  bool m_dragging = false;
  QPoint m_dragStart;

  QLabel *createImageLabel(const QString &rel) const
  {
    QLabel *lab = new QLabel;
    lab->setAlignment(Qt::AlignCenter);
    lab->setMinimumSize(320, 360);
    lab->setStyleSheet("background:transparent;color:#A39588;font-size:20px;font-weight:600;");

    QPixmap pix(resolveImagePath(rel));
    if (!pix.isNull())
      lab->setPixmap(pix.scaled(310, 340, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    else if (!rel.isEmpty())
      lab->setText("[фото]");
    return lab;
  }

  QWidget *makeInfoRow(const QString &iconKind,
             const QString &label,
             const QString &value,
             QWidget *parent) const
  {
    QWidget *row = new QWidget(parent);
    row->setStyleSheet("background:transparent;border:none;");
    QHBoxLayout *lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    QLabel *iconWidget = new QLabel(row);
    iconWidget->setFixedSize(24, 24);
    iconWidget->setAlignment(Qt::AlignCenter);
    iconWidget->setPixmap(makeInfoIcon(iconKind, value));
    lay->addWidget(iconWidget, 0, Qt::AlignTop);

    QLabel *labelWidget = new QLabel(QString("<b>%1:</b>").arg(label), row);
    labelWidget->setStyleSheet("color:#1D1C22;font-size:14px;background:transparent;border:none;");
    labelWidget->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    lay->addWidget(labelWidget, 0, Qt::AlignVCenter);

    QLabel *valueWidget = new QLabel(value, row);
    valueWidget->setWordWrap(true);
    valueWidget->setStyleSheet("color:#353238;font-size:14px;background:transparent;border:none;");
    valueWidget->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    lay->addWidget(valueWidget, 1, Qt::AlignVCenter);
    return row;
  }

  QPixmap makeInfoIcon(const QString &kind, const QString &value = QString()) const
  {
    QPixmap px(24, 24);
    px.fill(Qt::transparent);

    QPainter painter(&px);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor("#5A433A"));
    pen.setWidthF(1.7);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if (kind == "material") {
      QPainterPath path;
      path.moveTo(12, 5);
      path.cubicTo(9.5, 3.2, 6.2, 4.0, 5.2, 6.7);
      path.cubicTo(2.9, 6.5, 1.9, 8.2, 2.0, 10.1);
      path.cubicTo(2.2, 12.5, 4.3, 13.9, 6.2, 13.9);
      path.cubicTo(6.3, 17.7, 8.8, 20.2, 12, 20.8);
      path.cubicTo(15.2, 20.2, 17.7, 17.7, 17.8, 13.9);
      path.cubicTo(19.7, 13.9, 21.8, 12.5, 22.0, 10.1);
      path.cubicTo(22.1, 8.2, 21.1, 6.5, 18.8, 6.7);
      path.cubicTo(17.8, 4.0, 14.5, 3.2, 12, 5);
      painter.drawPath(path);
      painter.drawLine(QPointF(12, 11), QPointF(12, 18));
    } else if (kind == "season") {
      painter.drawEllipse(QRectF(7.5, 7.5, 9, 9));
      painter.drawLine(QPointF(12, 2.5), QPointF(12, 5));
      painter.drawLine(QPointF(12, 19), QPointF(12, 21.5));
      painter.drawLine(QPointF(2.5, 12), QPointF(5, 12));
      painter.drawLine(QPointF(19, 12), QPointF(21.5, 12));
      painter.drawLine(QPointF(5, 5), QPointF(6.8, 6.8));
      painter.drawLine(QPointF(17.2, 17.2), QPointF(19, 19));
      painter.drawLine(QPointF(17.2, 6.8), QPointF(19, 5));
      painter.drawLine(QPointF(5, 19), QPointF(6.8, 17.2));
    } else if (kind == "style") {
      painter.drawLine(QPointF(4, 18.5), QPointF(10.5, 12.8));
      painter.drawLine(QPointF(20, 18.5), QPointF(13.5, 12.8));
      painter.drawLine(QPointF(10.5, 12.8), QPointF(12, 11.4));
      painter.drawLine(QPointF(13.5, 12.8), QPointF(12, 11.4));
      painter.drawLine(QPointF(10.7, 13.1), QPointF(13.3, 13.1));
      painter.drawArc(QRectF(9.2, 6.2, 5.6, 4.6), 20 * 16, 140 * 16);
    } else if (kind == "category") {
      QPainterPath tee;
      tee.moveTo(8, 6.5);
      tee.lineTo(5, 8.2);
      tee.lineTo(3.3, 12.5);
      tee.lineTo(6.2, 13.5);
      tee.lineTo(6.8, 20.0);
      tee.lineTo(17.2, 20.0);
      tee.lineTo(17.8, 13.5);
      tee.lineTo(20.7, 12.5);
      tee.lineTo(19.0, 8.2);
      tee.lineTo(16.0, 6.5);
      tee.lineTo(14.0, 8.4);
      tee.lineTo(10.0, 8.4);
      tee.closeSubpath();
      painter.drawPath(tee);
    } else if (kind == "color") {
      QColor fill(colorHexForName(value));
      painter.setPen(QPen(QColor("#4C4039"), 1.2));
      painter.setBrush(fill);
      painter.drawEllipse(QRectF(3.5, 3.5, 17, 17));
    }
    return px;
  }

  QString colorHexForName(const QString &raw) const
  {
    const QString s = raw.trimmed().toLower();
    if (s.contains("черн")) return "#0E1218";
    if (s.contains("бел")) return "#F5F5F2";
    if (s.contains("сер")) return "#70757E";
    if (s.contains("крас")) return "#C43A32";
    if (s.contains("син")) return "#2150B5";
    if (s.contains("голуб")) return "#4E9FE9";
    if (s.contains("зелен") || s.contains("зелён")) return "#3F8F4B";
    if (s.contains("желт")) return "#D8A200";
    if (s.contains("оранж")) return "#D96F1A";
    if (s.contains("роз")) return "#D35F97";
    if (s.contains("фиолет")) return "#7C56C7";
    if (s.contains("беж") || s.contains("крем") || s.contains("молоч")) return "#C5A177";
    if (s.contains("корич")) return "#7A4B2C";
    return "#8D8176";
  }

  QString safeText(const QString &s) const
  {
    return s.trimmed().isEmpty() ? QString::fromUtf8("—") : s.trimmed();
  }

  QStringList parseSizes(const QString &raw) const
  {
    QString s = raw.trimmed().simplified();
    if (s.isEmpty())
      return {};
    QStringList out;
    const QStringList parts = s.split(QRegularExpression("\\s*[,;/|]\\s*"), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
      QString t = part.trimmed();
      const bool isOneSize = t.compare("one size", Qt::CaseInsensitive) == 0;
      const QStringList tokens = (!isOneSize && t.contains(' '))
                      ? t.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)
                      : QStringList{t};
      for (QString token : tokens) {
        token = token.trimmed();
        if (token.isEmpty())
          continue;
        const QString display = token.compare("one size", Qt::CaseInsensitive) == 0
                      ? QString("one size")
                      : token.toUpper();
        if (!out.contains(display))
          out << display;
      }
    }
    return out;
  }

  QString makeSku(const Product &p) const
  {
    return QString::number(25011000 + qMax(0, p.id));
  }

  double previousPrice(double currentPrice) const
  {
    if (currentPrice <= 0.0)
      return 0.0;
    return currentPrice / 0.80;
  }

  void updateImageState()
  {
    const int total = m_imageStack ? m_imageStack->count() : 0;
    const int current = m_imageStack ? m_imageStack->currentIndex() : 0;
    if (m_prevBtn) m_prevBtn->setEnabled(total > 1);
    if (m_nextBtn) m_nextBtn->setEnabled(total > 1);
    for (int i = 0; i < m_dots.size(); ++i) {
      if (!m_dots[i]) continue;
      m_dots[i]->setStyleSheet(i == current
                   ? "background:#8B6C63;border-radius:5px;"
                   : "background:#D6C9BE;border-radius:5px;");
    }
  }

  void showAddedToCartDialog()
  {
    QDialog dlg(this);
    dlg.setModal(true);
    dlg.setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg.setAttribute(Qt::WA_TranslucentBackground, true);

    QVBoxLayout *outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(16, 16, 16, 16);

    QFrame *card = new QFrame(&dlg);
    card->setStyleSheet("QFrame{background:#F8F5F1;border:none;border-radius:22px;}");
    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 6);
    shadow->setColor(QColor(0,0,0,35));
    card->setGraphicsEffect(shadow);
    outer->addWidget(card);

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(24, 24, 24, 24);
    lay->setSpacing(14);

    QHBoxLayout *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(8);

    QLabel *title = new QLabel("Товар добавлен в корзину", card);
    title->setStyleSheet("color:#25222A;font-size:18px;font-weight:800;");
    title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->addWidget(title, 1);

    QPushButton *closeBtn = new RoundCloseButton(card);
    closeBtn->setFixedSize(32, 32);
    closeBtn->setStyleSheet(
     "QPushButton{background:#FFFFFF;border:1px solid #E2D8CF;border-radius:16px;}"
     "QPushButton:hover{background:#F1E7DE;}"
     "QPushButton:pressed{background:#E7D9CC;}"
    );
    header->addWidget(closeBtn, 0, Qt::AlignTop);
    lay->addLayout(header);

    QLabel *text = new QLabel(QString("\"%1\" успешно добавлен в корзину.").arg(m_product.name), card);
    text->setWordWrap(true);
    text->setAlignment(Qt::AlignCenter);
    text->setStyleSheet("color:#4E4944;font-size:14px;");
    lay->addWidget(text);

    QPushButton *okBtn = new QPushButton("Понятно", card);
    okBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setMinimumHeight(46);
    okBtn->setStyleSheet(
     "QPushButton{background:#B46A42;color:white;border:none;border-radius:14px;padding:12px 18px;font-size:15px;font-weight:800;}"
     "QPushButton:hover{background:#A95E37;}"
     "QPushButton:pressed{background:#9C5631;}"
    );
    lay->addWidget(okBtn);
    QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    dlg.resize(360, 180);
    dlg.exec();
  }

  void onAddToCart()
  {
    m_cart.addProduct(m_product, 1);
    showAddedToCartDialog();
    accept();
  }
};
