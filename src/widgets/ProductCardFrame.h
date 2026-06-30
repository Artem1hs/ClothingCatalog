#pragma once

/*
  File: src/widgets/ProductCardFrame.h
  Purpose: Product-card frame with hover shadow animation.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "../models/DataModels.h"

class ProductCardFrame : public QFrame
{
public:
  explicit ProductCardFrame(QWidget *parent = nullptr)
    : QFrame(parent)
  {
    setObjectName("ProductCard");
    setFrameShape(QFrame::StyledPanel);
    setMinimumSize(220, 220);

    m_shadow = new QGraphicsDropShadowEffect(this);
    m_shadow->setColor(QColor(0, 0, 0, 160));
    m_shadow->setBlurRadius(0);
    m_shadow->setOffset(0, 0);
    setGraphicsEffect(m_shadow);
  }

protected:
  void enterEvent(QEnterEvent *event) override
  {
    QFrame::enterEvent(event);

    animateShadow(m_shadow->blurRadius(), 22.0);
    animateOffset(m_shadow->offset(), QPointF(0, 6));
  }

  void leaveEvent(QEvent *event) override
  {
    QFrame::leaveEvent(event);

    animateShadow(m_shadow->blurRadius(), 0.0);
    animateOffset(m_shadow->offset(), QPointF(0, 0));
  }

private:
  QGraphicsDropShadowEffect *m_shadow = nullptr;

  void animateShadow(qreal from, qreal to)
  {
    auto *anim = new QPropertyAnimation(m_shadow, "blurRadius", this);
    anim->setDuration(140);
    anim->setStartValue(from);
    anim->setEndValue(to);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
  }

  void animateOffset(QPointF from, QPointF to)
  {
    auto *anim = new QPropertyAnimation(m_shadow, "offset", this);
    anim->setDuration(140);
    anim->setStartValue(from);
    anim->setEndValue(to);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
  }
};
