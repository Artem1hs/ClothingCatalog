#pragma once

/*
  File: src/style/AppTheme.h
  Purpose: Application palette and shared QSS style helpers.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "../app/AppCommon.h"

void setupLightTheme(QApplication &app)
{
  app.setStyle("Fusion");

  QPalette pal;
  pal.setColor(QPalette::Window,     QColor("#F5F1EC")); // application background
  pal.setColor(QPalette::Base,      QColor("#FFFFFF")); // cards and input fields
  pal.setColor(QPalette::AlternateBase,  QColor("#F8F3EE"));
  pal.setColor(QPalette::WindowText,   QColor("#202226")); // primary text color
  pal.setColor(QPalette::Text,      QColor("#202226"));
  pal.setColor(QPalette::Button,     QColor("#F5F1EC"));
  pal.setColor(QPalette::ButtonText,   QColor("#202226"));
  pal.setColor(QPalette::Highlight,    QColor("#E0A58F")); // interface accent color
  pal.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
  pal.setColor(QPalette::ToolTipBase,   QColor("#FFFFFF"));
  pal.setColor(QPalette::ToolTipText,   QColor("#202226"));

  app.setPalette(pal);

  app.setStyleSheet(R"QSS(
    QWidget#AppRoot, QWidget#MainContent {
      background: #F4EFE9;
      color: #1E2A36;
      font-family: "Segoe UI", Arial, sans-serif;
    }

    QWidget#CustomTitleBar {
      background: #F4EFE9;
      border-bottom: 1px solid #E4DAD1;
    }
    QLabel#WindowTitleText {
      color: #1E2A36;
      font-size: 13px;
      font-weight: 800;
      letter-spacing: 0.4px;
      padding-left: 4px;
    }
    QPushButton#TitleBarButton,
    QPushButton#TitleBarCloseButton {
      background: rgba(255, 255, 255, 0.54);
      color: #1E2A36;
      border: 1px solid rgba(228, 218, 209, 0.92);
      border-radius: 12px;
      font-size: 15px;
      font-weight: 800;
      padding: 0px;
      margin: 0px;
    }
    QPushButton#TitleBarButton:hover {
      background: #F8E8DF;
      border: 1px solid #DFA08C;
      color: #BF7864;
    }
    QPushButton#TitleBarButton:pressed {
      background: #EED6CC;
      border: 1px solid #D18D77;
    }
    QPushButton#TitleBarCloseButton:hover {
      background: #DFA08C;
      border: 1px solid #DFA08C;
      color: white;
    }
    QPushButton#TitleBarCloseButton:pressed {
      background: #BF7864;
      border: 1px solid #BF7864;
      color: white;
    }

    QScrollArea#HomeScroll,
    QScrollArea#HomeScroll > QWidget,
    QScrollArea#HomeScroll > QWidget > QWidget {
      background: transparent;
      border: none;
    }
    QFrame#HomeSection {
      background: transparent;
      border: none;
    }

    QFrame#HeroBanner, QFrame#ProductCard, QFrame#HomeInfoSection, QFrame#InfoMiniCard {
      background: #FFFFFF;
      border: 1px solid #E4DAD1;
      border-radius: 20px;
    }
    QFrame#HeroBanner {
      background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                    stop:0 #FFFFFF, stop:1 #F8E8DF);
    }
    QFrame#ProductCard:hover {
      border: 1px solid #DFA08C;
      background: #FFFCFA;
    }
    QLabel#ProductTitle {
      color: #1E2A36;
      font-size: 14px;
      font-weight: 700;
    }
    QLabel#ProductMeta {
      color: #7A8491;
      font-size: 12px;
    }
    QLabel#ProductPrice {
      color: #1E2A36;
      font-size: 18px;
      font-weight: 800;
    }
    QLabel#SectionTitle {
      color: #1E2A36;
      font-size: 17px;
      font-weight: 800;
    }
    QLabel#MutedText {
      color: #7A8491;
      font-size: 12px;
      line-height: 145%;
    }
    QLabel#InfoNumber {
      color: #DFA08C;
      font-size: 22px;
      font-weight: 900;
    }
    QLabel#InfoCardTitle {
      color: #1E2A36;
      font-size: 14px;
      font-weight: 800;
    }
    QFrame#HomeInfoSection {
      background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                    stop:0 #FFFFFF, stop:1 #FFF4EF);
    }
    QFrame#InfoMiniCard {
      background: #FFFFFF;
      border-radius: 16px;
    }
    QLabel#ProductImage {
      background: #F5F1EC;
      border-radius: 14px;
    }

    QPushButton {
      background: #DFA08C;
      color: #FFFFFF;
      border: none;
      border-radius: 12px;
      padding: 9px 14px;
      font-weight: 700;
    }
    QPushButton:hover { background: #D18D77; }
    QPushButton:pressed { background: #BF7864; }
    QPushButton[secondary="true"] {
      background: #FFFFFF;
      color: #1E2A36;
      border: 1px solid #E4DAD1;
    }
    QPushButton[secondary="true"]:hover {
      background: #F8F3EE;
      border-color: #DFA08C;
    }
    QPushButton#PrimaryButton {
      background: #DFA08C;
      color: white;
      border-radius: 12px;
      padding: 9px 14px;
    }
    QPushButton#PrimaryButton:hover { background: #D18D77; }

    QScrollArea#RecommendationsScroll {
      background: transparent;
      border: none;
    }
    QScrollArea#RecommendationsScroll > QWidget > QWidget {
      background: transparent;
    }
    QScrollBar:horizontal {
      background: transparent;
      height: 10px;
      margin: 2px 8px 2px 8px;
    }
    QScrollBar::handle:horizontal {
      background: #DFA08C;
      border-radius: 5px;
      min-width: 42px;
    }
    QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
      width: 0px;
    }
    QScrollBar:vertical {
      background: transparent;
      width: 10px;
      margin: 8px 2px 8px 2px;
    }
    QScrollBar::handle:vertical {
      background: #DFA08C;
      border-radius: 5px;
      min-height: 42px;
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
      height: 0px;
    }

    QFrame#BottomNav {
      background: #FFFFFF;
      border-top: 1px solid #E4DAD1;
    }
    QToolButton[bottomNav="true"], QToolButton[bottomNavExit="true"] {
      background: transparent;
      border: none;
      border-radius: 12px;
      padding: 6px 10px;
      color: #7A8491;
      font-size: 11px;
      font-weight: 700;
    }
    QToolButton[bottomNav="true"]:hover, QToolButton[bottomNav="true"]:checked {
      background: #F8E8DF;
      color: #1E2A36;
    }
    QToolButton[bottomNavExit="true"]:hover {
      background: #FBE7E7;
      color: #B94E4E;
    }
  )QSS");

}


// ================== Database helpers ==================
