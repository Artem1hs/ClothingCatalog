#pragma once

/*
  File: src/widgets/RoundCloseButton.h
  Purpose: Custom round close button painted manually with QPainter.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include <QColor>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QtGlobal>

class RoundCloseButton : public QPushButton
{
public:
  explicit RoundCloseButton(QWidget *parent = nullptr)
    : QPushButton(parent)
  {
    setText(QString());
    setCursor(Qt::PointingHandCursor);
    setToolTip("Закрыть");
    setAccessibleName("Закрыть");
    setMinimumSize(24, 24);
    setFocusPolicy(Qt::NoFocus);
  }

protected:
  void paintEvent(QPaintEvent *event) override
  {
    QPushButton::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor color("#4F4036");
    if (isDown()) {
      color = QColor("#2F2723");
    } else if (underMouse()) {
      color = QColor("#3B312C");
    }

    const qreal side = qMin(width(), height());
    const qreal margin = qMax<qreal>(7.0, side * 0.34);
    QPen pen(color, qMax<qreal>(2.0, side * 0.075), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.drawLine(QPointF(margin, margin), QPointF(width() - margin, height() - margin));
    painter.drawLine(QPointF(width() - margin, margin), QPointF(margin, height() - margin));
  }
};
