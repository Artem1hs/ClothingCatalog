#pragma once

/*
  File: src/widgets/CustomTitleBar.h
  Purpose: Custom frameless-window title bar with minimize, maximize, and close buttons.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include <QWidget>
#include <QPoint>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>

class CustomTitleBar : public QWidget
{
  Q_OBJECT

public:
  explicit CustomTitleBar(QWidget *parent = nullptr)
    : CustomTitleBar(QStringLiteral("ClothingCatalog"), parent)
  {
  }

  explicit CustomTitleBar(const QString& title, QWidget *parent = nullptr)
    : QWidget(parent)
  {
    setupUi(title);
  }

signals:
  void minimizeRequested();
  void maximizeRestoreRequested();
  void closeRequested();

protected:
  void mousePressEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton) {
      QWidget *w = window();
      if (w && !w->isMaximized()) {
        m_dragging = true;
        m_dragPosition = mouseGlobalPos(event) - w->frameGeometry().topLeft();
        event->accept();
        return;
      }
    }

    QWidget::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override
  {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
      QWidget *w = window();
      if (w && !w->isMaximized()) {
        w->move(mouseGlobalPos(event) - m_dragPosition);
        event->accept();
        return;
      }
    }

    QWidget::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent *event) override
  {
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
  }

  void mouseDoubleClickEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton) {
      updateMaxButtonText();
      emit maximizeRestoreRequested();
      event->accept();
      return;
    }

    QWidget::mouseDoubleClickEvent(event);
  }

private:
  void setupUi(const QString& title)
  {
    setObjectName(QStringLiteral("CustomTitleBar"));
    setFixedHeight(48);
    setAttribute(Qt::WA_StyledBackground, true);

    // The title bar background matches the application background.
    setStyleSheet(
     "QWidget#CustomTitleBar {"
     " background: #F4EFE9;"
     " border-bottom: 1px solid #E4DAD1;"
     "}"
    );

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 6, 14, 6);
    layout->setSpacing(8);

    m_titleLabel = new QLabel(title.isEmpty() ? QStringLiteral("ClothingCatalog") : title, this);
    m_titleLabel->setObjectName(QStringLiteral("WindowTitleText"));
    m_titleLabel->setStyleSheet(
     "QLabel#WindowTitleText {"
     " color: #1E2A36;"
     " font-size: 13px;"
     " font-weight: 800;"
     " letter-spacing: 0.4px;"
     " padding-left: 4px;"
     "}"
    );

    layout->addWidget(m_titleLabel);
    layout->addStretch();

    m_minButton = new QPushButton(QStringLiteral("−"), this);
    m_maxButton = new QPushButton(QStringLiteral("□"), this);
    m_closeButton = new QPushButton(QStringLiteral("×"), this);

    const QString btnStyle =
     "QPushButton {"
     " background-color: rgba(255, 255, 255, 0.60);"
     " color: #4A3F35;"
     " border: none;"
     " border-radius: 8px;"
     " font-weight: 800;"
     " padding: 6px 14px;"
     " margin: 0px;"
     "}"
     "QPushButton:hover {"
     " background-color: #EDE8E0;"
     " color: #A39788;"
     " border: none;"
     "}"
     "QPushButton:pressed {"
     " background-color: #E3DCD0;"
     " color: #4A3F35;"
     " border: none;"
     "}";
    QList<QPushButton*> buttons = { m_minButton, m_maxButton, m_closeButton };
    for (QPushButton *btn : buttons) {
      btn->setFixedSize(52, 32);
      btn->setCursor(Qt::PointingHandCursor);
      btn->setFocusPolicy(Qt::NoFocus);
      btn->setStyleSheet(btnStyle);
    }

    layout->addWidget(m_minButton);
    layout->addWidget(m_maxButton);
    layout->addWidget(m_closeButton);

    connect(m_minButton, &QPushButton::clicked, this, [this]() {
      emit minimizeRequested();
    });

    connect(m_maxButton, &QPushButton::clicked, this, [this]() {
      updateMaxButtonText();
      emit maximizeRestoreRequested();
    });

    connect(m_closeButton, &QPushButton::clicked, this, [this]() {
      emit closeRequested();
    });
  }

  void updateMaxButtonText()
  {
    QWidget *w = window();
    if (!w || !m_maxButton)
      return;

    // Check the window state before toggling maximize/restore mode.
    m_maxButton->setText(w->isMaximized() ? QStringLiteral("□") : QStringLiteral(""));
  }

  QPoint mouseGlobalPos(QMouseEvent *event) const
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
  }

private:
  QLabel *m_titleLabel = nullptr;
  QPushButton *m_minButton = nullptr;
  QPushButton *m_maxButton = nullptr;
  QPushButton *m_closeButton = nullptr;

  bool m_dragging = false;
  QPoint m_dragPosition;
};
