#pragma once

/*
  File: src/windows/CatalogWindow.h
  Purpose: Main application window: navigation, catalog grid, home screen, orders, lookbook, projects, and embedded cart panel.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "../widgets/RoundCloseButton.h"
#include "../api/ApiClient.h"
#include "../cart/CartManager.h"
#include "../dialogs/ProductDetailsDialog.h"
#include "../dialogs/CartDialog.h"
#include "../dialogs/RecommendationsDialog.h"
#include "../dialogs/UserProfileDialog.h"
#include "../dialogs/MyOrdersDialog.h"
#include "../dialogs/QuickRecommendDialog.h"
#include "../dialogs/ProductEditDialog.h"
#include "../dialogs/AdminDialog.h"
#include "../widgets/ProductCardFrame.h"
#include "../widgets/CustomTitleBar.h"
#include "../data/DatabaseService.h"
#include <QDialogButtonBox>
#include <QRadioButton>
#include <QFile>
#include <QTextEdit>
#include <QWheelEvent>
#include <QScrollBar>
#include <QStyle>
#include <QProgressBar>
#include <QTimer>
#include <QFileInfo>
#include <QDateTime>
#include <QPixmapCache>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QEasingCurve>
#include <QAbstractAnimation>
#include <functional>
#include <memory>

// Main desktop window. Most navigation and page switching is centralized here.
class CatalogWindow : public QMainWindow
{
  Q_OBJECT

public:
  CatalogWindow(const QString &username, bool isAdmin, QWidget *parent = nullptr)
    : QMainWindow(parent)
    , m_username(username)
    , m_isAdmin(isAdmin)
  {
    setWindowTitle("Подбор одежды");

    // Custom window shell: the standard Windows frame is disabled,
    // and a custom title bar is added to the root layout.
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);

    resize(1280, 850);
    setMinimumSize(1180, 760);
    QPixmapCache::setCacheLimit(128 * 1024);
    m_products = loadProductsFromDb();

    m_catalogBase = m_products;
    m_smartCatalogActive = false;

    // The toolbar keeps the original actions, but the visible UI uses the bottom navigation bar.
    QToolBar *toolbar = addToolBar("MainToolbar");
    toolbar->setMovable(false);

    QAction *homeAction = toolbar->addAction("Главная");
    QAction *tshirtsAct = toolbar->addAction("Футболки");
    QAction *allAct   = toolbar->addAction("Вся одежда");

    QActionGroup *navGroup = new QActionGroup(this);
    navGroup->setExclusive(true);

    homeAction->setCheckable(true);
    tshirtsAct->setCheckable(true);
    allAct->setCheckable(true);

    navGroup->addAction(homeAction);
    navGroup->addAction(tshirtsAct);
    navGroup->addAction(allAct);

    homeAction->setChecked(true);  // The Home page is active by default.

    toolbar->addSeparator();

    QAction *ordersAct  = toolbar->addAction("Мои заказы");
    QAction *profileAct = toolbar->addAction("Профиль");

    QAction *adminAct  = nullptr;
    if (m_isAdmin) {
      adminAct = toolbar->addAction("Админка");
    }
    toolbar->addSeparator();

    QAction *cartAct  = toolbar->addAction("Корзина");
    QAction *exitAct  = toolbar->addAction("Выход");

    toolbar->addSeparator();

    QLabel *userLabel = new QLabel(
      QString("Пользователь: %1%2")
        .arg(username)
        .arg(m_isAdmin ? " (admin)" : ""),
      this);

    toolbar->addWidget(userLabel);


    connect(homeAction, &QAction::triggered, this, &CatalogWindow::resetFilters);
    connect(tshirtsAct,&QAction::triggered, this,[this](){
      m_categoryBox->setCurrentText("Футболка");
      this->applyFilter();
    });
    connect(allAct,  &QAction::triggered, this,[this](){
      resetFilters();
    });

    connect(ordersAct, &QAction::triggered, this, &CatalogWindow::showMyOrders);
    connect(profileAct,&QAction::triggered, this, &CatalogWindow::editProfile);
    if (adminAct) {
      connect(adminAct, &QAction::triggered, this, &CatalogWindow::openAdmin);
    }
    connect(cartAct,  &QAction::triggered, this, &CatalogWindow::openCart);
    connect(exitAct,  &QAction::triggered, this, &CatalogWindow::logoutRequested);

    // The visible navigation controls are moved to the bottom navigation bar.
    toolbar->hide();

    // ================= Root shell and central content area ================
    QWidget *root = new QWidget(this);
    root->setObjectName("AppRoot");
    QVBoxLayout *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // CustomTitleBar replaces the native window frame for a cleaner UI style.
    CustomTitleBar *titleBar = new CustomTitleBar("ClothingCatalog", root);
    rootLayout->addWidget(titleBar);

    connect(titleBar, &CustomTitleBar::minimizeRequested, this, &CatalogWindow::showMinimized);
    connect(titleBar, &CustomTitleBar::maximizeRestoreRequested, this, [this]() {
      if (isMaximized()) showNormal();
      else showMaximized();
    });
    connect(titleBar, &CustomTitleBar::closeRequested, this, &CatalogWindow::close);

    QWidget *central = new QWidget(root);
    central->setObjectName("MainContent");
    m_contentHost = central;
    central->installEventFilter(this);
    QVBoxLayout *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(28, 18, 28, 18);
    centralLayout->setSpacing(22);
    rootLayout->addWidget(central, 1);


    // =================== Home page ===================
    // Home section contains the landing banner, feature cards, and recommendation entry points.
    m_homeSection = new QFrame(central);
    m_homeSection->setObjectName("HomeSection");
    QVBoxLayout *homeLayout = new QVBoxLayout(m_homeSection);
    homeLayout->setContentsMargins(0, 0, 0, 0);
    homeLayout->setSpacing(16);

    const int HOME_CONTENT_WIDTH = 1110;

    QWidget *topWrap = new QWidget(m_homeSection);
    topWrap->setMaximumWidth(HOME_CONTENT_WIDTH);
    QHBoxLayout *topWrapLay = new QHBoxLayout(topWrap);
    topWrapLay->setContentsMargins(0, 0, 0, 0);
    topWrapLay->setSpacing(16);

    QFrame *hero = new QFrame(topWrap);
    hero->setObjectName("HomeHeroBanner");
    hero->setStyleSheet("QFrame#HomeHeroBanner{background:#FFFFFF;border:1px solid #E6DDD5;border-radius:18px;}");
    hero->setFixedSize(734, 160);
    QHBoxLayout *heroLayout = new QHBoxLayout(hero);
    heroLayout->setContentsMargins(18, 14, 18, 14);
    heroLayout->setSpacing(16);

    QLabel *heroImage = new QLabel(hero);
    heroImage->setFixedSize(220, 130);
    heroImage->setAlignment(Qt::AlignCenter);
    heroImage->setStyleSheet("background:transparent;border:none;");
    heroLayout->addWidget(heroImage, 0, Qt::AlignLeft | Qt::AlignVCenter);

    QVBoxLayout *heroTextLay = new QVBoxLayout();
    heroTextLay->setContentsMargins(0, 4, 0, 0);
    heroTextLay->setSpacing(7);

    QLabel *heroTitle = new QLabel(hero);
    heroTitle->setStyleSheet("color:#24222A;font-size:20px;font-weight:900;");
    heroTextLay->addWidget(heroTitle);

    QLabel *heroSub = new QLabel(hero);
    heroSub->setWordWrap(true);
    heroSub->setStyleSheet("color:#37332F;font-size:12px;line-height:1.35em;");
    heroTextLay->addWidget(heroSub);

    QPushButton *heroBtn = new QPushButton(hero);
    heroBtn->setCursor(Qt::PointingHandCursor);
    heroBtn->setFixedSize(238, 36);
    heroBtn->setStyleSheet("QPushButton{background:#B56D46;color:white;border:none;border-radius:10px;padding:0 18px;font-size:13px;font-weight:800;}QPushButton:hover{background:#A96440;}QPushButton:pressed{background:#995B39;}");
    heroTextLay->addWidget(heroBtn, 0, Qt::AlignLeft);

    QLabel *heroBonus = new QLabel(hero);
    heroBonus->setStyleSheet("color:#4D4640;font-size:12px;");
    heroTextLay->addWidget(heroBonus);
    heroTextLay->addStretch();
    heroLayout->addLayout(heroTextLay, 1);
    topWrapLay->addWidget(hero, 0, Qt::AlignTop);

    QWidget *collectionsPanel = new QWidget(topWrap);
    collectionsPanel->setFixedWidth(360);
    QVBoxLayout *collectionsLay = new QVBoxLayout(collectionsPanel);
    collectionsLay->setContentsMargins(0, 0, 0, 0);
    collectionsLay->setSpacing(8);
    QLabel *collectionsTitle = new QLabel("НОВЫЕ КОЛЛЕКЦИИ", collectionsPanel);
    collectionsTitle->setStyleSheet("color:#1F1D22;font-size:15px;font-weight:900;");
    collectionsLay->addWidget(collectionsTitle);
    QHBoxLayout *collectionsCards = new QHBoxLayout();
    collectionsCards->setContentsMargins(0, 0, 0, 0);
    collectionsCards->setSpacing(14);
    if (!m_products.isEmpty()) {
      const Product &p1 = m_products[qMin(1, m_products.size() - 1)];
      const Product &p2 = m_products[qMin(2, m_products.size() - 1)];
      collectionsCards->addWidget(createHomeCollectionCard(p1, "ЖЕНСКАЯ НОВИНКА:\nКОЛЛЕКЦИЯ \"ESSENTIALS\""));
      collectionsCards->addWidget(createHomeCollectionCard(p2, "МУЖСКАЯ НОВИНКА:\nКОЛЛЕКЦИЯ \"CITY PACE\""));
    }
    collectionsLay->addLayout(collectionsCards);
    topWrapLay->addWidget(collectionsPanel, 0, Qt::AlignTop);

    homeLayout->addWidget(topWrap, 0, Qt::AlignHCenter);

    QWidget *heroDots = new QWidget(m_homeSection);
    heroDots->setMaximumWidth(HOME_CONTENT_WIDTH);
    QHBoxLayout *heroDotsLay = new QHBoxLayout(heroDots);
    heroDotsLay->setContentsMargins(0, 0, 0, 0);
    heroDotsLay->setSpacing(6);
    heroDotsLay->setAlignment(Qt::AlignHCenter);

    struct HomeHeroSlide {
      QString title;
      QString subtitle;
      QString button;
      QString bonus;
      QString imagePath;
    };

    auto heroSlides = std::make_shared<QVector<HomeHeroSlide>>(QVector<HomeHeroSlide>{
      {"КУРС ПО СТИЛЮ ДЛЯ СЕБЯ", "Подбор одежды и образов под ваш стиль,\nрост и бюджет.", "СТАРТУЙ СЕЙЧАС", "+ Персональные рекомендации в подарок", ":/images/assets/home_hero_girl.png"},
      {"РЕКЛАМА: КАПСУЛА НА НЕДЕЛЮ", "Готовые сочетания без долгого поиска.\nВыбирай стиль — каталог соберёт варианты.", "ПОДОБРАТЬ ОБРАЗ", "+ Быстрый переход к подбору", ""},
      {"НОВЫЕ БАЗОВЫЕ ВЕЩИ", "Кепки, худи, футболки и брюки для повседневного гардероба.", "СМОТРЕТЬ НОВИНКИ", "+ Новые товары уже в каталоге", ""},
      {"ПЕРСОНАЛЬНЫЙ ПОДБОР", "Фильтрация по размеру, цвету, стилю и бюджету.\nПодходит для быстрого заказа.", "ОТКРЫТЬ ПОДБОР", "+ Можно сразу добавить вещи в корзину", ""}
    });
    for (int i = 0; i < heroSlides->size(); ++i) {
      if (!(*heroSlides)[i].imagePath.isEmpty())
        continue;
      if (!m_products.isEmpty())
        (*heroSlides)[i].imagePath = resolveImagePath(m_products[qMin(i, m_products.size() - 1)].imagePath);
      else
        (*heroSlides)[i].imagePath = ":/images/assets/home_hero_girl.png";
    }

    QVector<QPushButton*> heroDotButtons;
    auto heroDotStyle = [](bool active) {
      return active
        ? QString("QPushButton{background:#A96A50;border:none;border-radius:3px;min-width:7px;max-width:7px;min-height:7px;max-height:7px;padding:0;}QPushButton:hover{background:#965A42;}")
        : QString("QPushButton{background:#D8CFC8;border:none;border-radius:3px;min-width:7px;max-width:7px;min-height:7px;max-height:7px;padding:0;}QPushButton:hover{background:#C8BDB6;}");
    };
    for (int i = 0; i < heroSlides->size(); ++i) {
      QPushButton *dot = new QPushButton(heroDots);
      dot->setCursor(Qt::PointingHandCursor);
      dot->setFocusPolicy(Qt::NoFocus);
      dot->setFlat(true);
      dot->setFixedSize(7, 7);
      dot->setStyleSheet(heroDotStyle(i == 0));
      heroDotsLay->addWidget(dot);
      heroDotButtons << dot;
    }
    homeLayout->addWidget(heroDots, 0, Qt::AlignHCenter);

    auto heroIndex = std::make_shared<int>(0);
    std::function<void(int, bool)> showHeroSlide;
    showHeroSlide = [=](int targetIndex, bool animate) mutable {
      if (heroSlides->isEmpty())
        return;
      Q_UNUSED(animate);

      int next = targetIndex % heroSlides->size();
      if (next < 0)
        next += heroSlides->size();

      auto applySlide = [=]() {
        const HomeHeroSlide &slide = heroSlides->at(next);
        heroTitle->setText(slide.title);
        heroSub->setText(slide.subtitle);
        heroBtn->setText(slide.button);
        heroBonus->setText(slide.bonus);

        QPixmap pix = cachedScaledPixmap(slide.imagePath, heroImage->size());
        if (!pix.isNull()) {
          heroImage->setText(QString());
          heroImage->setPixmap(pix);
        } else {
          heroImage->setPixmap(QPixmap());
          heroImage->setText("AD");
        }

        for (int i = 0; i < heroDotButtons.size(); ++i) {
          const bool active = (i == next);
          heroDotButtons[i]->setStyleSheet(heroDotStyle(active));
        }
        *heroIndex = next;
      };

      applySlide();
    };

    for (int i = 0; i < heroDotButtons.size(); ++i) {
      connect(heroDotButtons[i], &QPushButton::clicked, this, [showHeroSlide, i]() mutable {
        showHeroSlide(i, true);
      });
    }

    QTimer *heroTimer = new QTimer(this);
    heroTimer->setInterval(4200);
    connect(heroTimer, &QTimer::timeout, this, [showHeroSlide, heroIndex]() mutable {
      showHeroSlide(*heroIndex + 1, true);
    });
    heroTimer->start();
    showHeroSlide(0, false);

    QWidget *recsSection = new QWidget(m_homeSection);
    recsSection->setMaximumWidth(HOME_CONTENT_WIDTH);
    QVBoxLayout *recsLayout = new QVBoxLayout(recsSection);
    recsLayout->setContentsMargins(0, 0, 0, 0);
    recsLayout->setSpacing(12);

    QLabel *recsTitle = new QLabel("Рекомендации", recsSection);
    recsTitle->setStyleSheet("color:#23222A;font-size:16px;font-weight:900;");
    recsLayout->addWidget(recsTitle);

    QPushButton *recsAllBtn = new QPushButton(recsSection);
    recsAllBtn->setVisible(false);

    QScrollArea *recsScroll = new QScrollArea(recsSection);
    recsScroll->setObjectName("HomeRecommendationsInvisibleScroll");
    recsScroll->setFrameShape(QFrame::NoFrame);
    recsScroll->setWidgetResizable(false);
    recsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    recsScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    recsScroll->setFixedHeight(232);
    recsScroll->setStyleSheet("QScrollArea#HomeRecommendationsInvisibleScroll{background:transparent;border:none;}");

    QWidget *recsContainer = new QWidget(recsScroll);
    recsContainer->setStyleSheet("background:transparent;border:none;");
    QHBoxLayout *recsCards = new QHBoxLayout(recsContainer);
    recsCards->setContentsMargins(0, 0, 0, 0);
    recsCards->setSpacing(18);

    const int recCardW = 322;
    const int recCardH = 214;
    const int recGap = 18;
    const int recCount = qMin(8, m_products.size());
    for (int i = 0; i < recCount; ++i)
      recsCards->addWidget(createHomeRecommendationCard(m_products[i]));
    recsCards->addStretch();

    const int recVisibleWidth = 3 * recCardW + 2 * recGap + 2;
    const int recTotalWidth = qMax(recVisibleWidth, recCount * recCardW + qMax(0, recCount - 1) * recGap);
    recsContainer->setFixedSize(recTotalWidth, recCardH);
    recsScroll->setFixedWidth(recVisibleWidth);
    recsScroll->setWidget(recsContainer);
    recsScroll->viewport()->setProperty("home_hidden_horizontal_scroll", true);
    recsScroll->viewport()->installEventFilter(this);
    recsLayout->addWidget(recsScroll, 0, Qt::AlignLeft);
    homeLayout->addWidget(recsSection, 0, Qt::AlignHCenter);


    QWidget *lookbookHomeSection = new QWidget(m_homeSection);
    lookbookHomeSection->setMaximumWidth(HOME_CONTENT_WIDTH);
    QVBoxLayout *lookbookHomeLay = new QVBoxLayout(lookbookHomeSection);
    lookbookHomeLay->setContentsMargins(0, 6, 0, 0);
    lookbookHomeLay->setSpacing(10);

    QHBoxLayout *lookbookHeader = new QHBoxLayout();
    QVBoxLayout *lookbookTitleCol = new QVBoxLayout();
    lookbookTitleCol->setSpacing(2);

    QLabel *lookbookHomeTitle = new QLabel("Рекомендации из лукбука", lookbookHomeSection);
    lookbookHomeTitle->setStyleSheet("color:#23222A;font-size:16px;font-weight:900;");

    QLabel *lookbookHomeSub = new QLabel("Готовые идеи образов: можно открыть раздел лукбука и посмотреть сочетания вещей.", lookbookHomeSection);
    lookbookHomeSub->setStyleSheet("color:#7E7770;font-size:12px;");

    lookbookTitleCol->addWidget(lookbookHomeTitle);
    lookbookTitleCol->addWidget(lookbookHomeSub);
    lookbookHeader->addLayout(lookbookTitleCol);
    lookbookHeader->addStretch();
    lookbookHomeLay->addLayout(lookbookHeader);

    QHBoxLayout *lookbookCards = new QHBoxLayout();
    lookbookCards->setContentsMargins(0, 0, 0, 0);
    lookbookCards->setSpacing(18);

    auto makeHomeLookbookLink = [&](const QString &title,
                    const QString &subtitle,
                    const QStringList &keywords) -> QWidget*
    {
      QFrame *card = new QFrame(lookbookHomeSection);
      card->setStyleSheet(
       "QFrame{background:#FFFFFF;border:1px solid #D7D1CC;border-radius:14px;}"
       "QLabel{background:transparent;border:none;}"
      );
      card->setFixedSize(322, 148);
      card->setCursor(Qt::PointingHandCursor);

      QHBoxLayout *cardLay = new QHBoxLayout(card);
      cardLay->setContentsMargins(12, 12, 12, 12);
      cardLay->setSpacing(12);

      QWidget *thumbs = new QWidget(card);
      thumbs->setStyleSheet("background:transparent;border:none;");
      QGridLayout *thumbsLay = new QGridLayout(thumbs);
      thumbsLay->setContentsMargins(0, 0, 0, 0);
      thumbsLay->setHorizontalSpacing(6);
      thumbsLay->setVerticalSpacing(6);

      const QVector<Product> items = findLookbookProductsForSlots(keywords);
      for (int i = 0; i < 4; ++i) {
        const Product *prod = (i < items.size()) ? &items[i] : nullptr;
        thumbsLay->addWidget(createLookbookThumb(thumbs, prod, 50, 50), i / 2, i % 2);
      }

      cardLay->addWidget(thumbs, 0, Qt::AlignVCenter);

      QVBoxLayout *textLay = new QVBoxLayout();
      textLay->setContentsMargins(0, 0, 0, 0);
      textLay->setSpacing(5);

      QLabel *t = new QLabel(title, card);
      t->setWordWrap(true);
      t->setStyleSheet("color:#201D1C;font-size:13px;font-weight:900;");

      QLabel *s = new QLabel(subtitle, card);
      s->setWordWrap(true);
      s->setStyleSheet("color:#6E6762;font-size:11px;");

      QPushButton *openLookbookBtn = new QPushButton("Открыть", card);
      openLookbookBtn->setCursor(Qt::PointingHandCursor);
      openLookbookBtn->setFixedSize(118, 28);
      openLookbookBtn->setStyleSheet(
       "QPushButton{background:#B56D46;color:white;border:none;border-radius:8px;font-size:11px;font-weight:800;}"
       "QPushButton:hover{background:#A96440;}"
       "QPushButton:pressed{background:#995B39;}"
      );

      textLay->addWidget(t);
      textLay->addWidget(s);
      textLay->addStretch();
      textLay->addWidget(openLookbookBtn, 0, Qt::AlignLeft);

      cardLay->addLayout(textLay, 1);

      auto openLookbook = [this]() { openLookbookPage(); };

      connect(openLookbookBtn, &QPushButton::clicked, this, openLookbook);
      card->setProperty("home_open_lookbook_link", true);
      card->installEventFilter(this);

      return card;
    };

    lookbookCards->addWidget(makeHomeLookbookLink(
     "Минимализм: Беж и деним",
     "Спокойная база на каждый день",
      {"беж", "молоч", "крем", "джин", "деним", "рубаш"}));

    lookbookCards->addWidget(makeHomeLookbookLink(
     "Яркие акценты: Красный",
     "Акцентные вещи для смелых образов",
      {"крас", "синий", "ярк", "персик", "терракот"}));

    lookbookCards->addWidget(makeHomeLookbookLink(
     "Винтажный шик: 70-е",
     "Тёплые оттенки и ретро-силуэты",
      {"терракот", "карамел", "олив", "брюк", "юб"}));

    lookbookCards->addStretch();
    lookbookHomeLay->addLayout(lookbookCards);
    homeLayout->addWidget(lookbookHomeSection, 0, Qt::AlignHCenter);
    homeLayout->addSpacing(10);

    m_homeScroll = new QScrollArea(central);
    m_homeScroll->setObjectName("HomeScroll");
    m_homeScroll->setWidgetResizable(true);
    m_homeScroll->setFrameShape(QFrame::NoFrame);
    m_homeScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_homeScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_homeScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_homeScroll->setWidget(m_homeSection);
    centralLayout->addWidget(m_homeScroll, 1);


    // =================== MAIN CATALOG ===================
    m_catalogScroll = new QScrollArea(central);
    m_catalogScroll->setWidgetResizable(true);
    m_catalogScroll->setFrameShape(QFrame::NoFrame);
    m_catalogScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Keep the area width stable when the scrollbar appears or disappears.
    m_catalogScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_catalogScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);


    m_gridWidget = new QWidget(m_catalogScroll);
    m_gridLayout = new QGridLayout(m_gridWidget);
    m_gridLayout->setSpacing(18);

    m_catalogScroll->setWidget(m_gridWidget);

    // The catalog is hidden on the home page by default.
    m_catalogScroll->setVisible(false);

    centralLayout->addWidget(m_catalogScroll, 1);

    // =================== My orders ===================
    // Orders section is embedded in the main window for quick user access.
    m_ordersSection = new QWidget(central);
    m_ordersSection->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_ordersSection->setStyleSheet(
     "#OrdersPageTitle{font-size:20px;font-weight:800;color:#1E2A36;}"
     "#OrdersCard{background:#FFFFFF;border:1px solid #E8DED5;border-radius:16px;}"
     "#OrdersSearch{background:#FFFFFF;border:1px solid #E0D7CF;border-radius:12px;padding:0 14px;min-height:44px;color:#1E2A36;font-size:14px;}"
     "#OrdersSearch:focus{border:1px solid #DFA08C;}"
     "#OrdersFilterBtn{background:#FFFFFF;border:1px solid #E0D7CF;border-radius:12px;min-height:44px;padding:0 18px;color:#111111;font-weight:700;}"
     "#OrdersFilterBtn:hover{background:#FFFFFF;border:1px solid #D6B7A8;}"
     "#OrdersFilterBtn:pressed{background:#FFFFFF;border:1px solid #E0D7CF;}"
     "#OrdersStatusQuickBox{background:#FFFFFF;border:1px solid #E0D7CF;border-radius:12px;min-height:44px;padding:0 12px;color:#111111;font-weight:600;min-width:142px;}"
     "#OrdersStatusQuickBox:hover{background:#FFFFFF;border:1px solid #D6B7A8;}"
     "#OrdersStatusQuickBox:pressed{background:#FFFFFF;border:1px solid #E0D7CF;}"
     "#OrdersFiltersPanel{background:#FFFFFF;border:1px solid #E8DED5;border-radius:16px;}"
     "#OrdersFiltersPanel QLabel[filterCaption='true']{color:#6F6760;font-size:12px;font-weight:700;}"
     "#OrdersFiltersPanel QComboBox,#OrdersFiltersPanel QDateEdit,#OrdersFiltersPanel QSpinBox{background:#FFFFFF;border:1px solid #E0D7CF;border-radius:10px;min-height:34px;padding:0 10px;color:#1E2A36;}"
     "#OrdersFiltersPanel QCheckBox{color:#1E2A36;background:#FFFFFF;}"
     "#OrdersApplyBtn{background:#DFA08C;color:white;border:none;border-radius:12px;min-height:38px;padding:0 18px;font-weight:800;}"
     "#OrdersApplyBtn:hover{background:#C98A76;}"
     "#OrdersApplyBtn:pressed{background:#DFA08C;}"
     "#OrdersResetBtn{background:#FFFFFF;color:#6B625B;border:1px solid #E0D7CF;border-radius:12px;min-height:38px;padding:0 18px;font-weight:700;}"
     "#OrdersResetBtn:hover{background:#FFFFFF;border:1px solid #D6B7A8;}"
     "#OrdersResetBtn:pressed{background:#FFFFFF;border:1px solid #E0D7CF;}"
     "#OrdersCountLabel{color:#575757;font-size:13px;}"
     "#OrdersPagerLabel{color:#575757;font-size:13px;}"
     "QTableWidget#OrdersTable{background:#FFFFFF;border:none;gridline-color:#FFFFFF;color:#111111;selection-background-color:#FFFFFF;selection-color:#111111;outline:none;font-size:14px;}"
     "QTableWidget#OrdersTable::item{background:#FFFFFF;color:#111111;padding:10px;border-bottom:1px solid #E8E2DC;}"
     "QTableWidget#OrdersTable::item:selected{background:#FFFFFF;color:#111111;}"
     "QTableWidget#OrdersTable::item:hover{background:#FFFFFF;color:#111111;}"
     "QHeaderView::section{background:#F5F1EC;color:#1F1F1F;border:none;border-bottom:1px solid #E8DED5;padding:10px 12px;font-weight:700;font-size:14px;}"
    );

    QVBoxLayout *ordersLay = new QVBoxLayout(m_ordersSection);
    ordersLay->setContentsMargins(32, 24, 32, 22);
    ordersLay->setSpacing(12);

    QLabel *ordersTitle = new QLabel("МОИ ЗАКАЗЫ", m_ordersSection);
    ordersTitle->setObjectName("OrdersPageTitle");
    ordersLay->addWidget(ordersTitle);

    QFrame *ordersCard = new QFrame(m_ordersSection);
    ordersCard->setObjectName("OrdersCard");
    ordersCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    QVBoxLayout *ordersCardLay = new QVBoxLayout(ordersCard);
    ordersCardLay->setContentsMargins(24, 24, 24, 20);
    ordersCardLay->setSpacing(16);

    QHBoxLayout *ordersTopBar = new QHBoxLayout();
    ordersTopBar->setSpacing(10);

    QLineEdit *ordersSearchEdit = new QLineEdit(ordersCard);
    ordersSearchEdit->setObjectName("OrdersSearch");
    ordersSearchEdit->setPlaceholderText("Поиск по номеру заказа или дате");
    ordersSearchEdit->setClearButtonEnabled(true);

    QPushButton *ordersFilterButton = new QPushButton(QString::fromUtf8("Фильтр"), ordersCard);
    ordersFilterButton->setObjectName("OrdersFilterBtn");
    ordersFilterButton->setCursor(Qt::PointingHandCursor);
    ordersFilterButton->setFocusPolicy(Qt::NoFocus);

    QComboBox *ordersStatusQuickBox = new QComboBox(ordersCard);
    ordersStatusQuickBox->setObjectName("OrdersStatusQuickBox");
    ordersStatusQuickBox->addItems({"Все статусы", "Доставлен", "В обработке", "Отправлен", "Отменен"});
    ordersStatusQuickBox->setCursor(Qt::PointingHandCursor);
    ordersStatusQuickBox->setFocusPolicy(Qt::NoFocus);

    ordersTopBar->addWidget(ordersSearchEdit, 1);
    ordersTopBar->addWidget(ordersFilterButton, 0);
    ordersTopBar->addWidget(ordersStatusQuickBox, 0);
    ordersCardLay->addLayout(ordersTopBar);

    QFrame *ordersFiltersPanel = new QFrame(ordersCard);
    ordersFiltersPanel->setObjectName("OrdersFiltersPanel");
    ordersFiltersPanel->setVisible(false);
    QGridLayout *filtersGrid = new QGridLayout(ordersFiltersPanel);
    filtersGrid->setContentsMargins(14, 14, 14, 14);
    filtersGrid->setHorizontalSpacing(12);
    filtersGrid->setVerticalSpacing(10);

    auto makeOrderFilterCaption = [&](const QString &text) {
      QLabel *label = new QLabel(text, ordersFiltersPanel);
      label->setProperty("filterCaption", true);
      return label;
    };

    QComboBox *ordersDatePresetBox = new QComboBox(ordersFiltersPanel);
    ordersDatePresetBox->addItems({"За всё время", "За месяц", "За год", "Точные даты"});

    QDateEdit *ordersDateFromEdit = new QDateEdit(ordersFiltersPanel);
    ordersDateFromEdit->setDisplayFormat("dd.MM.yyyy");
    ordersDateFromEdit->setCalendarPopup(true);
    ordersDateFromEdit->setDate(QDate::currentDate().addMonths(-1));

    QDateEdit *ordersDateToEdit = new QDateEdit(ordersFiltersPanel);
    ordersDateToEdit->setDisplayFormat("dd.MM.yyyy");
    ordersDateToEdit->setCalendarPopup(true);
    ordersDateToEdit->setDate(QDate::currentDate());

    QCheckBox *statusDeliveredCheck = new QCheckBox("Доставлен", ordersFiltersPanel);
    QCheckBox *statusProcessingCheck = new QCheckBox("В обработке", ordersFiltersPanel);
    QCheckBox *statusSentCheck = new QCheckBox("Отправлен", ordersFiltersPanel);
    QCheckBox *statusCanceledCheck = new QCheckBox("Отменен", ordersFiltersPanel);
    for (QCheckBox *cb : {statusDeliveredCheck, statusProcessingCheck, statusSentCheck, statusCanceledCheck}) {
      cb->setChecked(true);
    }

    QComboBox *ordersPaymentBox = new QComboBox(ordersFiltersPanel);
    ordersPaymentBox->addItems({"Любой способ", "Карта онлайн", "СБП", "Наличные"});

    QSpinBox *ordersPriceMinSpin = new QSpinBox(ordersFiltersPanel);
    ordersPriceMinSpin->setRange(0, 999999);
    ordersPriceMinSpin->setSingleStep(500);
    ordersPriceMinSpin->setSuffix(" ₽");

    QSpinBox *ordersPriceMaxSpin = new QSpinBox(ordersFiltersPanel);
    ordersPriceMaxSpin->setRange(0, 999999);
    ordersPriceMaxSpin->setSingleStep(500);
    ordersPriceMaxSpin->setValue(999999);
    ordersPriceMaxSpin->setSuffix(" ₽");

    QSpinBox *ordersItemsMinSpin = new QSpinBox(ordersFiltersPanel);
    ordersItemsMinSpin->setRange(0, 999);
    ordersItemsMinSpin->setValue(0);
    ordersItemsMinSpin->setSuffix(" шт.");

    QPushButton *ordersApplyButton = new QPushButton("Применить", ordersFiltersPanel);
    ordersApplyButton->setObjectName("OrdersApplyBtn");
    ordersApplyButton->setCursor(Qt::PointingHandCursor);

    QPushButton *ordersResetButton = new QPushButton("Сбросить", ordersFiltersPanel);
    ordersResetButton->setObjectName("OrdersResetBtn");
    ordersResetButton->setCursor(Qt::PointingHandCursor);

    filtersGrid->addWidget(makeOrderFilterCaption("Диапазон дат"), 0, 0);
    filtersGrid->addWidget(ordersDatePresetBox, 1, 0);
    filtersGrid->addWidget(makeOrderFilterCaption("С"), 0, 1);
    filtersGrid->addWidget(ordersDateFromEdit, 1, 1);
    filtersGrid->addWidget(makeOrderFilterCaption("До"), 0, 2);
    filtersGrid->addWidget(ordersDateToEdit, 1, 2);

    QWidget *statusChecksWidget = new QWidget(ordersFiltersPanel);
    QHBoxLayout *statusChecksLay = new QHBoxLayout(statusChecksWidget);
    statusChecksLay->setContentsMargins(0, 0, 0, 0);
    statusChecksLay->setSpacing(10);
    statusChecksLay->addWidget(statusDeliveredCheck);
    statusChecksLay->addWidget(statusProcessingCheck);
    statusChecksLay->addWidget(statusSentCheck);
    statusChecksLay->addWidget(statusCanceledCheck);
    statusChecksLay->addStretch();

    filtersGrid->addWidget(makeOrderFilterCaption("Статус заказа"), 2, 0, 1, 3);
    filtersGrid->addWidget(statusChecksWidget, 3, 0, 1, 3);

    filtersGrid->addWidget(makeOrderFilterCaption("Способ оплаты"), 4, 0);
    filtersGrid->addWidget(ordersPaymentBox, 5, 0);
    filtersGrid->addWidget(makeOrderFilterCaption("Стоимость от"), 4, 1);
    filtersGrid->addWidget(ordersPriceMinSpin, 5, 1);
    filtersGrid->addWidget(makeOrderFilterCaption("Стоимость до"), 4, 2);
    filtersGrid->addWidget(ordersPriceMaxSpin, 5, 2);
    filtersGrid->addWidget(makeOrderFilterCaption("Количество товаров от"), 6, 0);
    filtersGrid->addWidget(ordersItemsMinSpin, 7, 0);

    QHBoxLayout *filterButtonsLay = new QHBoxLayout();
    filterButtonsLay->setContentsMargins(0, 0, 0, 0);
    filterButtonsLay->setSpacing(10);
    filterButtonsLay->addStretch();
    filterButtonsLay->addWidget(ordersResetButton);
    filterButtonsLay->addWidget(ordersApplyButton);
    filtersGrid->addLayout(filterButtonsLay, 7, 1, 1, 2);

    ordersCardLay->addWidget(ordersFiltersPanel);

    m_ordersTable = new QTableWidget(ordersCard);
    m_ordersTable->setObjectName("OrdersTable");
    m_ordersTable->setColumnCount(7);
    m_ordersTable->setHorizontalHeaderLabels({"№", "Дата", "Статус", "Кол-во", "Оплата", "Сумма", ""});
    m_ordersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_ordersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ordersTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_ordersTable->setFocusPolicy(Qt::NoFocus);
    m_ordersTable->setShowGrid(false);
    m_ordersTable->setAlternatingRowColors(false);
    m_ordersTable->setFrameShape(QFrame::NoFrame);
    m_ordersTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_ordersTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_ordersTable->verticalHeader()->setVisible(false);
    m_ordersTable->horizontalHeader()->setStretchLastSection(false);
    m_ordersTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_ordersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_ordersTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    m_ordersTable->setColumnWidth(6, 56);
    m_ordersTable->verticalHeader()->setDefaultSectionSize(58);
    ordersCardLay->addWidget(m_ordersTable, 0);

    QHBoxLayout *ordersBottomBar = new QHBoxLayout();
    QLabel *ordersCountLabel = new QLabel("Показано 0 из 0 заказов", ordersCard);
    ordersCountLabel->setObjectName("OrdersCountLabel");
    QLabel *ordersPagerLabel = new QLabel("Пред.  1  След.", ordersCard);
    ordersPagerLabel->setObjectName("OrdersPagerLabel");
    ordersBottomBar->addWidget(ordersCountLabel);
    ordersBottomBar->addStretch();
    ordersBottomBar->addWidget(ordersPagerLabel);
    ordersCardLay->addLayout(ordersBottomBar);

    ordersLay->addWidget(ordersCard, 0);
    ordersLay->addStretch(1);

    // The orders block is hidden by default.
    m_ordersSection->setVisible(false);

    centralLayout->addWidget(m_ordersSection, 1);

    // =================== Lookbook ===================
    // Section structure: project list and a four-slot outfit editor.
    // =================== Lookbook ===================
    // Lookbook section manages outfit projects and four outfit slots.
    m_lookbookSection = new QWidget(central);
    m_lookbookSection->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout *lbRootLay = new QVBoxLayout(m_lookbookSection);
    lbRootLay->setContentsMargins(24, 12, 6, 12);
    lbRootLay->setSpacing(12);

    m_lookbookStack = new QStackedWidget(m_lookbookSection);

    // Page 0: lookbook gallery.
    m_lbProjectsPage = new QWidget(m_lookbookSection);
    QVBoxLayout *lbProjectsLay = new QVBoxLayout(m_lbProjectsPage);
    lbProjectsLay->setContentsMargins(0, 0, 0, 0);
    lbProjectsLay->setSpacing(0);

    QScrollArea *lbScroll = new QScrollArea(m_lbProjectsPage);
    lbScroll->setWidgetResizable(true);
    lbScroll->setFrameShape(QFrame::NoFrame);
    lbScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lbProjectsLay->addWidget(lbScroll, 1);

    QWidget *lbListHost = new QWidget(lbScroll);
    lbListHost->setObjectName("LookbookHost");
    lbListHost->setStyleSheet(
     "#LookbookHost{background:#F4EFE9;}"
     "#LookbookTitle{font-size:18px;font-weight:800;color:#1E2A36;}"
     "#LookbookSectionTitle{font-size:13px;font-weight:800;color:#2E2E2E;letter-spacing:0.5px;}"
     "#LookbookSearch{background:#FFFFFF;border:1px solid #E2D9D1;border-radius:12px;padding:0 14px;min-height:42px;color:#1E2A36;}"
     "#LookbookSearch:focus{border:1px solid #D7A28F;}"
     "#LookbookSortLabel{color:#4B4B4B;font-weight:600;}"
     "#LookbookSortBox{background:#FFFFFF;border:1px solid #E2D9D1;border-radius:12px;min-height:42px;padding:0 12px;color:#2F2F2F;}"
     "#LookbookCreateBtn{background:#FFFFFF;border:1px solid #D8CEC5;border-radius:12px;min-height:42px;padding:0 18px;color:#2F2F2F;font-weight:700;}"
     "#LookbookCreateBtn:hover{background:#FBF7F3;border:1px solid #D2A08A;}"
    );
    QVBoxLayout *lbPageLay = new QVBoxLayout(lbListHost);
    lbPageLay->setContentsMargins(10, 8, 22, 22);
    lbPageLay->setSpacing(18);
    lbScroll->setWidget(lbListHost);

    QHBoxLayout *lbTop = new QHBoxLayout();
    lbTop->setSpacing(12);

    QLabel *lbTitle = new QLabel("ЛУКБУК", lbListHost);
    lbTitle->setObjectName("LookbookTitle");
    lbTitle->setMinimumWidth(120);
    lbTop->addWidget(lbTitle);

    m_lbSearchEdit = new QLineEdit(lbListHost);
    m_lbSearchEdit->setObjectName("LookbookSearch");
    m_lbSearchEdit->setPlaceholderText("Поиск...");
    m_lbSearchEdit->addAction(QIcon(":/icons/search.png"), QLineEdit::LeadingPosition);
    lbTop->addWidget(m_lbSearchEdit, 1);

    QLabel *lbSortLabel = new QLabel("Сортировать", lbListHost);
    lbSortLabel->setObjectName("LookbookSortLabel");
    lbTop->addWidget(lbSortLabel);

    m_lbSortBox = new QComboBox(lbListHost);
    m_lbSortBox->setObjectName("LookbookSortBox");
    m_lbSortBox->addItems({"По дате", "По количеству вещей", "По названию"});
    m_lbSortBox->setMinimumWidth(180);
    lbTop->addWidget(m_lbSortBox);

    QPushButton *lbCreateBtn = new QPushButton("＋ СОЗДАТЬ НОВЫЙ ЛУКБУК", lbListHost);
    lbCreateBtn->setObjectName("LookbookCreateBtn");
    lbCreateBtn->setCursor(Qt::PointingHandCursor);
    lbTop->addWidget(lbCreateBtn);
    lbPageLay->addLayout(lbTop);

    auto makeSectionTitle = [lbListHost](const QString &text) {
      QLabel *lab = new QLabel(text, lbListHost);
      lab->setObjectName("LookbookSectionTitle");
      return lab;
    };

    lbPageLay->addWidget(makeSectionTitle("МОИ ЛУКБУКИ"));
    QWidget *myBooksHost = new QWidget(lbListHost);
    m_lbProjectsListLay = new QGridLayout(myBooksHost);
    m_lbProjectsListLay->setContentsMargins(0, 0, 0, 0);
    m_lbProjectsListLay->setHorizontalSpacing(16);
    m_lbProjectsListLay->setVerticalSpacing(16);
    lbPageLay->addWidget(myBooksHost);

    lbPageLay->addWidget(makeSectionTitle("ТРЕНДЫ И ВДОХНОВЕНИЕ"));
    QWidget *trendsHost = new QWidget(lbListHost);
    m_lbTrendsLay = new QGridLayout(trendsHost);
    m_lbTrendsLay->setContentsMargins(0, 0, 0, 0);
    m_lbTrendsLay->setHorizontalSpacing(16);
    m_lbTrendsLay->setVerticalSpacing(16);
    lbPageLay->addWidget(trendsHost);

    lbPageLay->addWidget(makeSectionTitle("ГОТОВЫЕ КАПСУЛЫ"));
    QWidget *capsulesHost = new QWidget(lbListHost);
    m_lbCapsulesLay = new QGridLayout(capsulesHost);
    m_lbCapsulesLay->setContentsMargins(0, 0, 0, 0);
    m_lbCapsulesLay->setHorizontalSpacing(16);
    m_lbCapsulesLay->setVerticalSpacing(16);
    lbPageLay->addWidget(capsulesHost);
    lbPageLay->addStretch();

    // Page 1: lookbook editor.
    m_lbEditorPage = new QWidget(m_lookbookSection);
    m_lbEditorPage->setStyleSheet(
     "#LbEditorHost{background:#F4EFE9;}"
     "#LbTopAction{background:#FFFFFF;border:1px solid #E4DAD2;border-radius:10px;padding:0 12px;min-height:34px;font-weight:600;color:#3A322C;}"
     "#LbTopAction:hover{background:#FBF7F3;border:1px solid #D0A48E;}"
     "#LbSaveBtn{background:#6F4B37;color:white;border:none;border-radius:10px;padding:0 14px;min-height:34px;font-weight:700;}"
     "#LbSaveBtn:hover{background:#5F3E2D;}"
     "#LbTitleEdit{background:#FFFFFF;border:1px solid #E2D9D1;border-radius:10px;padding:0 12px;min-height:34px;max-height:34px;font-size:14px;font-weight:700;color:#1E2A36;}"
     "#LbUpdatedLabel{color:#857A71;font-size:12px;}"
     "#LbSlotCard{background:#FFFFFF;border:1px solid #E4DAD2;border-radius:16px;}"
     "#LbSlotTitle{font-size:14px;font-weight:800;color:#1F1F1F;}"
     "#LbSlotImage{background:#F5F1EC;border:1px solid #EDE3DB;border-radius:14px;color:#A39990;}"
     "#LbSlotName{font-size:17px;font-weight:800;color:#1B1B1B;line-height:1.15;}"
     "#LbSlotMeta{font-size:14px;color:#554E48;line-height:1.25;}"
     "#LbColorChip{border:1px solid #E0D5CC;border-radius:10px;background:#FFFFFF;padding:1px;}"
     "#LbColorChip:checked{border:2px solid #6F4B37;background:#FFFDFB;}"
     "#LbSlotAction{background:#F7F2EC;border:1px solid #E0D6CD;border-radius:10px;padding:7px 13px;min-width:82px;font-weight:700;color:#3B352F;}"
     "#LbSlotAction:hover{background:#F0E7DE;}"
     "#LbSlotTitle,#LbSlotName,#LbSlotMeta,#LbDetailsTitle,#LbDetailsSub{background:transparent;border:none;}"
     "#LbCenterPreview{background:#FFFFFF;border:2px solid #ECE3DB;border-radius:14px;}"
     "#LbCenterPreviewInner{background:#F8F3EE;border:1px dashed #D9CEC4;border-radius:12px;color:#8B8178;font-size:12px;}"
     "#LbDetailsCard{background:#FFFFFF;border:1px solid #E4DAD2;border-radius:16px;}"
     "#LbDetailsTitle{font-size:18px;font-weight:800;color:#222222;}"
     "#LbDetailsSub{font-size:13px;font-weight:700;color:#37312D;}"
     "#LbTag{background:#F7F2EC;border:1px solid #E4DAD2;border-radius:10px;padding:4px 8px;color:#5D534B;}"
     "#LbMiniRec{background:#F8F3EE;border:1px solid #E5DBD2;border-radius:12px;}"
    );
    QVBoxLayout *lbEditRoot = new QVBoxLayout(m_lbEditorPage);
    lbEditRoot->setContentsMargins(0, 0, 0, 0);
    lbEditRoot->setSpacing(0);

    QScrollArea *lbEditScroll = new QScrollArea(m_lbEditorPage);
    lbEditScroll->setWidgetResizable(true);
    lbEditScroll->setFrameShape(QFrame::NoFrame);
    lbEditScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lbEditRoot->addWidget(lbEditScroll, 1);

    QWidget *lbEditHost = new QWidget(lbEditScroll);
    lbEditHost->setObjectName("LbEditorHost");
    lbEditScroll->setWidget(lbEditHost);

    QVBoxLayout *lbEditLay = new QVBoxLayout(lbEditHost);
    lbEditLay->setContentsMargins(18, 14, 18, 18);
    lbEditLay->setSpacing(14);

    QHBoxLayout *lbEditTop = new QHBoxLayout();
    lbEditTop->setSpacing(10);
    lbEditTop->setAlignment(Qt::AlignTop);

    QToolButton *lbBackBtn = new QToolButton(lbEditHost);
    lbBackBtn->setText("←");
    lbBackBtn->setFixedSize(34, 34);
    lbBackBtn->setStyleSheet(
     "QToolButton{background:#FFFFFF;border:1px solid #E2D9D1;border-radius:17px;font-size:18px;font-weight:700;}"
     "QToolButton:hover{background:#FBF7F3;border:1px solid #D0A48E;}"
    );
    lbEditTop->addWidget(lbBackBtn, 0, Qt::AlignTop);

    QVBoxLayout *titleCol = new QVBoxLayout();
    titleCol->setContentsMargins(0, 0, 0, 0);
    titleCol->setSpacing(4);
    m_lbProjectTitleEdit = new QLineEdit(lbEditHost);
    m_lbProjectTitleEdit->setObjectName("LbTitleEdit");
    m_lbProjectTitleEdit->setPlaceholderText("Название лукбука");
    m_lbProjectTitleEdit->setFixedWidth(320);
    titleCol->addWidget(m_lbProjectTitleEdit);
    m_lbUpdatedLabel = new QLabel("Обновлено: —", lbEditHost);
    m_lbUpdatedLabel->setObjectName("LbUpdatedLabel");
    titleCol->addWidget(m_lbUpdatedLabel);
    lbEditTop->addLayout(titleCol, 0);
    lbEditTop->addStretch(1);

    auto makeTopBtn = [lbEditHost](const QString &text, const QString &obj = QString()) {
      QPushButton *btn = new QPushButton(text, lbEditHost);
      btn->setCursor(Qt::PointingHandCursor);
      if (!obj.isEmpty()) btn->setObjectName(obj);
      else btn->setObjectName("LbTopAction");
      return btn;
    };
    QPushButton *lbSaveBtn = makeTopBtn("Сохранить", "LbSaveBtn");
    QPushButton *lbShareBtn = makeTopBtn("Поделиться");
    QPushButton *lbSettingsBtn = makeTopBtn("Настройки");
    QPushButton *lbEditStructureBtn = makeTopBtn("Структура лукбука");
    QToolButton *lbQuickAddBtn = new QToolButton(lbEditHost);
    lbQuickAddBtn->setText("+");
    lbQuickAddBtn->setFixedSize(34, 34);
    lbQuickAddBtn->setStyleSheet(
     "QToolButton{background:#FFFFFF;border:1px solid #E2D9D1;border-radius:17px;font-size:18px;font-weight:700;}"
     "QToolButton:hover{background:#FBF7F3;border:1px solid #D0A48E;}"
    );
    lbEditTop->addWidget(lbSaveBtn, 0, Qt::AlignTop);
    lbEditTop->addWidget(lbShareBtn, 0, Qt::AlignTop);
    lbEditTop->addWidget(lbSettingsBtn, 0, Qt::AlignTop);
    lbEditTop->addWidget(lbEditStructureBtn, 0, Qt::AlignTop);
    lbEditTop->addWidget(lbQuickAddBtn, 0, Qt::AlignTop);
    lbEditLay->addLayout(lbEditTop);

    QHBoxLayout *editorBody = new QHBoxLayout();
    editorBody->setSpacing(16);

    m_lbEditorMain = new QWidget(lbEditHost);
    m_lbEditorMain->setProperty("lb_editor_main", true);
    m_lbEditorMain->installEventFilter(this);
    QGridLayout *editorGrid = new QGridLayout(m_lbEditorMain);
    editorGrid->setContentsMargins(0, 0, 0, 0);
    editorGrid->setHorizontalSpacing(14);
    editorGrid->setVerticalSpacing(14);
    editorGrid->setColumnStretch(0, 1);
    editorGrid->setColumnStretch(1, 1);
    editorGrid->setRowStretch(0, 1);
    editorGrid->setRowStretch(1, 1);

    auto makeSlot = [this, lbEditHost](const QString &title, int pos) -> QWidget* {
      QFrame *slot = new QFrame(lbEditHost);
      slot->setObjectName("LbSlotCard");
      QVBoxLayout *sl = new QVBoxLayout(slot);
      sl->setContentsMargins(12, 12, 12, 12);
      sl->setSpacing(8);

      QLabel *t = new QLabel(title, slot);
      t->setObjectName("LbSlotTitle");
      sl->addWidget(t);

      QLabel *img = new QLabel(slot);
      img->setObjectName("LbSlotImage");
      img->setAlignment(Qt::AlignCenter);
      img->setFixedHeight(128);
      img->setText("Нажмите, чтобы\nдобавить вещь");
      img->setCursor(Qt::PointingHandCursor);
      img->installEventFilter(this);
      img->setProperty("lb_pos", pos);
      sl->addWidget(img);

      QHBoxLayout *infoActionsRow = new QHBoxLayout();
      infoActionsRow->setContentsMargins(0, 0, 0, 0);
      infoActionsRow->setSpacing(10);

      QVBoxLayout *textCol = new QVBoxLayout();
      // Adjust margins for individual slots:
      // pos 1 = top: shift text to the right so the central preview does not overlap it.
      // pos 3 = shoes: use a small margin.
      textCol->setContentsMargins(pos == 1 ? 72 : (pos == 3 ? 8 : 0), 0, 0, 0);
      textCol->setSpacing(3);

      QLabel *nm = new QLabel("Пока ничего не выбрано", slot);
      nm->setObjectName("LbSlotName");
      nm->setWordWrap(true);
      nm->setMinimumHeight(24);
      textCol->addWidget(nm);

      QWidget *colorHost = new QWidget(slot);
      QHBoxLayout *colorLay = new QHBoxLayout(colorHost);
      colorLay->setContentsMargins(0, 0, 0, 0);
      colorLay->setSpacing(6);
      colorHost->setVisible(false);
      textCol->addWidget(colorHost);

      QLabel *meta = new QLabel("", slot);
      meta->setObjectName("LbSlotMeta");
      meta->setWordWrap(true);
      meta->setMinimumHeight(50);
      textCol->addWidget(meta);

      QPushButton *actionBtn = new QPushButton("Добавить", slot);
      actionBtn->setObjectName("LbSlotAction");
      actionBtn->setCursor(Qt::PointingHandCursor);
      actionBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

      if (pos == 0) {
        // For the headwear slot, the button is moved closer to the text,
        // so the central preview does not overlap it.
        infoActionsRow->addLayout(textCol, 0);
        infoActionsRow->addSpacing(8);
        infoActionsRow->addWidget(actionBtn, 0, Qt::AlignBottom | Qt::AlignLeft);
        infoActionsRow->addStretch(1);
      } else {
        infoActionsRow->addLayout(textCol, 1);
        infoActionsRow->addWidget(actionBtn, 0, Qt::AlignBottom | Qt::AlignRight);
      }
      sl->addLayout(infoActionsRow);

      QPushButton *removeBtn = nullptr;

      slot->setCursor(Qt::PointingHandCursor);
      slot->installEventFilter(this);
      slot->setProperty("lb_pos", pos);

      connect(actionBtn, &QPushButton::clicked, this, [this, pos]() {
        pickProductForPosition(pos);
      });

      m_lbSlotImages.resize(4);
      m_lbSlotNames.resize(4);
      m_lbSlotMeta.resize(4);
      m_lbSlotActionButtons.resize(4);
      m_lbSlotRemoveButtons.resize(4);
      m_lbColorHosts.resize(4);
      m_lbSlotProductIds.resize(4);
      m_lbSelectedColorIndex.resize(4);
      m_lbSlotColorVariants.resize(4);
      m_lbColorVariantProductIds.resize(4);
      m_lbSlotImages[pos] = img;
      m_lbSlotNames[pos] = nm;
      m_lbSlotMeta[pos] = meta;
      m_lbSlotActionButtons[pos] = actionBtn;
      m_lbSlotRemoveButtons[pos] = removeBtn;
      m_lbColorHosts[pos] = colorHost;
      m_lbSlotProductIds[pos] = 0;
      m_lbSelectedColorIndex[pos] = 0;
      return slot;
    };

    QWidget *headSlot = makeSlot("Головной убор", 0);
    QWidget *topSlot = makeSlot("Верх", 1);
    QWidget *bottomSlot = makeSlot("Низ", 2);
    QWidget *shoesSlot = makeSlot("Обувь", 3);
    editorGrid->addWidget(headSlot, 0, 0);
    editorGrid->addWidget(topSlot, 0, 1);
    editorGrid->addWidget(bottomSlot, 1, 0);
    editorGrid->addWidget(shoesSlot, 1, 1);

    QFrame *centerPreview = new QFrame(m_lbEditorMain);
    m_lbCenterOverlay = centerPreview;
    centerPreview->setObjectName("LbCenterPreview");
    centerPreview->setFixedSize(168, 206);
    QVBoxLayout *lbCenterLay = new QVBoxLayout(centerPreview);
    lbCenterLay->setContentsMargins(8, 8, 8, 8);
    lbCenterLay->setSpacing(8);
    m_lbCenterPreviewLabel = new QLabel(centerPreview);
    m_lbCenterPreviewLabel->setObjectName("LbCenterPreviewInner");
    m_lbCenterPreviewLabel->setAlignment(Qt::AlignCenter);
    m_lbCenterPreviewLabel->setMinimumSize(140, 150);
    m_lbCenterPreviewLabel->setWordWrap(true);
    m_lbCenterPreviewLabel->setText("Здесь будет\nпредпросмотр\nлука");
    // The central block is used only for the finished outfit preview.
    lbCenterLay->addWidget(m_lbCenterPreviewLabel, 1);
    m_lbCenterAddButton = new QPushButton("Финальный образ", centerPreview);
    m_lbCenterAddButton->setObjectName("LbSlotAction");
    m_lbCenterAddButton->setEnabled(false);
    m_lbCenterAddButton->setVisible(false);
    lbCenterLay->addWidget(m_lbCenterAddButton);
    centerPreview->raise();
    QTimer::singleShot(0, this, [this]() { positionLookbookCenterOverlay(); });

    editorBody->addWidget(m_lbEditorMain, 1);

    QFrame *detailsCard = new QFrame(lbEditHost);
    detailsCard->setObjectName("LbDetailsCard");
    detailsCard->setFixedWidth(250);
    QVBoxLayout *detailsLay = new QVBoxLayout(detailsCard);
    detailsLay->setContentsMargins(14, 14, 14, 14);
    detailsLay->setSpacing(10);
    QLabel *detailsTitle = new QLabel("Детали лукбука", detailsCard);
    detailsTitle->setObjectName("LbDetailsTitle");
    detailsLay->addWidget(detailsTitle);

    m_lbDetailsCostTitleLabel = new QLabel("Стоимость", detailsCard);
    m_lbDetailsCostTitleLabel->setObjectName("LbDetailsSub");
    detailsLay->addWidget(m_lbDetailsCostTitleLabel);
    m_lbDetailsCostLabel = new QLabel("Итого: 0 ₽", detailsCard);
    detailsLay->addWidget(m_lbDetailsCostLabel);

    QLabel *recTitle = new QLabel("Рекомендации", detailsCard);
    recTitle->setObjectName("LbDetailsSub");
    detailsLay->addWidget(recTitle);
    QHBoxLayout *recLay = new QHBoxLayout();

    auto makeMiniRec = [detailsCard](const QString &text) {
      QFrame *mini = new QFrame(detailsCard);
      mini->setObjectName("LbMiniRec");
      QVBoxLayout *ml = new QVBoxLayout(mini);
      ml->setContentsMargins(8, 8, 8, 8);
      ml->setSpacing(4);
      QLabel *box = new QLabel("фото", mini);
      box->setAlignment(Qt::AlignCenter);
      box->setFixedSize(64, 64);
      box->setStyleSheet("background:#FFFFFF;border:1px solid #E5DBD2;border-radius:10px;color:#A39990;");
      ml->addWidget(box, 0, Qt::AlignCenter);
      QLabel *tt = new QLabel(text, mini);
      tt->setWordWrap(true);
      tt->setAlignment(Qt::AlignCenter);
      tt->setStyleSheet("font-size:11px;color:#564E48;");
      ml->addWidget(tt);
      return mini;
    };
    recLay->addWidget(makeMiniRec("Сумка"));
    recLay->addWidget(makeMiniRec("Носки"));
    detailsLay->addLayout(recLay);
    detailsLay->addStretch();

    editorBody->addWidget(detailsCard, 0, Qt::AlignTop);
    lbEditLay->addLayout(editorBody, 1);
    m_lookbookStack->addWidget(m_lbProjectsPage);
    m_lookbookStack->addWidget(m_lbEditorPage);
    lbRootLay->addWidget(m_lookbookStack, 1);

    // The block is hidden by default.
    m_lookbookSection->setVisible(false);
    centralLayout->addWidget(m_lookbookSection, 1);


    // =================== Projects ===================
    m_projectsSection = new QWidget(central);
    m_projectsSection->setObjectName("ProjectsSection");
    m_projectsSection->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_projectsSection->setStyleSheet(
     "QWidget#ProjectsSection{background:#F4EFEA;}"
     "QLabel#ProjectsPageTitle{font-size:18px;font-weight:800;color:#1F1A17;background:transparent;border:none;}"
     "QToolButton#ProjectsRoundBtn{background:#FFFFFF;border:1px solid #E4DAD2;border-radius:16px;color:#2B2521;font-weight:900;}"
     "QToolButton#ProjectsRoundBtn:hover{background:#FBF7F3;}"
     "QLabel#ProjectsEmptyTitle{font-size:24px;font-weight:900;color:#1F1A17;background:transparent;border:none;}"
     "QLabel#ProjectsWhyTitle{font-size:16px;font-weight:800;color:#1F1A17;background:transparent;border:none;}"
     "QLabel#ProjectsDescription{font-size:13px;color:#3B342E;background:transparent;border:none;}"
     "QFrame#ProjectsPreviewCard{background:#FFFFFF;border:1px solid #E5DBD2;border-radius:14px;}"
     "QLabel#ProjectsPreviewTitle{font-size:13px;font-weight:800;color:#2B2521;background:transparent;border:none;}"
     "QLabel#ProjectsPreviewMeta{font-size:12px;color:#6B625A;background:transparent;border:none;}"
     "QPushButton#ProjectsPrimaryBtn{background:#FFFFFF;border:1px solid #D8CEC5;border-radius:8px;min-height:42px;padding:0 22px;color:#1F1A17;font-size:13px;font-weight:700;}"
     "QPushButton#ProjectsPrimaryBtn:hover{background:#FBF7F3;border:1px solid #CDBFB4;}"
     "QPushButton#ProjectsCreateBtn{background:#FFFFFF;border:1px solid #D8CEC5;border-radius:8px;min-height:42px;padding:0 22px;color:#1F1A17;font-size:13px;font-weight:800;}"
     "QPushButton#ProjectsCreateBtn:hover{background:#FBF7F3;border:1px solid #CDBFB4;}"
     "QListWidget#ProjectsList{background:#FFFFFF;border:1px solid #E5DBD2;border-radius:16px;padding:12px;font-size:14px;color:#1F1A17;}"
     "QListWidget#ProjectsList::item{background:#FBF8F5;border:1px solid #E8DED5;border-radius:12px;margin:5px;padding:12px;}"
     "QListWidget#ProjectsList::item:selected{background:#F2E7DD;border:1px solid #CFAE95;color:#1F1A17;}"
    );

    QVBoxLayout *prRoot = new QVBoxLayout(m_projectsSection);
    prRoot->setContentsMargins(32, 18, 32, 4);
    prRoot->setSpacing(14);

    QHBoxLayout *prTop = new QHBoxLayout();
    QLabel *prTitle = new QLabel("Проекты", m_projectsSection);
    prTitle->setObjectName("ProjectsPageTitle");

    QToolButton *prHelp = new QToolButton(m_projectsSection);
    prHelp->setObjectName("ProjectsRoundBtn");
    prHelp->setText("?");
    prHelp->setFixedSize(32, 32);
    prHelp->setCursor(Qt::PointingHandCursor);

    QToolButton *prAdd = new QToolButton(m_projectsSection);
    prAdd->setObjectName("ProjectsRoundBtn");
    prAdd->setText("+");
    prAdd->setFixedSize(34, 34);
    prAdd->setCursor(Qt::PointingHandCursor);
    prAdd->setStyleSheet("QToolButton#ProjectsRoundBtn{font-size:18px;background:#FFFFFF;border:1px solid #E4DAD2;border-radius:17px;color:#2B2521;font-weight:900;}");

    QLabel *createProjectHintTop = new QLabel("Чтобы создать проект, нажмите на +", m_projectsSection);
    createProjectHintTop->setStyleSheet("background:transparent;border:none;color:#7B7068;font-size:12px;font-weight:600;");

    prTop->addWidget(prTitle);
    prTop->addSpacing(10);
    prTop->addWidget(prHelp);
    prTop->addStretch();
    prTop->addWidget(createProjectHintTop);
    prTop->addSpacing(10);
    prTop->addWidget(prAdd);
    prRoot->addLayout(prTop);

    m_projectsEmptyPanel = new QWidget(m_projectsSection);
    QVBoxLayout *emptyLay = new QVBoxLayout(m_projectsEmptyPanel);
    emptyLay->setContentsMargins(0, 4, 0, 0);
    emptyLay->setSpacing(16);
    emptyLay->setAlignment(Qt::AlignHCenter);

    QLabel *emptyHeader = new QLabel(m_projectsEmptyPanel);
    emptyHeader->setFixedSize(520, 120);
    emptyHeader->setAlignment(Qt::AlignCenter);
    QPixmap projectsHeaderPix(":/images/assets/projects_empty_header.png");
    if (!projectsHeaderPix.isNull())
      emptyHeader->setPixmap(projectsHeaderPix.scaled(emptyHeader->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    emptyLay->addWidget(emptyHeader, 0, Qt::AlignHCenter);

    QLabel *emptyTitle = new QLabel("Создайте свой первый проект!", m_projectsEmptyPanel);
    emptyTitle->setObjectName("ProjectsEmptyTitle");
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLay->addWidget(emptyTitle, 0, Qt::AlignHCenter);

    QLabel *whyTitle = new QLabel("Для чего нужен проект?", m_projectsEmptyPanel);
    whyTitle->setObjectName("ProjectsWhyTitle");
    whyTitle->setAlignment(Qt::AlignCenter);
    emptyLay->addWidget(whyTitle, 0, Qt::AlignHCenter);

    QLabel *emptyDescription = new QLabel(
     "Проект — это ваша персональная мастерская стиля. Создавайте лукбуки для разных случаев,\n"
     "планируйте сезонные капсулы и сохраняйте идеи для будущих покупок.\n"
     "Это место, где ваши мысли об одежде превращаются в чёткие планы.",
      m_projectsEmptyPanel);
    emptyDescription->setObjectName("ProjectsDescription");
    emptyDescription->setAlignment(Qt::AlignCenter);
    emptyDescription->setWordWrap(true);
    emptyDescription->setMaximumWidth(740);
    emptyLay->addWidget(emptyDescription, 0, Qt::AlignHCenter);

    QHBoxLayout *sampleCards = new QHBoxLayout();
    sampleCards->setContentsMargins(0, 18, 0, 0);
    sampleCards->setSpacing(24);
    sampleCards->setAlignment(Qt::AlignHCenter);

    auto makeProjectSampleCard = [&](const QString &title, int itemCount, int startIndex) -> QFrame* {
      QFrame *sample = new QFrame(m_projectsEmptyPanel);
      sample->setObjectName("ProjectsPreviewCard");
      sample->setFixedSize(230, 170);

      QVBoxLayout *sampleLay = new QVBoxLayout(sample);
      sampleLay->setContentsMargins(10, 10, 10, 10);
      sampleLay->setSpacing(6);

      QWidget *thumbs = new QWidget(sample);
      thumbs->setStyleSheet("background:transparent;border:none;");
      QGridLayout *thumbsLay = new QGridLayout(thumbs);
      thumbsLay->setContentsMargins(0, 0, 0, 0);
      thumbsLay->setHorizontalSpacing(6);
      thumbsLay->setVerticalSpacing(6);

      for (int i = 0; i < 4; ++i) {
        QLabel *pic = new QLabel(thumbs);
        pic->setFixedSize(i == 0 ? 112 : 50, i == 0 ? 92 : 42);
        pic->setAlignment(Qt::AlignCenter);
        pic->setStyleSheet("background:#F8F3EE;border:1px solid #E8DED5;border-radius:8px;color:#B1A79F;font-size:11px;");
        if (!m_products.isEmpty()) {
          const int idx = (startIndex + i) % m_products.size();
          QPixmap px = cachedScaledPixmap(m_products[idx].imagePath, pic->size() - QSize(8, 8));
          if (!px.isNull())
            pic->setPixmap(px);
          else
            pic->setText("фото");
        } else {
          pic->setText("фото");
        }

        if (i == 0)
          thumbsLay->addWidget(pic, 0, 0, 2, 1);
        else
          thumbsLay->addWidget(pic, i - 1, 1);
      }

      sampleLay->addWidget(thumbs);

      QLabel *t = new QLabel(title, sample);
      t->setObjectName("ProjectsPreviewTitle");
      t->setWordWrap(true);
      sampleLay->addWidget(t);

      QLabel *meta = new QLabel(QString("%1 предмета").arg(itemCount), sample);
      meta->setObjectName("ProjectsPreviewMeta");
      sampleLay->addWidget(meta);

      return sample;
    };

    sampleCards->addWidget(makeProjectSampleCard("Проект: Осенний Лукбук", 3, 0));
    sampleCards->addWidget(makeProjectSampleCard("Проект: Базовый гардероб", 3, 2));
    sampleCards->addWidget(makeProjectSampleCard("Проект: Летний Набор", 2, 4));
    emptyLay->addLayout(sampleCards);

    QHBoxLayout *emptyActions = new QHBoxLayout();
    emptyActions->setContentsMargins(0, 18, 0, 0);
    emptyActions->setSpacing(18);
    emptyActions->setAlignment(Qt::AlignHCenter);

    QPushButton *onboardingBtn = new QPushButton("Посмотреть онбординг  ?", m_projectsEmptyPanel);
    onboardingBtn->setObjectName("ProjectsPrimaryBtn");
    onboardingBtn->setCursor(Qt::PointingHandCursor);

    QPushButton *createFirstProjectBtn = new QPushButton("+ Создать свой первый проект", m_projectsEmptyPanel);
    createFirstProjectBtn->setObjectName("ProjectsCreateBtn");
    createFirstProjectBtn->setCursor(Qt::PointingHandCursor);

    emptyActions->addWidget(onboardingBtn);
    emptyActions->addWidget(createFirstProjectBtn);
    emptyLay->addLayout(emptyActions);
    emptyLay->addStretch();

    prRoot->addWidget(m_projectsEmptyPanel, 1);

    m_projectsList = new QListWidget(m_projectsSection);
    m_projectsList->setObjectName("ProjectsList");
    m_projectsList->setVisible(false);
    m_projectsList->setSpacing(10);
    m_projectsList->setMinimumHeight(620);
    m_projectsList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_projectsList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_projectsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_projectsList->setSelectionMode(QAbstractItemView::SingleSelection);
    prRoot->addWidget(m_projectsList, 1);

    auto showProjectsOnboarding = [this]() {
      QDialog dlg(this);
      dlg.setObjectName("ProjectsOnboardingDialog");
      dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
      dlg.setAttribute(Qt::WA_TranslucentBackground, true);
      dlg.setModal(true);
      dlg.resize(this->size());
      dlg.move(this->mapToGlobal(QPoint(0, 0)));
      dlg.setStyleSheet(
       "QDialog#ProjectsOnboardingDialog{background:#00000000;}"
       "QFrame#ProjectsOnboardingDim{background:rgba(20,20,20,115);border:none;}"
       "QFrame#ProjectsOnboardingCard{background:#F7FBFF;border:1px solid #DDE7F0;border-radius:14px;}"
       "QLabel#ProjectsOnboardingTitle{font-size:22px;font-weight:800;color:#15171A;background:transparent;border:none;}"
       "QLabel#ProjectsOnboardingStepTitle{font-size:16px;font-weight:800;color:#111111;background:transparent;border:none;}"
       "QLabel#ProjectsOnboardingDescription{font-size:13px;color:#222222;background:transparent;border:none;}"
       "QLabel#ProjectsOnboardingCircle{background:#EEF4F9;border:2px solid #C9D5E0;border-radius:16px;color:#45627A;font-size:14px;font-weight:900;}"
       "QLabel#ProjectsOnboardingCircle[active='true']{background:#EAF6FF;border:2px solid #4C86B8;color:#2F6D9E;}"
       "QFrame#ProjectsOnboardingLine{background:#C9D5E0;border:none;min-height:3px;max-height:3px;}"
       "QLabel#ProjectsStepBadge{background:#4C86B8;color:white;border-radius:10px;padding:2px 8px;font-size:12px;font-weight:800;}"
       "QPushButton#ProjectsBackBtn{background:#DCE3EA;color:#8B949D;border:none;border-radius:8px;min-height:36px;padding:0 18px;font-weight:700;}"
       "QPushButton#ProjectsNextBtn{background:#4C86B8;color:white;border:2px solid #D9ECFA;border-radius:10px;min-height:38px;padding:0 20px;font-weight:800;}"
       "QPushButton#ProjectsNextBtn:hover{background:#3F79AA;}"
       "QPushButton#ProjectsCloseBtn{background:transparent;border:none;color:#111111;font-size:22px;font-weight:500;}"
      );

      QVBoxLayout *dialogRoot = new QVBoxLayout(&dlg);
      dialogRoot->setContentsMargins(0, 0, 0, 0);

      QFrame *dim = new QFrame(&dlg);
      dim->setObjectName("ProjectsOnboardingDim");
      QVBoxLayout *dimLay = new QVBoxLayout(dim);
      dimLay->setContentsMargins(0, 0, 0, 0);
      dimLay->setAlignment(Qt::AlignCenter);
      dialogRoot->addWidget(dim);

      QFrame *card = new QFrame(dim);
      card->setObjectName("ProjectsOnboardingCard");
      card->setFixedSize(690, 560);
      QVBoxLayout *cardLay = new QVBoxLayout(card);
      cardLay->setContentsMargins(28, 18, 28, 18);
      cardLay->setSpacing(14);

      QHBoxLayout *header = new QHBoxLayout();
      QLabel *title = new QLabel("Как создать идеальный гардероб", card);
      title->setObjectName("ProjectsOnboardingTitle");
      title->setAlignment(Qt::AlignCenter);
      header->addStretch();
      header->addWidget(title);
      header->addStretch();

      QPushButton *closeBtn = new RoundCloseButton(card);
      closeBtn->setObjectName("ProjectsCloseBtn");
      closeBtn->setFixedSize(32, 32);
      closeBtn->setCursor(Qt::PointingHandCursor);
      header->addWidget(closeBtn, 0, Qt::AlignRight);
      cardLay->addLayout(header);

      QHBoxLayout *progress = new QHBoxLayout();
      progress->setContentsMargins(58, 0, 58, 0);
      progress->setSpacing(0);
      QVector<QLabel*> circles;
      for (int i = 0; i < 4; ++i) {
        QLabel *c = new QLabel(card);
        c->setObjectName("ProjectsOnboardingCircle");
        c->setFixedSize(34, 34);
        c->setAlignment(Qt::AlignCenter);
        circles.push_back(c);
        progress->addWidget(c);

        if (i < 3) {
          QFrame *line = new QFrame(card);
          line->setObjectName("ProjectsOnboardingLine");
          progress->addWidget(line, 1, Qt::AlignVCenter);
        }
      }
      cardLay->addLayout(progress);

      QLabel *badge = new QLabel("1/4", card);
      badge->setObjectName("ProjectsStepBadge");
      badge->setFixedWidth(42);
      badge->setAlignment(Qt::AlignCenter);
      cardLay->addWidget(badge, 0, Qt::AlignLeft);

      QStackedWidget *stepStack = new QStackedWidget(card);
      const QStringList stepTitles = {
       "1. Вдохновитесь",
       "2. Спланируйте капсулу",
       "3. Подберите вещи",
       "4. Сохраните проект"
      };
      const QStringList stepDescriptions = {
       "Создайте проект для любого случая: работа, отпуск или новый сезон. Это ваше пространство для идей.",
       "Разделите гардероб на понятные задачи: верх, низ, обувь и аксессуары. Так легче собрать цельный образ.",
       "Добавляйте товары из каталога и проверяйте, как они сочетаются между собой по цвету, стилю и сезону.",
       "Сохраните проект и возвращайтесь к нему позже. Проект поможет превратить идеи в готовый план покупок."
      };
      const QStringList stepIcons = {"1", "2", "3", "4"};
      const QStringList stepImages = {
       ":/images/assets/projects_onboarding_collage.png",
       ":/images/assets/projects_onboarding_capsule.png",
       ":/images/assets/projects_onboarding_capsule.png",
       ":/images/assets/projects_onboarding_collage.png"
      };

      for (int i = 0; i < 4; ++i) {
        QWidget *page = new QWidget(stepStack);
        page->setStyleSheet("background:transparent;border:none;");
        QHBoxLayout *pageLay = new QHBoxLayout(page);
        pageLay->setContentsMargins(36, 12, 10, 0);
        pageLay->setSpacing(36);

        QLabel *bigIcon = new QLabel(page);
        bigIcon->setFixedSize(170, 210);
        bigIcon->setAlignment(Qt::AlignCenter);
        if (i == 0) {
          QPixmap bulb(":/images/assets/projects_onboarding_bulb.png");
          if (!bulb.isNull())
            bigIcon->setPixmap(bulb.scaled(bigIcon->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
          else
            bigIcon->setText("Идея");
        } else {
          bigIcon->setText(stepIcons[i]);
          bigIcon->setStyleSheet("font-size:74px;color:#3D4A55;");
        }
        pageLay->addWidget(bigIcon, 0, Qt::AlignVCenter);

        QVBoxLayout *right = new QVBoxLayout();
        right->setSpacing(10);
        QLabel *st = new QLabel(stepTitles[i], page);
        st->setObjectName("ProjectsOnboardingStepTitle");
        QLabel *sd = new QLabel(stepDescriptions[i], page);
        sd->setObjectName("ProjectsOnboardingDescription");
        sd->setWordWrap(true);
        sd->setMaximumWidth(360);

        QLabel *visual = new QLabel(page);
        visual->setFixedSize(360, 220);
        visual->setAlignment(Qt::AlignCenter);
        visual->setStyleSheet("background:#FFFFFF;border:1px solid #DDE7F0;border-radius:12px;");
        QPixmap v(stepImages[i]);
        if (!v.isNull())
          visual->setPixmap(v.scaled(visual->size() - QSize(10, 10), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        else
          visual->setText("визуализация");

        right->addWidget(st);
        right->addWidget(sd);
        right->addWidget(visual);
        right->addStretch();
        pageLay->addLayout(right, 1);

        stepStack->addWidget(page);
      }

      cardLay->addWidget(stepStack, 1);

      QHBoxLayout *actions = new QHBoxLayout();
      QPushButton *backBtn = new QPushButton("← Назад", card);
      backBtn->setObjectName("ProjectsBackBtn");
      backBtn->setCursor(Qt::PointingHandCursor);

      QPushButton *nextBtn = new QPushButton("Далее →", card);
      nextBtn->setObjectName("ProjectsNextBtn");
      nextBtn->setCursor(Qt::PointingHandCursor);

      actions->addWidget(backBtn);
      actions->addStretch();
      actions->addWidget(nextBtn);
      cardLay->addLayout(actions);

      int step = 0;
      auto updateStep = [&]() {
        stepStack->setCurrentIndex(step);
        badge->setText(QString("%1/4").arg(step + 1));
        backBtn->setEnabled(step > 0);
        backBtn->setStyleSheet(step > 0
          ? "QPushButton#ProjectsBackBtn{background:#E8EEF4;color:#45515C;border:none;border-radius:8px;min-height:36px;padding:0 18px;font-weight:700;}QPushButton#ProjectsBackBtn:hover{background:#DCE6EF;}"
          : "QPushButton#ProjectsBackBtn{background:#DCE3EA;color:#8B949D;border:none;border-radius:8px;min-height:36px;padding:0 18px;font-weight:700;}");
        nextBtn->setText(step == 3 ? "Завершить" : "Далее →");
        for (int i = 0; i < circles.size(); ++i) {
          circles[i]->setProperty("active", i == step);
          circles[i]->setText(i == step ? QString::number(i + 1) : "");
          circles[i]->style()->unpolish(circles[i]);
          circles[i]->style()->polish(circles[i]);
        }
      };

      updateStep();

      connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
      connect(backBtn, &QPushButton::clicked, &dlg, [&]() {
        if (step > 0) {
          --step;
          updateStep();
        }
      });
      connect(nextBtn, &QPushButton::clicked, &dlg, [&]() {
        if (step < 3) {
          ++step;
          updateStep();
        } else {
          dlg.accept();
        }
      });

      dimLay->addWidget(card, 0, Qt::AlignCenter);
      dlg.exec();
    };

    auto openProjectDetailsDialog = [this](int projectId) {
      QDialog dlg(this);
      dlg.setObjectName("ProjectDetailsDialog");
      dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
      dlg.setAttribute(Qt::WA_TranslucentBackground, true);
      dlg.setModal(true);
      dlg.resize(620, 520);
      dlg.setStyleSheet(
       "QDialog#ProjectDetailsDialog{background:#00000000;}"
       "QFrame#ProjectDetailsCard{background:#F4EFEA;border:1px solid #E1D6CE;border-radius:18px;}"
       "QLabel#ProjectDetailsTitle{font-size:22px;font-weight:900;color:#1F1A17;background:transparent;border:none;}"
       "QLabel#ProjectDetailsTotal{font-size:13px;color:#6D625A;background:transparent;border:none;}"
       "QListWidget#ProjectDetailsItems{background:#FFFFFF;border:1px solid #E5DBD2;border-radius:12px;padding:8px;color:#1F1A17;}"
       "QListWidget#ProjectDetailsItems::item{padding:10px;border-bottom:1px solid #F0E7DF;}"
       "QListWidget#ProjectDetailsItems::item:selected{background:#F2E7DD;color:#1F1A17;}"
       "QPushButton#ProjectPrimary{background:#6F4B37;color:white;border:none;border-radius:10px;min-height:38px;padding:0 14px;font-weight:800;}"
       "QPushButton#ProjectPrimary:hover{background:#5F3E2D;}"
       "QPushButton#ProjectSecondary{background:#FFFFFF;color:#3A342F;border:1px solid #DDD2CA;border-radius:10px;min-height:38px;padding:0 14px;font-weight:800;}"
       "QPushButton#ProjectSecondary:hover{background:#FBF7F3;}"
       "QPushButton#ProjectX{background:transparent;border:none;color:#1F1A17;font-size:22px;font-weight:900;}"
      );

      QVBoxLayout *outer = new QVBoxLayout(&dlg);
      outer->setContentsMargins(0, 0, 0, 0);
      QFrame *card = new QFrame(&dlg);
      card->setObjectName("ProjectDetailsCard");
      outer->addWidget(card);

      QVBoxLayout *root = new QVBoxLayout(card);
      root->setContentsMargins(22, 18, 22, 18);
      root->setSpacing(12);

      QHBoxLayout *header = new QHBoxLayout();
      QLabel *title = new QLabel("Вещи проекта", card);
      title->setObjectName("ProjectDetailsTitle");
      header->addWidget(title);
      header->addStretch();
      QPushButton *xBtn = new RoundCloseButton(card);
      xBtn->setObjectName("ProjectX");
      xBtn->setFixedSize(32, 32);
      xBtn->setCursor(Qt::PointingHandCursor);
      header->addWidget(xBtn);
      root->addLayout(header);

      QListWidget *itemsList = new QListWidget(card);
      itemsList->setObjectName("ProjectDetailsItems");
      itemsList->setSpacing(4);
      root->addWidget(itemsList, 1);

      QLabel *totalLbl = new QLabel(card);
      totalLbl->setObjectName("ProjectDetailsTotal");
      root->addWidget(totalLbl);

      auto reload = [&]() {
        itemsList->clear();

        double total = 0.0;
        QSqlQuery q;
        q.prepare(
         "SELECT p.id, p.name, pi.qty, p.price "
         "FROM project_items pi "
         "JOIN products p ON p.id = pi.product_id "
         "WHERE pi.project_id = ? "
         "ORDER BY p.name");
        q.addBindValue(projectId);
        q.exec();

        while (q.next()) {
          const int productId = q.value(0).toInt();
          const QString name = q.value(1).toString();
          const int qty = q.value(2).toInt();
          const double price = q.value(3).toDouble();
          total += price * qty;

          QListWidgetItem *row = new QListWidgetItem(QString("%1  ×%2  •  %3 ₽").arg(name).arg(qty).arg(price, 0, 'f', 2), itemsList);
          row->setData(Qt::UserRole, productId);
        }

        if (itemsList->count() == 0)
          itemsList->addItem("Пока нет вещей. Нажмите «Добавить вещь».");

        totalLbl->setText(QString("Итого по проекту: %1 ₽").arg(total, 0, 'f', 2));
      };

      QHBoxLayout *btns = new QHBoxLayout();
      QPushButton *btnAdd = new QPushButton("Добавить вещь", card);
      btnAdd->setObjectName("ProjectPrimary");
      QPushButton *btnDel = new QPushButton("Удалить вещь", card);
      btnDel->setObjectName("ProjectSecondary");
      QPushButton *btnClose = new QPushButton("Закрыть", card);
      btnClose->setObjectName("ProjectSecondary");
      btns->addWidget(btnAdd);
      btns->addWidget(btnDel);
      btns->addStretch();
      btns->addWidget(btnClose);
      root->addLayout(btns);

      connect(xBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
      connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);

      connect(btnAdd, &QPushButton::clicked, &dlg, [&]() {
        QStringList names;
        QList<int> ids;

        QSqlQuery p("SELECT id, name FROM products ORDER BY name");
        while (p.next()) {
          ids << p.value(0).toInt();
          names << p.value(1).toString();
        }

        QDialog pickDlg(&dlg);
        pickDlg.setObjectName("ProjectPickProductDialog");
        pickDlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        pickDlg.setAttribute(Qt::WA_TranslucentBackground, true);
        pickDlg.setModal(true);
        pickDlg.resize(420, 230);
        pickDlg.setStyleSheet(
         "QDialog#ProjectPickProductDialog{background:#00000000;}"
         "QFrame#ProjectPickCard{background:#F4EFEA;border:1px solid #E1D6CE;border-radius:16px;}"
         "QLabel{background:transparent;border:none;color:#1F1A17;}"
         "QComboBox{background:#FFFFFF;border:1px solid #D8CEC5;border-radius:10px;min-height:36px;padding:0 10px;}"
         "QPushButton#ProjectPrimary{background:#6F4B37;color:white;border:none;border-radius:10px;min-height:38px;padding:0 14px;font-weight:800;}"
         "QPushButton#ProjectSecondary{background:#FFFFFF;color:#3A342F;border:1px solid #DDD2CA;border-radius:10px;min-height:38px;padding:0 14px;font-weight:800;}"
        );

        QVBoxLayout *pickOuter = new QVBoxLayout(&pickDlg);
        pickOuter->setContentsMargins(0, 0, 0, 0);
        QFrame *pickCard = new QFrame(&pickDlg);
        pickCard->setObjectName("ProjectPickCard");
        pickOuter->addWidget(pickCard);

        QVBoxLayout *pickRoot = new QVBoxLayout(pickCard);
        pickRoot->setContentsMargins(20, 18, 20, 18);
        pickRoot->setSpacing(12);

        QLabel *pickTitle = new QLabel("Добавить вещь в проект", pickCard);
        pickTitle->setStyleSheet("font-size:18px;font-weight:900;");
        pickRoot->addWidget(pickTitle);

        QComboBox *box = new QComboBox(pickCard);
        box->addItems(names);
        pickRoot->addWidget(box);

        QLabel *empty = new QLabel(names.isEmpty() ? "В каталоге пока нет товаров." : "", pickCard);
        empty->setStyleSheet("color:#8A8178;font-size:12px;");
        pickRoot->addWidget(empty);

        QHBoxLayout *pickButtons = new QHBoxLayout();
        pickButtons->addStretch();
        QPushButton *cancel = new QPushButton("Отмена", pickCard);
        cancel->setObjectName("ProjectSecondary");
        QPushButton *add = new QPushButton("Добавить", pickCard);
        add->setObjectName("ProjectPrimary");
        add->setEnabled(!names.isEmpty());
        pickButtons->addWidget(cancel);
        pickButtons->addWidget(add);
        pickRoot->addLayout(pickButtons);

        connect(cancel, &QPushButton::clicked, &pickDlg, &QDialog::reject);
        connect(add, &QPushButton::clicked, &pickDlg, [&]() {
          const int idx = box->currentIndex();
          if (idx < 0 || idx >= ids.size())
            return;

          const int productId = ids[idx];

          QSqlQuery upd;
          upd.prepare("UPDATE project_items SET qty = qty + 1 WHERE project_id = ? AND product_id = ?");
          upd.addBindValue(projectId);
          upd.addBindValue(productId);
          upd.exec();

          if (upd.numRowsAffected() == 0) {
            QSqlQuery ins;
            ins.prepare("INSERT INTO project_items(project_id, product_id, qty) VALUES(?, ?, 1)");
            ins.addBindValue(projectId);
            ins.addBindValue(productId);
            ins.exec();
          }

          pickDlg.accept();
        });

        if (pickDlg.exec() == QDialog::Accepted) {
          reload();
          refreshProjectsUI();
        }
      });

      connect(btnDel, &QPushButton::clicked, &dlg, [&]() {
        QListWidgetItem *cur = itemsList->currentItem();
        if (!cur || !cur->data(Qt::UserRole).isValid())
          return;

        const int productId = cur->data(Qt::UserRole).toInt();

        QSqlQuery q;
        q.prepare("SELECT qty FROM project_items WHERE project_id=? AND product_id=?");
        q.addBindValue(projectId);
        q.addBindValue(productId);
        q.exec();
        if (!q.next())
          return;

        const int qty = q.value(0).toInt();
        if (qty > 1) {
          QSqlQuery dec;
          dec.prepare("UPDATE project_items SET qty = qty - 1 WHERE project_id=? AND product_id=?");
          dec.addBindValue(projectId);
          dec.addBindValue(productId);
          dec.exec();
        } else {
          QSqlQuery del;
          del.prepare("DELETE FROM project_items WHERE project_id=? AND product_id=?");
          del.addBindValue(projectId);
          del.addBindValue(productId);
          del.exec();
        }

        reload();
        refreshProjectsUI();
      });

      reload();
      dlg.exec();
    };

    connect(m_projectsList, &QListWidget::itemDoubleClicked, this,
        [openProjectDetailsDialog](QListWidgetItem *it) {
          if (!it)
            return;
          openProjectDetailsDialog(it->data(Qt::UserRole).toInt());
        });

    m_projectsSection->setVisible(false);
    centralLayout->addWidget(m_projectsSection, 1);

    connect(prHelp, &QToolButton::clicked, this, showProjectsOnboarding);
    connect(onboardingBtn, &QPushButton::clicked, this, showProjectsOnboarding);

    connect(prAdd, &QToolButton::clicked, this, [this]() {
      addProjectFlow();
    });
    connect(createFirstProjectBtn, &QPushButton::clicked, this, [this]() {
      addProjectFlow();
    });


    // Lookbook section buttons.
    connect(lbCreateBtn, &QPushButton::clicked, this, [this]() {
      createLookbookProject();
    });
    connect(m_lbSearchEdit, &QLineEdit::textChanged, this, [this](const QString &) {
      refreshLookbookProjectsList();
    });
    connect(m_lbSortBox, &QComboBox::currentTextChanged, this, [this](const QString &) {
      refreshLookbookProjectsList();
    });
    connect(lbBackBtn, &QToolButton::clicked, this, [this]() {
      showLookbookProjects();
    });
    connect(lbQuickAddBtn, &QToolButton::clicked, this, [this, lbQuickAddBtn]() {
      openLookbookAddMenu(lbQuickAddBtn);
    });
    connect(lbSaveBtn, &QPushButton::clicked, this, [this]() {
      saveCurrentLookbookTitle();
      showLookbookInfo("Лукбук", "Лукбук сохранён.");
    });
    connect(lbShareBtn, &QPushButton::clicked, this, [this]() {
      shareCurrentLookbook();
    });
    connect(lbSettingsBtn, &QPushButton::clicked, this, [this]() {
      showLookbookSettingsDialog();
    });
    connect(lbEditStructureBtn, &QPushButton::clicked, this, [this]() {
      showLookbookInfo("Структура лукбука", "Сейчас доступны 4 секции: головной убор, верх, низ и обувь.");
    });
    connect(m_lbProjectTitleEdit, &QLineEdit::editingFinished, this, [this]() {
      saveCurrentLookbookTitle();
    });


    // Open the user-facing window with a short order summary.
    auto showOrderDetails = [this](int orderId) {
      QSqlDatabase db = QSqlDatabase::database();
      if (!db.isOpen())
        return;

      auto parseDateTime = [](const QString &raw) {
        QString s = raw.trimmed();
        QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
        if (!dt.isValid())
          dt = QDateTime::fromString(s, "yyyy-MM-dd HH:mm");
        if (!dt.isValid()) {
          QDate d = QDate::fromString(s.left(10), Qt::ISODate);
          if (d.isValid())
            dt = QDateTime(d, QTime(0, 0));
        }
        return dt;
      };

      auto formatDateTime = [&](const QString &raw) {
        const QDateTime dt = parseDateTime(raw);
        if (dt.isValid())
          return dt.time().hour() == 0 && dt.time().minute() == 0
            ? dt.date().toString("dd.MM.yyyy")
            : dt.toString("dd.MM.yyyy, HH:mm");
        return raw.trimmed().isEmpty() ? QString("Дата не указана") : raw.trimmed();
      };

      auto normalizeStatus = [](QString s) {
        s = s.trimmed().toLower();
        s.replace("ё", "е");
        return s;
      };

      auto statusStyle = [&](const QString &status) {
        const QString n = normalizeStatus(status);
        if (n == "оформлен") return QStringList{"Оформлен", "#EFE7DF", "#6B5444", "#D8C9BE"};
        if (n == "оплачен") return QStringList{"Оплачен", "#E5F4E8", "#2E7D46", "#B7DFC0"};
        if (n.contains("формир")) return QStringList{"Формируется", "#FFF1D7", "#9A641F", "#F0D09B"};
        if (n == "собран") return QStringList{"Собран", "#EAE6FF", "#5B4BB2", "#C9C1F4"};
        if (n.contains("ожидает отправ")) return QStringList{"Ожидает отправки", "#E5EDFF", "#325EA8", "#BFD0F5"};
        if (n.contains("в пути") || n.contains("отправ")) return QStringList{"В пути", "#DCEEFF", "#2D6FA8", "#BBD8F3"};
        if (n.contains("достав")) return QStringList{"Доставлен", "#DDF4E3", "#2E7D46", "#B7DFC0"};
        if (n.contains("отмен")) return QStringList{"Отменён", "#F9DCDD", "#B64B53", "#E8B5BB"};
        if (n.contains("возврат")) return QStringList{"↩ Возврат", "#F4E1F4", "#8C4A86", "#D8B7D8"};
        return QStringList{"◔ В обработке", "#EEE7E1", "#5F5750", "#D8CBC0"};
      };

      auto paymentText = [](const QString &raw) {
        const QString v = raw.trimmed();
        const QString low = v.toLower();
        if (v.isEmpty()) return QString("Не указан");
        if (low.contains("карта")) return QString::fromUtf8("") + v;
        if (low.contains("сбп"))  return QString::fromUtf8(" ") + v;
        if (low.contains("нал"))  return QString::fromUtf8(" ") + v;
        return v;
      };

      QSqlQuery orderQ(db);
      orderQ.prepare(R"(
        SELECT
          o.id,
          o.created_at,
          o.status,
          (SELECT IFNULL(SUM(oi.quantity),0) FROM order_items oi WHERE oi.order_id=o.id) AS items_count,
          IFNULL(o.payment_method, '') AS payment_method,
          o.total
        FROM orders o
        WHERE o.id = :id AND o.username = :u
        LIMIT 1
      )");
      orderQ.bindValue(":id", orderId);
      orderQ.bindValue(":u", m_username);
      if (!orderQ.exec() || !orderQ.next())
        return;

      const QString orderNumber = QString("#%1").arg(orderQ.value(0).toInt());
      const QString orderDate = formatDateTime(orderQ.value(1).toString());
      const QString orderStatus = orderQ.value(2).toString().trimmed().isEmpty()
        ? QString("В обработке")
        : orderQ.value(2).toString().trimmed();
      const int itemsCount = orderQ.value(3).toInt();
      const QString orderPayment = paymentText(orderQ.value(4).toString());
      const double orderTotal = orderQ.value(5).toDouble();

      struct OrderLine
      {
        QString name;
        QString meta;
        int quantity = 0;
        double price = 0.0;
      };

      QVector<OrderLine> lines;
      QSqlQuery itemsQ(db);
      itemsQ.prepare(R"(
        SELECT
          IFNULL(p.name, 'Товар удалён') AS name,
          IFNULL(p.category, '') AS category,
          IFNULL(p.color, '') AS color,
          oi.quantity,
          IFNULL(oi.price, IFNULL(p.price, 0)) AS price
        FROM order_items oi
        LEFT JOIN products p ON p.id = oi.product_id
        WHERE oi.order_id = :id
        ORDER BY oi.id ASC
      )");
      itemsQ.bindValue(":id", orderId);
      if (itemsQ.exec()) {
        while (itemsQ.next()) {
          OrderLine line;
          line.name = itemsQ.value(0).toString();
          const QString category = itemsQ.value(1).toString().trimmed();
          const QString color = itemsQ.value(2).toString().trimmed();
          QStringList metaParts;
          if (!category.isEmpty()) metaParts << category;
          if (!color.isEmpty()) metaParts << color;
          line.meta = metaParts.join(" • ");
          line.quantity = itemsQ.value(3).toInt();
          line.price = itemsQ.value(4).toDouble();
          lines.push_back(line);
        }
      }

      QDialog dlg(this);
      dlg.setObjectName("OrderDetailsDialog");
      dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
      dlg.setAttribute(Qt::WA_TranslucentBackground, true);
      dlg.setModal(true);
      dlg.resize(560, 520);
      dlg.setStyleSheet(
       "QDialog#OrderDetailsDialog{background:#00000000;}"
       "QFrame#OrderDetailsCard{background:#FFFFFF;border:1px solid #E7DDD4;border-radius:22px;}"
       "QLabel#OrderDialogTitle{font-size:22px;font-weight:900;color:#141414;background:transparent;border:none;}"
       "QLabel#OrderDialogSub{font-size:13px;color:#7A7068;background:transparent;border:none;}"
       "QLabel#OrderInfoCaption{font-size:11px;font-weight:800;color:#8A8178;background:transparent;border:none;text-transform:uppercase;}"
       "QLabel#OrderInfoValue{font-size:14px;font-weight:800;color:#1F1F1F;background:transparent;border:none;}"
       "QLabel#OrderItemName{font-size:14px;font-weight:800;color:#1F1F1F;background:transparent;border:none;}"
       "QLabel#OrderItemMeta{font-size:12px;color:#7A7068;background:transparent;border:none;}"
       "QLabel#OrderSectionTitle{font-size:13px;font-weight:900;color:#2E2925;background:transparent;border:none;}"
       "QFrame#OrderInfoBox{background:#FAF7F4;border:1px solid #EEE5DD;border-radius:14px;}"
       "QFrame#OrderItemRow{background:#FFFFFF;border:1px solid #EEE5DD;border-radius:14px;}"
       "QPushButton#OrderCloseBtn{background:#6F4B37;color:white;border:none;border-radius:12px;min-height:40px;padding:0 22px;font-weight:900;}"
       "QPushButton#OrderCloseBtn:hover{background:#5F3E2D;}"
       "QPushButton#OrderXBtn{background:transparent;color:#1F1A17;border:none;border-radius:16px;font-family:'Arial';font-size:18px;font-weight:900;min-width:32px;min-height:32px;max-width:32px;max-height:32px;padding:0;}"
       "QPushButton#OrderXBtn:hover{background:#E8DDD4;color:#14100D;}QPushButton#OrderXBtn:pressed{background:#DED1C6;color:#14100D;}"
       "QScrollArea{background:transparent;border:none;}"
      );

      QVBoxLayout *outer = new QVBoxLayout(&dlg);
      outer->setContentsMargins(12, 12, 12, 12);
      QFrame *card = new QFrame(&dlg);
      card->setObjectName("OrderDetailsCard");
      outer->addWidget(card);

      QVBoxLayout *root = new QVBoxLayout(card);
      root->setContentsMargins(24, 22, 24, 22);
      root->setSpacing(16);

      QHBoxLayout *head = new QHBoxLayout();
      QVBoxLayout *headText = new QVBoxLayout();
      headText->setSpacing(3);
      QLabel *title = new QLabel("Заказ " + orderNumber, card);
      title->setObjectName("OrderDialogTitle");
      QLabel *sub = new QLabel("Краткая информация о заказе и составе покупки", card);
      sub->setObjectName("OrderDialogSub");
      headText->addWidget(title);
      headText->addWidget(sub);
      QPushButton *xBtn = new RoundCloseButton(card);
      xBtn->setObjectName("OrderXBtn");
      xBtn->setFixedSize(32, 32);
      xBtn->setCursor(Qt::PointingHandCursor);
      head->addLayout(headText, 1);
      head->addWidget(xBtn, 0, Qt::AlignTop);
      root->addLayout(head);

      QFrame *infoBox = new QFrame(card);
      infoBox->setObjectName("OrderInfoBox");
      QGridLayout *infoGrid = new QGridLayout(infoBox);
      infoGrid->setContentsMargins(16, 14, 16, 14);
      infoGrid->setHorizontalSpacing(18);
      infoGrid->setVerticalSpacing(10);

      auto addInfo = [&](int row, int col, const QString &caption, QWidget *valueWidget) {
        QVBoxLayout *box = new QVBoxLayout();
        box->setSpacing(3);
        QLabel *cap = new QLabel(caption, infoBox);
        cap->setObjectName("OrderInfoCaption");
        box->addWidget(cap);
        box->addWidget(valueWidget);
        infoGrid->addLayout(box, row, col);
      };
      auto makeValue = [&](const QString &text) {
        QLabel *lab = new QLabel(text, infoBox);
        lab->setObjectName("OrderInfoValue");
        lab->setWordWrap(true);
        return lab;
      };

      const QStringList st = statusStyle(orderStatus);
      QLabel *statusChip = new QLabel(st.value(0), infoBox);
      statusChip->setAlignment(Qt::AlignCenter);
      statusChip->setMinimumHeight(28);
      statusChip->setStyleSheet(QString("background:%1;color:%2;border:1px solid %3;border-radius:10px;padding:0 12px;font-weight:800;").arg(st.value(1), st.value(2), st.value(3, QString("transparent"))));

      addInfo(0, 0, "Дата", makeValue(orderDate));
      addInfo(0, 1, "Статус", statusChip);
      addInfo(1, 0, "Оплата", makeValue(orderPayment));
      addInfo(1, 1, "Сумма", makeValue(QString("%1 ₽").arg(QString::number(orderTotal, 'f', 0))));
      addInfo(2, 0, "Товары", makeValue(QString("%1 шт.").arg(itemsCount)));
      addInfo(2, 1, "Номер", makeValue(orderNumber));
      root->addWidget(infoBox);

      QLabel *itemsTitle = new QLabel("Состав заказа", card);
      itemsTitle->setObjectName("OrderSectionTitle");
      root->addWidget(itemsTitle);

      QScrollArea *scroll = new QScrollArea(card);
      scroll->setWidgetResizable(true);
      scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      scroll->setMinimumHeight(150);
      QWidget *body = new QWidget(scroll);
      body->setStyleSheet("background:transparent;border:none;");
      QVBoxLayout *bodyLay = new QVBoxLayout(body);
      bodyLay->setContentsMargins(0, 0, 0, 0);
      bodyLay->setSpacing(8);

      if (lines.isEmpty()) {
        QLabel *empty = new QLabel("Состав заказа недоступен.", body);
        empty->setObjectName("OrderDialogSub");
        empty->setAlignment(Qt::AlignCenter);
        empty->setMinimumHeight(90);
        bodyLay->addWidget(empty);
      } else {
        for (const OrderLine &line : lines) {
          QFrame *row = new QFrame(body);
          row->setObjectName("OrderItemRow");
          QHBoxLayout *rowLay = new QHBoxLayout(row);
          rowLay->setContentsMargins(14, 10, 14, 10);
          rowLay->setSpacing(10);

          QVBoxLayout *textLay = new QVBoxLayout();
          textLay->setSpacing(3);
          QLabel *name = new QLabel(line.name, row);
          name->setObjectName("OrderItemName");
          name->setWordWrap(true);
          QLabel *meta = new QLabel(line.meta.isEmpty() ? QString("Количество: %1 шт.").arg(line.quantity)
                                 : QString("%1 • %2 шт.").arg(line.meta).arg(line.quantity), row);
          meta->setObjectName("OrderItemMeta");
          textLay->addWidget(name);
          textLay->addWidget(meta);

          QLabel *price = new QLabel(QString("%1 ₽").arg(QString::number(line.price * qMax(1, line.quantity), 'f', 0)), row);
          price->setObjectName("OrderInfoValue");
          price->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
          price->setMinimumWidth(92);

          rowLay->addLayout(textLay, 1);
          rowLay->addWidget(price);
          bodyLay->addWidget(row);
        }
      }
      bodyLay->addStretch();
      scroll->setWidget(body);
      root->addWidget(scroll, 1);

      QHBoxLayout *buttons = new QHBoxLayout();
      buttons->addStretch();
      QPushButton *closeBtn = new QPushButton("Закрыть", card);
      closeBtn->setObjectName("OrderCloseBtn");
      closeBtn->setCursor(Qt::PointingHandCursor);
      buttons->addWidget(closeBtn);
      root->addLayout(buttons);

      connect(xBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
      connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
      dlg.exec();
    };

    auto refreshOrders = [this,
               ordersSearchEdit,
               ordersDatePresetBox,
               ordersStatusQuickBox,
               ordersDateFromEdit,
               ordersDateToEdit,
               statusDeliveredCheck,
               statusProcessingCheck,
               statusSentCheck,
               statusCanceledCheck,
               ordersPaymentBox,
               ordersPriceMinSpin,
               ordersPriceMaxSpin,
               ordersItemsMinSpin,
               ordersCountLabel,
               ordersPagerLabel,
               showOrderDetails]() {
      QSqlDatabase db = QSqlDatabase::database();
      if (!db.isOpen()) return;

      auto normalizeStatus = [](QString s) {
        s = s.trimmed().toLower();
        s.replace("ё", "е");
        return s;
      };
      auto statusGroup = [&](const QString &status) {
        const QString n = normalizeStatus(status);
        if (n.contains("достав")) return QString("delivered");
        if (n.contains("в пути") || n.contains("отправ")) return QString("sent");
        if (n.contains("отмен") || n.contains("возврат")) return QString("canceled");
        return QString("processing");
      };
      auto parseOrderDate = [](const QString &raw) {
        QString s = raw.trimmed();
        QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
        if (dt.isValid()) return dt.date();
        if (s.contains('T')) s = s.left(10);
        if (s.contains(' ')) s = s.left(10);
        QDate d = QDate::fromString(s, Qt::ISODate);
        if (!d.isValid()) d = QDate::fromString(s, "yyyy-MM-dd");
        if (!d.isValid()) d = QDate::fromString(s, "dd.MM.yyyy");
        return d;
      };
      auto formatDate = [&](const QString &raw) {
        QDate d = parseOrderDate(raw);
        if (d.isValid()) return d.toString("dd.MM.yyyy");
        return raw;
      };
      auto paymentText = [](const QString &raw) {
        const QString v = raw.trimmed();
        const QString low = v.toLower();
        if (low.contains("карта")) return QString::fromUtf8("") + v;
        if (low.contains("сбп"))  return QString::fromUtf8(" ") + v;
        if (low.contains("нал"))  return QString::fromUtf8(" ") + v;
        return v;
      };
      auto makeTextItem = [](const QString &text, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
        auto *item = new QTableWidgetItem(text);
        item->setTextAlignment(align);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable & ~Qt::ItemIsSelectable);
        return item;
      };
      auto makeStatusChip = [&](const QString &status) {
        const QString n = normalizeStatus(status);
        QString bg = "#EEE7E1";
        QString fg = "#5F5750";
        QString br = "#D8CBC0";
        QString text = "◔ В обработке";
        if (n == "оформлен") {
          bg = "#EFE7DF"; fg = "#6B5444"; br = "#D8C9BE"; text = "Оформлен";
        } else if (n == "оплачен") {
          bg = "#E5F4E8"; fg = "#2E7D46"; br = "#B7DFC0"; text = "Оплачен";
        } else if (n.contains("формир")) {
          bg = "#FFF1D7"; fg = "#9A641F"; br = "#F0D09B"; text = "Формируется";
        } else if (n == "собран") {
          bg = "#EAE6FF"; fg = "#5B4BB2"; br = "#C9C1F4"; text = "Собран";
        } else if (n.contains("ожидает отправ")) {
          bg = "#E5EDFF"; fg = "#325EA8"; br = "#BFD0F5"; text = "Ожидает отправки";
        } else if (n.contains("в пути") || n.contains("отправ")) {
          bg = "#DCEEFF"; fg = "#2D6FA8"; br = "#BBD8F3"; text = "В пути";
        } else if (n.contains("достав")) {
          bg = "#DDF4E3"; fg = "#2E7D46"; br = "#B7DFC0"; text = "Доставлен";
        } else if (n.contains("отмен")) {
          bg = "#F9DCDD"; fg = "#B64B53"; br = "#E8B5BB"; text = "Отменён";
        } else if (n.contains("возврат")) {
          bg = "#F4E1F4"; fg = "#8C4A86"; br = "#D8B7D8"; text = "↩ Возврат";
        }
        QLabel *lab = new QLabel(text);
        lab->setAlignment(Qt::AlignCenter);
        lab->setMinimumHeight(28);
        lab->setStyleSheet(QString("background:%1;color:%2;border:1px solid %3;border-radius:10px;padding:0 12px;font-weight:700;").arg(bg, fg, br));
        QWidget *wrap = new QWidget();
        QHBoxLayout *lay = new QHBoxLayout(wrap);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(lab, 0, Qt::AlignLeft | Qt::AlignVCenter);
        lay->addStretch();
        return wrap;
      };

      const QString search = ordersSearchEdit->text().trimmed().toLower();
      const QString datePreset = ordersDatePresetBox->currentText();
      const QDate today = QDate::currentDate();
      QDate dateFrom;
      QDate dateTo;
      if (datePreset == "За месяц") {
        dateFrom = today.addMonths(-1);
        dateTo = today;
      } else if (datePreset == "За год") {
        dateFrom = today.addYears(-1);
        dateTo = today;
      } else if (datePreset == "Точные даты") {
        dateFrom = ordersDateFromEdit->date();
        dateTo = ordersDateToEdit->date();
      }

      QSet<QString> allowedStatuses;
      if (statusDeliveredCheck->isChecked()) allowedStatuses.insert("delivered");
      if (statusProcessingCheck->isChecked()) allowedStatuses.insert("processing");
      if (statusSentCheck->isChecked())    allowedStatuses.insert("sent");
      if (statusCanceledCheck->isChecked())  allowedStatuses.insert("canceled");

      const QString quickStatus = ordersStatusQuickBox->currentText().trimmed().toLower();
      if (quickStatus != "все статусы") {
        allowedStatuses.clear();
        if (quickStatus.contains("достав")) allowedStatuses.insert("delivered");
        else if (quickStatus.contains("отправ")) allowedStatuses.insert("sent");
        else if (quickStatus.contains("отмен")) allowedStatuses.insert("canceled");
        else allowedStatuses.insert("processing");
      }

      const QString paymentFilter = ordersPaymentBox->currentText().trimmed().toLower();
      const int minPrice = ordersPriceMinSpin->value();
      const int maxPrice = ordersPriceMaxSpin->value();
      const int minItems = ordersItemsMinSpin->value();

      QSqlQuery q(db);
      q.prepare(R"(
        SELECT
          o.id,
          o.created_at,
          o.status,
          (SELECT IFNULL(SUM(oi.quantity),0) FROM order_items oi WHERE oi.order_id=o.id) AS items_count,
          IFNULL(o.payment_method, '') AS payment_method,
          o.total
        FROM orders o
        WHERE o.username = :u
        ORDER BY o.id DESC
      )");
      q.bindValue(":u", m_username);
      if (!q.exec()) return;

      QVector<QVariantList> rows;
      while (q.next()) {
        rows.push_back({ q.value(0), q.value(1), q.value(2), q.value(3), q.value(4), q.value(5) });
      }

      m_ordersTable->setUpdatesEnabled(false);
      m_ordersTable->setRowCount(0);
      int shown = 0;
      const int total = rows.size();

      for (const auto &r : rows) {
        const int orderId = r[0].toInt();
        const QString rawDate = r[1].toString();
        const QString status = r[2].toString().trimmed();
        const int itemsCount = r[3].toInt();
        const QString payment = r[4].toString();
        const double totalSum = r[5].toDouble();

        const QDate orderDate = parseOrderDate(rawDate);
        const QString dateText = orderDate.isValid() ? orderDate.toString("dd.MM.yyyy") : formatDate(rawDate);
        const QString orderText = QString("#%1").arg(orderId);

        if (!search.isEmpty()) {
          const QString hay = (orderText + " " + dateText + " " + status + " " + payment).toLower();
          if (!hay.contains(search))
            continue;
        }

        if (dateFrom.isValid() && orderDate.isValid() && orderDate < dateFrom)
          continue;
        if (dateTo.isValid() && orderDate.isValid() && orderDate > dateTo)
          continue;

        const QString group = statusGroup(status);
        if (!allowedStatuses.contains(group))
          continue;

        if (paymentFilter != "любой способ") {
          const QString p = payment.toLower();
          if (!p.contains(paymentFilter))
            continue;
        }

        if (totalSum < minPrice || totalSum > maxPrice)
          continue;
        if (itemsCount < minItems)
          continue;

        const int row = m_ordersTable->rowCount();
        m_ordersTable->insertRow(row);
        m_ordersTable->setItem(row, 0, makeTextItem(orderText));
        m_ordersTable->setItem(row, 1, makeTextItem(dateText));
        m_ordersTable->setCellWidget(row, 2, makeStatusChip(status));
        m_ordersTable->setItem(row, 3, makeTextItem(QString("%1 шт.").arg(itemsCount)));
        m_ordersTable->setItem(row, 4, makeTextItem(paymentText(payment)));
        m_ordersTable->setItem(row, 5, makeTextItem(QString("%1 ₽").arg(QString::number(totalSum, 'f', 0))));

        QToolButton *viewBtn = new QToolButton(m_ordersTable);
        viewBtn->setText(QString::fromUtf8(""));
        viewBtn->setCursor(Qt::PointingHandCursor);
        viewBtn->setToolTip("Посмотреть заказ");
        viewBtn->setFocusPolicy(Qt::NoFocus);
        viewBtn->setStyleSheet("QToolButton{border:none;background:transparent;font-size:16px;padding:0 4px;color:#333333;}QToolButton:hover{background:transparent;color:#333333;}QToolButton:pressed{background:transparent;color:#333333;}");
        connect(viewBtn, &QToolButton::clicked, this, [showOrderDetails, orderId]() { showOrderDetails(orderId); });
        QWidget *btnWrap = new QWidget(m_ordersTable);
        QHBoxLayout *btnLay = new QHBoxLayout(btnWrap);
        btnLay->setContentsMargins(0, 0, 0, 0);
        btnLay->addStretch();
        btnLay->addWidget(viewBtn);
        btnLay->addStretch();
        m_ordersTable->setCellWidget(row, 6, btnWrap);

        ++shown;
      }

      const int headerH = m_ordersTable->horizontalHeader()->height();
      const int rowH = m_ordersTable->verticalHeader()->defaultSectionSize();
      const int tableH = qBound(118, headerH + qMax(1, qMin(shown, 5)) * rowH + 6, 338);
      m_ordersTable->setMinimumHeight(tableH);
      m_ordersTable->setMaximumHeight(tableH);

      ordersCountLabel->setText(QString("Показано %1 из %2 заказов").arg(shown).arg(total));
      ordersPagerLabel->setText(shown <= 5 ? "1" : "Пред.  1  2  След.");
      m_ordersTable->setUpdatesEnabled(true);
    };

    connect(ordersSearchEdit, &QLineEdit::textChanged, this, [refreshOrders](const QString &) {
      refreshOrders();
    });
    connect(ordersStatusQuickBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [refreshOrders](int) {
      refreshOrders();
    });
    connect(ordersFilterButton, &QPushButton::clicked, this, [ordersFiltersPanel]() {
      ordersFiltersPanel->setVisible(!ordersFiltersPanel->isVisible());
    });
    connect(ordersApplyButton, &QPushButton::clicked, this, [refreshOrders]() {
      refreshOrders();
    });
    connect(ordersResetButton, &QPushButton::clicked, this,
        [ordersSearchEdit,
         ordersDatePresetBox,
         ordersStatusQuickBox,
         ordersDateFromEdit,
         ordersDateToEdit,
         statusDeliveredCheck,
         statusProcessingCheck,
         statusSentCheck,
         statusCanceledCheck,
         ordersPaymentBox,
         ordersPriceMinSpin,
         ordersPriceMaxSpin,
         ordersItemsMinSpin,
         refreshOrders]() {
      ordersSearchEdit->clear();
      ordersDatePresetBox->setCurrentIndex(0);
      ordersStatusQuickBox->setCurrentIndex(0);
      ordersDateFromEdit->setDate(QDate::currentDate().addMonths(-1));
      ordersDateToEdit->setDate(QDate::currentDate());
      statusDeliveredCheck->setChecked(true);
      statusProcessingCheck->setChecked(true);
      statusSentCheck->setChecked(true);
      statusCanceledCheck->setChecked(true);
      ordersPaymentBox->setCurrentIndex(0);
      ordersPriceMinSpin->setValue(0);
      ordersPriceMaxSpin->setValue(999999);
      ordersItemsMinSpin->setValue(0);
      refreshOrders();
    });
    connect(ordersPriceMinSpin, qOverload<int>(&QSpinBox::valueChanged), this, [ordersPriceMaxSpin](int value) {
      if (value > ordersPriceMaxSpin->value())
        ordersPriceMaxSpin->setValue(value);
    });
    connect(ordersPriceMaxSpin, qOverload<int>(&QSpinBox::valueChanged), this, [ordersPriceMinSpin](int value) {
      if (value < ordersPriceMinSpin->value())
        ordersPriceMinSpin->setValue(value);
    });

    // The timer refreshes order statuses for the current user.
    m_ordersTimer = new QTimer(this);
    connect(m_ordersTimer, &QTimer::timeout, this, refreshOrders);
    m_ordersTimer->start(2000); // every 2 seconds

    // Initial order loading when the window starts.
    refreshOrders();

    createPageLoadingOverlay();

    // =================== Bottom navigation bar ===================
    QFrame *bottomNav = new QFrame(this);
    bottomNav->setObjectName("BottomNav");
    bottomNav->setFixedHeight(72);

    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomNav);
    bottomLayout->setContentsMargins(24, 4, 24, 8);
    bottomLayout->setSpacing(0);

    // Factory for bottom menu buttons.
    auto makeNavButton = [bottomNav](const QString &iconPath,
                     const QString &text) -> QToolButton *
    {
      QToolButton *btn = new QToolButton(bottomNav);
      btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
      if (!iconPath.isEmpty())
        btn->setIcon(QIcon(iconPath));
      btn->setIconSize(QSize(24, 24));
      btn->setText(text);
      btn->setCheckable(true);
      btn->setProperty("bottomNav", true);  // styled through QSS
      return btn;
    };

    // Menu items.
    QToolButton *btnHome   = makeNavButton(":/icons/home.png",  "ГЛАВНАЯ");
    QToolButton *btnCatalog = makeNavButton(":/icons/catalog.png","КАТАЛОГ");
    QToolButton *btnWardrobe = makeNavButton(":/icons/wardrobe.png", "МОИ ЗАКАЗЫ");
    QToolButton *btnLookbook = makeNavButton(":/icons/lookbook.png", "ЛУКБУК");
    QToolButton *btnProjects = makeNavButton(":/icons/projects.png", "ПРОЕКТЫ");
    QToolButton *btnProfile = makeNavButton(":/icons/profile.png","ПРОФИЛЬ");

    QToolButton *btnAdmin = nullptr;
    if (m_isAdmin) {
      btnAdmin = makeNavButton(":/icons/admin.png", "АДМИНКА");
    }

    QToolButton *btnRecommend = makeNavButton(":/icons/empty.png", "ПОДОБРАТЬ ОДЕЖДУ");
    QToolButton *btnCart   = makeNavButton(":/icons/cart.png","КОРЗИНА");

    // Separate logout button.
    QToolButton *btnExit = new QToolButton(bottomNav);
    btnExit->setText("ВЫХОД");
    btnExit->setProperty("bottomNavExit", true);

    // Exclusive group for switchable bottom menu buttons.
    QButtonGroup *bottomGroup = new QButtonGroup(this);
    bottomGroup->setExclusive(true);
    bottomGroup->addButton(btnCart);
    bottomGroup->addButton(btnHome);
    bottomGroup->addButton(btnWardrobe);
    bottomGroup->addButton(btnCatalog);
    bottomGroup->addButton(btnLookbook);
    bottomGroup->addButton(btnProjects);
    bottomGroup->addButton(btnProfile);
    bottomGroup->addButton(btnRecommend);
    btnHome->setChecked(true);

    // Inner layout used to center menu items.
    QHBoxLayout *navCenter = new QHBoxLayout();
    navCenter->setSpacing(32);
    navCenter->addWidget(btnCart);
    navCenter->addWidget(btnHome);
    navCenter->addWidget(btnCatalog);
    navCenter->addWidget(btnWardrobe);
    navCenter->addWidget(btnLookbook);
    navCenter->addWidget(btnProjects);
    navCenter->addWidget(btnProfile);
    if (btnAdmin) navCenter->addWidget(btnAdmin);
    navCenter->addWidget(btnRecommend);

    // Element placement: [stretch] [center menu] [stretch] [logout].
    bottomLayout->addStretch();
    bottomLayout->addLayout(navCenter);
    bottomLayout->addStretch();
    bottomLayout->addWidget(btnExit);

    // Add the bottom bar to the main layout.
    //centralLayout->addWidget(bottomNav);


    // Bottom navigation button logic.
    enum PageMode { HOME = 0, CATALOG = 1, ORDERS = 2, LOOKBOOK = 3, PROJECTS = 4 };

    // This lambda hides all major sections and then shows the requested page.
    auto setMode = [this](PageMode mode) {
      if (m_homeScroll)
        m_homeScroll->setVisible(mode == HOME);

      if (m_catalogScroll)
        m_catalogScroll->setVisible(mode == CATALOG);

      if (m_filtersDock)
        m_filtersDock->setVisible(mode == CATALOG);

      if (m_ordersSection)
        m_ordersSection->setVisible(mode == ORDERS);

      if (m_lookbookSection)
        m_lookbookSection->setVisible(mode == LOOKBOOK);

      if (m_projectsSection)
        m_projectsSection->setVisible(mode == PROJECTS);
    };




    // Home: show only the home page.
    connect(btnHome, &QToolButton::clicked, this,
        [this, setMode]() {
          runPageTransition("ГЛАВНАЯ", [this, setMode]() {
            setMode(HOME);
            if (m_catalogScroll && m_catalogScroll->verticalScrollBar())
              m_catalogScroll->verticalScrollBar()->setValue(0);
          });
        });

    connect(btnCatalog, &QToolButton::clicked, this,
        [this]() {
          openCatalogPage(true);
        });



    connect(btnWardrobe, &QToolButton::clicked, this,
        [this, setMode, refreshOrders]() {
          runPageTransitionAsync("МОИ ЗАКАЗЫ", [this, setMode, refreshOrders](const std::function<void()> &done) {
            refreshOrders();
            setMode(ORDERS);
            if (done)
              done();
          });
        });



    // Lookbook: show the section without the main banner.
    connect(btnLookbook, &QToolButton::clicked, this,
        [this]() {
          openLookbookPage();
        });

    // Projects: this section is available to administrators.
    connect(btnProjects, &QToolButton::clicked, this, [this, setMode]() {
      runPageTransitionAsync("ПРОЕКТЫ", [this, setMode](const std::function<void()> &done) {
        refreshProjectsUI();
        setMode(PROJECTS);
        if (done)
          done();
      });
    });
    connect(btnProfile, &QToolButton::clicked, this,
        [this]() {
          editProfile();
        });
    // The administrator panel is available only to administrators.
    if (btnAdmin) {
      connect(btnAdmin, &QToolButton::clicked, this, [this]() {
        openAdmin();
      });
    }



    // Logout: the signal is handled in main(), then the login dialog is shown again.
    connect(btnExit, &QToolButton::clicked, this, [this]() {
      emit logoutRequested();
    });

    connect(btnRecommend, &QToolButton::clicked,
        this, &CatalogWindow::onRecommendClicked);

    connect(heroBtn, &QPushButton::clicked, this, [btnRecommend]() {
      btnRecommend->click();
    });

    connect(btnCart, &QToolButton::clicked, this,
        [this, setMode]() {
          setMode(CATALOG);   // mode without the main banner
          openCart();     // open the cart
        });

    connect(recsAllBtn, &QPushButton::clicked, this, [this, btnCatalog]() {
      btnCatalog->setChecked(true);
      openCatalogPage(true);
    });



    // The home page is shown by default.
    // Only the home page is shown by default.
    setMode(HOME);

    resize(1280, 850);
    setMinimumSize(1180, 760);
    setMinimumSize(1120, 720);

    // Set the root layout: custom title bar at the top, main content below it.
    setCentralWidget(root);

    // Add the status bar and bottom navigation bar.
    QStatusBar *sb = new QStatusBar(this);
    sb->setSizeGripEnabled(false);
    sb->addPermanentWidget(bottomNav, 1);
    setStatusBar(sb);
    // ===== Cart =====
    m_cartDock = new QDockWidget("Корзина", this);
    m_cartDock->setAllowedAreas(Qt::RightDockWidgetArea);
    m_cartDock->setFeatures(QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::RightDockWidgetArea, m_cartDock);
    m_cartDock->hide();

    QWidget *cartWidget = new QWidget(m_cartDock);
    QVBoxLayout *cartLayout = new QVBoxLayout(cartWidget);

    m_cartTable = new QTableWidget(0, 4, this);
    m_cartTable->setHorizontalHeaderLabels({ "Товар", "Цена", "Количество", "Сумма" });
    m_cartTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_cartTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    cartLayout->addWidget(m_cartTable);

    m_cartTotalLabel = new QLabel("Итого: 0 ₽", this);
    cartLayout->addWidget(m_cartTotalLabel);


    // Control buttons.
    QHBoxLayout *cartBtns = new QHBoxLayout();
    QPushButton *btnOrder = new QPushButton("Оформить заказ");
    QPushButton *btnRemove = new QPushButton("Удалить");
    QPushButton *btnClear = new QPushButton("Очистить");

    cartBtns->addWidget(btnOrder);
    cartBtns->addWidget(btnRemove);
    cartBtns->addWidget(btnClear);

    cartLayout->addLayout(cartBtns);
    m_cartDock->setWidget(cartWidget);



    m_filtersDock = new QDockWidget("", this);
    m_filtersDock->setFeatures(QDockWidget::NoDockWidgetFeatures);

    QWidget *filtersWidget = new QWidget(m_filtersDock);
    filtersWidget->setObjectName("CatalogFiltersWidget");
    filtersWidget->setStyleSheet(
     "#CatalogFiltersWidget { background:#F4EFE9; border-right:1px solid #E4DAD1; }"
     "#CatalogFiltersWidget QLabel[filterCaption='true'] { color:#7A858F; font-size:12px; font-weight:600; }"
     "#CatalogFiltersWidget QLabel[filterTitle='true'] { color:#1E2A36; font-size:26px; font-weight:800; }"
     "#CatalogFiltersWidget QLabel[filterHint='true'] { color:#8E97A1; font-size:12px; }"
     "#CatalogFiltersWidget QComboBox, #CatalogFiltersWidget QSpinBox {"
     " background:#FFFFFF; color:#1E2A36; border:1px solid #DED4CB; border-radius:14px; min-height:40px; padding:0 14px; }"
     "#CatalogFiltersWidget QComboBox::drop-down { border:none; width:28px; }"
     "#CatalogFiltersWidget QSpinBox::up-button, #CatalogFiltersWidget QSpinBox::down-button { width:18px; border:none; }"
     "#CatalogFiltersWidget QPushButton {"
     " background:#DFA08C; color:white; border:none; border-radius:14px; min-height:42px; font-weight:700; padding:0 16px; }"
     "#CatalogFiltersWidget QPushButton:hover { background:#C98A76; }"
     "#CatalogFiltersWidget QPushButton:pressed { background:#B87463; }"
     "#CatalogFiltersWidget QFrame[filterCard='true'] { background:#F8F4F0; border:1px solid #E7DDD4; border-radius:18px; }"
    );

    QVBoxLayout *filtersLayout = new QVBoxLayout(filtersWidget);
    filtersLayout->setContentsMargins(14, 14, 14, 14);
    filtersLayout->setSpacing(12);
    m_filtersDock->setWidget(filtersWidget);

    QLabel *fTitle = new QLabel("Фильтры", filtersWidget);
    fTitle->setProperty("filterTitle", true);
    filtersLayout->addWidget(fTitle);

    QLabel *fHint = new QLabel("Подберите вещи быстрее: категория, цвет, сезон, стиль и цена.", filtersWidget);
    fHint->setWordWrap(true);
    fHint->setProperty("filterHint", true);
    filtersLayout->addWidget(fHint);

    auto makeCaption = [&](const QString &text) {
      QLabel *lab = new QLabel(text, filtersWidget);
      lab->setProperty("filterCaption", true);
      return lab;
    };

    auto *filterCard = new QFrame(filtersWidget);
    filterCard->setProperty("filterCard", true);
    QVBoxLayout *cardLayout = new QVBoxLayout(filterCard);
    cardLayout->setContentsMargins(14, 14, 14, 14);
    cardLayout->setSpacing(10);

    m_categoryBox = new QComboBox(filtersWidget);
    m_categoryBox->addItems({"Все категории", "Футболка", "Лонгслив", "Майка", "Топ", "Рубашка", "Блузка", "Худи", "Свитшот", "Свитер", "Кардиган", "Жакет", "Пиджак", "Куртка", "Пальто", "Тренч", "Брюки", "Джинсы", "Юбка", "Шорты", "Платье", "Костюм", "Обувь", "Кроссовки", "Кеды", "Ботинки", "Лоферы", "Сумка", "Ремень", "Шарф", "Головной убор", "Шапка", "Кепка", "Аксессуары"});

    m_sizeBox = new QComboBox(filtersWidget);
    m_sizeBox->addItems({"Любой", "XXS", "XS", "S", "M", "L", "XL", "XXL", "3XL", "one size", "35", "36", "37", "38", "39", "40", "41", "42", "43", "44", "45", "46"});

    m_colorBox = new QComboBox(filtersWidget);
    m_colorBox->addItems({"Любой", "Чёрный", "Белый", "Синий", "Красный", "Серый", "Бежевый", "Молочный", "Кремовый", "Песочный", "Персиковый", "Терракотовый", "Карамельный", "Оливковый"});

    m_seasonBox = new QComboBox(filtersWidget);
    m_seasonBox->addItems({"Любой", "Весна", "Лето", "Осень", "Зима", "Демисезон"});

    m_styleFilterBox = new QComboBox(filtersWidget);
    m_styleFilterBox->addItems({"Любой", "Кэжуал", "Минималистичный", "Базовый", "Спортивный", "Деловой", "Streetwear", "Smart Casual", "Oversize", "Гранж", "Винтаж", "Классика", "Романтический", "Бохо", "Офисный", "Basic", "Casual", "Minimal", "Street", "Sport", "Business"});

    m_priceMinSpin = new QSpinBox(filtersWidget);
    m_priceMinSpin->setRange(0, 1000000);
    m_priceMinSpin->setSingleStep(500);
    m_priceMinSpin->setSuffix(" ₽");

    m_priceMaxSpin = new QSpinBox(filtersWidget);
    m_priceMaxSpin->setRange(0, 1000000);
    m_priceMaxSpin->setSingleStep(500);
    m_priceMaxSpin->setValue(10000);
    m_priceMaxSpin->setSuffix(" ₽");

    cardLayout->addWidget(makeCaption("Категория"));
    cardLayout->addWidget(m_categoryBox);

    cardLayout->addWidget(makeCaption("Размер"));
    cardLayout->addWidget(m_sizeBox);

    cardLayout->addWidget(makeCaption("Цвет"));
    cardLayout->addWidget(m_colorBox);

    cardLayout->addWidget(makeCaption("Сезон"));
    cardLayout->addWidget(m_seasonBox);

    cardLayout->addWidget(makeCaption("Стиль"));
    cardLayout->addWidget(m_styleFilterBox);

    cardLayout->addWidget(makeCaption("Цена"));
    QWidget *priceRow = new QWidget(filtersWidget);
    QHBoxLayout *priceLayout = new QHBoxLayout(priceRow);
    priceLayout->setContentsMargins(0, 0, 0, 0);
    priceLayout->setSpacing(8);
    priceLayout->addWidget(m_priceMinSpin);
    priceLayout->addWidget(m_priceMaxSpin);
    cardLayout->addWidget(priceRow);

    m_resetFiltersButton = new QPushButton("Сбросить фильтры", filtersWidget);
    cardLayout->addWidget(m_resetFiltersButton);

    filtersLayout->addWidget(filterCard);
    filtersLayout->addStretch();

    connect(m_categoryBox,    &QComboBox::currentTextChanged, this, &CatalogWindow::applyFilter);
    connect(m_sizeBox,      &QComboBox::currentTextChanged, this, &CatalogWindow::applyFilter);
    connect(m_colorBox,     &QComboBox::currentTextChanged, this, &CatalogWindow::applyFilter);
    connect(m_seasonBox,     &QComboBox::currentTextChanged, this, &CatalogWindow::applyFilter);
    connect(m_styleFilterBox,  &QComboBox::currentTextChanged, this, &CatalogWindow::applyFilter);
    connect(m_priceMinSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
      if (m_priceMaxSpin && value > m_priceMaxSpin->value())
        m_priceMaxSpin->setValue(value);
      applyFilter();
    });
    connect(m_priceMaxSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
      if (m_priceMinSpin && value < m_priceMinSpin->value())
        m_priceMinSpin->setValue(value);
      applyFilter();
    });
    connect(m_resetFiltersButton, &QPushButton::clicked, this, &CatalogWindow::resetFilters);

    addDockWidget(Qt::LeftDockWidgetArea, m_filtersDock);

    m_filtersDock->setMinimumWidth(300);
    m_filtersDock->setMaximumWidth(300);

    m_filtersDock->hide();


    rebuildGridChunked(m_products);
  }

signals:
  void logoutRequested();

private:
  QVector<Product> m_products;
  void applyFilter();
  QVector<Product> m_softRecommendations;
  QVector<Product> m_catalogBase;  // current catalog product set
  bool m_smartCatalogActive = false;
  QString m_username;
  bool  m_isAdmin = false;

  QWidget *m_contentHost = nullptr;
  QFrame *m_pageLoadingOverlay = nullptr;
  QGraphicsOpacityEffect *m_pageLoadingOpacity = nullptr;
  QPropertyAnimation *m_pageLoadingFade = nullptr;
  QLabel *m_pageLoadingTitle = nullptr;
  QLabel *m_pageLoadingText = nullptr;
  bool m_pageTransitionActive = false;
  int m_gridBuildGeneration = 0;
  int m_lookbookBuildGeneration = 0;
  int m_lookbookShowcaseGeneration = 0;

  CartManager   m_cart;

  QWidget   *m_gridWidget = nullptr;
  QGridLayout *m_gridLayout = nullptr;

  QWidget   *m_homeSection = nullptr;
  QScrollArea *m_homeScroll  = nullptr;

  QComboBox *m_categoryBox  = nullptr;
  QComboBox *m_sizeBox    = nullptr;
  QComboBox *m_colorBox    = nullptr;
  QComboBox *m_seasonBox   = nullptr;
  QComboBox *m_styleFilterBox = nullptr;
  QSpinBox *m_priceMinSpin  = nullptr;
  QSpinBox *m_priceMaxSpin  = nullptr;
  QPushButton *m_resetFiltersButton = nullptr;

  QScrollArea *m_catalogScroll = nullptr;  // catalog scroll area
  QDockWidget *m_filtersDock  = nullptr;

  // ===== Cart: embedded right-side panel =====
  QDockWidget *m_cartDock    = nullptr;
  QTableWidget *m_cartTable   = nullptr;
  QLabel    *m_cartTotalLabel = nullptr;

  QWidget *m_ordersSection = nullptr;
  QTableWidget *m_ordersTable = nullptr;
  QTimer *m_ordersTimer = nullptr;

  // ===== Lookbook =====
  QWidget *m_lookbookSection = nullptr;
  QStackedWidget *m_lookbookStack = nullptr;
  QWidget *m_lbProjectsPage = nullptr;
  QWidget *m_lbEditorPage = nullptr;
  QGridLayout *m_lbProjectsListLay = nullptr;
  QGridLayout *m_lbTrendsLay = nullptr;
  QGridLayout *m_lbCapsulesLay = nullptr;
  QLineEdit *m_lbSearchEdit = nullptr;
  QComboBox *m_lbSortBox = nullptr;
  QWidget *m_lbEditorMain = nullptr;
  QFrame *m_lbCenterOverlay = nullptr;
  QLineEdit *m_lbProjectTitleEdit = nullptr;
  QLabel *m_lbUpdatedLabel = nullptr;
  QLabel *m_lbCenterPreviewLabel = nullptr;
  QPushButton *m_lbCenterAddButton = nullptr;
  QLabel *m_lbDetailsRatingTitleLabel = nullptr;
  QLabel *m_lbDetailsRatingValueLabel = nullptr;
  QLabel *m_lbDetailsCostTitleLabel = nullptr;
  QLabel *m_lbDetailsCostLabel = nullptr;
  QLabel *m_lbDetailsCommentsTitleLabel = nullptr;
  QLabel *m_lbDetailsCommentsLabel = nullptr;
  int m_lbCurrentProjectId = 0;
  QVector<QLabel*> m_lbSlotImages;
  QVector<QLabel*> m_lbSlotNames;
  QVector<QLabel*> m_lbSlotMeta;
  QVector<QPushButton*> m_lbSlotActionButtons;
  QVector<QPushButton*> m_lbSlotRemoveButtons;
  QVector<QWidget*> m_lbColorHosts;
  QVector<int> m_lbSlotProductIds;
  QVector<int> m_lbSelectedColorIndex;
  QVector<QStringList> m_lbSlotColorVariants;
  QVector<QVector<int>> m_lbColorVariantProductIds;

  struct LookbookSettings
  {
    int projectId = 0;
    QString title;
    QString coverPath;
    QString description;
    QString accessLevel = "private";  // private, link, public
    bool commentsEnabled = true;
    bool ratingsEnabled = true;
    QString currency = "RUB";     // RUB, USD, EUR
    bool showPrices = true;
    bool affiliateEnabled = false;
    QString shareToken;
  };

  // ===== Projects =====
  QWidget *m_projectsSection = nullptr;
  QWidget *m_projectsEmptyPanel = nullptr; // empty-list panel
  QListWidget *m_projectsList = nullptr;  // project list


  void createPageLoadingOverlay()
  {
    if (!m_contentHost || m_pageLoadingOverlay)
      return;

    m_pageLoadingOverlay = new QFrame(m_contentHost);
    m_pageLoadingOverlay->setObjectName("PageLoadingOverlay");
    m_pageLoadingOverlay->setStyleSheet(
     "QFrame#PageLoadingOverlay{background:rgba(246,242,237,232);border:none;}"
     "QFrame#PageLoadingCard{background:#FFFFFF;border:1px solid #E2D6CC;border-radius:20px;}"
     "QLabel#PageLoadingTitle{color:#211D22;font-size:20px;font-weight:900;}"
     "QLabel#PageLoadingText{color:#7C7168;font-size:13px;font-weight:600;}"
     "QProgressBar#PageLoadingBar{height:10px;border:1px solid #E4D8CF;border-radius:5px;background:#F7F1EC;text-align:center;color:transparent;}"
     "QProgressBar#PageLoadingBar::chunk{border-radius:5px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #B56D46,stop:0.55 #D9A06F,stop:1 #B56D46);}"
    );
    m_pageLoadingOpacity = new QGraphicsOpacityEffect(m_pageLoadingOverlay);
    m_pageLoadingOpacity->setOpacity(0.0);
    m_pageLoadingOverlay->setGraphicsEffect(m_pageLoadingOpacity);

    QVBoxLayout *overlayLay = new QVBoxLayout(m_pageLoadingOverlay);
    overlayLay->setContentsMargins(0, 0, 0, 0);
    overlayLay->setAlignment(Qt::AlignCenter);

    QFrame *card = new QFrame(m_pageLoadingOverlay);
    card->setObjectName("PageLoadingCard");
    card->setFixedSize(380, 184);

    QVBoxLayout *cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(28, 24, 28, 24);
    cardLay->setSpacing(12);
    cardLay->setAlignment(Qt::AlignCenter);

    m_pageLoadingTitle = new QLabel("Загрузка страницы", card);
    m_pageLoadingTitle->setObjectName("PageLoadingTitle");
    m_pageLoadingTitle->setAlignment(Qt::AlignCenter);

    m_pageLoadingText = new QLabel("Подождите, раздел открывается", card);
    m_pageLoadingText->setObjectName("PageLoadingText");
    m_pageLoadingText->setAlignment(Qt::AlignCenter);

    QProgressBar *bar = new QProgressBar(card);
    bar->setObjectName("PageLoadingBar");
    bar->setRange(0, 0);
    bar->setTextVisible(false);
    bar->setFixedWidth(230);

    cardLay->addStretch();
    cardLay->addWidget(m_pageLoadingTitle);
    cardLay->addWidget(m_pageLoadingText);
    cardLay->addWidget(bar, 0, Qt::AlignHCenter);
    cardLay->addStretch();
    overlayLay->addWidget(card, 0, Qt::AlignCenter);

    positionPageLoadingOverlay();
    m_pageLoadingOverlay->hide();
  }

  void positionPageLoadingOverlay()
  {
    if (!m_contentHost || !m_pageLoadingOverlay)
      return;
    m_pageLoadingOverlay->setGeometry(m_contentHost->rect());
    if (m_pageLoadingOverlay->isVisible())
      m_pageLoadingOverlay->raise();
  }

  void startPageLoadingFade(qreal start,
               qreal end,
               int durationMs,
               const std::function<void()> &finished = std::function<void()>())
  {
    if (!m_pageLoadingOpacity) {
      if (finished)
        finished();
      return;
    }

    if (m_pageLoadingFade) {
      m_pageLoadingFade->stop();
      m_pageLoadingFade->deleteLater();
      m_pageLoadingFade = nullptr;
    }

    QPropertyAnimation *anim = new QPropertyAnimation(m_pageLoadingOpacity, "opacity", this);
    m_pageLoadingFade = anim;
    anim->setDuration(durationMs);
    anim->setStartValue(start);
    anim->setEndValue(end);
    anim->setEasingCurve(end > start ? QEasingCurve::OutCubic : QEasingCurve::InOutCubic);
    connect(anim, &QPropertyAnimation::finished, this, [this, anim, finished]() {
      if (m_pageLoadingFade == anim)
        m_pageLoadingFade = nullptr;
      anim->deleteLater();
      if (finished)
        finished();
    });
    anim->start();
  }

  void showPageLoading(const QString &pageName)
  {
    createPageLoadingOverlay();
    if (!m_pageLoadingOverlay)
      return;

    if (m_pageLoadingTitle) {
      m_pageLoadingTitle->setText(QString("Загрузка: %1").arg(pageName));
    }
    if (m_pageLoadingText) {
      if (pageName.contains("ЛУКБУК", Qt::CaseInsensitive))
        m_pageLoadingText->setText("Подгружаем лукбуки и готовые капсулы");
      else if (pageName.contains("КАТАЛОГ", Qt::CaseInsensitive))
        m_pageLoadingText->setText("Подгружаем товары и обновляем фильтры");
      else
        m_pageLoadingText->setText("Подождите, раздел открывается");
    }

    positionPageLoadingOverlay();
    m_pageLoadingOverlay->show();
    m_pageLoadingOverlay->raise();
    if (m_pageLoadingOpacity)
      m_pageLoadingOpacity->setOpacity(0.0);
    startPageLoadingFade(0.0, 1.0, 280);
  }

  void hidePageLoading(const std::function<void()> &afterHidden = std::function<void()>())
  {
    if (!m_pageLoadingOverlay) {
      if (afterHidden)
        afterHidden();
      return;
    }

    if (!m_pageLoadingOpacity) {
      m_pageLoadingOverlay->hide();
      if (afterHidden)
        afterHidden();
      return;
    }

    const qreal start = m_pageLoadingOpacity->opacity();
    startPageLoadingFade(start, 0.0, 360, [this, afterHidden]() {
      if (m_pageLoadingOverlay)
        m_pageLoadingOverlay->hide();
      if (afterHidden)
        afterHidden();
    });
  }

  void runPageTransition(const QString &pageName,
              const std::function<void()> &work,
              const std::function<void()> &afterShown = std::function<void()>())
  {
    if (m_pageTransitionActive)
      return;

    m_pageTransitionActive = true;
    showPageLoading(pageName);

    QTimer::singleShot(180, this, [this, work, afterShown]() {
      if (work)
        work();
      QTimer::singleShot(220, this, [this, afterShown]() {
        hidePageLoading([this, afterShown]() {
          m_pageTransitionActive = false;
          if (afterShown)
            afterShown();
        });
      });
    });
  }

  void runPageTransitionAsync(const QString &pageName,
                const std::function<void(const std::function<void()> &)> &work,
                const std::function<void()> &afterShown = std::function<void()>())
  {
    if (m_pageTransitionActive)
      return;

    m_pageTransitionActive = true;
    showPageLoading(pageName);

    QTimer::singleShot(120, this, [this, work, afterShown]() {
      auto finishWork = [this, afterShown]() {
        QTimer::singleShot(120, this, [this, afterShown]() {
          hidePageLoading([this, afterShown]() {
            m_pageTransitionActive = false;
            if (afterShown)
              afterShown();
          });
        });
      };

      if (work)
        work(finishWork);
      else
        finishWork();
    });
  }

  QPixmap cachedScaledPixmap(const QString &rawPath,
                const QSize &target,
                Qt::AspectRatioMode aspectMode = Qt::KeepAspectRatio,
                Qt::TransformationMode transformMode = Qt::SmoothTransformation) const
  {
    const QString resolvedPath = resolveImagePath(rawPath);
    if (resolvedPath.trimmed().isEmpty())
      return QPixmap();

    QFileInfo info(resolvedPath);
    if (!info.exists())
      return QPixmap();

    const QSize safeSize(qMax(1, target.width()), qMax(1, target.height()));
    const QString cacheKey = QString("catalog:%1:%2x%3:%4:%5")
                   .arg(info.absoluteFilePath())
                   .arg(safeSize.width())
                   .arg(safeSize.height())
                   .arg(static_cast<int>(aspectMode))
                   .arg(info.lastModified().toMSecsSinceEpoch());

    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached))
      return cached;

    QPixmap source(info.absoluteFilePath());
    if (source.isNull())
      return QPixmap();

    QPixmap scaled = source.scaled(safeSize, aspectMode, transformMode);
    QPixmapCache::insert(cacheKey, scaled);
    return scaled;
  }

  void openLookbookPage()
  {
    runPageTransitionAsync("ЛУКБУК", [this](const std::function<void()> &done) {
      if (m_lookbookStack)
        m_lookbookStack->setCurrentIndex(0);

      if (m_homeScroll) m_homeScroll->setVisible(false);
      if (m_catalogScroll) m_catalogScroll->setVisible(false);
      if (m_filtersDock) m_filtersDock->setVisible(false);
      if (m_ordersSection) m_ordersSection->setVisible(false);
      if (m_projectsSection) m_projectsSection->setVisible(false);
      if (m_lookbookSection) m_lookbookSection->setVisible(true);

      QTimer::singleShot(0, this, [this, done]() {
        refreshLookbookProjectsList([this, done]() {
          refreshLookbookShowcaseSections(done);
        });
      });
    });
  }

  void resetCatalogFiltersSilently()
  {
    auto setComboIndex = [](QComboBox *box, int index) {
      if (!box)
        return;
      QSignalBlocker blocker(box);
      box->setCurrentIndex(index);
    };
    auto setSpinValue = [](QSpinBox *spin, int value) {
      if (!spin)
        return;
      QSignalBlocker blocker(spin);
      spin->setValue(value);
    };

    setComboIndex(m_categoryBox, 0);
    setComboIndex(m_sizeBox, 0);
    setComboIndex(m_colorBox, 0);
    setComboIndex(m_seasonBox, 0);
    setComboIndex(m_styleFilterBox, 0);
    setSpinValue(m_priceMinSpin, 0);
    setSpinValue(m_priceMaxSpin, m_priceMaxSpin ? m_priceMaxSpin->maximum() : 1000000);
  }

  void showCatalogPageWidgets()
  {
    if (m_homeScroll) m_homeScroll->setVisible(false);
    if (m_catalogScroll) m_catalogScroll->setVisible(true);
    if (m_filtersDock) m_filtersDock->setVisible(true);
    if (m_ordersSection) m_ordersSection->setVisible(false);
    if (m_projectsSection) m_projectsSection->setVisible(false);
    if (m_lookbookSection) m_lookbookSection->setVisible(false);
  }

  void openCatalogPage(bool resetSmartCatalog = true,
             const std::function<void()> &afterShown = std::function<void()>())
  {
    runPageTransitionAsync("КАТАЛОГ", [this, resetSmartCatalog](const std::function<void()> &done) {
      if (resetSmartCatalog) {
        m_smartCatalogActive = false;
        m_catalogBase = m_products;
        m_softRecommendations.clear();
      }

      resetCatalogFiltersSilently();

      if (m_catalogScroll && m_catalogScroll->verticalScrollBar())
        m_catalogScroll->verticalScrollBar()->setValue(0);

      showCatalogPageWidgets();
      rebuildGridChunked(m_smartCatalogActive ? m_catalogBase : m_products, done);
    }, afterShown);
  }

  void openCatalogAndShowProduct(int productId)
  {
    openCatalogPage(true, [this, productId]() {
      for (const Product &product : m_products) {
        if (product.id == productId) {
          ProductDetailsDialog dlg(product, m_cart, this);
          dlg.exec();
          break;
        }
      }
    });
  }


  static QString normRu(QString s)
  {
    s = s.toLower().trimmed();
    s.replace("ё", "е");
    s = s.simplified();     // remove extra spaces
    return s;
  }

  static QStringList parseColorsUnique(const QString &input)
  {
    QString tmp = input;
    tmp.replace(";", ",");
    tmp.replace("|", ",");
    tmp.replace("/", ",");
    tmp = tmp.toLower();

    QStringList raw = tmp.split(",", Qt::SkipEmptyParts);

    QSet<QString> uniq;
    QStringList out;
    for (QString c : raw) {
      c = normRu(c);
      if (c.isEmpty()) continue;
      if (!uniq.contains(c)) {
        uniq.insert(c);
        out << c;
      }
    }
    return out;
  }

  static bool colorExactMatch(const QString &productColor, const QStringList &needColors)
  {
    if (needColors.isEmpty()) return true;
    const QString pc = normRu(productColor);
    for (const QString &c : needColors) {
      if (pc == c) return true;   // exact match
    }
    return false;
  }

  QVector<Product> buildStrictCatalog(const RecommendCriteria &crit)
  {
    struct RankedProduct
    {
      int score = 0;
      Product product;
    };

    QVector<RankedProduct> ranked;

    const QStringList needCategories = parseColorsUnique(crit.category);
    const QStringList needColors = parseColorsUnique(crit.colors);
    const QStringList needStyles = parseColorsUnique(crit.style);
    const double minPrice = qMax(0.0, crit.minBudget);
    const double maxPrice = (crit.maxBudget > 0.0) ? crit.maxBudget : 1e12;

    auto categoryMatchesSmart = [](const Product &p, const QStringList &categories) {
      if (categories.isEmpty())
        return true;

      const QString cat = normRu(p.category);
      const QString name = normRu(p.name);

      for (const QString &raw : categories) {
        const QString c = normRu(raw);
        if (c.contains("футбол") && (cat.contains("футбол") || name.contains("футбол"))) return true;
        if (c.contains("брюк") && (cat.contains("брюк") || cat.contains("джинс") || name.contains("брюк") || name.contains("джинс"))) return true;
        if (c.contains("куртк") && (cat.contains("куртк") || cat.contains("пальто") || cat.contains("тренч") || name.contains("куртк") || name.contains("пальто") || name.contains("тренч"))) return true;
        if (c.contains("худи") && (cat.contains("худи") || cat.contains("свитшот") || name.contains("худи") || name.contains("свитшот"))) return true;
        if (c.contains("рубаш") && (cat.contains("рубаш") || name.contains("рубаш"))) return true;
        if (c.contains("обув") && (cat.contains("обув") || cat.contains("кед") || cat.contains("крос") || cat.contains("бот") || name.contains("кед") || name.contains("крос") || name.contains("бот"))) return true;
        if (cat == c || cat.contains(c) || c.contains(cat)) return true;
      }
      return false;
    };

    auto sizeMatchesSmart = [](const Product &p, const QString &sizeText) {
      QString s = normRu(sizeText);
      if (s.isEmpty() || s == "не важно")
        return true;

      auto containsDigit = [](const QString &text) {
        for (const QChar ch : text) {
          if (ch.isDigit())
            return true;
        }
        return false;
      };

      const QString hay = normRu(p.name + " " + p.category);
      const bool isFootwear = hay.contains("обув") || hay.contains("кед") ||
                  hay.contains("крос") || hay.contains("сникер") ||
                  hay.contains("ботин") || hay.contains("shoe");
      const bool requestedShoeSize = containsDigit(s);
      if (isFootwear) {
        if (!requestedShoeSize)
          return true;
        return p.size.compare(sizeText, Qt::CaseInsensitive) == 0;
      }

      if (requestedShoeSize)
        return p.size.compare(sizeText, Qt::CaseInsensitive) == 0;

      if (s.contains("s, m, l")) {
        return p.size.compare("S", Qt::CaseInsensitive) == 0 ||
            p.size.compare("M", Qt::CaseInsensitive) == 0 ||
            p.size.compare("L", Qt::CaseInsensitive) == 0 ||
            p.size.compare("one size", Qt::CaseInsensitive) == 0;
      }

      return p.size.compare(sizeText, Qt::CaseInsensitive) == 0 ||
          p.size.compare("one size", Qt::CaseInsensitive) == 0;
    };

    auto colorMatchesSmart = [](const Product &p, const QStringList &colors) {
      if (colors.isEmpty())
        return true;

      const QString pc = normRu(p.color);
      for (const QString &raw : colors) {
        const QString c = normRu(raw);
        if (pc == c || pc.contains(c) || c.contains(pc))
          return true;
        if ((pc.contains("чер") && (c.contains("black") || c.contains("чер"))) ||
          (pc.contains("бел") && (c.contains("white") || c.contains("бел"))) ||
          (pc.contains("син") && (c.contains("blue") || c.contains("син"))) ||
          (pc.contains("крас") && (c.contains("red") || c.contains("крас"))))
          return true;
      }
      return false;
    };

    auto styleMatchesSmart = [this](const Product &p, const QStringList &styles) {
      if (styles.isEmpty())
        return true;

      for (const QString &style : styles) {
        if (productMatchesStyle(p, style))
          return true;
      }
      return false;
    };

    auto bodySizeScore = [](const Product &p, int weight, int height) {
      int score = 0;
      const QString ps = normRu(p.size);

      if (weight > 0) {
        if (weight <= 60 && (ps == "xs" || ps == "s" || ps == "m" || ps == "one size"))
          score += 5;
        else if (weight > 60 && weight <= 82 && (ps == "m" || ps == "l" || ps == "one size"))
          score += 5;
        else if (weight > 82 && (ps == "l" || ps == "xl" || ps == "xxl" || ps == "one size"))
          score += 5;
      }

      if (height > 0) {
        if (height <= 165 && (ps == "xs" || ps == "s" || ps == "m" || ps == "one size"))
          score += 4;
        else if (height > 165 && height <= 185 && (ps == "m" || ps == "l" || ps == "one size"))
          score += 4;
        else if (height > 185 && (ps == "l" || ps == "xl" || ps == "xxl" || ps == "one size"))
          score += 4;
      }

      return score;
    };

    for (const Product &p : m_products) {
      if (!categoryMatchesSmart(p, needCategories))
        continue;

      if (!styleMatchesSmart(p, needStyles))
        continue;

      if (!colorMatchesSmart(p, needColors))
        continue;

      if (!sizeMatchesSmart(p, crit.size))
        continue;

      if (p.price < minPrice)
        continue;

      if (p.price > maxPrice)
        continue;

      int score = 0;
      score += needCategories.isEmpty() ? 5 : 35;
      score += needStyles.isEmpty() ? 0 : 25;
      score += needColors.isEmpty() ? 0 : 25;
      score += bodySizeScore(p, crit.weight, crit.height);

      ranked.push_back({score, p});
    }

    std::sort(ranked.begin(), ranked.end(), [](const RankedProduct &a, const RankedProduct &b) {
      if (a.score != b.score)
        return a.score > b.score;
      return a.product.price < b.product.price;
    });

    QVector<Product> out;
    for (const RankedProduct &rp : ranked)
      out.push_back(rp.product);

    return out;
  }

  QVector<Product> buildSoftReco(const RecommendCriteria &crit,
                  const QVector<Product> &strictList,
                  int limit = 8)
  {
    QSet<int> strictIds;
    for (const auto &p : strictList)
      strictIds.insert(p.id);

    const QStringList favColors = parseColorsUnique(crit.colors);
    const QStringList styles = parseColorsUnique(crit.style);
    const QStringList categories = parseColorsUnique(crit.category);

    auto categorySoft = [](const Product &p, const QStringList &categories) {
      if (categories.isEmpty())
        return 1;

      const QString cat = normRu(p.category);
      const QString name = normRu(p.name);
      for (const QString &raw : categories) {
        const QString c = normRu(raw);
        if (cat.contains(c) || c.contains(cat) || name.contains(c))
          return 3;
        if (c.contains("футбол") && (cat.contains("футбол") || name.contains("футбол"))) return 3;
        if (c.contains("брюк") && (cat.contains("брюк") || cat.contains("джинс") || name.contains("брюк") || name.contains("джинс"))) return 3;
        if (c.contains("обув") && (cat.contains("обув") || name.contains("кед") || name.contains("крос") || name.contains("бот"))) return 3;
      }
      return 0;
    };

    QVector<QPair<int, Product>> scored;
    for (const Product &p : m_products) {
      if (strictIds.contains(p.id))
        continue;

      int score = 0;
      score += categorySoft(p, categories) * 4;

      if (!favColors.isEmpty()) {
        const QString pc = normRu(p.color);
        for (const QString &c : favColors) {
          if (pc == c) score += 6;
          else if (pc.contains(c) || c.contains(pc)) score += 4;
        }
      }

      for (const QString &style : styles) {
        if (productMatchesStyle(p, style))
          score += 4;
      }

      if (crit.maxBudget > 0 && p.price <= crit.maxBudget)
        score += 2;
      if (crit.minBudget > 0 && p.price >= crit.minBudget)
        score += 1;

      if (score > 0)
        scored.push_back(qMakePair(score, p));
    }

    std::sort(scored.begin(), scored.end(), [](const auto &a, const auto &b) {
      if (a.first != b.first)
        return a.first > b.first;
      return a.second.price < b.second.price;
    });

    QVector<Product> out;
    for (int i = 0; i < scored.size() && i < limit; ++i)
      out.push_back(scored[i].second);

    return out;
  }





  QWidget* createHomeCollectionCard(const Product &p, const QString &caption)
  {
    QFrame *card = new QFrame();
    card->setStyleSheet("QFrame{background:#FFFFFF;border:1px solid #D7D1CC;border-radius:12px;} QLabel{background:transparent;border:none;}");
    card->setFixedSize(170, 142);
    card->setCursor(Qt::PointingHandCursor);
    card->setProperty("home_collection_product_id", p.id);
    card->installEventFilter(this);

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(5);

    QLabel *image = new QLabel(card);
    image->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    image->setAlignment(Qt::AlignCenter);
    image->setFixedSize(158, 92);
    image->setStyleSheet("background:transparent;border:none;");
    if (!p.imagePath.isEmpty()) {
      QPixmap pix = cachedScaledPixmap(p.imagePath, image->size(), Qt::KeepAspectRatioByExpanding);
      if (!pix.isNull())
        image->setPixmap(pix);
      else
        image->setText("фото");
    } else {
      image->setText("фото");
    }
    layout->addWidget(image, 0, Qt::AlignHCenter);

    QLabel *text = new QLabel(caption, card);
    text->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    text->setWordWrap(true);
    text->setAlignment(Qt::AlignCenter);
    text->setStyleSheet("color:#2A2522;font-size:10px;font-weight:800;line-height:1.1em;");
    layout->addWidget(text);
    return card;
  }

  QWidget* createHomeRecommendationCard(const Product &p)
  {
    ProductCardFrame *card = new ProductCardFrame();
    card->setFixedSize(322, 214);
    card->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    card->setStyleSheet("QFrame{background:#FFFFFF;border:1px solid #CFC8C3;border-radius:14px;} QLabel{background:transparent;border:none;}");

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(6);

    QLabel *image = new QLabel(card);
    image->setAlignment(Qt::AlignCenter);
    image->setFixedHeight(106);
    image->setStyleSheet("background:transparent;border:none;");
    if (!p.imagePath.isEmpty()) {
      QPixmap pix = cachedScaledPixmap(p.imagePath, QSize(290, 104));
      if (!pix.isNull())
        image->setPixmap(pix);
      else
        image->setText("фото");
    } else {
      image->setText("фото");
    }
    layout->addWidget(image);

    QLabel *name = new QLabel(p.name, card);
    name->setWordWrap(true);
    name->setMinimumHeight(26);
    name->setStyleSheet("color:#201D1C;font-size:13px;font-weight:700;");
    layout->addWidget(name);

    QLabel *info = new QLabel(QString("%1 • %2 • %3").arg(p.category).arg(p.color).arg(p.size), card);
    info->setWordWrap(true);
    info->setStyleSheet("color:#5E5854;font-size:12px;");
    layout->addWidget(info);
    layout->addStretch();

    QPushButton *quickBtn = new QPushButton("Посмотреть", card);
    quickBtn->setCursor(Qt::PointingHandCursor);
    quickBtn->setFixedSize(96, 26);
    quickBtn->setStyleSheet("QPushButton{background:#F3F0ED;border:1px solid #CFC8C3;border-radius:8px;color:#2F2A28;font-size:11px;font-weight:800;padding:0 8px;}QPushButton:hover{background:#ECE6E1;}QPushButton:pressed{background:#E1D8D2;}");
    layout->addWidget(quickBtn, 0, Qt::AlignRight);

    QObject::connect(quickBtn, &QPushButton::clicked, card, [this, p]() {
      ProductDetailsDialog dlg(p, m_cart, this);
      dlg.exec();
    });
    return card;
  }

  QWidget* createProductCard(const Product &p)
  {
    ProductCardFrame *card = new ProductCardFrame();
    const QString metaText = QString("%1 • %2 • %3")
                   .arg(p.category)
                   .arg(p.color)
                   .arg(p.size);
    const bool needsTallMeta = normRu(p.category).contains("голов") ||
                  p.size.trimmed().length() > 4 ||
                  metaText.length() > 27;

    // Single card size: three cards fit fully on the home page,
    // and the catalog grid also stays stable.
    card->setFixedSize(274, needsTallMeta ? 458 : 430);
    card->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);

    QLabel *image = new QLabel(card);

    image->setAlignment(Qt::AlignCenter);
    image->setFixedHeight(150);
    image->setObjectName("ProductImage");
    image->setStyleSheet("background:transparent; border:none;");

    if (!p.imagePath.isEmpty()) {
      QPixmap pix = cachedScaledPixmap(p.imagePath, QSize(220, 146));
      if (!pix.isNull()) {
        image->setPixmap(pix);
      } else {
        image->setText("фото");
      }
    } else {
      image->setText("фото");
    }
    layout->addWidget(image);

    QLabel *name = new QLabel(p.name, card);
    name->setObjectName("ProductTitle");
    name->setWordWrap(true);
    name->setMinimumHeight(42);
    layout->addWidget(name);

    QLabel *info = new QLabel(metaText, card);
    info->setObjectName("ProductMeta");
    info->setStyleSheet("color:#8C8F95;");
    info->setWordWrap(true);
    if (needsTallMeta)
      info->setMinimumHeight(38);
    layout->addWidget(info);
    layout->addStretch(1);

    QLabel *price = new QLabel(
      QString("%1 ₽").arg(QString::number(p.price, 'f', 0)),
      card);
    price->setObjectName("ProductPrice");
    price->setProperty("price", true); // used by QSS
    layout->addWidget(price);

    QPushButton *detailsBtn = new QPushButton("Подробнее", card);
    detailsBtn->setObjectName("PrimaryButton");
    detailsBtn->setMinimumHeight(38);
    layout->addWidget(detailsBtn);

    connect(detailsBtn, &QPushButton::clicked, card, [this, p]() {
      ProductDetailsDialog dlg(p, m_cart, this);
      dlg.exec();
    });

    return card;
  }
  // ===================== Lookbook: database and UI =====================
  void clearLookbookLayout(QLayout *layout)
  {
    if (!layout) return;
    while (QLayoutItem *item = layout->takeAt(0)) {
      if (item->layout())
        clearLookbookLayout(item->layout());
      if (item->widget())
        item->widget()->deleteLater();
      delete item;
    }
  }

  QLabel *createLookbookThumb(QWidget *parent, const Product *product, int w = 76, int h = 76) const
  {
    QLabel *img = new QLabel(parent);
    img->setFixedSize(w, h);
    img->setAlignment(Qt::AlignCenter);
    img->setObjectName("LookbookThumb");
    img->setStyleSheet("QLabel#LookbookThumb{background:transparent;border:none;color:#A29A92;}");
    if (product) {
      QPixmap px = cachedScaledPixmap(product->imagePath, QSize(w - 10, h - 10));
      if (!px.isNull())
        img->setPixmap(px);
      else
        img->setText("фото");
    } else {
      img->setText("пусто");
    }
    return img;
  }

  QVector<Product> fallbackLookbookProducts(int offset, int count) const
  {
    QVector<Product> out;
    if (m_products.isEmpty() || count <= 0)
      return out;
    for (int i = 0; i < count; ++i)
      out.push_back(m_products[(offset + i) % m_products.size()]);
    return out;
  }

  QVector<Product> findLookbookProducts(const QStringList &keywords, int count) const
  {
    QVector<Product> out;
    QSet<int> used;
    for (const Product &p : m_products) {
      const QString hay = (p.name + " " + p.category + " " + p.color + " " + p.styleTag + " " + p.season).toLower();
      for (QString key : keywords) {
        key = key.trimmed().toLower();
        if (!key.isEmpty() && hay.contains(key) && !used.contains(p.id)) {
          out.push_back(p);
          used.insert(p.id);
          break;
        }
      }
      if (out.size() >= count)
        break;
    }
    if (out.size() < count) {
      for (const Product &p : m_products) {
        if (!used.contains(p.id)) {
          out.push_back(p);
          used.insert(p.id);
          if (out.size() >= count)
            break;
        }
      }
    }
    return out;
  }

  QVector<Product> findLookbookProductsForSlots(const QStringList &keywords) const
  {
    QVector<Product> candidates = findLookbookProducts(keywords, qMax(24, static_cast<int>(m_products.size())));
    QVector<const Product*> picked(4, nullptr);
    QSet<int> used;

    auto pickForSlot = [&](int pos, const QVector<Product> &pool, bool strictSlot) {
      if (picked[pos])
        return;
      for (const Product &p : pool) {
        if (used.contains(p.id))
          continue;
        if (strictSlot && !productAllowedForLookbookSlot(pos, p))
          continue;
        picked[pos] = &p;
        used.insert(p.id);
        return;
      }
    };

    for (int pos = 0; pos < 4; ++pos)
      pickForSlot(pos, candidates, true);
    for (int pos = 0; pos < 4; ++pos)
      pickForSlot(pos, m_products, true);

    QVector<Product> out;
    for (const Product *p : picked) {
      if (p)
        out.push_back(*p);
    }
    return out;
  }

  QVector<Product> loadLookbookProjectProducts(int projectId) const
  {
    QVector<Product> out;
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen())
      return out;

    QSqlQuery q(db);
    q.prepare(R"(
      SELECT p.id, p.name, p.category, p.color, p.size, p.price,
          IFNULL(p.image_path,''), IFNULL(p.material,''), IFNULL(p.season,''), IFNULL(p.style_tag,'')
      FROM lookbook_items li
      LEFT JOIN products p ON p.id = li.product_id
      WHERE li.project_id = :id
      ORDER BY li.position ASC
    )");
    q.bindValue(":id", projectId);
    if (!q.exec())
      return out;

    while (q.next()) {
      Product p;
      p.id = q.value(0).toInt();
      p.name = q.value(1).toString();
      p.category = q.value(2).toString();
      p.color = q.value(3).toString();
      p.size = q.value(4).toString();
      p.price = q.value(5).toDouble();
      p.imagePath = q.value(6).toString();
      p.material = q.value(7).toString();
      p.season = q.value(8).toString();
      p.styleTag = q.value(9).toString();
      out.push_back(p);
    }
    return out;
  }

  QWidget *createAutoScrollingLabel(const QString &text,
                    QWidget *parent,
                    int fixedWidth = 130,
                    int fixedHeight = 22,
                    const QString &labelStyle = QString()) const
  {
    QWidget *host = new QWidget(parent);
    host->setObjectName("AutoScrollTextHost");
    host->setStyleSheet("QWidget#AutoScrollTextHost{background:transparent;border:none;}");
    host->setFixedHeight(fixedHeight);
    if (fixedWidth > 0)
      host->setFixedWidth(fixedWidth);

    QVBoxLayout *hostLay = new QVBoxLayout(host);
    hostLay->setContentsMargins(0, 0, 0, 0);
    hostLay->setSpacing(0);

    QScrollArea *area = new QScrollArea(host);
    area->setFrameShape(QFrame::NoFrame);
    area->setWidgetResizable(false);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->setFixedHeight(fixedHeight);
    area->setStyleSheet("QScrollArea{background:transparent;border:none;}");
    hostLay->addWidget(area);

    QLabel *label = new QLabel(text + " ", area);
    label->setWordWrap(false);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setStyleSheet((labelStyle.isEmpty() ? QString("color:#7B746E;font-size:12px;") : labelStyle) +
              "background:transparent;border:none;");
    const int textWidth = label->fontMetrics().horizontalAdvance(label->text()) + 12;
    label->setMinimumWidth(qMax(textWidth, fixedWidth));
    label->setFixedHeight(fixedHeight);
    area->setWidget(label);

    QTimer *timer = new QTimer(area);
    connect(timer, &QTimer::timeout, area, [area]() {
      QScrollBar *bar = area->horizontalScrollBar();
      if (!bar || bar->maximum() <= 0) {
        if (bar) bar->setValue(0);
        return;
      }
      int next = bar->value() + 1;
      if (next > bar->maximum())
        next = 0;
      bar->setValue(next);
    });
    timer->start(55);

    return host;
  }

  QVector<int> chooseLookbookProductIdsForSlots(const QVector<Product> &items) const
  {
    QVector<int> slotIds(4, 0);
    QSet<int> used;

    for (int pos = 0; pos < 4; ++pos) {
      for (const Product &p : items) {
        if (used.contains(p.id))
          continue;
        if (productAllowedForLookbookSlot(pos, p)) {
          slotIds[pos] = p.id;
          used.insert(p.id);
          break;
        }
      }
    }

    for (int pos = 0; pos < 4; ++pos) {
      if (slotIds[pos] > 0)
        continue;
      for (const Product &p : items) {
        if (!used.contains(p.id)) {
          slotIds[pos] = p.id;
          used.insert(p.id);
          break;
        }
      }
    }

    return slotIds;
  }

  int createLookbookCopyFromProducts(const QString &sourceTitle,
                    const QString &description,
                    const QVector<Product> &items)
  {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen())
      return 0;

    QString copyTitle = sourceTitle.trimmed();
    if (copyTitle.isEmpty())
      copyTitle = "Лукбук";
    copyTitle = "Копия: " + copyTitle;

    QSqlQuery q(db);
    q.prepare("INSERT INTO lookbook_projects(username, title, created_at, description, access_level, comments_enabled, ratings_enabled, currency, show_prices, affiliate_enabled) "
        "VALUES(:u, :t, :c, :d, 'private', 1, 1, 'RUB', 1, 0)");
    q.bindValue(":u", m_username);
    q.bindValue(":t", copyTitle);
    q.bindValue(":c", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm"));
    q.bindValue(":d", description);
    if (!q.exec()) {
      showLookbookWarning("Лукбук", "Не удалось создать копию: " + q.lastError().text());
      return 0;
    }

    const int newId = q.lastInsertId().toInt();
    if (newId <= 0)
      return 0;

    const QVector<int> slotIds = chooseLookbookProductIdsForSlots(items);
    for (int pos = 0; pos < slotIds.size(); ++pos) {
      if (slotIds[pos] <= 0)
        continue;
      QSqlQuery itemQ(db);
      itemQ.prepare("INSERT INTO lookbook_items(project_id, position, product_id) VALUES(:pid, :pos, :prod)");
      itemQ.bindValue(":pid", newId);
      itemQ.bindValue(":pos", pos);
      itemQ.bindValue(":prod", slotIds[pos]);
      itemQ.exec();
    }

    refreshLookbookProjectsList();
    refreshLookbookShowcaseSections();
    return newId;
  }

  void showLookbookReadonlyDialog(const QString &title,
                  const QString &description,
                  const QVector<Product> &items,
                  bool templateMode)
  {
    QDialog dlg(this);
    dlg.setObjectName("LookbookPreviewDialog");
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setAttribute(Qt::WA_TranslucentBackground, true);
    dlg.setModal(true);
    dlg.resize(1040, 760);
    dlg.setStyleSheet(
     "QDialog#LookbookPreviewDialog{background:#00000000;}"
     "QFrame#LookbookPreviewCard{background:#F4EFEA;border:1px solid #E1D6CE;border-radius:22px;}"
     "QLabel#LbPreviewTitle{font-size:24px;font-weight:900;color:#1F1F1F;background:transparent;border:none;}"
     "QLabel#LbPreviewDescription{font-size:13px;color:#6D625A;background:transparent;border:none;}"
     "QLabel#LbPreviewHint{font-size:12px;color:#8A8178;background:transparent;border:none;}"
     "QWidget#LbReadonlyEditorMain{background:transparent;border:none;}"
     "QFrame#LbSlotCard{background:#FFFFFF;border:1px solid #E4DAD2;border-radius:16px;}"
     "QLabel#LbSlotTitle{font-size:14px;font-weight:800;color:#1F1F1F;}"
     "QLabel#LbSlotImage{background:#F5F1EC;border:1px solid #EDE3DB;border-radius:14px;color:#A39990;}"
     "QLabel#LbSlotName{font-size:17px;font-weight:800;color:#1B1B1B;line-height:1.15;}"
     "QLabel#LbSlotMeta{font-size:14px;color:#554E48;line-height:1.25;}"
     "QLabel#LbSlotActionLabel{background:#F7F2EC;border:1px solid #E0D6CD;border-radius:10px;padding:7px 13px;min-width:82px;font-weight:700;color:#3B352F;}"
     "QFrame#LbDetailsCard{background:#FFFFFF;border:1px solid #CDBFB4;border-radius:16px;}"
     "QLabel#LbDetailsTitle{font-size:18px;font-weight:800;color:#222222;}"
     "QLabel#LbDetailsSub{font-size:13px;font-weight:700;color:#37312D;}"
     "QLabel#LbDetailsValue{font-size:13px;color:#2B2521;font-weight:800;}"
     "QLabel#LbTag{background:#F7F2EC;border:1px solid #E4DAD2;border-radius:10px;padding:4px 8px;color:#5D534B;}"
     "QLabel#LbTemplateComment{background:#F8F3EE;border:1px solid #E5DBD2;border-radius:10px;padding:10px;color:#5A514A;font-size:12px;}"
     "QFrame#LbMiniRec{background:#F8F3EE;border:1px solid #E5DBD2;border-radius:12px;}"
     "#LbSlotTitle,#LbSlotName,#LbSlotMeta,#LbDetailsTitle,#LbDetailsSub,#LbDetailsValue{background:transparent;border:none;}"
     "QPushButton#CopyLookbookBtn{background:#6F4B37;color:white;border:none;border-radius:12px;min-height:40px;padding:0 16px;font-weight:900;}"
     "QPushButton#CopyLookbookBtn:hover{background:#5F3E2D;}"
     "QPushButton#CloseLookbookBtn{background:#FFFFFF;color:#3A342F;border:1px solid #DDD2CA;border-radius:12px;min-height:40px;padding:0 16px;font-weight:800;}"
     "QPushButton#CloseLookbookBtn:hover{background:#FBF7F3;}"
     "QPushButton#LbPreviewXBtn{background:transparent;color:#1F1A17;border:none;border-radius:16px;font-family:'Arial';font-size:18px;font-weight:900;min-width:32px;min-height:32px;max-width:32px;max-height:32px;padding:0;}"
     "QPushButton#LbPreviewXBtn:hover{background:#E8DDD4;color:#14100D;}QPushButton#LbPreviewXBtn:pressed{background:#DED1C6;color:#14100D;}"
    );

    QVBoxLayout *outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(0, 0, 0, 0);
    QFrame *card = new QFrame(&dlg);
    card->setObjectName("LookbookPreviewCard");
    outer->addWidget(card);

    QVBoxLayout *root = new QVBoxLayout(card);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(12);

    QHBoxLayout *header = new QHBoxLayout();
    QLabel *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("LbPreviewTitle");
    titleLabel->setWordWrap(true);
    header->addWidget(titleLabel, 1);
    QPushButton *xBtn = new RoundCloseButton(card);
    xBtn->setObjectName("LbPreviewXBtn");
    xBtn->setFixedSize(32, 32);
    xBtn->setCursor(Qt::PointingHandCursor);
    header->addWidget(xBtn, 0, Qt::AlignTop);
    root->addLayout(header);

    const QString descText = description.trimmed().isEmpty()
      ? QString("Описание пока не добавлено.")
      : description.trimmed();
    QLabel *descLabel = new QLabel(descText, card);
    descLabel->setObjectName("LbPreviewDescription");
    descLabel->setWordWrap(true);
    root->addWidget(descLabel);

    QLabel *hint = new QLabel(templateMode
                 ? "Готовый лукбук открыт только для просмотра. Чтобы редактировать его под себя, создай копию."
                 : "Это режим просмотра без редактирования. Для изменений можно создать копию лукбука.", card);
    hint->setObjectName("LbPreviewHint");
    hint->setWordWrap(true);
    root->addWidget(hint);

    QScrollArea *scroll = new QScrollArea(card);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea{background:transparent;border:none;}");
    QWidget *body = new QWidget(scroll);
    body->setStyleSheet("background:transparent;border:none;");
    QHBoxLayout *bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(16);

    const QVector<int> slotIds = chooseLookbookProductIdsForSlots(items);
    auto productById = [&](int id) -> const Product* {
      for (const Product &p : items) {
        if (p.id == id)
          return &p;
      }
      return nullptr;
    };

    auto makePreviewClickable = [&](QWidget *widget, int productId) {
      if (!widget || productId <= 0)
        return;
      widget->setCursor(Qt::PointingHandCursor);
      widget->setProperty("lb_preview_product_id", productId);
      widget->installEventFilter(this);
    };

    QWidget *editorMain = new QWidget(body);
    editorMain->setObjectName("LbReadonlyEditorMain");
    editorMain->setMinimumSize(640, 560);
    QGridLayout *grid = new QGridLayout(editorMain);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(14);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);

    auto makeReadonlySlot = [&](const QString &slotTitle, int pos, const Product *p) -> QFrame* {
      QFrame *slot = new QFrame(editorMain);
      slot->setObjectName("LbSlotCard");
      slot->setMinimumSize(292, 260);
      QVBoxLayout *sl = new QVBoxLayout(slot);
      sl->setContentsMargins(12, 12, 12, 12);
      sl->setSpacing(8);

      QLabel *titleLabel = new QLabel(slotTitle, slot);
      titleLabel->setObjectName("LbSlotTitle");
      sl->addWidget(titleLabel);

      QLabel *img = new QLabel(slot);
      img->setObjectName("LbSlotImage");
      img->setAlignment(Qt::AlignCenter);
      img->setFixedHeight(128);
      if (p) {
        QPixmap px = lookbookProductPixmap(*p, QSize(210, 145));
        if (!px.isNull()) {
          img->setPixmap(px);
          img->setText(QString());
        } else {
          img->setText("фото");
        }
      } else {
        img->setText("Позиция\nне подобрана");
      }
      sl->addWidget(img);

      QHBoxLayout *infoActionsRow = new QHBoxLayout();
      infoActionsRow->setContentsMargins(0, 0, 0, 0);
      infoActionsRow->setSpacing(10);

      QVBoxLayout *textCol = new QVBoxLayout();
      textCol->setContentsMargins(pos == 1 ? 72 : (pos == 3 ? 8 : 0), 0, 0, 0);
      textCol->setSpacing(3);

      QLabel *name = new QLabel(p ? p->name : "Пока ничего не выбрано", slot);
      name->setObjectName("LbSlotName");
      name->setWordWrap(true);
      name->setMinimumHeight(24);
      textCol->addWidget(name);

      QLabel *meta = new QLabel(p ? QString("Категория: %1\nЦвет: %2\nЦена: %3 ₽")
                       .arg(p->category, p->color)
                       .arg(QString::number(p->price, 'f', 0))
                    : QString(), slot);
      meta->setObjectName("LbSlotMeta");
      meta->setWordWrap(true);
      meta->setMinimumHeight(50);
      textCol->addWidget(meta);

      QLabel *state = new QLabel(p ? "В образе" : "Пусто", slot);
      state->setObjectName("LbSlotActionLabel");
      state->setAlignment(Qt::AlignCenter);

      if (pos == 0) {
        infoActionsRow->addLayout(textCol, 0);
        infoActionsRow->addSpacing(8);
        infoActionsRow->addWidget(state, 0, Qt::AlignBottom | Qt::AlignLeft);
        infoActionsRow->addStretch(1);
      } else {
        infoActionsRow->addLayout(textCol, 1);
        infoActionsRow->addWidget(state, 0, Qt::AlignBottom | Qt::AlignRight);
      }
      sl->addLayout(infoActionsRow);

      if (p) {
        makePreviewClickable(slot, p->id);
        makePreviewClickable(titleLabel, p->id);
        makePreviewClickable(img, p->id);
        makePreviewClickable(name, p->id);
        makePreviewClickable(meta, p->id);
        makePreviewClickable(state, p->id);
      }

      return slot;
    };

    for (int pos = 0; pos < 4; ++pos) {
      const Product *p = productById(slotIds[pos]);
      QFrame *cell = makeReadonlySlot(lookbookSlotTitle(pos), pos, p);
      grid->addWidget(cell, pos / 2, pos % 2);
    }

    bodyLay->addWidget(editorMain, 1);

    QFrame *detailsCard = new QFrame(body);
    detailsCard->setObjectName("LbDetailsCard");
    detailsCard->setFixedWidth(290);
    detailsCard->setMinimumHeight(560);
    QVBoxLayout *detailsLay = new QVBoxLayout(detailsCard);
    detailsLay->setContentsMargins(14, 14, 14, 14);
    detailsLay->setSpacing(10);

    QLabel *detailsTitle = new QLabel("Детали лукбука", detailsCard);
    detailsTitle->setObjectName("LbDetailsTitle");
    detailsLay->addWidget(detailsTitle);

    if (templateMode) {
      QLabel *ratingTitle = new QLabel("Оценка", detailsCard);
      ratingTitle->setObjectName("LbDetailsSub");
      detailsLay->addWidget(ratingTitle);

      QLabel *ratingValue = new QLabel("4.8 / 5 (42 оценки)", detailsCard);
      ratingValue->setObjectName("LbDetailsValue");
      detailsLay->addWidget(ratingValue);
    }

    double total = 0.0;
    for (int id : slotIds) {
      if (const Product *p = productById(id))
        total += p->price;
    }

    QLabel *costTitle = new QLabel("Стоимость", detailsCard);
    costTitle->setObjectName("LbDetailsSub");
    detailsLay->addWidget(costTitle);
    QLabel *costValue = new QLabel(QString("Итого: %1 ₽").arg(QString::number(total, 'f', 0)), detailsCard);
    costValue->setObjectName("LbDetailsValue");
    detailsLay->addWidget(costValue);

    if (templateMode) {
      QLabel *tagsTitle = new QLabel("Теги", detailsCard);
      tagsTitle->setObjectName("LbDetailsSub");
      detailsLay->addWidget(tagsTitle);

      QWidget *tagsHost = new QWidget(detailsCard);
      tagsHost->setStyleSheet("background:transparent;border:none;");
      QGridLayout *tagsGrid = new QGridLayout(tagsHost);
      tagsGrid->setContentsMargins(0, 0, 0, 0);
      tagsGrid->setHorizontalSpacing(6);
      tagsGrid->setVerticalSpacing(6);
      const QStringList tags = {"#минимализм", "#капсула", "#город", "#сезон", "#выходные"};
      for (int i = 0; i < tags.size(); ++i) {
        QLabel *tag = new QLabel(tags[i], tagsHost);
        tag->setObjectName("LbTag");
        tag->setAlignment(Qt::AlignCenter);
        tagsGrid->addWidget(tag, i / 2, i % 2);
      }
      detailsLay->addWidget(tagsHost);

      QLabel *commentsTitle = new QLabel("Комментарии", detailsCard);
      commentsTitle->setObjectName("LbDetailsSub");
      detailsLay->addWidget(commentsTitle);

      QLabel *comment = new QLabel("Образ собран гармонично: верх, низ и обувь поддерживают общий стиль, а аксессуар завершает комплект.", detailsCard);
      comment->setObjectName("LbTemplateComment");
      comment->setWordWrap(true);
      detailsLay->addWidget(comment);
    }

    QLabel *recTitle = new QLabel("Рекомендации", detailsCard);
    recTitle->setObjectName("LbDetailsSub");
    detailsLay->addWidget(recTitle);
    QHBoxLayout *recLay = new QHBoxLayout();

    QSet<int> usedRecIds;
    for (int id : slotIds) {
      if (id > 0)
        usedRecIds.insert(id);
    }

    auto matchesNeedles = [](const Product &product, const QStringList &needles) {
      const QString hay = normRu(product.name + " " + product.category + " " +
                   product.styleTag + " " + product.color + " " +
                   product.material + " " + product.season);
      for (const QString &needle : needles) {
        if (hay.contains(normRu(needle)))
          return true;
      }
      return false;
    };

    auto findRecProduct = [&](const QStringList &needles) -> const Product* {
      for (const Product &product : m_products) {
        if (!usedRecIds.contains(product.id) && matchesNeedles(product, needles)) {
          usedRecIds.insert(product.id);
          return &product;
        }
      }
      for (const Product &product : m_products) {
        if (!usedRecIds.contains(product.id)) {
          usedRecIds.insert(product.id);
          return &product;
        }
      }
      return nullptr;
    };

    const Product *firstRec = findRecProduct({"сум", "шарф", "рем", "аксесс", "украш"});
    const Product *secondRec = findRecProduct({"нос", "обув", "кед", "крос", "лофер", "ботин"});

    auto makeMiniRec = [&](const QString &fallbackText, const Product *product) {
      QFrame *mini = new QFrame(detailsCard);
      mini->setObjectName("LbMiniRec");
      QVBoxLayout *ml = new QVBoxLayout(mini);
      ml->setContentsMargins(8, 8, 8, 8);
      ml->setSpacing(4);
      QLabel *box = new QLabel("фото", mini);
      box->setAlignment(Qt::AlignCenter);
      box->setFixedSize(64, 64);
      box->setStyleSheet("background:#FFFFFF;border:1px solid #E5DBD2;border-radius:10px;color:#A39990;");
      if (product) {
        const QPixmap px = lookbookProductPixmap(*product, QSize(64, 64));
        if (!px.isNull()) {
          box->setPixmap(px);
          box->setText(QString());
        }
      }
      ml->addWidget(box, 0, Qt::AlignCenter);
      QLabel *tt = new QLabel(product ? product->name : fallbackText, mini);
      tt->setWordWrap(true);
      tt->setAlignment(Qt::AlignCenter);
      tt->setStyleSheet("font-size:11px;color:#564E48;");
      ml->addWidget(tt);

      if (product) {
        makePreviewClickable(mini, product->id);
        makePreviewClickable(box, product->id);
        makePreviewClickable(tt, product->id);
      }

      return mini;
    };
    recLay->addWidget(makeMiniRec("Товар", firstRec));
    recLay->addWidget(makeMiniRec("Товар", secondRec));
    detailsLay->addLayout(recLay);
    detailsLay->addStretch();
    bodyLay->addWidget(detailsCard, 0, Qt::AlignTop);

    scroll->setWidget(body);
    root->addWidget(scroll, 1);

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();
    QPushButton *copyBtn = new QPushButton("Создать копию", card);
    copyBtn->setObjectName("CopyLookbookBtn");
    copyBtn->setCursor(Qt::PointingHandCursor);
    QPushButton *closeBtn = new QPushButton("Закрыть", card);
    closeBtn->setObjectName("CloseLookbookBtn");
    closeBtn->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(copyBtn);
    buttons->addWidget(closeBtn);
    root->addLayout(buttons);

    connect(xBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(copyBtn, &QPushButton::clicked, &dlg, [&]() {
      if (items.isEmpty()) {
        showLookbookInfo("Лукбук", "В этой подборке пока нет товаров для копирования.");
        return;
      }
      const int newId = createLookbookCopyFromProducts(title, descText, items);
      if (newId > 0) {
        dlg.accept();
        openLookbookProject(newId);
      }
    });

    dlg.exec();
  }

  void showLookbookProjectPreview(int projectId)
  {
    LookbookSettings st = loadLookbookSettings(projectId);
    QVector<Product> items = loadLookbookProjectProducts(projectId);
    showLookbookReadonlyDialog(st.title, st.description, items, false);
  }

  QWidget *createLookbookProjectCard(int projectId,
                    const QString &title,
                    const QString &createdAt,
                    const QString &description,
                    int itemsCount,
                    const QVector<Product> &items)
  {
    QFrame *card = new QFrame(m_lbProjectsPage);
    card->setObjectName("LookbookProjectCard");
    card->setStyleSheet(
     "QFrame#LookbookProjectCard{background:#FFFFFF;border:1px solid #E4DAD2;border-radius:18px;}"
     "QWidget#LookbookProjectInfoPane{background:transparent;border:none;}"
     "QLabel[meta='true'],QLabel[title='true'],QLabel[description='true']{background:transparent;border:none;}"
     "QLabel[meta='true']{color:#7B746E;font-size:12px;}"
     "QLabel[title='true']{color:#232323;font-size:16px;font-weight:800;}"
    );
    card->setMinimumHeight(210);

    QHBoxLayout *root = new QHBoxLayout(card);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(14);

    QWidget *infoPane = new QWidget(card);
    infoPane->setObjectName("LookbookProjectInfoPane");
    // The increased width prevents the View button from being clipped.
    infoPane->setFixedWidth(220);
    QVBoxLayout *infoLay = new QVBoxLayout(infoPane);
    infoLay->setContentsMargins(0, 0, 0, 0);
    infoLay->setSpacing(6);

    QLabel *name = new QLabel(title, infoPane);
    name->setProperty("title", true);
    name->setWordWrap(true);
    infoLay->addWidget(name);

    const QString createdText = createdAt.isEmpty() ? QString() : ("Создан: " + createdAt);
    QLabel *date = new QLabel(createdText, infoPane);
    date->setProperty("meta", true);
    date->setFixedHeight(22);
    date->setTextInteractionFlags(Qt::NoTextInteraction);
    date->setStyleSheet("color:#7B746E;font-size:12px;background:transparent;border:none;");
    infoLay->addWidget(date);

    if (!description.trimmed().isEmpty()) {
      QLabel *desc = new QLabel(description.trimmed(), infoPane);
      desc->setProperty("description", true);
      desc->setWordWrap(true);
      desc->setStyleSheet("color:#7B746E;font-size:12px;background:transparent;border:none;");
      infoLay->addWidget(desc);
    }

    QLabel *count = new QLabel(QString("%1 предметов").arg(itemsCount), infoPane);
    count->setProperty("meta", true);
    count->setStyleSheet("color:#433E39;font-weight:700;font-size:13px;background:transparent;border:none;");
    infoLay->addWidget(count);
    infoLay->addStretch();

    QHBoxLayout *actions = new QHBoxLayout();
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(10);
    actions->setAlignment(Qt::AlignLeft);

    QPushButton *openBtn = new QPushButton("Просмотреть", infoPane);
    openBtn->setCursor(Qt::PointingHandCursor);
    openBtn->setFixedSize(126, 48);
    openBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    openBtn->setStyleSheet("QPushButton{background:#C9964E;color:white;border:none;border-radius:12px;padding:0 14px;font-size:13px;font-weight:800;}QPushButton:hover{background:#B98541;}QPushButton:pressed{background:#C9964E;color:white;}");
    actions->addWidget(openBtn);

    QToolButton *editBtn = new QToolButton(infoPane);
    editBtn->setText("");
    editBtn->setCursor(Qt::PointingHandCursor);
    editBtn->setFixedSize(42, 48);
    editBtn->setStyleSheet("QToolButton{background:#F8F3EE;border:1px solid #E1D7CF;border-radius:12px;padding:0;font-size:14px;}QToolButton:hover{background:#F1E8DF;}");
    actions->addWidget(editBtn);

    QToolButton *delBtn = new QToolButton(infoPane);
    delBtn->setText("");
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setFixedSize(42, 48);
    delBtn->setStyleSheet("QToolButton{background:#F8F3EE;border:1px solid #E1D7CF;border-radius:12px;padding:0;font-size:14px;}QToolButton:hover{background:#F1E8DF;}");
    actions->addWidget(delBtn);
    infoLay->addLayout(actions);
    root->addWidget(infoPane);

    QWidget *previewPane = new QWidget(card);
    QGridLayout *grid = new QGridLayout(previewPane);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);
    const int tileCount = qMax(4, qMin(8, items.size()));
    for (int i = 0; i < tileCount; ++i) {
      const Product *prod = (i < items.size()) ? &items[i] : nullptr;
      grid->addWidget(createLookbookThumb(previewPane, prod, 72, 72), i / 4, i % 4);
    }
    root->addWidget(previewPane, 1);

    connect(openBtn, &QPushButton::clicked, this, [this, projectId]() { showLookbookProjectPreview(projectId); });
    connect(editBtn, &QToolButton::clicked, this, [this, projectId]() { openLookbookProject(projectId); });
    connect(delBtn, &QToolButton::clicked, this, [this, projectId]() { deleteLookbookProject(projectId); });

    return card;
  }

  QWidget *createLookbookShowcaseCard(const QString &title,
                    const QString &subtitle,
                    int itemsCount,
                    const QVector<Product> &items,
                    const QString &buttonText = QString())
  {
    QFrame *card = new QFrame(m_lbProjectsPage);
    card->setObjectName("LookbookShowcaseCard");
    card->setStyleSheet(
     "QFrame#LookbookShowcaseCard{background:#FFFFFF;border:1px solid #E4DAD2;border-radius:18px;}"
     "QWidget#LookbookShowcasePreview{background:transparent;border:none;}"
     "QLabel[lbCardTitle='true'],QLabel[lbCardMeta='true']{background:transparent;border:none;}"
     "QLabel[lbCardTitle='true']{color:#232323;font-size:15px;font-weight:800;}"
     "QLabel[lbCardMeta='true']{color:#7B746E;font-size:12px;}"
    );
    card->setMinimumHeight(220);

    QVBoxLayout *root = new QVBoxLayout(card);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    QWidget *preview = new QWidget(card);
    preview->setObjectName("LookbookShowcasePreview");
    QGridLayout *grid = new QGridLayout(preview);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);
    const int thumbs = 4;
    for (int i = 0; i < thumbs; ++i) {
      const Product *prod = (i < items.size()) ? &items[i] : nullptr;
      grid->addWidget(createLookbookThumb(preview, prod, 86, 86), i / 2, i % 2);
    }
    root->addWidget(preview);

    QHBoxLayout *footer = new QHBoxLayout();
    QVBoxLayout *textCol = new QVBoxLayout();
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(3);
    QLabel *t = new QLabel(title, card);
    t->setProperty("lbCardTitle", true);
    t->setWordWrap(true);
    textCol->addWidget(t);
    if (!subtitle.isEmpty()) {
      QLabel *s = new QLabel(subtitle, card);
      s->setProperty("lbCardMeta", true);
      s->setWordWrap(true);
      textCol->addWidget(s);
    }
    footer->addLayout(textCol, 1);
    QLabel *count = new QLabel(QString("%1 предметов").arg(itemsCount), card);
    count->setProperty("lbCardMeta", true);
    footer->addWidget(count);
    root->addLayout(footer);

    if (!buttonText.isEmpty()) {
      QPushButton *btn = new QPushButton(buttonText, card);
      btn->setCursor(Qt::PointingHandCursor);
      btn->setStyleSheet("QPushButton{background:#F8F3EE;border:1px solid #E1D7CF;border-radius:10px;padding:9px 14px;font-weight:700;color:#3A342F;}QPushButton:hover{background:#F1E7DE;}");
      connect(btn, &QPushButton::clicked, this, [this, title, subtitle, items]() {
        showLookbookReadonlyDialog(title, subtitle, items, true);
      });
      root->addWidget(btn, 0, Qt::AlignLeft);
    }

    return card;
  }

  void refreshLookbookShowcaseSections(const std::function<void()> &done = std::function<void()>())
  {
    if (!m_lbTrendsLay || !m_lbCapsulesLay) {
      if (done)
        done();
      return;
    }

    const int generation = ++m_lookbookShowcaseGeneration;
    clearLookbookLayout(m_lbTrendsLay);
    clearLookbookLayout(m_lbCapsulesLay);

    struct ShowcaseInfo {
      QString title;
      QString subtitle;
      QStringList keywords;
      QString buttonText;
    };
    struct ShowcaseTask {
      bool trendLayout = true;
      int column = 0;
      ShowcaseInfo info;
    };

    const QVector<ShowcaseInfo> trends = {
      {"Минимализм: Беж и деним", "Спокойная база на каждый день", {"беж", "молоч", "крем", "джин", "деним", "рубаш"}, "Просмотреть"},
      {"Яркие акценты: Красный", "Акцентные вещи для смелых образов", {"крас", "синий", "ярк", "персик", "терракот"}, "Просмотреть"},
      {"Винтажный шик: 70-е", "Теплые оттенки и ретро-силуэты", {"терракот", "карамел", "олив", "брюк", "юб"}, "Просмотреть"}
    };
    const QVector<ShowcaseInfo> capsules = {
      {"Капсула для офиса", "Собранные вещи для рабочих будней", {"рубаш", "брюк", "юб", "кардиган", "блейз"}, "Просмотреть"},
      {"Летний weekend", "Легкие и комфортные сочетания", {"футбол", "топ", "кеды", "сум", "рубаш"}, "Просмотреть"},
      {"Теплый цвет лета", "Песочные, персиковые и оливковые акценты", {"персик", "песоч", "карамел", "олив", "молоч"}, "Просмотреть"}
    };

    QVector<ShowcaseTask> tasks;
    for (int i = 0; i < trends.size(); ++i) {
      ShowcaseTask task;
      task.trendLayout = true;
      task.column = i;
      task.info = trends[i];
      tasks.push_back(task);
    }
    for (int i = 0; i < capsules.size(); ++i) {
      ShowcaseTask task;
      task.trendLayout = false;
      task.column = i;
      task.info = capsules[i];
      tasks.push_back(task);
    }

    auto index = std::make_shared<int>(0);
    auto step = std::make_shared<std::function<void()>>();
    *step = [this, generation, tasks, index, step, done]() {
      if (generation != m_lookbookShowcaseGeneration) {
        *step = std::function<void()>();
        if (done)
          done();
        return;
      }

      if (*index >= tasks.size()) {
        *step = std::function<void()>();
        if (done)
          done();
        return;
      }

      const ShowcaseTask task = tasks[*index];
      const ShowcaseInfo info = task.info;
      QVector<Product> items = findLookbookProductsForSlots(info.keywords);
      QGridLayout *target = task.trendLayout ? m_lbTrendsLay : m_lbCapsulesLay;
      if (target) {
        target->addWidget(createLookbookShowcaseCard(info.title, info.subtitle, items.size(), items, info.buttonText),
                 0,
                 task.column);
      }
      ++(*index);
      QTimer::singleShot(0, this, *step);
    };

    QTimer::singleShot(0, this, *step);
  }

  void showLookbookProjects()
  {
    if (!m_lookbookStack) return;
    m_lookbookStack->setCurrentIndex(0);
    refreshLookbookProjectsList();
    refreshLookbookShowcaseSections();
  }

  void refreshLookbookProjectsList(const std::function<void()> &done = std::function<void()>())
  {
    if (!m_lbProjectsListLay) {
      if (done)
        done();
      return;
    }

    const int generation = ++m_lookbookBuildGeneration;
    clearLookbookLayout(m_lbProjectsListLay);

    struct ProjectInfo {
      int projectId = 0;
      QString title;
      QString createdAt;
      QString description;
      int itemsCount = 0;
      QVector<Product> items;
    };
    QVector<ProjectInfo> projects;

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
      if (done)
        done();
      return;
    }

    QSqlQuery q(db);
    q.prepare(R"(
      SELECT p.id,
          p.title,
          IFNULL(p.created_at,''),
          IFNULL(p.description,''),
          (SELECT COUNT(*) FROM lookbook_items li WHERE li.project_id = p.id) AS items_count
      FROM lookbook_projects p
      WHERE p.username = :u
    )");
    q.bindValue(":u", m_username);
    if (!q.exec()) {
      if (done)
        done();
      return;
    }

    const QString filter = m_lbSearchEdit ? m_lbSearchEdit->text().trimmed().toLower() : QString();
    while (q.next()) {
      ProjectInfo info;
      info.projectId = q.value(0).toInt();
      info.title = q.value(1).toString();
      info.createdAt = q.value(2).toString();
      info.description = q.value(3).toString();
      info.itemsCount = q.value(4).toInt();
      info.items = loadLookbookProjectProducts(info.projectId);

      QString hay = (info.title + " " + info.createdAt + " " + info.description).toLower();
      if (!filter.isEmpty() && !hay.contains(filter))
        continue;

      projects.push_back(info);
    }

    const QString sortMode = m_lbSortBox ? m_lbSortBox->currentText() : QString();
    std::sort(projects.begin(), projects.end(), [&](const ProjectInfo &a, const ProjectInfo &b) {
      if (sortMode == "По количеству вещей") {
        if (a.itemsCount != b.itemsCount) return a.itemsCount > b.itemsCount;
        return a.projectId > b.projectId;
      }
      if (sortMode == "По названию") {
        return a.title.toLower() < b.title.toLower();
      }
      return a.projectId > b.projectId;
    });

    if (projects.isEmpty()) {
      QFrame *empty = new QFrame(m_lbProjectsPage);
      empty->setStyleSheet("QFrame{background:#FFFFFF;border:1px dashed #DACFC6;border-radius:18px;} QLabel{color:#7E7771;font-size:14px;}");
      QVBoxLayout *lay = new QVBoxLayout(empty);
      lay->setContentsMargins(18, 22, 18, 22);
      QLabel *msg = new QLabel("Пока нет лукбуков. Создай первый сет и он появится здесь красивой карточкой.", empty);
      msg->setWordWrap(true);
      lay->addWidget(msg);
      m_lbProjectsListLay->addWidget(empty, 0, 0, 1, 3);
      if (done)
        done();
      return;
    }

    if (m_lbProjectsPage)
      m_lbProjectsPage->setUpdatesEnabled(false);

    auto index = std::make_shared<int>(0);
    auto step = std::make_shared<std::function<void()>>();
    *step = [this, generation, projects, index, step, done]() {
      if (generation != m_lookbookBuildGeneration) {
        if (m_lbProjectsPage)
          m_lbProjectsPage->setUpdatesEnabled(true);
        *step = std::function<void()>();
        if (done)
          done();
        return;
      }

      const int chunkSize = 2;
      int added = 0;
      while (*index < projects.size() && added < chunkSize) {
        const auto pr = projects[*index];
        QWidget *card = createLookbookProjectCard(pr.projectId, pr.title, pr.createdAt, pr.description, pr.itemsCount, pr.items);
        m_lbProjectsListLay->addWidget(card, *index / 3, *index % 3);
        ++(*index);
        ++added;
      }

      if (*index >= projects.size()) {
        if (m_lbProjectsPage)
          m_lbProjectsPage->setUpdatesEnabled(true);
        *step = std::function<void()>();
        if (done)
          done();
        return;
      }

      QTimer::singleShot(0, this, *step);
    };

    QTimer::singleShot(0, this, *step);
  }

  void positionLookbookCenterOverlay()
  {
    if (!m_lbEditorMain || !m_lbCenterOverlay)
      return;

    const int x = (m_lbEditorMain->width() - m_lbCenterOverlay->width()) / 2;
    const int y = (m_lbEditorMain->height() - m_lbCenterOverlay->height()) / 2;
    m_lbCenterOverlay->move(qMax(0, x), qMax(0, y));
    m_lbCenterOverlay->raise();
  }

  void openLookbookAddMenu(QWidget *anchor = nullptr)
  {
    Q_UNUSED(anchor);
    if (m_lbCurrentProjectId <= 0) {
      showLookbookInfo("Лукбук", "Сначала создай или открой лукбук.");
      return;
    }

    QDialog dlg(this);
    dlg.setObjectName("LookbookSlotDialog");
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setAttribute(Qt::WA_TranslucentBackground, true);
    dlg.setModal(true);
    dlg.setFixedWidth(460);
    dlg.setStyleSheet(
     "QDialog#LookbookSlotDialog{background:#00000000;}"
     "QFrame#LookbookSlotCard{background:#F4EFEA;border:1px solid #E1D6CE;border-radius:22px;}"
     "QLabel#SlotDialogTitle{font-size:22px;font-weight:900;color:#151515;background:transparent;border:none;}"
     "QLabel#SlotDialogSub{font-size:13px;color:#6D625A;background:transparent;border:none;}"
     "QPushButton#SlotDialogClose{background:transparent;color:#1F1A17;border:none;border-radius:16px;font-family:'Arial';font-size:18px;font-weight:900;min-width:32px;min-height:32px;max-width:32px;max-height:32px;padding:0;}"
     "QPushButton#SlotDialogClose:hover{background:#E8DDD4;color:#14100D;}QPushButton#SlotDialogClose:pressed{background:#DED1C6;color:#14100D;}"
     "QPushButton#SlotChoiceBtn{background:#FFFFFF;color:#2C2723;border:1px solid #E2D8CF;border-radius:14px;min-height:48px;padding:0 16px;text-align:left;font-weight:800;}"
     "QPushButton#SlotChoiceBtn:hover{background:#FBF7F3;border:1px solid #D2A08A;}"
     "QPushButton#SlotCancelBtn{background:#FFFFFF;color:#3A342F;border:1px solid #DDD2CA;border-radius:12px;min-height:40px;padding:0 18px;font-weight:800;}"
     "QPushButton#SlotCancelBtn:hover{background:#FBF7F3;}"
    );

    QVBoxLayout *outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(0, 0, 0, 0);
    QFrame *card = new QFrame(&dlg);
    card->setObjectName("LookbookSlotCard");
    outer->addWidget(card);

    QVBoxLayout *root = new QVBoxLayout(card);
    root->setContentsMargins(22, 20, 22, 20);
    root->setSpacing(12);

    QHBoxLayout *header = new QHBoxLayout();
    QLabel *title = new QLabel("Добавить вещь", card);
    title->setObjectName("SlotDialogTitle");
    header->addWidget(title, 1);
    QPushButton *xBtn = new RoundCloseButton(card);
    xBtn->setObjectName("SlotDialogClose");
    xBtn->setFixedSize(32, 32);
    xBtn->setCursor(Qt::PointingHandCursor);
    header->addWidget(xBtn);
    root->addLayout(header);

    QLabel *sub = new QLabel("Выбери секцию лукбука, куда нужно добавить товар.", card);
    sub->setObjectName("SlotDialogSub");
    sub->setWordWrap(true);
    root->addWidget(sub);

    int picked = -1;
    const QStringList names = {"Головной убор", "Верх", "Низ", "Обувь"};
    for (int i = 0; i < names.size(); ++i) {
      QPushButton *btn = new QPushButton(QString("%1 %2").arg(i + 1).arg(names[i]), card);
      btn->setObjectName("SlotChoiceBtn");
      btn->setCursor(Qt::PointingHandCursor);
      root->addWidget(btn);
      connect(btn, &QPushButton::clicked, &dlg, [&, i]() { picked = i; dlg.accept(); });
    }

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();
    QPushButton *cancel = new QPushButton("Отмена", card);
    cancel->setObjectName("SlotCancelBtn");
    cancel->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(cancel);
    root->addLayout(buttons);

    connect(xBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted && picked >= 0)
      pickProductForPosition(picked);
  }

  void saveCurrentLookbookTitle()
  {
    if (m_lbCurrentProjectId <= 0 || !m_lbProjectTitleEdit)
      return;

    QString title = m_lbProjectTitleEdit->text().trimmed();
    if (title.isEmpty())
      title = "Мой лукбук";

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen())
      return;

    QSqlQuery q(db);
    q.prepare("UPDATE lookbook_projects SET title=:t WHERE id=:id AND username=:u");
    q.bindValue(":t", title);
    q.bindValue(":id", m_lbCurrentProjectId);
    q.bindValue(":u", m_username);
    if (!q.exec()) {
      showLookbookWarning("Лукбук", "Не удалось сохранить название: " + q.lastError().text());
      return;
    }

    if (m_lbProjectTitleEdit->text() != title)
      m_lbProjectTitleEdit->setText(title);
    if (m_lbUpdatedLabel)
      m_lbUpdatedLabel->setText("Обновлено: " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm"));

    refreshLookbookProjectsList();
  }

  LookbookSettings loadLookbookSettings(int projectId) const
  {
    LookbookSettings st;
    st.projectId = projectId;
    st.title = m_lbProjectTitleEdit ? m_lbProjectTitleEdit->text().trimmed() : QString("Мой лукбук");

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen() || projectId <= 0)
      return st;

    QSqlQuery q(db);
    q.prepare(
     "SELECT title, IFNULL(cover_path,''), IFNULL(description,''), "
     "IFNULL(access_level,'private'), comments_enabled, ratings_enabled, "
     "IFNULL(currency,'RUB'), show_prices, affiliate_enabled, IFNULL(share_token,'') "
     "FROM lookbook_projects WHERE id=:id AND username=:u");
    q.bindValue(":id", projectId);
    q.bindValue(":u", m_username);
    if (q.exec() && q.next()) {
      st.title = q.value(0).toString();
      st.coverPath = q.value(1).toString();
      st.description = q.value(2).toString();
      st.accessLevel = q.value(3).toString().trimmed();
      if (st.accessLevel != "private" && st.accessLevel != "link" && st.accessLevel != "public")
        st.accessLevel = "private";
      st.commentsEnabled = q.value(4).isNull() ? true : (q.value(4).toInt() != 0);
      st.ratingsEnabled = q.value(5).isNull() ? true : (q.value(5).toInt() != 0);
      st.currency = q.value(6).toString().trimmed().toUpper();
      if (st.currency != "RUB" && st.currency != "USD" && st.currency != "EUR")
        st.currency = "RUB";
      st.showPrices = q.value(7).isNull() ? true : (q.value(7).toInt() != 0);
      st.affiliateEnabled = q.value(8).isNull() ? false : (q.value(8).toInt() != 0);
      st.shareToken = q.value(9).toString();
    }
    return st;
  }

  bool saveLookbookSettings(const LookbookSettings &st)
  {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen() || st.projectId <= 0)
      return false;

    QSqlQuery q(db);
    q.prepare(
     "UPDATE lookbook_projects SET "
     "title=:title, cover_path=:cover, description=:descr, access_level=:access, "
     "comments_enabled=:comments, ratings_enabled=:ratings, currency=:currency, "
     "show_prices=:show_prices, affiliate_enabled=:affiliate "
     "WHERE id=:id AND username=:u");
    q.bindValue(":title", st.title.trimmed().isEmpty() ? QString("Мой лукбук") : st.title.trimmed());
    q.bindValue(":cover", st.coverPath);
    q.bindValue(":descr", st.description);
    q.bindValue(":access", st.accessLevel);
    q.bindValue(":comments", st.commentsEnabled ? 1 : 0);
    q.bindValue(":ratings", st.ratingsEnabled ? 1 : 0);
    q.bindValue(":currency", st.currency);
    q.bindValue(":show_prices", st.showPrices ? 1 : 0);
    q.bindValue(":affiliate", st.affiliateEnabled ? 1 : 0);
    q.bindValue(":id", st.projectId);
    q.bindValue(":u", m_username);
    if (!q.exec()) {
      showLookbookWarning("Лукбук", "Не удалось сохранить настройки: " + q.lastError().text());
      return false;
    }
    return true;
  }

  QString spacedNumber(double value) const
  {
    QString n = QString::number(value, 'f', 0);
    for (int i = n.length() - 3; i > 0; i -= 3)
      n.insert(i, ' ');
    return n;
  }

  QString formatLookbookMoney(double rubValue, const QString &currency) const
  {
    const QString cur = currency.trimmed().toUpper();
    if (cur == "USD")
      return "$" + spacedNumber(rubValue / 90.0);
    if (cur == "EUR")
      return "€" + spacedNumber(rubValue / 100.0);
    return spacedNumber(rubValue) + " ₽";
  }

  QString lookbookAccessText(const QString &access) const
  {
    if (access == "link") return "по прямой ссылке";
    if (access == "public") return "публичный";
    return "только мне";
  }


  bool showLookbookCustomMessage(const QString &title,
                  const QString &message,
                  const QString &primaryText = QString("ОК"),
                  const QString &secondaryText = QString(),
                  bool warning = false)
  {
    QDialog dlg(this);
    dlg.setObjectName("LookbookMessageDialog");
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setAttribute(Qt::WA_TranslucentBackground, true);
    dlg.setModal(true);
    dlg.setFixedWidth(500);
    dlg.setStyleSheet(
     "QDialog#LookbookMessageDialog{background:#00000000;}"
     "QFrame#LookbookMessageCard{background:#F4EFEA;border:1px solid #E1D6CE;border-radius:22px;}"
     "QLabel#LookbookMessageIcon{background:#F8F3EE;border:1px solid #E7DDD4;border-radius:18px;color:#6F4B37;font-size:17px;font-weight:900;}"
     "QLabel#LookbookMessageTitle{font-size:20px;font-weight:900;color:#171717;background:transparent;border:none;}"
     "QLabel#LookbookMessageText{font-size:13px;color:#665D55;line-height:1.35;background:transparent;border:none;}"
     "QPushButton#LookbookMessageClose{background:transparent;color:#1F1A17;border:none;border-radius:16px;font-family:'Arial';font-size:18px;font-weight:900;min-width:32px;min-height:32px;max-width:32px;max-height:32px;padding:0;}"
     "QPushButton#LookbookMessageClose:hover{background:#E8DDD4;color:#14100D;}QPushButton#LookbookMessageClose:pressed{background:#DED1C6;color:#14100D;}"
     "QPushButton#LookbookPrimaryBtn{background:#6F4B37;color:white;border:none;border-radius:12px;min-height:40px;padding:0 18px;font-weight:900;}"
     "QPushButton#LookbookPrimaryBtn:hover{background:#5F3E2D;}"
     "QPushButton#LookbookSecondaryBtn{background:#FFFFFF;color:#3A342F;border:1px solid #DDD2CA;border-radius:12px;min-height:40px;padding:0 18px;font-weight:800;}"
     "QPushButton#LookbookSecondaryBtn:hover{background:#FBF7F3;}"
    );

    QVBoxLayout *outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(0, 0, 0, 0);
    QFrame *card = new QFrame(&dlg);
    card->setObjectName("LookbookMessageCard");
    outer->addWidget(card);

    QVBoxLayout *root = new QVBoxLayout(card);
    root->setContentsMargins(22, 20, 22, 20);
    root->setSpacing(14);

    QHBoxLayout *header = new QHBoxLayout();
    header->setSpacing(12);
    QLabel *icon = new QLabel(warning ? "!" : "i", card);
    icon->setObjectName("LookbookMessageIcon");
    icon->setAlignment(Qt::AlignCenter);
    icon->setFixedSize(36, 36);
    header->addWidget(icon);

    QLabel *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("LookbookMessageTitle");
    titleLabel->setWordWrap(true);
    header->addWidget(titleLabel, 1);

    QPushButton *xBtn = new RoundCloseButton(card);
    xBtn->setObjectName("LookbookMessageClose");
    xBtn->setFixedSize(32, 32);
    xBtn->setCursor(Qt::PointingHandCursor);
    header->addWidget(xBtn);
    root->addLayout(header);

    QLabel *textLabel = new QLabel(message, card);
    textLabel->setObjectName("LookbookMessageText");
    textLabel->setWordWrap(true);
    root->addWidget(textLabel);

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();
    if (!secondaryText.isEmpty()) {
      QPushButton *secondary = new QPushButton(secondaryText, card);
      secondary->setObjectName("LookbookSecondaryBtn");
      secondary->setCursor(Qt::PointingHandCursor);
      buttons->addWidget(secondary);
      connect(secondary, &QPushButton::clicked, &dlg, &QDialog::reject);
    }
    QPushButton *primary = new QPushButton(primaryText, card);
    primary->setObjectName("LookbookPrimaryBtn");
    primary->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(primary);
    root->addLayout(buttons);

    connect(xBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(primary, &QPushButton::clicked, &dlg, &QDialog::accept);
    return dlg.exec() == QDialog::Accepted;
  }

  void showLookbookInfo(const QString &title, const QString &message)
  {
    showLookbookCustomMessage(title, message, "Понятно");
  }

  void showLookbookWarning(const QString &title, const QString &message)
  {
    showLookbookCustomMessage(title, message, "Понятно", QString(), true);
  }

  bool askLookbookConfirm(const QString &title,
              const QString &message,
              const QString &primaryText = QString("Да"),
              const QString &secondaryText = QString("Отмена"))
  {
    return showLookbookCustomMessage(title, message, primaryText, secondaryText, true);
  }

  QString lookbookSlotTitle(int pos) const
  {
    static const QStringList titles = {"Головной убор", "Верх", "Низ", "Обувь"};
    return (pos >= 0 && pos < titles.size()) ? titles[pos] : QString("Позиция %1").arg(pos + 1);
  }

  QString lookbookProductMetaText(const Product &p, const LookbookSettings &st) const
  {
    QString meta = QString("Категория: %1\nЦвет: %2").arg(p.category, p.color);
    if (st.showPrices)
      meta += QString("\nЦена: %1").arg(formatLookbookMoney(p.price, st.currency));
    else
      meta += "\nЦена скрыта";
    return meta;
  }

  void applyLookbookSettingsToEditor(const LookbookSettings &st)
  {
    if (m_lbProjectTitleEdit && !st.title.trimmed().isEmpty())
      m_lbProjectTitleEdit->setText(st.title.trimmed());

    if (m_lbDetailsRatingTitleLabel) m_lbDetailsRatingTitleLabel->setVisible(st.ratingsEnabled);
    if (m_lbDetailsRatingValueLabel) m_lbDetailsRatingValueLabel->setVisible(st.ratingsEnabled);
    if (m_lbDetailsCostTitleLabel) m_lbDetailsCostTitleLabel->setVisible(st.showPrices);
    if (m_lbDetailsCostLabel) m_lbDetailsCostLabel->setVisible(st.showPrices);
    if (m_lbDetailsCommentsTitleLabel) m_lbDetailsCommentsTitleLabel->setVisible(st.commentsEnabled);
    if (m_lbDetailsCommentsLabel) m_lbDetailsCommentsLabel->setVisible(st.commentsEnabled);
  }

  void showLookbookSettingsDialog()
  {
    if (m_lbCurrentProjectId <= 0) {
      showLookbookInfo("Настройки лукбука", "Сначала создай или открой лукбук.");
      return;
    }

    saveCurrentLookbookTitle();
    LookbookSettings st = loadLookbookSettings(m_lbCurrentProjectId);

    QDialog dlg(this);
    dlg.setObjectName("LookbookSettingsDialog");
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setAttribute(Qt::WA_TranslucentBackground, true);
    dlg.setModal(true);
    dlg.resize(700, 640);
    dlg.setStyleSheet(
     "QDialog#LookbookSettingsDialog{background:#00000000;}"
     "QFrame#LookbookSettingsCard{background:#F4EFEA;border:1px solid #E1D6CE;border-radius:22px;}"
     "QLabel#SettingsTitle{font-size:22px;font-weight:900;color:#151515;background:transparent;border:none;}"
     "QLabel#SettingsSub{font-size:12px;color:#7A7068;background:transparent;border:none;}"
     "QLabel#SettingsSection{font-size:13px;font-weight:900;color:#1F1F1F;margin-top:8px;background:transparent;border:none;}"
     "QLineEdit,QTextEdit,QComboBox{background:#FFFFFF;border:1px solid #DDD3CA;border-radius:10px;padding:8px;color:#2F2F2F;}"
     "QLineEdit:focus,QTextEdit:focus,QComboBox:focus{border:1px solid #D2A08A;}"
     "QPushButton#SettingsAccessBtn{background:#F7F2EC;border:1px solid #D8CEC5;border-radius:10px;padding:8px 12px;color:#2F2F2F;font-weight:700;}"
     "QPushButton#SettingsAccessBtn:hover{background:#EFE5DC;}"
     "QPushButton#SettingsAccessBtn:checked{background:#6F4B37;color:white;border:1px solid #6F4B37;}"
     "QPushButton#SettingsCoverBtn{background:#FFFFFF;border:1px solid #D8CEC5;border-radius:10px;padding:8px 12px;color:#2F2F2F;font-weight:700;}"
     "QPushButton#SettingsCoverBtn:hover{background:#FBF7F3;}"
     "QPushButton#SettingsSaveBtn{background:#6F4B37;color:white;border:none;border-radius:12px;min-height:40px;padding:0 18px;font-weight:900;}"
     "QPushButton#SettingsSaveBtn:hover{background:#5F3E2D;}"
     "QPushButton#SettingsCancelBtn{background:#FFFFFF;color:#3A342F;border:1px solid #DDD2CA;border-radius:12px;min-height:40px;padding:0 18px;font-weight:800;}"
     "QPushButton#SettingsCancelBtn:hover{background:#FBF7F3;}"
     "QPushButton#SettingsXBtn{background:transparent;color:#1F1A17;border:none;border-radius:16px;font-family:'Arial';font-size:18px;font-weight:900;min-width:32px;min-height:32px;max-width:32px;max-height:32px;padding:0;}"
     "QPushButton#SettingsXBtn:hover{background:#E8DDD4;color:#14100D;}QPushButton#SettingsXBtn:pressed{background:#DED1C6;color:#14100D;}"
     "QCheckBox{spacing:8px;color:#2F2F2F;background:transparent;border:none;}"
     "QLabel{background:transparent;border:none;}"
    );

    QVBoxLayout *outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(0, 0, 0, 0);
    QFrame *card = new QFrame(&dlg);
    card->setObjectName("LookbookSettingsCard");
    outer->addWidget(card);

    QVBoxLayout *root = new QVBoxLayout(card);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(10);

    QHBoxLayout *header = new QHBoxLayout();
    QVBoxLayout *titleCol = new QVBoxLayout();
    titleCol->setContentsMargins(0, 0, 0, 0);
    titleCol->setSpacing(2);
    QLabel *title = new QLabel(QString("Настройки лукбука: %1").arg(st.title), card);
    title->setObjectName("SettingsTitle");
    title->setWordWrap(true);
    QLabel *sub = new QLabel("Технические, приватные и коммерческие параметры подборки.", card);
    sub->setObjectName("SettingsSub");
    sub->setWordWrap(true);
    titleCol->addWidget(title);
    titleCol->addWidget(sub);
    header->addLayout(titleCol, 1);
    QPushButton *xBtn = new RoundCloseButton(card);
    xBtn->setObjectName("SettingsXBtn");
    xBtn->setFixedSize(32, 32);
    xBtn->setCursor(Qt::PointingHandCursor);
    header->addWidget(xBtn, 0, Qt::AlignTop);
    root->addLayout(header);

    auto sectionLabel = [card](const QString &text) {
      QLabel *lab = new QLabel(text, card);
      lab->setObjectName("SettingsSection");
      return lab;
    };

    root->addWidget(sectionLabel("ОСНОВНАЯ ИНФОРМАЦИЯ"));
    QFormLayout *mainForm = new QFormLayout();
    mainForm->setLabelAlignment(Qt::AlignLeft);
    mainForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    mainForm->setHorizontalSpacing(14);
    mainForm->setVerticalSpacing(9);

    QLineEdit *titleEdit = new QLineEdit(st.title, card);
    titleEdit->setPlaceholderText("Например: Spring Essentials");
    mainForm->addRow("Название:", titleEdit);

    QString selectedCoverPath = st.coverPath;
    QWidget *coverRow = new QWidget(card);
    QHBoxLayout *coverLay = new QHBoxLayout(coverRow);
    coverLay->setContentsMargins(0, 0, 0, 0);
    coverLay->setSpacing(8);
    QPushButton *chooseCoverBtn = new QPushButton("Выбрать фото", coverRow);
    chooseCoverBtn->setObjectName("SettingsCoverBtn");
    QCheckBox *autoCollageBox = new QCheckBox("Автоматический коллаж из вещей", coverRow);
    autoCollageBox->setChecked(selectedCoverPath.trimmed().isEmpty());
    QLabel *coverPathLabel = new QLabel(selectedCoverPath.isEmpty() ? "" : QFileInfo(selectedCoverPath).fileName(), coverRow);
    coverPathLabel->setStyleSheet("color:#756B62;background:transparent;border:none;");
    coverLay->addWidget(chooseCoverBtn);
    coverLay->addWidget(autoCollageBox);
    coverLay->addWidget(coverPathLabel, 1);
    mainForm->addRow("Обложка:", coverRow);

    connect(chooseCoverBtn, &QPushButton::clicked, &dlg, [&]() {
      QString file = QFileDialog::getOpenFileName(&dlg, "Выбрать обложку", QString(),
                           "Изображения (*.png *.jpg *.jpeg *.webp *.bmp)");
      if (file.isEmpty()) return;
      selectedCoverPath = file;
      autoCollageBox->setChecked(false);
      coverPathLabel->setText(QFileInfo(file).fileName());
    });
    connect(autoCollageBox, &QCheckBox::toggled, &dlg, [&](bool checked) {
      if (checked) {
        selectedCoverPath.clear();
        coverPathLabel->clear();
      }
    });

    QTextEdit *descriptionEdit = new QTextEdit(card);
    descriptionEdit->setPlaceholderText("Добавьте концепцию лукбука или заметки для клиента...");
    descriptionEdit->setPlainText(st.description);
    descriptionEdit->setFixedHeight(78);
    mainForm->addRow("Описание/заметки:", descriptionEdit);
    root->addLayout(mainForm);

    root->addWidget(sectionLabel("ПРИВАТНОСТЬ И ШЕРИНГ"));
    QFormLayout *privacyForm = new QFormLayout();
    privacyForm->setHorizontalSpacing(14);
    privacyForm->setVerticalSpacing(9);

    QWidget *accessRow = new QWidget(card);
    QHBoxLayout *accessLay = new QHBoxLayout(accessRow);
    accessLay->setContentsMargins(0, 0, 0, 0);
    accessLay->setSpacing(6);
    QButtonGroup *accessGroup = new QButtonGroup(accessRow);
    accessGroup->setExclusive(true);
    auto makeAccessBtn = [&](const QString &text, int id) {
      QPushButton *b = new QPushButton(text, accessRow);
      b->setObjectName("SettingsAccessBtn");
      b->setCheckable(true);
      b->setCursor(Qt::PointingHandCursor);
      accessGroup->addButton(b, id);
      accessLay->addWidget(b);
      return b;
    };
    makeAccessBtn("Только мне", 0);
    makeAccessBtn("По прямой ссылке", 1);
    makeAccessBtn("Публичный", 2);
    int accessId = st.accessLevel == "link" ? 1 : (st.accessLevel == "public" ? 2 : 0);
    if (accessGroup->button(accessId)) accessGroup->button(accessId)->setChecked(true);
    privacyForm->addRow("Уровень доступа:", accessRow);

    QCheckBox *commentsBox = new QCheckBox("Включить комментарии справа на макете", card);
    commentsBox->setChecked(st.commentsEnabled);
    privacyForm->addRow("Комментарии:", commentsBox);

    QCheckBox *ratingsBox = new QCheckBox("Показывать рейтинг и звёзды", card);
    ratingsBox->setChecked(st.ratingsEnabled);
    privacyForm->addRow("Оценки:", ratingsBox);
    root->addLayout(privacyForm);

    root->addWidget(sectionLabel("КОММЕРЧЕСКИЕ ПАРАМЕТРЫ"));
    QFormLayout *commercialForm = new QFormLayout();
    commercialForm->setHorizontalSpacing(14);
    commercialForm->setVerticalSpacing(9);

    QComboBox *currencyBox = new QComboBox(card);
    currencyBox->addItem("₽ (рубли)", "RUB");
    currencyBox->addItem("$ (доллары)", "USD");
    currencyBox->addItem("€ (евро)", "EUR");
    int currencyIndex = currencyBox->findData(st.currency);
    currencyBox->setCurrentIndex(currencyIndex >= 0 ? currencyIndex : 0);
    commercialForm->addRow("Валюта:", currencyBox);

    QCheckBox *showPricesBox = new QCheckBox("Показывать цены товаров", card);
    showPricesBox->setChecked(st.showPrices);
    commercialForm->addRow("Цены:", showPricesBox);

    QCheckBox *affiliateBox = new QCheckBox("Включить партнёрские ссылки для монетизации", card);
    affiliateBox->setChecked(st.affiliateEnabled);
    commercialForm->addRow("Партнёрские ссылки:", affiliateBox);
    root->addLayout(commercialForm);

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();
    QPushButton *cancelBtn = new QPushButton("Отмена", card);
    cancelBtn->setObjectName("SettingsCancelBtn");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    QPushButton *saveBtn = new QPushButton("Сохранить изменения", card);
    saveBtn->setObjectName("SettingsSaveBtn");
    saveBtn->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(cancelBtn);
    buttons->addWidget(saveBtn);
    root->addLayout(buttons);

    connect(xBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, &dlg, [&]() {
      LookbookSettings next = st;
      next.title = titleEdit->text().trimmed().isEmpty() ? QString("Мой лукбук") : titleEdit->text().trimmed();
      next.coverPath = selectedCoverPath;
      next.description = descriptionEdit->toPlainText().trimmed();
      const int checkedAccess = accessGroup->checkedId();
      next.accessLevel = checkedAccess == 1 ? "link" : (checkedAccess == 2 ? "public" : "private");
      next.commentsEnabled = commentsBox->isChecked();
      next.ratingsEnabled = ratingsBox->isChecked();
      next.currency = currencyBox->currentData().toString();
      next.showPrices = showPricesBox->isChecked();
      next.affiliateEnabled = affiliateBox->isChecked();
      if (saveLookbookSettings(next)) {
        applyLookbookSettingsToEditor(next);
        loadLookbookSlots();
        refreshLookbookProjectsList();
        refreshLookbookShowcaseSections();
        dlg.accept();
      }
    });

    dlg.exec();
  }

  QString ensureLookbookShareToken()
  {
    LookbookSettings st = loadLookbookSettings(m_lbCurrentProjectId);
    if (!st.shareToken.trimmed().isEmpty())
      return st.shareToken.trimmed();

    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlDatabase db = QSqlDatabase::database();
    if (db.isOpen() && m_lbCurrentProjectId > 0) {
      QSqlQuery q(db);
      q.prepare("UPDATE lookbook_projects SET share_token=:t WHERE id=:id AND username=:u");
      q.bindValue(":t", token);
      q.bindValue(":id", m_lbCurrentProjectId);
      q.bindValue(":u", m_username);
      q.exec();
    }
    return token;
  }

  QString buildLookbookPreviewHtml(const LookbookSettings &st, const QString &deepLink) const
  {
    QString cards;
    for (int pos = 0; pos < 4; ++pos) {
      const Product *p = (pos < m_lbSlotProductIds.size()) ? findProductById(m_lbSlotProductIds[pos]) : nullptr;
      QString imageHtml = "<div class='noimg'>Нет фото</div>";
      QString name = "Позиция не выбрана";
      QString meta = "";
      if (p) {
        name = p->name;
        const QString img = resolveImagePath(p->imagePath);
        if (QFileInfo::exists(img))
          imageHtml = QString("<img src='%1' alt=''>").arg(QUrl::fromLocalFile(img).toString());
        meta = QString("<p>Категория: %1</p><p>Цвет: %2</p>").arg(p->category.toHtmlEscaped(), p->color.toHtmlEscaped());
        if (st.showPrices)
          meta += QString("<p class='price'>%1</p>").arg(formatLookbookMoney(p->price, st.currency).toHtmlEscaped());
        else
          meta += "<p class='muted'>Цена скрыта</p>";
      }
      cards += QString(
       "<article class='card'><h2>%1</h2>%2<h3>%3</h3>%4</article>")
        .arg(lookbookSlotTitle(pos).toHtmlEscaped(), imageHtml, name.toHtmlEscaped(), meta);
    }

    const QString desc = st.description.trimmed().isEmpty()
      ? QString("Описание не добавлено.")
      : st.description.toHtmlEscaped().replace("\n", "<br>");

    return QString(
     "<!doctype html><html lang='ru'><head><meta charset='utf-8'>"
     "<meta name='viewport' content='width=device-width,initial-scale=1'>"
     "<title>%1</title>"
     "<style>body{margin:0;background:#f4efe9;font-family:Arial,sans-serif;color:#1f1f1f;}"
     ".wrap{max-width:1100px;margin:0 auto;padding:32px;}"
     ".hero{background:#fff;border:1px solid #e4dad2;border-radius:22px;padding:24px;margin-bottom:18px;}"
     ".hero h1{margin:0 0 8px;font-size:30px}.hero p{color:#5b534d;line-height:1.45}.badge{display:inline-block;background:#f7f2ec;border:1px solid #e4dad2;border-radius:999px;padding:6px 10px;margin-right:6px;color:#5d534b;font-size:13px;}"
     ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:16px}.card{background:#fff;border:1px solid #e4dad2;border-radius:18px;padding:16px;}"
     ".card h2{font-size:14px;text-transform:uppercase;color:#6f4b37;margin:0 0 10px}.card h3{font-size:18px;margin:12px 0 8px}.card p{margin:4px 0;color:#554e48}.price{font-weight:700;color:#1f1f1f!important}.muted{color:#8a8178!important}.card img,.noimg{width:100%;height:180px;object-fit:contain;background:#f8f3ee;border:1px solid #eadfd6;border-radius:14px}.noimg{display:flex;align-items:center;justify-content:center;color:#9a9088}.link{font-size:12px;color:#7b7068;word-break:break-all;margin-top:14px}</style>"
     "</head><body><main class='wrap'><section class='hero'><h1>%1</h1><p>%2</p>"
     "<span class='badge'>Доступ: %3</span><span class='badge'>Валюта: %4</span><span class='badge'>%5</span><div class='link'>Deep link: %6</div></section>"
     "<section class='grid'>%7</section></main></body></html>")
      .arg(st.title.toHtmlEscaped(), desc, lookbookAccessText(st.accessLevel).toHtmlEscaped(),
         st.currency.toHtmlEscaped(), st.showPrices ? QString("цены видны") : QString("цены скрыты"),
         deepLink.toHtmlEscaped(), cards);
  }

  bool exportCurrentLookbookFiles(const LookbookSettings &st, const QString &token,
                  QString *jsonPath, QString *htmlPath, QString *error) const
  {
    const QString exportDir = QCoreApplication::applicationDirPath() + "/shared_lookbooks";
    QDir dir;
    if (!dir.mkpath(exportDir)) {
      if (error) *error = "Не удалось создать папку: " + exportDir;
      return false;
    }

    QString safeTitle = st.title;
    safeTitle.replace(QRegularExpression("[^A-Za-zА-Яа-я0-9_ -]"), "_");
    safeTitle = safeTitle.trimmed().left(40);
    if (safeTitle.isEmpty()) safeTitle = "lookbook";

    const QString base = QString("lookbook_%1_%2_%3").arg(st.projectId).arg(token.left(8)).arg(safeTitle);
    const QString jsonFilePath = exportDir + "/" + base + ".json";
    const QString htmlFilePath = exportDir + "/" + base + ".html";

    QUrl deepUrl;
    deepUrl.setScheme("clothingcatalog");
    deepUrl.setHost("lookbook");
    deepUrl.setPath("/view/" + token);
    QUrlQuery deepQuery;
    deepQuery.addQueryItem("file", QUrl::fromLocalFile(jsonFilePath).toString());
    deepQuery.addQueryItem("mode", "view");
    deepUrl.setQuery(deepQuery);
    const QString deepLink = deepUrl.toString();

    QJsonObject root;
    root["type"] = "clothingcatalog.lookbook";
    root["version"] = 1;
    root["project_id"] = st.projectId;
    root["share_token"] = token;
    root["title"] = st.title;
    root["description"] = st.description;
    root["cover_path"] = st.coverPath;
    root["access_level"] = st.accessLevel;
    root["comments_enabled"] = st.commentsEnabled;
    root["ratings_enabled"] = st.ratingsEnabled;
    root["currency"] = st.currency;
    root["show_prices"] = st.showPrices;
    root["affiliate_enabled"] = st.affiliateEnabled;
    root["deep_link"] = deepLink;

    QJsonArray items;
    for (int pos = 0; pos < 4; ++pos) {
      const Product *p = (pos < m_lbSlotProductIds.size()) ? findProductById(m_lbSlotProductIds[pos]) : nullptr;
      if (!p) continue;
      QJsonObject item;
      item["position"] = pos;
      item["slot"] = lookbookSlotTitle(pos);
      item["product_id"] = p->id;
      item["name"] = p->name;
      item["category"] = p->category;
      item["color"] = p->color;
      item["size"] = p->size;
      item["material"] = p->material;
      item["season"] = p->season;
      item["style_tag"] = p->styleTag;
      if (st.showPrices)
        item["price_rub"] = p->price;
      item["image_path"] = p->imagePath;
      item["resolved_image"] = resolveImagePath(p->imagePath);
      items.append(item);
    }
    root["items"] = items;

    QFile jsonFile(jsonFilePath);
    if (!jsonFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      if (error) *error = "Не удалось записать JSON: " + jsonFile.errorString();
      return false;
    }
    jsonFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    jsonFile.close();

    QFile htmlFile(htmlFilePath);
    if (!htmlFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      if (error) *error = "Не удалось записать HTML-просмотр: " + htmlFile.errorString();
      return false;
    }
    htmlFile.write(buildLookbookPreviewHtml(st, deepLink).toUtf8());
    htmlFile.close();

    if (jsonPath) *jsonPath = jsonFilePath;
    if (htmlPath) *htmlPath = htmlFilePath;
    return true;
  }


  int showLookbookShareResultDialog(const QString &previewLink,
                   const QString &jsonPath,
                   const QString &htmlPath)
  {
    QDialog dlg(this);
    dlg.setObjectName("LookbookShareResultDialog");
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setAttribute(Qt::WA_TranslucentBackground, true);
    dlg.setModal(true);
    dlg.resize(620, 430);
    dlg.setStyleSheet(
     "QDialog#LookbookShareResultDialog{background:#00000000;}"
     "QFrame#LookbookShareResultCard{background:#F4EFEA;border:1px solid #E1D6CE;border-radius:22px;}"
     "QLabel#ShareDialogTitle{font-size:22px;font-weight:900;color:#151515;background:transparent;border:none;}"
     "QLabel#ShareDialogSub{font-size:13px;color:#6D625A;background:transparent;border:none;}"
     "QTextEdit#ShareDialogDetails{background:#FBF8F5;border:1px solid #E7DDD4;border-radius:14px;padding:10px;color:#554E48;font-size:12px;}"
     "QPushButton#ShareDialogClose{background:transparent;color:#1F1A17;border:none;border-radius:16px;font-family:'Arial';font-size:18px;font-weight:900;min-width:32px;min-height:32px;max-width:32px;max-height:32px;padding:0;}"
     "QPushButton#ShareDialogClose:hover{background:#E8DDD4;color:#14100D;}QPushButton#ShareDialogClose:pressed{background:#DED1C6;color:#14100D;}"
     "QPushButton#ShareDialogMain{background:#6F4B37;color:white;border:none;border-radius:12px;min-height:40px;padding:0 16px;font-weight:900;}"
     "QPushButton#ShareDialogMain:hover{background:#5F3E2D;}"
     "QPushButton#ShareDialogAction{background:#FFFFFF;color:#3A342F;border:1px solid #DDD2CA;border-radius:12px;min-height:40px;padding:0 14px;font-weight:800;}"
     "QPushButton#ShareDialogAction:hover{background:#FBF7F3;}"
    );

    QVBoxLayout *outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(0, 0, 0, 0);
    QFrame *card = new QFrame(&dlg);
    card->setObjectName("LookbookShareResultCard");
    outer->addWidget(card);

    QVBoxLayout *root = new QVBoxLayout(card);
    root->setContentsMargins(22, 20, 22, 20);
    root->setSpacing(12);

    QHBoxLayout *header = new QHBoxLayout();
    QLabel *title = new QLabel("Поделиться лукбуком", card);
    title->setObjectName("ShareDialogTitle");
    header->addWidget(title, 1);
    QPushButton *xBtn = new RoundCloseButton(card);
    xBtn->setObjectName("ShareDialogClose");
    xBtn->setFixedSize(32, 32);
    xBtn->setCursor(Qt::PointingHandCursor);
    header->addWidget(xBtn);
    root->addLayout(header);

    QLabel *sub = new QLabel("Ссылка просмотра создана и скопирована в буфер обмена. HTML можно открыть как страницу просмотра, а JSON использовать для будущего импорта.", card);
    sub->setObjectName("ShareDialogSub");
    sub->setWordWrap(true);
    root->addWidget(sub);

    QTextEdit *details = new QTextEdit(card);
    details->setObjectName("ShareDialogDetails");
    details->setReadOnly(true);
    details->setFixedHeight(140);
    details->setPlainText(QString("Ссылка просмотра:\n%1\n\nJSON-файл:\n%2\n\nПапка:\n%3")
               .arg(previewLink, jsonPath, QFileInfo(htmlPath).absolutePath()));
    root->addWidget(details);

    int action = 0;
    QHBoxLayout *buttons = new QHBoxLayout();
    QPushButton *openPreviewBtn = new QPushButton("Открыть просмотр", card);
    openPreviewBtn->setObjectName("ShareDialogMain");
    QPushButton *openFolderBtn = new QPushButton("Открыть папку", card);
    openFolderBtn->setObjectName("ShareDialogAction");
    QPushButton *copyJsonBtn = new QPushButton("Копировать JSON", card);
    copyJsonBtn->setObjectName("ShareDialogAction");
    QPushButton *okBtn = new QPushButton("Готово", card);
    okBtn->setObjectName("ShareDialogAction");
    openPreviewBtn->setCursor(Qt::PointingHandCursor);
    openFolderBtn->setCursor(Qt::PointingHandCursor);
    copyJsonBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(openPreviewBtn);
    buttons->addWidget(openFolderBtn);
    buttons->addWidget(copyJsonBtn);
    buttons->addStretch();
    buttons->addWidget(okBtn);
    root->addLayout(buttons);

    connect(xBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(openPreviewBtn, &QPushButton::clicked, &dlg, [&]() { action = 1; dlg.accept(); });
    connect(openFolderBtn, &QPushButton::clicked, &dlg, [&]() { action = 2; dlg.accept(); });
    connect(copyJsonBtn, &QPushButton::clicked, &dlg, [&]() { action = 3; dlg.accept(); });
    dlg.exec();
    return action;
  }

  void shareCurrentLookbook()
  {
    if (m_lbCurrentProjectId <= 0) {
      showLookbookInfo("Поделиться лукбуком", "Сначала создай или открой лукбук.");
      return;
    }

    saveCurrentLookbookTitle();
    LookbookSettings st = loadLookbookSettings(m_lbCurrentProjectId);

    if (st.accessLevel == "private") {
      if (!askLookbookConfirm("Поделиться лукбуком",
                 "Сейчас лукбук доступен только тебе. Переключить доступ на «По прямой ссылке» и создать ссылку?",
                 "Переключить",
                 "Отмена"))
        return;
      st.accessLevel = "link";
      saveLookbookSettings(st);
    }

    const QString token = ensureLookbookShareToken();
    st.shareToken = token;

    QString jsonPath, htmlPath, error;
    if (!exportCurrentLookbookFiles(st, token, &jsonPath, &htmlPath, &error)) {
      showLookbookWarning("Поделиться лукбуком", error);
      return;
    }

    const QString previewLink = QUrl::fromLocalFile(htmlPath).toString();
    QApplication::clipboard()->setText(previewLink);

    const int action = showLookbookShareResultDialog(previewLink, jsonPath, htmlPath);
    if (action == 1) {
      QDesktopServices::openUrl(QUrl::fromLocalFile(htmlPath));
    } else if (action == 2) {
      QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(htmlPath).absolutePath()));
    } else if (action == 3) {
      QApplication::clipboard()->setText(QUrl::fromLocalFile(jsonPath).toString());
    }
  }

  QColor lookbookColorFromName(QString colorName) const
  {
    const QString c = normRu(colorName);
    if (c.contains("чер")) return QColor("#2B2B2B");
    if (c.contains("бел") || c.contains("молоч")) return QColor("#F2EEE7");
    if (c.contains("крем")) return QColor("#EADCC8");
    if (c.contains("беж") || c.contains("пес")) return QColor("#D5B48D");
    if (c.contains("перс")) return QColor("#E3A590");
    if (c.contains("террак")) return QColor("#B6674A");
    if (c.contains("карам")) return QColor("#A97449");
    if (c.contains("олив")) return QColor("#7A8561");
    if (c.contains("граф")) return QColor("#50545A");
    if (c.contains("сер")) return QColor("#A4A6AA");
    if (c.contains("син") || c.contains("деним") || c.contains("джин")) return QColor("#6D89B8");
    if (c.contains("крас")) return QColor("#C24A4A");
    return QColor("#CDB79E");
  }

  QString lookbookModelKey(const Product &p) const
  {
    QString s = normRu(p.name);
    const QStringList wordsToRemove = {
     "черный", "черная", "черное", "черные", "чёрный", "чёрная", "чёрное", "чёрные",
     "белый", "белая", "белое", "белые",
     "синий", "синяя", "синее", "синие",
     "красный", "красная", "красное", "красные",
     "серый", "серая", "серое", "серые", "графитовый", "графитовая", "графитовые",
     "бежевый", "бежевая", "бежевое", "бежевые",
     "молочный", "молочная", "молочное", "молочные",
     "кремовый", "кремовая", "кремовое", "кремовые",
     "песочный", "песочная", "песочное", "песочные", "песка",
     "персиковый", "персиковая", "персиковое", "персиковые",
     "терракотовый", "терракотовая", "терракотовое", "терракотовые",
     "карамельный", "карамельная", "карамельное", "карамельные",
     "оливковый", "оливковая", "оливковое", "оливковые"
    };
    for (const QString &w : wordsToRemove) {
      QRegularExpression re(QString("\\b%1\\b").arg(QRegularExpression::escape(w)));
      s.replace(re, " ");
    }
    s = s.simplified();
    return normRu(p.category) + "|" + s;
  }

  QVector<const Product*> lookbookAvailableVariants(const Product &selected) const
  {
    QVector<const Product*> result;
    QSet<QString> usedColors;
    const QString key = lookbookModelKey(selected);

    auto addVariant = [&](const Product *p) {
      if (!p) return;
      const QString colorKey = normRu(p->color);
      if (colorKey.isEmpty() || usedColors.contains(colorKey)) return;
      result.push_back(p);
      usedColors.insert(colorKey);
    };

    // The first color dot always corresponds to the selected item.
    addVariant(&selected);

    for (const Product &p : m_products) {
      if (p.id == selected.id) continue;
      if (lookbookModelKey(p) == key)
        addVariant(&p);
    }
    return result;
  }

  bool productAllowedForLookbookSlot(int position, const Product &p) const
  {
    const QString cat = normRu(p.category);
    const QString name = normRu(p.name);
    const QString hay = cat + " " + name;

    switch (position) {
    case 0: // headwear
      return hay.contains("голов") || hay.contains("кеп") || hay.contains("бейсбол") ||
          hay.contains("шап") || hay.contains("панам") || hay.contains("берет") || hay.contains("шляп");
    case 1: // top
      return hay.contains("футбол") || hay.contains("майк") || hay.contains("топ") ||
          hay.contains("лонгслив") || hay.contains("худи") || hay.contains("свитш") ||
          hay.contains("свитер") || hay.contains("рубаш") || hay.contains("блуз") ||
          hay.contains("куртк") || hay.contains("пальто") || hay.contains("тренч") ||
          hay.contains("кардиган") || hay.contains("пидж") || hay.contains("жакет") ||
          hay.contains("плать");
    case 2: // bottom
      return hay.contains("брюк") || hay.contains("штаны") || hay.contains("джин") ||
          hay.contains("чинос") || hay.contains("юбк") || hay.contains("шорт") ||
          hay.contains("леггин") || hay.contains("низ");
    case 3: // shoes
      return hay.contains("обув") || hay.contains("кед") || hay.contains("кроссов") ||
          hay.contains("ботин") || hay.contains("туф") || hay.contains("сандал") ||
          hay.contains("лофер") || hay.contains("сапог") || hay.contains("сникер") ||
          hay.contains("балетк") || hay.contains("мюли");
    default:
      return true;
    }
  }

  QString lookbookSlotName(int position) const
  {
    switch (position) {
    case 0: return "головной убор";
    case 1: return "верх";
    case 2: return "низ";
    case 3: return "обувь";
    default: return "позицию";
    }
  }

  QPixmap lookbookProductPixmap(const Product &p, const QSize &target) const
  {
    return cachedScaledPixmap(p.imagePath, target);
  }

  void refreshLookbookCenterComposite()
  {
    if (!m_lbCenterPreviewLabel) return;

    QVector<int> filledPositions;
    for (int i = 0; i < 4; ++i) {
      if (i < m_lbSlotProductIds.size() && m_lbSlotProductIds[i] > 0)
        filledPositions.push_back(i);
    }

    // The central preview is shown after at least two outfit items are selected.
    if (filledPositions.size() < 2) {
      m_lbCenterPreviewLabel->setPixmap(QPixmap());
      m_lbCenterPreviewLabel->setText("Здесь появится\nфинальный образ\nпосле выбора\nминимум 2 вещей");
      return;
    }

    QPixmap canvas(146, 146);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#F8F3EE"));
    painter.drawRoundedRect(canvas.rect().adjusted(0, 0, -1, -1), 12, 12);

    const QRect previewRects[4] = {
      QRect(48, 6, 50, 34),   // headwear
      QRect(32, 42, 82, 48),  // top
      QRect(30, 86, 86, 40),  // bottom
      QRect(47, 118, 54, 24)  // shoes
    };

    for (int pos : filledPositions) {
      const Product *prod = findProductById(m_lbSlotProductIds[pos]);
      if (!prod) continue;

      QRect targetRect = previewRects[pos];
      QPixmap px = lookbookProductPixmap(*prod, targetRect.size());

      painter.setPen(QPen(QColor("#E7DDD4"), 1));
      painter.setBrush(QColor("#FFFFFF"));
      painter.drawRoundedRect(targetRect.adjusted(-2, -2, 2, 2), 8, 8);

      if (!px.isNull()) {
        QSize sz = px.size();
        QRect target(QPoint(0, 0), sz);
        target.moveCenter(targetRect.center());
        painter.drawPixmap(target.topLeft(), px);
      } else {
        painter.setPen(QColor("#8B8178"));
        painter.drawText(targetRect, Qt::AlignCenter, prod->name.left(12));
      }
    }
    painter.end();

    m_lbCenterPreviewLabel->setText(QString());
    m_lbCenterPreviewLabel->setPixmap(canvas);
  }

  bool setLookbookItemProduct(int position, int productId)
  {
    if (m_lbCurrentProjectId <= 0) return false;
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) return false;

    QSqlQuery q(db);
    q.prepare("DELETE FROM lookbook_items WHERE project_id=:pid AND position=:pos");
    q.bindValue(":pid", m_lbCurrentProjectId);
    q.bindValue(":pos", position);
    q.exec();

    q.prepare("INSERT INTO lookbook_items(project_id, position, product_id) VALUES(:pid,:pos,:prod)");
    q.bindValue(":pid", m_lbCurrentProjectId);
    q.bindValue(":pos", position);
    q.bindValue(":prod", productId);
    if (!q.exec()) {
      showLookbookWarning("Лукбук", "Не удалось добавить: " + q.lastError().text());
      return false;
    }
    return true;
  }

  void rebuildLookbookColorChoices(int pos, const Product *p)
  {
    if (pos < 0 || pos >= m_lbColorHosts.size()) return;
    QWidget *host = m_lbColorHosts[pos];
    if (!host || !host->layout()) return;
    auto *lay = qobject_cast<QHBoxLayout*>(host->layout());
    if (!lay) return;

    while (QLayoutItem *item = lay->takeAt(0)) {
      if (item->widget()) item->widget()->deleteLater();
      delete item;
    }

    if (pos >= m_lbColorVariantProductIds.size()) m_lbColorVariantProductIds.resize(pos + 1);
    if (pos >= m_lbSlotColorVariants.size()) m_lbSlotColorVariants.resize(pos + 1);
    if (pos >= m_lbSelectedColorIndex.size()) m_lbSelectedColorIndex.resize(pos + 1);
    m_lbColorVariantProductIds[pos].clear();
    m_lbSlotColorVariants[pos].clear();
    m_lbSelectedColorIndex[pos] = 0;

    if (!p) {
      host->setVisible(false);
      return;
    }

    QVector<const Product*> variants = lookbookAvailableVariants(*p);
    // If only the current color is available, color dots are not shown.
    if (variants.size() <= 1) {
      host->setVisible(false);
      return;
    }

    host->setVisible(true);
    auto *group = new QButtonGroup(host);
    group->setExclusive(true);

    for (int i = 0; i < variants.size(); ++i) {
      const Product *variant = variants[i];
      if (!variant) continue;

      m_lbColorVariantProductIds[pos].push_back(variant->id);
      m_lbSlotColorVariants[pos].push_back(variant->color);

      QColor c = lookbookColorFromName(variant->color);
      QToolButton *chip = new QToolButton(host);
      chip->setObjectName("LbColorChip");
      chip->setCheckable(true);
      chip->setAutoRaise(false);
      chip->setFixedSize(22, 22);
      chip->setToolTip(variant->color);
      chip->setStyleSheet(QString(
       "QToolButton#LbColorChip{background:%1;border:1px solid #D8CEC5;border-radius:11px;}"
       "QToolButton#LbColorChip:checked{border:2px solid #6F4B37;background:%1;}"
      ).arg(c.name()));
      chip->setChecked(i == 0);
      lay->addWidget(chip);
      group->addButton(chip, i);
    }
    lay->addStretch();

    QObject::connect(group, qOverload<int>(&QButtonGroup::idClicked), this, [this, pos](int id) {
      if (pos >= m_lbColorVariantProductIds.size()) return;
      if (id < 0 || id >= m_lbColorVariantProductIds[pos].size()) return;
      const int productId = m_lbColorVariantProductIds[pos][id];
      if (setLookbookItemProduct(pos, productId)) {
        if (pos < m_lbSelectedColorIndex.size())
          m_lbSelectedColorIndex[pos] = id;
        loadLookbookSlots();
        refreshLookbookProjectsList();
        refreshLookbookShowcaseSections();
      }
    });
  }

  void refreshLookbookEditorMeta()
  {
    LookbookSettings st = loadLookbookSettings(m_lbCurrentProjectId);
    applyLookbookSettingsToEditor(st);

    double total = 0.0;
    int filledCount = 0;
    for (int pos = 0; pos < 4; ++pos) {
      if (pos < m_lbSlotProductIds.size() && m_lbSlotProductIds[pos] > 0) {
        ++filledCount;
        const Product *p = findProductById(m_lbSlotProductIds[pos]);
        if (p) total += p->price;
      }
    }

    if (m_lbDetailsCostLabel) {
      if (st.showPrices)
        m_lbDetailsCostLabel->setText(QString("Итого: %1").arg(formatLookbookMoney(total, st.currency)));
      else
        m_lbDetailsCostLabel->setText("Цены скрыты");
    }
    if (m_lbCenterAddButton)
      m_lbCenterAddButton->setText(filledCount >= 2 ? "Финальный образ" : "Экспонат");

    refreshLookbookCenterComposite();
  }

  void createLookbookProject()
  {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) return;

    int nextNumber = 1;
    QSqlQuery countQ(db);
    countQ.prepare("SELECT COUNT(*) FROM lookbook_projects WHERE username=:u");
    countQ.bindValue(":u", m_username);
    if (countQ.exec() && countQ.next())
      nextNumber = countQ.value(0).toInt() + 1;

    QString title = QString("Лукбук %1").arg(nextNumber);
    QString description;

    QDialog dlg(this);
    dlg.setObjectName("CreateLookbookDialog");
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setAttribute(Qt::WA_TranslucentBackground, true);
    dlg.setModal(true);
    dlg.resize(520, 390);
    dlg.setStyleSheet(
     "QDialog#CreateLookbookDialog{background:#00000000;}"
     "QFrame#CreateLookbookCard{background:#F4EFEA;border:1px solid #E1D6CE;border-radius:22px;}"
     "QLabel#CreateLookbookTitle{font-size:22px;font-weight:900;color:#141414;background:transparent;border:none;}"
     "QLabel#CreateLookbookSub{font-size:13px;color:#7A7068;background:transparent;border:none;}"
     "QLabel#CreateLookbookCaption{font-size:12px;font-weight:900;color:#2E2925;background:transparent;border:none;}"
     "QLineEdit#CreateLookbookInput{background:#FFFFFF;border:1px solid #E0D7CF;border-radius:12px;min-height:42px;padding:0 14px;color:#1E2A36;font-size:14px;}"
     "QLineEdit#CreateLookbookInput:focus{border:1px solid #DFA08C;}"
     "QTextEdit#CreateLookbookText{background:#FFFFFF;border:1px solid #E0D7CF;border-radius:12px;padding:10px 12px;color:#1E2A36;font-size:13px;}"
     "QTextEdit#CreateLookbookText:focus{border:1px solid #DFA08C;}"
     "QPushButton#CreateLookbookMainBtn{background:#6F4B37;color:white;border:none;border-radius:12px;min-height:42px;padding:0 20px;font-weight:900;}"
     "QPushButton#CreateLookbookMainBtn:hover{background:#5F3E2D;}"
     "QPushButton#CreateLookbookCancelBtn{background:#FFFFFF;color:#3A342F;border:1px solid #DDD2CA;border-radius:12px;min-height:42px;padding:0 20px;font-weight:800;}"
     "QPushButton#CreateLookbookCancelBtn:hover{background:#FBF7F3;}"
     "QPushButton#CreateLookbookXBtn{background:transparent;color:#1F1A17;border:none;border-radius:16px;font-family:'Arial';font-size:18px;font-weight:900;min-width:32px;min-height:32px;max-width:32px;max-height:32px;padding:0;}"
     "QPushButton#CreateLookbookXBtn:hover{background:#E8DDD4;color:#14100D;}QPushButton#CreateLookbookXBtn:pressed{background:#DED1C6;color:#14100D;}"
    );

    QVBoxLayout *outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(12, 12, 12, 12);
    QFrame *card = new QFrame(&dlg);
    card->setObjectName("CreateLookbookCard");
    outer->addWidget(card);

    QVBoxLayout *root = new QVBoxLayout(card);
    root->setContentsMargins(24, 22, 24, 22);
    root->setSpacing(14);

    QHBoxLayout *head = new QHBoxLayout();
    QVBoxLayout *headText = new QVBoxLayout();
    headText->setSpacing(3);
    QLabel *dialogTitle = new QLabel("Создать лукбук", card);
    dialogTitle->setObjectName("CreateLookbookTitle");
    QLabel *dialogSub = new QLabel("Укажи название подборки. После создания лукбук откроется в редакторе.", card);
    dialogSub->setObjectName("CreateLookbookSub");
    dialogSub->setWordWrap(true);
    headText->addWidget(dialogTitle);
    headText->addWidget(dialogSub);
    QPushButton *xBtn = new RoundCloseButton(card);
    xBtn->setObjectName("CreateLookbookXBtn");
    xBtn->setFixedSize(32, 32);
    xBtn->setCursor(Qt::PointingHandCursor);
    head->addLayout(headText, 1);
    head->addWidget(xBtn, 0, Qt::AlignTop);
    root->addLayout(head);

    QLabel *nameCaption = new QLabel("Название", card);
    nameCaption->setObjectName("CreateLookbookCaption");
    root->addWidget(nameCaption);

    QLineEdit *titleEdit = new QLineEdit(card);
    titleEdit->setObjectName("CreateLookbookInput");
    titleEdit->setPlaceholderText("Например: Spring Essentials");
    titleEdit->setText(title);
    titleEdit->selectAll();
    root->addWidget(titleEdit);

    QLabel *descCaption = new QLabel("Описание / заметки", card);
    descCaption->setObjectName("CreateLookbookCaption");
    root->addWidget(descCaption);

    QTextEdit *descEdit = new QTextEdit(card);
    descEdit->setObjectName("CreateLookbookText");
    descEdit->setPlaceholderText("Краткая идея образа или ТЗ для клиента...");
    descEdit->setFixedHeight(86);
    root->addWidget(descEdit);

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();
    QPushButton *cancelBtn = new QPushButton("Отмена", card);
    cancelBtn->setObjectName("CreateLookbookCancelBtn");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    QPushButton *createBtn = new QPushButton("Создать", card);
    createBtn->setObjectName("CreateLookbookMainBtn");
    createBtn->setCursor(Qt::PointingHandCursor);
    createBtn->setDefault(true);
    buttons->addWidget(cancelBtn);
    buttons->addWidget(createBtn);
    root->addLayout(buttons);

    connect(xBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(createBtn, &QPushButton::clicked, &dlg, [&]() {
      QString entered = titleEdit->text().trimmed();
      if (entered.isEmpty()) {
        titleEdit->setFocus();
        titleEdit->setStyleSheet("background:#FFFFFF;border:1px solid #D85656;border-radius:12px;min-height:42px;padding:0 14px;color:#1E2A36;font-size:14px;");
        return;
      }
      title = entered;
      description = descEdit->toPlainText().trimmed();
      dlg.accept();
    });
    connect(titleEdit, &QLineEdit::returnPressed, createBtn, &QPushButton::click);
    titleEdit->setFocus();

    if (dlg.exec() != QDialog::Accepted)
      return;

    QSqlQuery q(db);
    q.prepare(R"(
      INSERT INTO lookbook_projects(
        username, title, created_at, description,
        access_level, comments_enabled, ratings_enabled,
        currency, show_prices, affiliate_enabled
      ) VALUES(:u,:t,:c,:d,'private',1,1,'RUB',1,0)
    )");
    q.bindValue(":u", m_username);
    q.bindValue(":t", title);
    q.bindValue(":c", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm"));
    q.bindValue(":d", description);
    if (!q.exec()) {
      showLookbookWarning("Лукбук", "Не удалось создать проект: " + q.lastError().text());
      return;
    }

    int newId = q.lastInsertId().toInt();
    refreshLookbookProjectsList();
    refreshLookbookShowcaseSections();
    if (newId > 0)
      openLookbookProject(newId);
  }

  void deleteLookbookProject(int projectId)
  {
    if (!askLookbookConfirm("Удалить лукбук", "Удалить этот сет?", "Удалить", "Отмена"))
      return;
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) return;
    QSqlQuery q(db);
    q.prepare("DELETE FROM lookbook_items WHERE project_id=:id");
    q.bindValue(":id", projectId);
    q.exec();
    q.prepare("DELETE FROM lookbook_projects WHERE id=:id AND username=:u");
    q.bindValue(":id", projectId);
    q.bindValue(":u", m_username);
    q.exec();
    refreshLookbookProjectsList();
    refreshLookbookShowcaseSections();
  }

  void openLookbookProject(int projectId)
  {
    m_lbCurrentProjectId = projectId;
    QSqlDatabase db = QSqlDatabase::database();
    if (db.isOpen()) {
      QSqlQuery q(db);
      q.prepare("SELECT title, IFNULL(created_at,'') FROM lookbook_projects WHERE id=:id AND username=:u");
      q.bindValue(":id", projectId);
      q.bindValue(":u", m_username);
      if (q.exec() && q.next()) {
        if (m_lbProjectTitleEdit) m_lbProjectTitleEdit->setText(q.value(0).toString());
        if (m_lbUpdatedLabel) m_lbUpdatedLabel->setText("Обновлено: " + q.value(1).toString());
      }
    }
    applyLookbookSettingsToEditor(loadLookbookSettings(projectId));
    loadLookbookSlots();
    if (m_lookbookStack) m_lookbookStack->setCurrentIndex(1);
  }

  const Product* findProductById(int id) const
  {
    for (const auto &p : m_products) {
      if (p.id == id) return &p;
    }
    return nullptr;
  }

  void loadLookbookSlots()
  {
    LookbookSettings st = loadLookbookSettings(m_lbCurrentProjectId);
    applyLookbookSettingsToEditor(st);

    // Reset the interface state.
    for (int i = 0; i < 4; ++i) {
      if (i < m_lbSlotImages.size() && m_lbSlotImages[i]) {
        m_lbSlotImages[i]->setText("Нажмите, чтобы\nдобавить вещь");
        m_lbSlotImages[i]->setPixmap(QPixmap());
      }
      if (i < m_lbSlotNames.size() && m_lbSlotNames[i]) {
        m_lbSlotNames[i]->setText("Пока ничего не выбрано");
      }
      if (i < m_lbSlotMeta.size() && m_lbSlotMeta[i]) {
        m_lbSlotMeta[i]->setText("");
      }
      if (i < m_lbSlotActionButtons.size() && m_lbSlotActionButtons[i]) {
        m_lbSlotActionButtons[i]->setText("Добавить");
      }
      if (i < m_lbSlotRemoveButtons.size() && m_lbSlotRemoveButtons[i]) {
        m_lbSlotRemoveButtons[i]->setVisible(false);
      }
      if (i < m_lbSlotProductIds.size())
        m_lbSlotProductIds[i] = 0;
      if (i < m_lbSelectedColorIndex.size())
        m_lbSelectedColorIndex[i] = 0;
      if (i < m_lbSlotColorVariants.size())
        m_lbSlotColorVariants[i].clear();
      if (i < m_lbColorVariantProductIds.size())
        m_lbColorVariantProductIds[i].clear();
      rebuildLookbookColorChoices(i, nullptr);
    }

    if (m_lbCenterPreviewLabel) {
      m_lbCenterPreviewLabel->setPixmap(QPixmap());
      m_lbCenterPreviewLabel->setText("Здесь появится\nфинальный образ\nпосле выбора\nминимум 2 вещей");
    }

    if (m_lbCurrentProjectId <= 0) {
      refreshLookbookEditorMeta();
      return;
    }
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
      refreshLookbookEditorMeta();
      return;
    }

    QSqlQuery q(db);
    q.prepare("SELECT position, product_id FROM lookbook_items WHERE project_id=:id");
    q.bindValue(":id", m_lbCurrentProjectId);
    if (!q.exec()) {
      refreshLookbookEditorMeta();
      return;
    }

    while (q.next()) {
      const int pos = q.value(0).toInt();
      const int pid = q.value(1).toInt();
      const Product *p = findProductById(pid);
      if (!p) continue;

      if (pos >= 0 && pos < m_lbSlotProductIds.size())
        m_lbSlotProductIds[pos] = pid;

      if (pos >= 0 && pos < m_lbSlotNames.size() && m_lbSlotNames[pos]) {
        m_lbSlotNames[pos]->setText(p->name);
      }
      if (pos >= 0 && pos < m_lbSlotMeta.size() && m_lbSlotMeta[pos]) {
        m_lbSlotMeta[pos]->setText(lookbookProductMetaText(*p, st));
      }
      if (pos >= 0 && pos < m_lbSlotActionButtons.size() && m_lbSlotActionButtons[pos]) {
        m_lbSlotActionButtons[pos]->setText("Изменить");
      }
      if (pos >= 0 && pos < m_lbSlotRemoveButtons.size() && m_lbSlotRemoveButtons[pos]) {
        m_lbSlotRemoveButtons[pos]->setVisible(false);
      }

      rebuildLookbookColorChoices(pos, p);
      if (pos >= 0 && pos < m_lbSlotImages.size() && m_lbSlotImages[pos]) {
        QPixmap px = lookbookProductPixmap(*p, QSize(210, 145));
        if (!px.isNull()) {
          m_lbSlotImages[pos]->setPixmap(px);
          m_lbSlotImages[pos]->setText("");
        }
      }
    }

    refreshLookbookEditorMeta();
  }

  void removeLookbookItem(int position)
  {
    if (m_lbCurrentProjectId <= 0) return;
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) return;
    QSqlQuery q(db);
    q.prepare("DELETE FROM lookbook_items WHERE project_id=:pid AND position=:pos");
    q.bindValue(":pid", m_lbCurrentProjectId);
    q.bindValue(":pos", position);
    q.exec();
    loadLookbookSlots();
    refreshLookbookProjectsList();
    refreshLookbookShowcaseSections();
  }

  void pickProductForPosition(int position)
  {
    if (m_lbCurrentProjectId <= 0) {
      showLookbookInfo("Лукбук", "Сначала открой мини-проект.");
      return;
    }

    const QString slotTitle = lookbookSlotTitle(position);
    const bool hasCurrentItem =
      position >= 0 &&
      position < m_lbSlotProductIds.size() &&
      m_lbSlotProductIds[position] > 0;

    QDialog dlg(this);
    dlg.setObjectName("LookbookPickProductDialog");
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setAttribute(Qt::WA_TranslucentBackground, true);
    dlg.setModal(true);
    dlg.resize(980, 680);
    dlg.setStyleSheet(
     "QDialog#LookbookPickProductDialog{background:#00000000;}"
     "QFrame#PickProductCard{background:#F4EFEA;border:1px solid #E1D6CE;border-radius:22px;}"
     "QLabel{background:transparent;border:none;color:#1F1A17;}"
     "QLabel#PickProductTitle{font-size:22px;font-weight:900;color:#151515;}"
     "QLabel#PickProductHint{font-size:13px;color:#6D625A;}"
     "QLabel#PickProductCounter{font-size:12px;color:#6B625A;}"
     "QLineEdit#PickProductSearch,QComboBox#PickProductFilter{background:#FFFFFF;border:1px solid #DDD3CA;border-radius:12px;min-height:42px;padding:0 14px;color:#2F2F2F;}"
     "QLineEdit#PickProductSearch:focus,QComboBox#PickProductFilter:focus{border:1px solid #D2A08A;}"
     "QScrollArea#PickProductScroll{background:transparent;border:none;}"
     "QWidget#PickProductGrid{background:transparent;border:none;}"
     "QFrame#PickProductItemCard{background:#FFFFFF;border:1px solid #E4DAD2;border-radius:14px;}"
     "QLabel#PickProductImage{background:#F8F3EE;border:1px solid #EADFD6;border-radius:10px;color:#A29A92;}"
     "QLabel#PickProductName{font-size:13px;font-weight:900;color:#1F1A17;}"
     "QLabel#PickProductMeta{font-size:11px;color:#665D55;}"
     "QLabel#PickProductPrice{font-size:13px;font-weight:900;color:#1F1A17;}"
     "QPushButton#PickProductXBtn{background:transparent;color:#1F1A17;border:none;border-radius:16px;font-family:'Arial';font-size:18px;font-weight:900;min-width:32px;min-height:32px;max-width:32px;max-height:32px;padding:0;}"
     "QPushButton#PickProductXBtn:hover{background:#E8DDD4;color:#14100D;}"
     "QPushButton#PickProductMainBtn{background:#6F4B37;color:white;border:none;border-radius:10px;min-height:34px;padding:0 14px;font-weight:900;}"
     "QPushButton#PickProductMainBtn:hover{background:#5F3E2D;}"
     "QPushButton#PickProductCancelBtn{background:#FFFFFF;color:#3A342F;border:1px solid #DDD2CA;border-radius:12px;min-height:40px;padding:0 18px;font-weight:800;}"
     "QPushButton#PickProductCancelBtn:hover{background:#FBF7F3;}"
    );

    QVBoxLayout *outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(0, 0, 0, 0);

    QFrame *card = new QFrame(&dlg);
    card->setObjectName("PickProductCard");
    outer->addWidget(card);

    QVBoxLayout *root = new QVBoxLayout(card);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(12);

    QHBoxLayout *header = new QHBoxLayout();
    QVBoxLayout *textCol = new QVBoxLayout();
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(3);

    QLabel *title = new QLabel((hasCurrentItem ? "Изменить: " : "Добавить: ") + slotTitle, card);
    title->setObjectName("PickProductTitle");

    QLabel *hint = new QLabel(QString("Мини-каталог показывает только товары для позиции «%1». Фильтры работают внутри этой категории.").arg(slotTitle), card);
    hint->setObjectName("PickProductHint");
    hint->setWordWrap(true);

    textCol->addWidget(title);
    textCol->addWidget(hint);
    header->addLayout(textCol, 1);

    QPushButton *xBtn = new RoundCloseButton(card);
    xBtn->setObjectName("PickProductXBtn");
    xBtn->setFixedSize(32, 32);
    xBtn->setCursor(Qt::PointingHandCursor);
    header->addWidget(xBtn, 0, Qt::AlignTop);
    root->addLayout(header);

    QHBoxLayout *filters = new QHBoxLayout();
    filters->setSpacing(10);

    QLineEdit *search = new QLineEdit(card);
    search->setObjectName("PickProductSearch");
    search->setPlaceholderText("Поиск по названию, цвету, стилю или сезону...");
    search->addAction(QIcon(":/icons/search.png"), QLineEdit::LeadingPosition);

    QComboBox *categoryBox = new QComboBox(card);
    categoryBox->setObjectName("PickProductFilter");
    categoryBox->addItem("Все категории");

    QComboBox *colorBox = new QComboBox(card);
    colorBox->setObjectName("PickProductFilter");
    colorBox->addItem("Все цвета");

    QComboBox *sizeBox = new QComboBox(card);
    sizeBox->setObjectName("PickProductFilter");
    sizeBox->addItem("Все размеры");

    QSet<QString> categories;
    QSet<QString> colors;
    QSet<QString> sizes;

    for (const Product &p : m_products) {
      if (!productAllowedForLookbookSlot(position, p))
        continue;

      if (!p.category.trimmed().isEmpty())
        categories.insert(p.category.trimmed());
      if (!p.color.trimmed().isEmpty())
        colors.insert(p.color.trimmed());
      if (!p.size.trimmed().isEmpty())
        sizes.insert(p.size.trimmed());
    }

    QStringList categoryList = categories.values();
    QStringList colorList = colors.values();
    QStringList sizeList = sizes.values();
    std::sort(categoryList.begin(), categoryList.end());
    std::sort(colorList.begin(), colorList.end());
    std::sort(sizeList.begin(), sizeList.end());

    categoryBox->addItems(categoryList);
    colorBox->addItems(colorList);
    sizeBox->addItems(sizeList);

    QLabel *counter = new QLabel(card);
    counter->setObjectName("PickProductCounter");

    filters->addWidget(search, 1);
    filters->addWidget(categoryBox);
    filters->addWidget(colorBox);
    filters->addWidget(sizeBox);
    filters->addWidget(counter);
    root->addLayout(filters);

    QScrollArea *scroll = new QScrollArea(card);
    scroll->setObjectName("PickProductScroll");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *gridWidget = new QWidget(scroll);
    gridWidget->setObjectName("PickProductGrid");

    QGridLayout *grid = new QGridLayout(gridWidget);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);

    scroll->setWidget(gridWidget);
    root->addWidget(scroll, 1);

    auto clearGrid = [&]() {
      while (QLayoutItem *child = grid->takeAt(0)) {
        if (QWidget *w = child->widget())
          w->deleteLater();
        delete child;
      }
    };

    auto productMatchesFilters = [&](const Product &p) {
      if (!productAllowedForLookbookSlot(position, p))
        return false;

      const QString needle = search->text().trimmed().toLower();
      const QString categoryFilter = categoryBox->currentText();
      const QString colorFilter = colorBox->currentText();
      const QString sizeFilter = sizeBox->currentText();

      const bool categoryOk =
        categoryFilter == "Все категории" ||
        p.category.compare(categoryFilter, Qt::CaseInsensitive) == 0;

      const bool colorOk =
        colorFilter == "Все цвета" ||
        p.color.compare(colorFilter, Qt::CaseInsensitive) == 0;

      const bool sizeOk =
        sizeFilter == "Все размеры" ||
        p.size.compare(sizeFilter, Qt::CaseInsensitive) == 0;

      const QString hay = (p.name + " " + p.category + " " + p.color + " " + p.styleTag + " " + p.season).toLower();
      const bool searchOk = needle.isEmpty() || hay.contains(needle);

      return categoryOk && colorOk && sizeOk && searchOk;
    };

    auto chooseProduct = [&](int productId) {
      if (setLookbookItemProduct(position, productId))
        dlg.accept();
    };

    auto makeProductCard = [&](const Product &p) -> QFrame* {
      QFrame *productCard = new QFrame(gridWidget);
      productCard->setObjectName("PickProductItemCard");
      productCard->setFixedSize(214, 254);

      QVBoxLayout *lay = new QVBoxLayout(productCard);
      lay->setContentsMargins(10, 10, 10, 10);
      lay->setSpacing(6);

      QLabel *img = new QLabel(productCard);
      img->setObjectName("PickProductImage");
      img->setFixedHeight(116);
      img->setAlignment(Qt::AlignCenter);

        QPixmap px = cachedScaledPixmap(p.imagePath, QSize(180, 106));
        if (!px.isNull())
          img->setPixmap(px);
      else
        img->setText("фото");

      QLabel *name = new QLabel(p.name, productCard);
      name->setObjectName("PickProductName");
      name->setWordWrap(true);
      name->setMaximumHeight(40);

      QLabel *meta = new QLabel(QString("%1 • %2 • %3").arg(p.category, p.color, p.size), productCard);
      meta->setObjectName("PickProductMeta");
      meta->setWordWrap(true);

      QLabel *price = new QLabel(QString("%1 ₽").arg(p.price, 0, 'f', 0), productCard);
      price->setObjectName("PickProductPrice");

      QPushButton *selectBtn = new QPushButton(hasCurrentItem ? "Заменить" : "Выбрать", productCard);
      selectBtn->setObjectName("PickProductMainBtn");
      selectBtn->setCursor(Qt::PointingHandCursor);

      lay->addWidget(img);
      lay->addWidget(name);
      lay->addWidget(meta);
      lay->addWidget(price);
      lay->addStretch();
      lay->addWidget(selectBtn);

      connect(selectBtn, &QPushButton::clicked, &dlg, [&, productId = p.id]() {
        chooseProduct(productId);
      });

      return productCard;
    };

    auto rebuild = [&]() {
      clearGrid();

      QVector<Product> matched;
      for (const Product &p : m_products) {
        if (productMatchesFilters(p))
          matched.push_back(p);
      }

      counter->setText(QString("Найдено: %1").arg(matched.size()));

      if (matched.isEmpty()) {
        QLabel *empty = new QLabel(QString("Нет товаров для позиции «%1». Попробуйте изменить фильтры.").arg(slotTitle), gridWidget);
        empty->setAlignment(Qt::AlignCenter);
        empty->setWordWrap(true);
        empty->setMinimumHeight(260);
        empty->setStyleSheet("background:#FFFFFF;border:1px dashed #D8CEC5;border-radius:12px;color:#8A8178;font-size:13px;padding:18px;");
        grid->addWidget(empty, 0, 0, 1, 4);
        return;
      }

      for (int i = 0; i < matched.size(); ++i)
        grid->addWidget(makeProductCard(matched[i]), i / 4, i % 4);
    };

    connect(search, &QLineEdit::textChanged, &dlg, [&](const QString&) { rebuild(); });
    connect(categoryBox, &QComboBox::currentTextChanged, &dlg, [&](const QString&) { rebuild(); });
    connect(colorBox, &QComboBox::currentTextChanged, &dlg, [&](const QString&) { rebuild(); });
    connect(sizeBox, &QComboBox::currentTextChanged, &dlg, [&](const QString&) { rebuild(); });

    QHBoxLayout *btns = new QHBoxLayout();
    btns->addStretch();

    QPushButton *cancel = new QPushButton("Отмена", card);
    cancel->setObjectName("PickProductCancelBtn");
    cancel->setCursor(Qt::PointingHandCursor);
    btns->addWidget(cancel);
    root->addLayout(btns);

    connect(xBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    rebuild();

    if (dlg.exec() == QDialog::Accepted) {
      loadLookbookSlots();
      refreshLookbookProjectsList();
      refreshLookbookShowcaseSections();
    }
  }



protected:
  bool eventFilter(QObject *obj, QEvent *ev) override
  {
    QWidget *w = qobject_cast<QWidget*>(obj);

    if (obj == m_contentHost && ev->type() == QEvent::Resize) {
      positionPageLoadingOverlay();
    }

    if (ev->type() == QEvent::Resize && w && w->property("lb_editor_main").toBool()) {
      positionLookbookCenterOverlay();
    }

    if (ev->type() == QEvent::Wheel && w && w->property("home_hidden_horizontal_scroll").toBool()) {
      QWheelEvent *wheel = static_cast<QWheelEvent*>(ev);
      QScrollArea *area = qobject_cast<QScrollArea*>(w->parentWidget());
      if (!area)
        area = qobject_cast<QScrollArea*>(w->parent());
      if (area && area->horizontalScrollBar()) {
        const int delta = wheel->angleDelta().y() != 0 ? wheel->angleDelta().y() : wheel->angleDelta().x();
        area->horizontalScrollBar()->setValue(area->horizontalScrollBar()->value() - delta);
        return true;
      }
    }

    if (ev->type() == QEvent::MouseButtonRelease) {
      if (w && w->property("home_collection_product_id").isValid()) {
        const int productId = w->property("home_collection_product_id").toInt();
        openCatalogAndShowProduct(productId);
        return true;
      }

      if (w && w->property("home_open_lookbook_link").toBool()) {
        openLookbookPage();
        return true;
      }
      if (w && w->property("lb_preview_product_id").isValid()) {
        const int productId = w->property("lb_preview_product_id").toInt();
        if (QDialog *dialog = qobject_cast<QDialog*>(w->window()))
          dialog->accept();
        openCatalogAndShowProduct(productId);
        return true;
      }
      if (w && w->property("lb_pos").isValid()) {
        const int pos = w->property("lb_pos").toInt();
        if (pos >= 0 && pos < 4)
          pickProductForPosition(pos);
        return true;
      }
    }
    return QMainWindow::eventFilter(obj, ev);
  }



  void clearGrid()
  {
    while (QLayoutItem *item = m_gridLayout->takeAt(0)) {
      if (QWidget *w = item->widget())
        w->deleteLater();
      delete item;
    }
  }

  void rebuildGrid(const QVector<Product> &list)
  {
    ++m_gridBuildGeneration;
    clearGrid();

    const int columns = 3;
    int row = 0, col = 0;

    for (const Product &p : list) {
      m_gridLayout->addWidget(createProductCard(p), row, col);
      if (++col == columns) { col = 0; ++row; }
    }

    if (list.isEmpty()) {
      auto *lbl = new QLabel("Ничего не найдено.", m_gridWidget);
      lbl->setAlignment(Qt::AlignCenter);
      m_gridLayout->addWidget(lbl, row, 0, 1, columns);
      return;
    }

    // ---- Additional recommendations ----
    if (m_smartCatalogActive && !m_softRecommendations.isEmpty()) {

      if (col != 0) { col = 0; ++row; }

      m_gridLayout->addItem(
        new QSpacerItem(0, 18, QSizePolicy::Minimum, QSizePolicy::Fixed),
        row, 0, 1, columns
        );
      ++row;

      QLabel *title = new QLabel("Вам может понравиться", m_gridWidget);
      title->setAlignment(Qt::AlignCenter);
      title->setStyleSheet("font-weight:700; font-size:16px; margin-top:6px; margin-bottom:10px;");
      m_gridLayout->addWidget(title, row, 0, 1, columns);
      ++row;

      int rcol = 0;
      for (const Product &p : m_softRecommendations) {
        m_gridLayout->addWidget(createProductCard(p), row, rcol);
        if (++rcol == columns) { rcol = 0; ++row; }
      }
    }
  }

  void rebuildGridChunked(const QVector<Product> &list,
              const std::function<void()> &done = std::function<void()>())
  {
    const int generation = ++m_gridBuildGeneration;
    clearGrid();

    if (!m_gridLayout || !m_gridWidget) {
      if (done)
        done();
      return;
    }

    m_gridWidget->setUpdatesEnabled(false);

    const int columns = 3;
    auto row = std::make_shared<int>(0);
    auto col = std::make_shared<int>(0);
    auto primaryIndex = std::make_shared<int>(0);
    auto softIndex = std::make_shared<int>(0);
    auto phase = std::make_shared<int>(0);
    auto primary = std::make_shared<QVector<Product>>(list);
    auto soft = std::make_shared<QVector<Product>>(m_smartCatalogActive ? m_softRecommendations : QVector<Product>());

    auto finish = [this, generation, done]() {
      if (generation != m_gridBuildGeneration) {
        if (done)
          done();
        return;
      }
      if (m_gridWidget) {
        m_gridWidget->setUpdatesEnabled(true);
        m_gridWidget->updateGeometry();
        m_gridWidget->update();
      }
      if (done)
        done();
    };

    if (primary->isEmpty()) {
      auto *lbl = new QLabel("Ничего не найдено.", m_gridWidget);
      lbl->setAlignment(Qt::AlignCenter);
      m_gridLayout->addWidget(lbl, 0, 0, 1, columns);
      finish();
      return;
    }

    auto addProduct = [this, row, col, columns](const Product &p) {
      m_gridLayout->addWidget(createProductCard(p), *row, *col);
      if (++(*col) == columns) {
        *col = 0;
        ++(*row);
      }
    };

    auto step = std::make_shared<std::function<void()>>();
    *step = [this, generation, primary, soft, primaryIndex, softIndex, row, col, phase, addProduct, step, finish]() {
      if (generation != m_gridBuildGeneration) {
        finish();
        *step = std::function<void()>();
        return;
      }

      const int chunkSize = 6;
      int made = 0;

      if (*phase == 0) {
        while (*primaryIndex < primary->size() && made < chunkSize) {
          addProduct(primary->at(*primaryIndex));
          ++(*primaryIndex);
          ++made;
        }

        if (*primaryIndex < primary->size()) {
          QTimer::singleShot(0, this, *step);
          return;
        }

        if (soft->isEmpty()) {
          finish();
          *step = std::function<void()>();
          return;
        }

        if (*col != 0) {
          *col = 0;
          ++(*row);
        }

        m_gridLayout->addItem(
          new QSpacerItem(0, 18, QSizePolicy::Minimum, QSizePolicy::Fixed),
          *row, 0, 1, columns
        );
        ++(*row);

        QLabel *title = new QLabel("Вам может понравиться", m_gridWidget);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet("font-weight:700; font-size:16px; margin-top:6px; margin-bottom:10px;");
        m_gridLayout->addWidget(title, *row, 0, 1, columns);
        ++(*row);

        *phase = 1;
        QTimer::singleShot(0, this, *step);
        return;
      }

      while (*softIndex < soft->size() && made < chunkSize) {
        addProduct(soft->at(*softIndex));
        ++(*softIndex);
        ++made;
      }

      if (*softIndex < soft->size()) {
        QTimer::singleShot(0, this, *step);
        return;
      }

      finish();
      *step = std::function<void()>();
    };

    QTimer::singleShot(0, this, *step);
  }

  void resetFilters()
  {
    m_smartCatalogActive = false;
    m_catalogBase = m_products;
    m_softRecommendations.clear();

    if (!m_categoryBox || !m_sizeBox || !m_colorBox) {
      rebuildGridChunked(m_products);
      return;
    }

    const QSignalBlocker b1(m_categoryBox);
    const QSignalBlocker b2(m_sizeBox);
    const QSignalBlocker b3(m_colorBox);
    const QSignalBlocker b4(m_seasonBox);
    const QSignalBlocker b5(m_styleFilterBox);
    const QSignalBlocker b6(m_priceMinSpin);
    const QSignalBlocker b7(m_priceMaxSpin);

    m_categoryBox->setCurrentIndex(0);
    m_sizeBox->setCurrentIndex(0);
    m_colorBox->setCurrentIndex(0);
    if (m_seasonBox) m_seasonBox->setCurrentIndex(0);
    if (m_styleFilterBox) m_styleFilterBox->setCurrentIndex(0);
    if (m_priceMinSpin) m_priceMinSpin->setValue(0);
    if (m_priceMaxSpin) m_priceMaxSpin->setValue(10000);

    applyFilter();
  }


  void editProfile()
  {
    UserProfileDialog dlg(m_username, this);
    dlg.exec();
  }

  void showMyOrders()
  {
    MyOrdersDialog dlg(m_username, this);
    dlg.exec();
  }
  void openAdmin()
  {
    // 1. Create the dialog on the stack.
    // Memory is released automatically when the dialog closes.
    AdminDialog dlg(this);

    // 2. Run the dialog in modal mode.
    // Execution continues after the administrator window is closed.
    dlg.exec();

    // 3. Refresh the catalog after the dialog closes.
    // By this point, database changes have already been applied.
    m_products = loadProductsFromDb();
    m_catalogBase = m_products;
    m_smartCatalogActive = false;
    applyFilter();
  }



  
  // ===== Project helper functions =====
  void refreshProjectsUI()
  {
    if (!m_projectsList)
      return;

    m_projectsList->setUpdatesEnabled(false);
    m_projectsList->clear();
    m_projectsList->setFrameShape(QFrame::NoFrame);
    m_projectsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_projectsList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_projectsList->setSelectionMode(QAbstractItemView::NoSelection);
    m_projectsList->setFocusPolicy(Qt::NoFocus);
    m_projectsList->setStyleSheet(
     "QListWidget#ProjectsList{background:#FFFFFF;border:1px solid #E5DBD2;border-radius:16px;padding:12px;}"
     "QListWidget#ProjectsList::item{background:transparent;border:none;margin:0;padding:0;}"
    );

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
      if (m_projectsEmptyPanel) m_projectsEmptyPanel->setVisible(true);
      m_projectsList->setVisible(false);
      m_projectsList->setUpdatesEnabled(true);
      return;
    }

    QSqlQuery q(db);
    q.prepare("SELECT id, title, IFNULL(created_at, '') FROM projects WHERE username=? ORDER BY id DESC");
    q.addBindValue(m_username);
    q.exec();

    int count = 0;

    auto formatProjectDate = [](const QString &raw) {
      QDateTime dt = QDateTime::fromString(raw, "yyyy-MM-dd HH:mm:ss");
      if (!dt.isValid())
        dt = QDateTime::fromString(raw, Qt::ISODate);
      if (dt.isValid())
        return dt.date().toString("dd.MM.yyyy");
      return QDate::currentDate().toString("dd.MM.yyyy");
    };

    auto displayProjectTitle = [](const QString &raw) {
      const QString t = raw.trimmed();
      return t.isEmpty() ? QString("Новый проект") : t;
    };

    auto productById = [&](int id) -> const Product* {
      for (const Product &p : m_products) {
        if (p.id == id)
          return &p;
      }
      return nullptr;
    };

    while (q.next()) {
      const int projectId = q.value(0).toInt();
      const QString title = q.value(1).toString();
      const QString createdAt = q.value(2).toString();

      struct ProjectItemView
      {
        int productId = 0;
        QString name;
        QString category;
        QString color;
        QString size;
        QString imagePath;
        double price = 0.0;
        int qty = 1;
      };

      QVector<ProjectItemView> items;
      QSqlQuery itemQ(db);
      itemQ.prepare(
       "SELECT p.id, p.name, IFNULL(p.category,''), IFNULL(p.color,''), IFNULL(p.size,''), "
       "IFNULL(p.image_path,''), IFNULL(p.price,0), IFNULL(pi.qty,1) "
       "FROM project_items pi "
       "JOIN products p ON p.id = pi.product_id "
       "WHERE pi.project_id=? "
       "ORDER BY pi.id ASC");
      itemQ.addBindValue(projectId);
      if (itemQ.exec()) {
        while (itemQ.next()) {
          ProjectItemView item;
          item.productId = itemQ.value(0).toInt();
          item.name = itemQ.value(1).toString();
          item.category = itemQ.value(2).toString();
          item.color = itemQ.value(3).toString();
          item.size = itemQ.value(4).toString();
          item.imagePath = itemQ.value(5).toString();
          item.price = itemQ.value(6).toDouble();
          item.qty = itemQ.value(7).toInt();
          items.push_back(item);
        }
      }


      int itemCount = 0;
      for (const ProjectItemView &item : items)
        itemCount += qMax(1, item.qty);

      QListWidgetItem *listItem = new QListWidgetItem(m_projectsList);
      listItem->setData(Qt::UserRole, projectId);
      listItem->setSizeHint(QSize(0, 540));

      QFrame *card = new QFrame(m_projectsList);
      card->setObjectName("ProjectDashboardCard");
      card->setMinimumHeight(520);
      card->setStyleSheet(
       "QFrame#ProjectDashboardCard{background:#FFFFFF;border:1px solid #E5DBD2;border-radius:14px;}"
       "QFrame#ProjectTopPanel{background:#F3E8DE;border:1px solid #DABBA4;border-radius:12px;}"
       "QLabel#ProjectDashTitle{font-size:18px;font-weight:900;color:#1F1A17;background:transparent;border:none;}"
       "QLabel#ProjectDashSubtitle{font-size:12px;color:#4E4540;background:transparent;border:none;}"
       "QLabel#ProjectDashMeta{font-size:12px;color:#2F2925;background:transparent;border:none;}"
       "QPushButton#ProjectSmallAction{background:#F8F3EE;border:1px solid #CDBFB4;border-radius:7px;min-height:28px;padding:0 10px;color:#3A342F;font-size:12px;font-weight:700;}"
       "QPushButton#ProjectSmallAction:hover{background:#EFE4DA;}"
       "QFrame#ProjectItemCard{background:#F8F3EE;border:1px solid #DACFC6;border-radius:10px;}"
       "QLabel#ProjectItemImage{background:#EFEAE5;border:none;border-radius:8px;color:#A39990;font-size:11px;}"
       "QLabel#ProjectItemName{font-size:12px;font-weight:800;color:#111111;background:transparent;border:none;}"
       "QLabel#ProjectItemPrice{font-size:12px;font-weight:900;color:#111111;background:transparent;border:none;}"
       "QLabel#ProjectItemSize{font-size:11px;color:#3F3833;background:transparent;border:none;}"
       "QToolButton#ProjectItemRemove{background:#FFFFFF;border:none;border-radius:9px;color:#4B4038;font-size:12px;font-weight:900;}"
       "QToolButton#ProjectItemRemove:hover{background:#EADDD2;}"
       "QFrame#ProjectSidePanel{background:#FFFFFF;border:none;}"
       "QLabel#ProjectSideTitle{font-size:13px;font-weight:900;color:#1F1A17;background:transparent;border:none;}"
       "QToolButton#ProjectColorChip{border:1px solid #CDBFB4;border-radius:6px;}""QToolButton#ProjectColorChip:hover{border:2px solid #6F4B37;}""QLabel#ProjectPaletteHint{font-size:11px;color:#7B7068;background:transparent;border:none;}"
       "QPushButton#ProjectSimilarBtn{background:#F8F3EE;border:1px solid #CDBFB4;border-radius:9px;min-height:62px;color:#2B2521;font-size:13px;font-weight:900;}"
       "QPushButton#ProjectSimilarBtn:hover{background:#EFE4DA;}"
       "QFrame#ProjectRecommendationBox{background:#F3E8DE;border:1px solid #DABBA4;border-radius:10px;}"
       "QLabel#ProjectRecommendationText{font-size:12px;color:#5F564F;background:transparent;border:none;}"
      );

      QVBoxLayout *cardRoot = new QVBoxLayout(card);
      cardRoot->setContentsMargins(14, 14, 14, 14);
      cardRoot->setSpacing(12);

      QFrame *topPanel = new QFrame(card);
      topPanel->setObjectName("ProjectTopPanel");
      QHBoxLayout *topLay = new QHBoxLayout(topPanel);
      topLay->setContentsMargins(14, 10, 14, 10);
      topLay->setSpacing(10);

      QVBoxLayout *titleCol = new QVBoxLayout();
      titleCol->setContentsMargins(0, 0, 0, 0);
      titleCol->setSpacing(2);

      QLabel *projectTitle = new QLabel(displayProjectTitle(title), topPanel);
      projectTitle->setObjectName("ProjectDashTitle");

      QLabel *subtitle = new QLabel("Ваш проект для подбора и сохранения вещей", topPanel);
      subtitle->setObjectName("ProjectDashSubtitle");

      QLabel *meta = new QLabel(QString("%1 • %2 вещей • Создан %3")
                   .arg(title.trimmed().isEmpty() ? QString("проект") : title.trimmed())
                   .arg(itemCount)
                   .arg(formatProjectDate(createdAt)), topPanel);
      meta->setObjectName("ProjectDashMeta");

      titleCol->addWidget(projectTitle);
      titleCol->addWidget(subtitle);
      titleCol->addWidget(meta);

      topLay->addLayout(titleCol, 1);

      QPushButton *addItemBtn = new QPushButton("+ Добавить", topPanel);
      addItemBtn->setObjectName("ProjectSmallAction");
      addItemBtn->setCursor(Qt::PointingHandCursor);

      QPushButton *editBtn = new QPushButton("Редактировать", topPanel);
      editBtn->setObjectName("ProjectSmallAction");
      editBtn->setCursor(Qt::PointingHandCursor);

      QPushButton *shareBtn = new QPushButton("↗ Поделиться", topPanel);
      shareBtn->setObjectName("ProjectSmallAction");
      shareBtn->setCursor(Qt::PointingHandCursor);

      QPushButton *deleteBtn = new QPushButton("Удалить", topPanel);
      deleteBtn->setObjectName("ProjectSmallAction");
      deleteBtn->setCursor(Qt::PointingHandCursor);

      topLay->addWidget(addItemBtn);
      topLay->addWidget(editBtn);
      topLay->addWidget(shareBtn);
      topLay->addWidget(deleteBtn);

      cardRoot->addWidget(topPanel);

      QHBoxLayout *mainRow = new QHBoxLayout();
      mainRow->setContentsMargins(0, 0, 0, 0);
      mainRow->setSpacing(12);

      QWidget *productsColumn = new QWidget(card);
      productsColumn->setStyleSheet("background:transparent;border:none;");
      QVBoxLayout *productsColumnLay = new QVBoxLayout(productsColumn);
      productsColumnLay->setContentsMargins(0, 0, 0, 0);
      productsColumnLay->setSpacing(12);

      QWidget *productsArea = new QWidget(productsColumn);
      productsArea->setStyleSheet("background:transparent;border:none;");
      productsArea->setMinimumHeight(245);
      QGridLayout *grid = new QGridLayout(productsArea);
      grid->setContentsMargins(0, 0, 0, 0);
      grid->setHorizontalSpacing(10);
      grid->setVerticalSpacing(10);

      productsColumnLay->addWidget(productsArea, 1);

      QVector<QFrame*> projectItemCards;
      QVector<QToolButton*> projectRemoveButtons;

      auto makeProductCard = [&](const ProjectItemView &item) -> QFrame* {
        QFrame *itemCard = new QFrame(productsArea);
        itemCard->setProperty("project_item_color_raw", item.color);
        itemCard->setObjectName("ProjectItemCard");
        itemCard->setFixedSize(134, 148);

        QVBoxLayout *itemLay = new QVBoxLayout(itemCard);
        itemLay->setContentsMargins(8, 8, 8, 6);
        itemLay->setSpacing(3);

        QWidget *imageWrap = new QWidget(itemCard);
        imageWrap->setStyleSheet("background:transparent;border:none;");
        imageWrap->setFixedHeight(78);

        QLabel *img = new QLabel(imageWrap);
        img->setObjectName("ProjectItemImage");
        img->setAlignment(Qt::AlignCenter);
        img->setGeometry(0, 0, 118, 78);

        QPixmap px = cachedScaledPixmap(item.imagePath, QSize(110, 72));
        if (!px.isNull())
          img->setPixmap(px);
        else
          img->setText("фото");

        QToolButton *removeBtn = new QToolButton(imageWrap);
        removeBtn->setObjectName("ProjectItemRemove");
        removeBtn->setText("×");
        removeBtn->setFixedSize(20, 20);
        removeBtn->move(98, 2);
        removeBtn->setCursor(Qt::PointingHandCursor);
        removeBtn->hide();
        projectRemoveButtons.push_back(removeBtn);

        itemLay->addWidget(imageWrap);

        QLabel *name = new QLabel(item.name, itemCard);
        name->setObjectName("ProjectItemName");
        name->setWordWrap(true);
        name->setMaximumHeight(30);
        itemLay->addWidget(name);

        QLabel *price = new QLabel(QString("%1 ₽").arg(item.price, 0, 'f', 2), itemCard);
        price->setObjectName("ProjectItemPrice");
        itemLay->addWidget(price);

        QLabel *size = new QLabel(QString("Размер: %1").arg(item.size.trimmed().isEmpty() ? QString("M") : item.size), itemCard);
        size->setObjectName("ProjectItemSize");
        itemLay->addWidget(size);

        connect(removeBtn, &QToolButton::clicked, this, [this, projectId, productId = item.productId]() {
          QSqlQuery del;
          del.prepare("DELETE FROM project_items WHERE project_id=? AND product_id=?");
          del.addBindValue(projectId);
          del.addBindValue(productId);
          del.exec();
          refreshProjectsUI();
        });

        projectItemCards.push_back(itemCard);
        return itemCard;
      };


      auto openMiniProjectCatalog = [this, projectId]() {
        QDialog catalogDlg(this);
        catalogDlg.setObjectName("ProjectMiniCatalogDialog");
        catalogDlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        catalogDlg.setAttribute(Qt::WA_TranslucentBackground, true);
        catalogDlg.setModal(true);
        catalogDlg.resize(980, 680);
        catalogDlg.setStyleSheet(
         "QDialog#ProjectMiniCatalogDialog{background:#00000000;}"
         "QFrame#MiniCatalogCard{background:#F4EFEA;border:1px solid #E1D6CE;border-radius:18px;}"
         "QLabel{background:transparent;border:none;color:#1F1A17;}"
         "QLabel#MiniCatalogTitle{font-size:22px;font-weight:900;}"
         "QLabel#MiniCatalogHint{font-size:12px;color:#7B7068;}"
         "QLabel#MiniCatalogCounter{font-size:12px;color:#6B625A;}"
         "QPushButton#MiniCatalogX{background:transparent;border:none;color:#1F1A17;font-size:24px;font-weight:900;}"
         "QLineEdit,QComboBox{background:#FFFFFF;border:1px solid #D8CEC5;border-radius:10px;min-height:38px;padding:0 12px;color:#1F1A17;}"
         "QLineEdit:focus,QComboBox:focus{border:1px solid #B56D46;}"
         "QScrollArea#MiniCatalogScroll{background:transparent;border:none;}"
         "QWidget#MiniCatalogGrid{background:transparent;border:none;}"
         "QFrame#MiniCatalogProductCard{background:#FFFFFF;border:1px solid #E5DBD2;border-radius:14px;}"
         "QLabel#MiniCatalogImage{background:#F8F3EE;border:1px solid #EADFD6;border-radius:10px;color:#A29A92;}"
         "QLabel#MiniCatalogName{font-size:13px;font-weight:900;color:#1F1A17;}"
         "QLabel#MiniCatalogMeta{font-size:11px;color:#665D55;}"
         "QLabel#MiniCatalogPrice{font-size:13px;font-weight:900;color:#1F1A17;}"
         "QPushButton#MiniCatalogAdd{background:#6F4B37;color:white;border:none;border-radius:9px;min-height:32px;padding:0 10px;font-weight:800;}"
         "QPushButton#MiniCatalogAdd:hover{background:#5F3E2D;}"
         "QPushButton#MiniCatalogClose{background:#FFFFFF;color:#3A342F;border:1px solid #DDD2CA;border-radius:10px;min-height:38px;padding:0 18px;font-weight:800;}"
        );

        QVBoxLayout *outer = new QVBoxLayout(&catalogDlg);
        outer->setContentsMargins(0, 0, 0, 0);

        QFrame *card = new QFrame(&catalogDlg);
        card->setObjectName("MiniCatalogCard");
        outer->addWidget(card);

        QVBoxLayout *root = new QVBoxLayout(card);
        root->setContentsMargins(22, 18, 22, 18);
        root->setSpacing(12);

        QHBoxLayout *header = new QHBoxLayout();
        QVBoxLayout *titleBox = new QVBoxLayout();
        titleBox->setSpacing(3);

        QLabel *title = new QLabel("Добавить вещь в проект", card);
        title->setObjectName("MiniCatalogTitle");

        QLabel *hint = new QLabel("Выберите товар из каталога. Фильтры можно комбинировать: категория + цвет + размер + поиск.", card);
        hint->setObjectName("MiniCatalogHint");
        hint->setWordWrap(true);

        titleBox->addWidget(title);
        titleBox->addWidget(hint);
        header->addLayout(titleBox, 1);

        QPushButton *closeX = new RoundCloseButton(card);
        closeX->setObjectName("MiniCatalogX");
        closeX->setFixedSize(34, 34);
        closeX->setCursor(Qt::PointingHandCursor);
        header->addWidget(closeX, 0, Qt::AlignTop);
        root->addLayout(header);

        QHBoxLayout *filters = new QHBoxLayout();
        filters->setSpacing(10);

        QLineEdit *searchEdit = new QLineEdit(card);
        searchEdit->setPlaceholderText("Поиск по названию, цвету, стилю или сезону...");

        QComboBox *categoryBox = new QComboBox(card);
        categoryBox->addItem("Все товары");

        QComboBox *colorBox = new QComboBox(card);
        colorBox->addItem("Все цвета");

        QComboBox *sizeBox = new QComboBox(card);
        sizeBox->addItem("Все размеры");

        QSet<QString> categorySet;
        QSet<QString> colorSet;
        QSet<QString> sizeSet;

        for (const Product &product : m_products) {
          if (!product.category.trimmed().isEmpty())
            categorySet.insert(product.category.trimmed());
          if (!product.color.trimmed().isEmpty())
            colorSet.insert(product.color.trimmed());
          if (!product.size.trimmed().isEmpty())
            sizeSet.insert(product.size.trimmed());
        }

        QStringList categories = categorySet.values();
        QStringList colors = colorSet.values();
        QStringList sizes = sizeSet.values();
        std::sort(categories.begin(), categories.end());
        std::sort(colors.begin(), colors.end());
        std::sort(sizes.begin(), sizes.end());

        categoryBox->addItems(categories);
        colorBox->addItems(colors);
        sizeBox->addItems(sizes);

        QLabel *countLabel = new QLabel(card);
        countLabel->setObjectName("MiniCatalogCounter");

        filters->addWidget(searchEdit, 1);
        filters->addWidget(categoryBox);
        filters->addWidget(colorBox);
        filters->addWidget(sizeBox);
        filters->addWidget(countLabel);
        root->addLayout(filters);

        QScrollArea *scroll = new QScrollArea(card);
        scroll->setObjectName("MiniCatalogScroll");
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        QWidget *gridWidget = new QWidget(scroll);
        gridWidget->setObjectName("MiniCatalogGrid");

        QGridLayout *catalogGrid = new QGridLayout(gridWidget);
        catalogGrid->setContentsMargins(0, 0, 0, 0);
        catalogGrid->setHorizontalSpacing(12);
        catalogGrid->setVerticalSpacing(12);

        scroll->setWidget(gridWidget);
        root->addWidget(scroll, 1);

        bool changed = false;

        auto clearCatalogGrid = [&]() {
          while (QLayoutItem *child = catalogGrid->takeAt(0)) {
            if (QWidget *w = child->widget())
              w->deleteLater();
            delete child;
          }
        };

        auto productMatchesFilters = [&](const Product &product) {
          const QString search = searchEdit->text().trimmed().toLower();
          const QString categoryFilter = categoryBox->currentText();
          const QString colorFilter = colorBox->currentText();
          const QString sizeFilter = sizeBox->currentText();

          const bool categoryOk =
            categoryFilter == "Все товары" ||
            product.category.compare(categoryFilter, Qt::CaseInsensitive) == 0;

          const bool colorOk =
            colorFilter == "Все цвета" ||
            product.color.compare(colorFilter, Qt::CaseInsensitive) == 0;

          const bool sizeOk =
            sizeFilter == "Все размеры" ||
            product.size.compare(sizeFilter, Qt::CaseInsensitive) == 0;

          const bool searchOk =
            search.isEmpty() ||
            product.name.toLower().contains(search) ||
            product.category.toLower().contains(search) ||
            product.color.toLower().contains(search) ||
            product.styleTag.toLower().contains(search) ||
            product.season.toLower().contains(search);

          return categoryOk && colorOk && sizeOk && searchOk;
        };

        auto makeCatalogProductCard = [&](const Product &product) -> QFrame* {
          QFrame *productCard = new QFrame(gridWidget);
          productCard->setObjectName("MiniCatalogProductCard");
          productCard->setFixedSize(214, 254);

          QVBoxLayout *lay = new QVBoxLayout(productCard);
          lay->setContentsMargins(10, 10, 10, 10);
          lay->setSpacing(6);

          QLabel *img = new QLabel(productCard);
          img->setObjectName("MiniCatalogImage");
          img->setFixedHeight(116);
          img->setAlignment(Qt::AlignCenter);

            QPixmap px = cachedScaledPixmap(product.imagePath, QSize(180, 106));
            if (!px.isNull())
              img->setPixmap(px);
          else
            img->setText("фото");

          QLabel *name = new QLabel(product.name, productCard);
          name->setObjectName("MiniCatalogName");
          name->setWordWrap(true);
          name->setMaximumHeight(40);

          QLabel *meta = new QLabel(QString("%1 • %2 • %3").arg(product.category, product.color, product.size), productCard);
          meta->setObjectName("MiniCatalogMeta");
          meta->setWordWrap(true);

          QLabel *price = new QLabel(QString("%1 ₽").arg(product.price, 0, 'f', 0), productCard);
          price->setObjectName("MiniCatalogPrice");

          QPushButton *addBtn = new QPushButton("Добавить", productCard);
          addBtn->setObjectName("MiniCatalogAdd");
          addBtn->setCursor(Qt::PointingHandCursor);

          lay->addWidget(img);
          lay->addWidget(name);
          lay->addWidget(meta);
          lay->addWidget(price);
          lay->addStretch();
          lay->addWidget(addBtn);

          connect(addBtn, &QPushButton::clicked, &catalogDlg, [&, productId = product.id, addBtn]() {
            QSqlQuery upd;
            upd.prepare("UPDATE project_items SET qty = qty + 1 WHERE project_id = ? AND product_id = ?");
            upd.addBindValue(projectId);
            upd.addBindValue(productId);
            upd.exec();

            if (upd.numRowsAffected() == 0) {
              QSqlQuery ins;
              ins.prepare("INSERT INTO project_items(project_id, product_id, qty) VALUES(?, ?, 1)");
              ins.addBindValue(projectId);
              ins.addBindValue(productId);
              ins.exec();
            }

            changed = true;
            addBtn->setText("Добавлено");
            addBtn->setEnabled(false);
          });

          return productCard;
        };

        auto rebuildCatalogGrid = [&]() {
          clearCatalogGrid();

          QVector<Product> matched;
          for (const Product &product : m_products) {
            if (productMatchesFilters(product))
              matched.push_back(product);
          }

          countLabel->setText(QString("Найдено: %1").arg(matched.size()));

          if (matched.isEmpty()) {
            QLabel *empty = new QLabel("Ничего не найдено. Попробуйте изменить фильтры.", gridWidget);
            empty->setAlignment(Qt::AlignCenter);
            empty->setWordWrap(true);
            empty->setMinimumHeight(240);
            empty->setStyleSheet("background:#FFFFFF;border:1px dashed #D8CEC5;border-radius:12px;color:#8A8178;font-size:13px;padding:18px;");
            catalogGrid->addWidget(empty, 0, 0, 1, 4);
            return;
          }

          for (int i = 0; i < matched.size(); ++i)
            catalogGrid->addWidget(makeCatalogProductCard(matched[i]), i / 4, i % 4);
        };

        connect(searchEdit, &QLineEdit::textChanged, &catalogDlg, [&](const QString &) {
          rebuildCatalogGrid();
        });
        connect(categoryBox, &QComboBox::currentTextChanged, &catalogDlg, [&](const QString &) {
          rebuildCatalogGrid();
        });
        connect(colorBox, &QComboBox::currentTextChanged, &catalogDlg, [&](const QString &) {
          rebuildCatalogGrid();
        });
        connect(sizeBox, &QComboBox::currentTextChanged, &catalogDlg, [&](const QString &) {
          rebuildCatalogGrid();
        });

        connect(closeX, &QPushButton::clicked, &catalogDlg, &QDialog::accept);

        QHBoxLayout *bottom = new QHBoxLayout();
        bottom->addStretch();

        QPushButton *closeBtn = new QPushButton("Закрыть", card);
        closeBtn->setObjectName("MiniCatalogClose");
        closeBtn->setCursor(Qt::PointingHandCursor);
        bottom->addWidget(closeBtn);
        root->addLayout(bottom);

        connect(closeBtn, &QPushButton::clicked, &catalogDlg, &QDialog::accept);

        rebuildCatalogGrid();
        catalogDlg.exec();

        if (changed)
          refreshProjectsUI();
      };

      QVector<ProjectItemView> shownItems = items;
      const int maxCards = qMin(10, shownItems.size());
      for (int i = 0; i < maxCards; ++i) {
        grid->addWidget(makeProductCard(shownItems[i]), i / 5, i % 5);
      }

      if (maxCards == 0) {
        QLabel *empty = new QLabel("В проекте пока нет вещей. Нажмите «Редактировать», чтобы добавить товары.", productsArea);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet("color:#8A8178;font-size:13px;background:#FBF8F5;border:1px dashed #D8CEC5;border-radius:12px;");
        empty->setMinimumHeight(220);
        grid->addWidget(empty, 0, 0, 1, 5);
      }

      mainRow->addWidget(productsColumn, 1);

      QFrame *sidePanel = new QFrame(card);
      sidePanel->setObjectName("ProjectSidePanel");
      sidePanel->setFixedWidth(330);
      QVBoxLayout *sideLay = new QVBoxLayout(sidePanel);
      sideLay->setContentsMargins(0, 0, 0, 0);
      sideLay->setSpacing(12);

      QLabel *paletteTitle = new QLabel("Цветовая палитра", sidePanel);
      paletteTitle->setObjectName("ProjectSideTitle");
      sideLay->addWidget(paletteTitle);

      QHBoxLayout *chips = new QHBoxLayout();
      chips->setContentsMargins(0, 0, 0, 0);
      chips->setSpacing(8);

      auto colorForName = [](QString raw) {
        const QString s = raw.trimmed().toLower();
        if (s.contains("черн")) return QString("#1E1E1E");
        if (s.contains("бел")) return QString("#F2EFEA");
        if (s.contains("сер")) return QString("#777B7E");
        if (s.contains("крас")) return QString("#C45A3F");
        if (s.contains("син")) return QString("#3D5F91");
        if (s.contains("беж") || s.contains("пес") || s.contains("молоч")) return QString("#CDB48D");
        if (s.contains("корич") || s.contains("карам")) return QString("#7A4B35");
        if (s.contains("террак")) return QString("#B86644");
        if (s.contains("олив") || s.contains("зел") || s.contains("хаки")) return QString("#68704B");
        if (s.contains("роз")) return QString("#D7A0A9");
        if (s.contains("желт")) return QString("#D6B04C");
        if (s.contains("фиолет") || s.contains("сирен")) return QString("#8A6FA8");
        return QString("#D6B04C");
      };

      auto colorDisplayName = [](QString raw) {
        const QString s = raw.trimmed().toLower();
        if (s.contains("черн")) return QString("чёрный");
        if (s.contains("бел")) return QString("белый");
        if (s.contains("сер")) return QString("серый");
        if (s.contains("крас")) return QString("красный");
        if (s.contains("син")) return QString("синий");
        if (s.contains("беж") || s.contains("пес")) return QString("бежевый / песочный");
        if (s.contains("молоч")) return QString("молочный");
        if (s.contains("корич") || s.contains("карам")) return QString("коричневый");
        if (s.contains("террак")) return QString("терракотовый");
        if (s.contains("олив") || s.contains("зел") || s.contains("хаки")) return QString("оливковый / хаки");
        if (s.contains("роз")) return QString("розовый");
        if (s.contains("желт")) return QString("жёлтый");
        if (s.contains("фиолет") || s.contains("сирен")) return QString("фиолетовый");
        return raw.trimmed().isEmpty() ? QString("акцентный") : raw.trimmed();
      };

      QVector<QPair<QString, QString>> paletteEntries;
      for (const ProjectItemView &item : items) {
        const QString hex = colorForName(item.color);
        bool exists = false;
        for (const auto &entry : paletteEntries) {
          if (entry.first == hex) {
            exists = true;
            break;
          }
        }
        if (!exists)
          paletteEntries.push_back(qMakePair(hex, colorDisplayName(item.color)));
      }

      QLabel *paletteHint = new QLabel(sidePanel);
      paletteHint->setObjectName("ProjectPaletteHint");
      paletteHint->setWordWrap(true);

      if (paletteEntries.isEmpty()) {
        paletteHint->setText("Палитра появится после добавления вещей в проект.");
        sideLay->addWidget(paletteHint);
      } else {
        paletteHint->setText("Палитра собрана из цветов добавленных вещей. Нажмите на цвет, чтобы отфильтровать проект.");
        sideLay->addWidget(paletteHint);

        for (int i = 0; i < qMin(6, paletteEntries.size()); ++i) {
          const QString hex = paletteEntries[i].first;
          const QString label = paletteEntries[i].second;

          QToolButton *chip = new QToolButton(sidePanel);
          chip->setObjectName("ProjectColorChip");
          chip->setFixedSize(42, 42);
          chip->setCursor(Qt::PointingHandCursor);
          chip->setToolTip(QString("Показать вещи цвета: %1").arg(label));
          chip->setStyleSheet(QString(
           "QToolButton#ProjectColorChip{background:%1;border:1px solid #CDBFB4;border-radius:6px;}"
           "QToolButton#ProjectColorChip:hover{border:2px solid #6F4B37;}"
          ).arg(hex));

          connect(chip, &QToolButton::clicked, this,
              [sidePanel, paletteHint, projectItemCards, colorForName, hex, label]() {
                const QString current = sidePanel->property("active_palette_hex").toString();
                const bool reset = (current == hex);
                sidePanel->setProperty("active_palette_hex", reset ? QString() : hex);

                for (QFrame *itemCard : projectItemCards) {
                  if (!itemCard)
                    continue;

                  const QString itemHex = colorForName(itemCard->property("project_item_color_raw").toString());
                  itemCard->setVisible(reset || itemHex == hex);
                }

                paletteHint->setText(reset
                  ? QString("Фильтр снят. Показаны все вещи проекта.")
                  : QString("Показаны вещи цвета: %1. Нажмите ещё раз, чтобы снять фильтр.").arg(label));
              });

          chips->addWidget(chip);
        }

        chips->addStretch();
        sideLay->addLayout(chips);
      }

      QPushButton *similarBtn = new QPushButton("Подобрать похожие вещи\nв умном каталоге ↗", sidePanel);
      similarBtn->setObjectName("ProjectSimilarBtn");
      similarBtn->setCursor(Qt::PointingHandCursor);
      sideLay->addWidget(similarBtn);

      QFrame *recommendBox = new QFrame(sidePanel);
      recommendBox->setObjectName("ProjectRecommendationBox");
      recommendBox->setMinimumHeight(132);
      QVBoxLayout *recLay = new QVBoxLayout(recommendBox);
      recLay->setContentsMargins(12, 10, 12, 12);
      recLay->setSpacing(6);

      QLabel *recTitle = new QLabel("Рекомендации", recommendBox);
      recTitle->setObjectName("ProjectSideTitle");
      QLabel *recText = new QLabel("Палитра помогает видеть цветовой баланс проекта. Кнопка ниже ищет в каталоге товары в тех же оттенках.", recommendBox);
      recText->setObjectName("ProjectRecommendationText");
      recText->setWordWrap(true);

      recLay->addWidget(recTitle);
      recLay->addWidget(recText);
      sideLay->addWidget(recommendBox);
      sideLay->addStretch();

      mainRow->addWidget(sidePanel);
      cardRoot->addLayout(mainRow);

      connect(addItemBtn, &QPushButton::clicked, this, openMiniProjectCatalog);

      m_projectsList->addItem(listItem);
      m_projectsList->setItemWidget(listItem, card);
      count++;

      connect(editBtn, &QPushButton::clicked, this,
          [card, editBtn, projectRemoveButtons]() {
            const bool editMode = !card->property("project_edit_mode").toBool();
            card->setProperty("project_edit_mode", editMode);

            editBtn->setText(editMode ? "Готово" : "Редактировать");

            for (QToolButton *btn : projectRemoveButtons) {
              if (!btn)
                continue;

              btn->setVisible(editMode);

              if (editMode) {
                const QPoint basePos = btn->pos();
                QPropertyAnimation *anim = new QPropertyAnimation(btn, "pos", btn);
                anim->setDuration(280);
                anim->setLoopCount(2);
                anim->setKeyValueAt(0.0, basePos);
                anim->setKeyValueAt(0.25, basePos + QPoint(-2, 0));
                anim->setKeyValueAt(0.50, basePos + QPoint(2, 0));
                anim->setKeyValueAt(0.75, basePos + QPoint(-1, 0));
                anim->setKeyValueAt(1.0, basePos);
                anim->start(QAbstractAnimation::DeleteWhenStopped);
              }
            }
          });

      connect(shareBtn, &QPushButton::clicked, this, [this, projectId, title]() {
        const QString text = QString("clothingcatalog://project/%1").arg(projectId);
        QApplication::clipboard()->setText(text);
        showLookbookInfo("Проект", QString("Ссылка на проект скопирована:\n%1").arg(text));
      });

      connect(deleteBtn, &QPushButton::clicked, this, [this, projectId]() {
        QDialog confirm(this);
        confirm.setObjectName("DeleteProjectConfirmDialog");
        confirm.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        confirm.setAttribute(Qt::WA_TranslucentBackground, true);
        confirm.setModal(true);
        confirm.resize(360, 180);
        confirm.setStyleSheet(
         "QDialog#DeleteProjectConfirmDialog{background:#00000000;}"
         "QFrame#DeleteProjectCard{background:#F4EFEA;border:1px solid #E1D6CE;border-radius:16px;}"
         "QLabel{background:transparent;border:none;color:#1F1A17;}"
         "QPushButton#ConfirmDanger{background:#8B3F32;color:white;border:none;border-radius:10px;min-height:36px;padding:0 14px;font-weight:800;}"
         "QPushButton#ConfirmCancel{background:#FFFFFF;color:#3A342F;border:1px solid #DDD2CA;border-radius:10px;min-height:36px;padding:0 14px;font-weight:800;}"
        );

        QVBoxLayout *outer = new QVBoxLayout(&confirm);
        outer->setContentsMargins(0, 0, 0, 0);
        QFrame *frame = new QFrame(&confirm);
        frame->setObjectName("DeleteProjectCard");
        outer->addWidget(frame);

        QVBoxLayout *lay = new QVBoxLayout(frame);
        lay->setContentsMargins(20, 18, 20, 18);
        QLabel *lbl = new QLabel("Удалить проект?", frame);
        lbl->setStyleSheet("font-size:18px;font-weight:900;");
        QLabel *sub = new QLabel("Проект и список вещей будут удалены.", frame);
        sub->setStyleSheet("font-size:12px;color:#7B7068;");
        lay->addWidget(lbl);
        lay->addWidget(sub);

        QHBoxLayout *buttons = new QHBoxLayout();
        buttons->addStretch();
        QPushButton *cancel = new QPushButton("Отмена", frame);
        cancel->setObjectName("ConfirmCancel");
        QPushButton *del = new QPushButton("Удалить", frame);
        del->setObjectName("ConfirmDanger");
        buttons->addWidget(cancel);
        buttons->addWidget(del);
        lay->addLayout(buttons);

        connect(cancel, &QPushButton::clicked, &confirm, &QDialog::reject);
        connect(del, &QPushButton::clicked, &confirm, &QDialog::accept);

        if (confirm.exec() == QDialog::Accepted) {
          QSqlQuery d(QSqlDatabase::database());
          d.prepare("DELETE FROM project_items WHERE project_id=?");
          d.addBindValue(projectId);
          d.exec();

          QSqlQuery p(QSqlDatabase::database());
          p.prepare("DELETE FROM projects WHERE id=? AND username=?");
          p.addBindValue(projectId);
          p.addBindValue(m_username);
          p.exec();

          refreshProjectsUI();
        }
      });

      connect(similarBtn, &QPushButton::clicked, this, [this, projectId, paletteEntries, colorForName]() {
        QSet<QString> paletteColors;
        QStringList paletteLabels;
        for (const auto &entry : paletteEntries) {
          paletteColors.insert(entry.first);
          if (!paletteLabels.contains(entry.second))
            paletteLabels << entry.second;
        }

        QDialog smartDlg(this);
        smartDlg.setObjectName("ProjectSmartCatalogDialog");
        smartDlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        smartDlg.setAttribute(Qt::WA_TranslucentBackground, true);
        smartDlg.setModal(true);
        smartDlg.resize(1040, 720);
        smartDlg.setStyleSheet(
         "QDialog#ProjectSmartCatalogDialog{background:#00000000;}"
         "QFrame#SmartCatalogCard{background:#F4EFEA;border:1px solid #E1D6CE;border-radius:18px;}"
         "QLabel{background:transparent;border:none;color:#1F1A17;}"
         "QLabel#SmartCatalogTitle{font-size:22px;font-weight:900;}"
         "QLabel#SmartCatalogHint{font-size:12px;color:#7B7068;}"
         "QLabel#SmartCatalogCounter{font-size:12px;color:#6B625A;}"
         "QPushButton#SmartCatalogX{background:transparent;border:none;color:#1F1A17;font-size:24px;font-weight:900;}"
         "QLineEdit,QComboBox{background:#FFFFFF;border:1px solid #D8CEC5;border-radius:10px;min-height:38px;padding:0 12px;color:#1F1A17;}"
         "QLineEdit:focus,QComboBox:focus{border:1px solid #B56D46;}"
         "QScrollArea#SmartCatalogScroll{background:transparent;border:none;}"
         "QWidget#SmartCatalogGrid{background:transparent;border:none;}"
         "QFrame#SmartCatalogProductCard{background:#FFFFFF;border:1px solid #E5DBD2;border-radius:14px;}"
         "QLabel#SmartCatalogImage{background:#F8F3EE;border:1px solid #EADFD6;border-radius:10px;color:#A29A92;}"
         "QLabel#SmartCatalogName{font-size:13px;font-weight:900;color:#1F1A17;}"
         "QLabel#SmartCatalogMeta{font-size:11px;color:#665D55;}"
         "QLabel#SmartCatalogPrice{font-size:13px;font-weight:900;color:#1F1A17;}"
         "QPushButton#SmartCatalogAdd{background:#6F4B37;color:white;border:none;border-radius:9px;min-height:32px;padding:0 10px;font-weight:800;}"
         "QPushButton#SmartCatalogAdd:hover{background:#5F3E2D;}"
         "QPushButton#SmartCatalogClose{background:#FFFFFF;color:#3A342F;border:1px solid #DDD2CA;border-radius:10px;min-height:38px;padding:0 18px;font-weight:800;}"
         "QFrame#SmartPaletteChip{border:1px solid #CDBFB4;border-radius:6px;}"
        );

        QVBoxLayout *outer = new QVBoxLayout(&smartDlg);
        outer->setContentsMargins(0, 0, 0, 0);

        QFrame *card = new QFrame(&smartDlg);
        card->setObjectName("SmartCatalogCard");
        outer->addWidget(card);

        QVBoxLayout *root = new QVBoxLayout(card);
        root->setContentsMargins(22, 18, 22, 18);
        root->setSpacing(12);

        QHBoxLayout *header = new QHBoxLayout();
        QVBoxLayout *titleBox = new QVBoxLayout();
        titleBox->setSpacing(3);

        QLabel *title = new QLabel("Умный каталог по палитре проекта", card);
        title->setObjectName("SmartCatalogTitle");

        QLabel *hint = new QLabel(card);
        hint->setObjectName("SmartCatalogHint");
        hint->setWordWrap(true);
        hint->setText(paletteColors.isEmpty()
          ? "В проекте пока нет палитры: добавьте вещи, и каталог начнёт подбирать товары по их цветам."
          : QString("Каталог ищет товары, которые совпадают с цветами проекта: %1. Фильтры работают вместе с палитрой, а не вместо неё.")
            .arg(paletteLabels.join(", ")));

        titleBox->addWidget(title);
        titleBox->addWidget(hint);
        header->addLayout(titleBox, 1);

        QPushButton *closeX = new RoundCloseButton(card);
        closeX->setObjectName("SmartCatalogX");
        closeX->setFixedSize(34, 34);
        closeX->setCursor(Qt::PointingHandCursor);
        header->addWidget(closeX, 0, Qt::AlignTop);
        root->addLayout(header);

        QHBoxLayout *paletteRow = new QHBoxLayout();
        paletteRow->setContentsMargins(0, 0, 0, 0);
        paletteRow->setSpacing(8);

        QLabel *paletteCaption = new QLabel("Палитра:", card);
        paletteCaption->setStyleSheet("font-size:12px;font-weight:900;color:#1F1A17;");
        paletteRow->addWidget(paletteCaption);

        if (paletteEntries.isEmpty()) {
          QLabel *emptyPalette = new QLabel("пока пусто", card);
          emptyPalette->setStyleSheet("font-size:12px;color:#7B7068;");
          paletteRow->addWidget(emptyPalette);
        } else {
          for (int i = 0; i < qMin(8, paletteEntries.size()); ++i) {
            QFrame *chip = new QFrame(card);
            chip->setObjectName("SmartPaletteChip");
            chip->setFixedSize(32, 32);
            chip->setToolTip(paletteEntries[i].second);
            chip->setStyleSheet(QString("QFrame#SmartPaletteChip{background:%1;border:1px solid #CDBFB4;border-radius:6px;}").arg(paletteEntries[i].first));
            paletteRow->addWidget(chip);
          }
        }
        paletteRow->addStretch();
        root->addLayout(paletteRow);

        QHBoxLayout *filters = new QHBoxLayout();
        filters->setSpacing(10);

        QLineEdit *searchEdit = new QLineEdit(card);
        searchEdit->setPlaceholderText("Поиск по названию, цвету или категории...");

        QComboBox *categoryBox = new QComboBox(card);
        categoryBox->addItem("Все товары");

        QSet<QString> categorySet;
        for (const Product &product : m_products) {
          if (!product.category.trimmed().isEmpty())
            categorySet.insert(product.category.trimmed());
        }
        QStringList categories = categorySet.values();
        std::sort(categories.begin(), categories.end());
        categoryBox->addItems(categories);

        QLabel *countLabel = new QLabel(card);
        countLabel->setObjectName("SmartCatalogCounter");

        filters->addWidget(searchEdit, 1);
        filters->addWidget(categoryBox);
        filters->addWidget(countLabel);
        root->addLayout(filters);

        QScrollArea *scroll = new QScrollArea(card);
        scroll->setObjectName("SmartCatalogScroll");
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        QWidget *gridWidget = new QWidget(scroll);
        gridWidget->setObjectName("SmartCatalogGrid");
        QGridLayout *grid = new QGridLayout(gridWidget);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(12);

        scroll->setWidget(gridWidget);
        root->addWidget(scroll, 1);

        bool changed = false;

        auto clearGrid = [&]() {
          while (QLayoutItem *child = grid->takeAt(0)) {
            if (QWidget *w = child->widget())
              w->deleteLater();
            delete child;
          }
        };

        auto productMatchesSmartFilters = [&](const Product &product) {
          const QString categoryFilter = categoryBox->currentText();
          const QString search = searchEdit->text().trimmed().toLower();

          const bool categoryOk =
            categoryFilter == "Все товары" ||
            product.category.compare(categoryFilter, Qt::CaseInsensitive) == 0;

          const bool paletteOk =
            paletteColors.isEmpty() ||
            paletteColors.contains(colorForName(product.color));

          const bool searchOk =
            search.isEmpty() ||
            product.name.toLower().contains(search) ||
            product.category.toLower().contains(search) ||
            product.color.toLower().contains(search) ||
            product.styleTag.toLower().contains(search) ||
            product.season.toLower().contains(search);

          return categoryOk && paletteOk && searchOk;
        };

        auto makeSmartProductCard = [&](const Product &product) -> QFrame* {
          QFrame *productCard = new QFrame(gridWidget);
          productCard->setObjectName("SmartCatalogProductCard");
          productCard->setFixedSize(214, 254);

          QVBoxLayout *lay = new QVBoxLayout(productCard);
          lay->setContentsMargins(10, 10, 10, 10);
          lay->setSpacing(6);

          QLabel *img = new QLabel(productCard);
          img->setObjectName("SmartCatalogImage");
          img->setFixedHeight(116);
          img->setAlignment(Qt::AlignCenter);

          QPixmap px = cachedScaledPixmap(product.imagePath, QSize(180, 106));
          if (!px.isNull())
            img->setPixmap(px);
          else
            img->setText("фото");

          QLabel *name = new QLabel(product.name, productCard);
          name->setObjectName("SmartCatalogName");
          name->setWordWrap(true);
          name->setMaximumHeight(40);

          QLabel *meta = new QLabel(QString("%1 • %2 • %3").arg(product.category, product.color, product.size), productCard);
          meta->setObjectName("SmartCatalogMeta");
          meta->setWordWrap(true);

          QLabel *price = new QLabel(QString("%1 ₽").arg(product.price, 0, 'f', 0), productCard);
          price->setObjectName("SmartCatalogPrice");

          QPushButton *addBtn = new QPushButton("Добавить в проект", productCard);
          addBtn->setObjectName("SmartCatalogAdd");
          addBtn->setCursor(Qt::PointingHandCursor);

          lay->addWidget(img);
          lay->addWidget(name);
          lay->addWidget(meta);
          lay->addWidget(price);
          lay->addStretch();
          lay->addWidget(addBtn);

          connect(addBtn, &QPushButton::clicked, &smartDlg, [&, productId = product.id, addBtn]() {
            QSqlQuery upd;
            upd.prepare("UPDATE project_items SET qty = qty + 1 WHERE project_id = ? AND product_id = ?");
            upd.addBindValue(projectId);
            upd.addBindValue(productId);
            upd.exec();

            if (upd.numRowsAffected() == 0) {
              QSqlQuery ins;
              ins.prepare("INSERT INTO project_items(project_id, product_id, qty) VALUES(?, ?, 1)");
              ins.addBindValue(projectId);
              ins.addBindValue(productId);
              ins.exec();
            }

            changed = true;
            addBtn->setText("Добавлено");
            addBtn->setEnabled(false);
          });

          return productCard;
        };

        auto rebuildSmartGrid = [&]() {
          clearGrid();

          QVector<Product> matched;
          for (const Product &product : m_products) {
            if (productMatchesSmartFilters(product))
              matched.push_back(product);
          }

          countLabel->setText(QString("Найдено: %1").arg(matched.size()));

          if (matched.isEmpty()) {
            QLabel *empty = new QLabel("Подходящих товаров не найдено. Попробуйте выбрать другую категорию или изменить поиск.", gridWidget);
            empty->setAlignment(Qt::AlignCenter);
            empty->setWordWrap(true);
            empty->setMinimumHeight(240);
            empty->setStyleSheet("background:#FFFFFF;border:1px dashed #D8CEC5;border-radius:12px;color:#8A8178;font-size:13px;padding:18px;");
            grid->addWidget(empty, 0, 0, 1, 4);
            return;
          }

          for (int i = 0; i < matched.size(); ++i) {
            grid->addWidget(makeSmartProductCard(matched[i]), i / 4, i % 4);
          }
        };

        connect(searchEdit, &QLineEdit::textChanged, &smartDlg, [&](const QString &) {
          rebuildSmartGrid();
        });
        connect(categoryBox, &QComboBox::currentTextChanged, &smartDlg, [&](const QString &) {
          rebuildSmartGrid();
        });
        connect(closeX, &QPushButton::clicked, &smartDlg, &QDialog::accept);

        QHBoxLayout *bottom = new QHBoxLayout();
        bottom->addStretch();

        QPushButton *closeBtn = new QPushButton("Закрыть", card);
        closeBtn->setObjectName("SmartCatalogClose");
        closeBtn->setCursor(Qt::PointingHandCursor);
        bottom->addWidget(closeBtn);
        root->addLayout(bottom);

        connect(closeBtn, &QPushButton::clicked, &smartDlg, &QDialog::accept);

        rebuildSmartGrid();

        smartDlg.exec();

        if (changed)
          refreshProjectsUI();
      });
    }

    const bool hasProjects = (count > 0);
    if (m_projectsEmptyPanel) m_projectsEmptyPanel->setVisible(!hasProjects);
    m_projectsList->setVisible(hasProjects);
    m_projectsList->setUpdatesEnabled(true);
  }


  int createProjectInDb(const QString &title)
  {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen())
      return -1;

    QSqlQuery q(db);
    q.prepare("INSERT INTO projects(username, title) VALUES(?, ?)");
    q.addBindValue(m_username);
    q.addBindValue(title);

    if (!q.exec())
      return -1;

    const int newProjectId = q.lastInsertId().toInt();

    return newProjectId;
  }


  void addProjectFlow()
  {
    QDialog dlg(this);
    dlg.setObjectName("CreateProjectDialog");
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setAttribute(Qt::WA_TranslucentBackground, true);
    dlg.setModal(true);
    dlg.resize(460, 280);
    dlg.setStyleSheet(
     "QDialog#CreateProjectDialog{background:#00000000;}"
     "QFrame#CreateProjectCard{background:#F4EFEA;border:1px solid #E1D6CE;border-radius:18px;}"
     "QLabel{background:transparent;border:none;color:#1F1A17;}"
     "QLabel#CreateProjectTitle{font-size:22px;font-weight:900;}"
     "QLabel#CreateProjectHint{font-size:12px;color:#7B7068;}"
     "QLabel#CreateProjectError{font-size:12px;color:#B64B53;}"
     "QLineEdit{background:#FFFFFF;border:1px solid #D8CEC5;border-radius:10px;min-height:38px;padding:0 12px;color:#1F1A17;}"
     "QLineEdit:focus{border:1px solid #B56D46;}"
     "QPushButton#CreateProjectPrimary{background:#6F4B37;color:white;border:none;border-radius:10px;min-height:40px;padding:0 16px;font-weight:900;}"
     "QPushButton#CreateProjectPrimary:hover{background:#5F3E2D;}"
     "QPushButton#CreateProjectSecondary{background:#FFFFFF;color:#3A342F;border:1px solid #DDD2CA;border-radius:10px;min-height:40px;padding:0 16px;font-weight:800;}"
     "QPushButton#CreateProjectSecondary:hover{background:#FBF7F3;}"
     "QPushButton#CreateProjectX{background:transparent;border:none;color:#1F1A17;font-size:22px;font-weight:900;}"
    );

    QVBoxLayout *outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(0, 0, 0, 0);

    QFrame *card = new QFrame(&dlg);
    card->setObjectName("CreateProjectCard");
    outer->addWidget(card);

    QVBoxLayout *root = new QVBoxLayout(card);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(12);

    QHBoxLayout *header = new QHBoxLayout();
    QLabel *title = new QLabel("Новый проект", card);
    title->setObjectName("CreateProjectTitle");
    header->addWidget(title);
    header->addStretch();

    QPushButton *xBtn = new RoundCloseButton(card);
    xBtn->setObjectName("CreateProjectX");
    xBtn->setFixedSize(32, 32);
    xBtn->setCursor(Qt::PointingHandCursor);
    header->addWidget(xBtn);
    root->addLayout(header);

    QLabel *hint = new QLabel("Назови проект так, чтобы сразу было понятно, для какого образа или события он нужен.", card);
    hint->setObjectName("CreateProjectHint");
    hint->setWordWrap(true);
    root->addWidget(hint);

    QLineEdit *titleEdit = new QLineEdit(card);
    titleEdit->setPlaceholderText("Например: Проект для офиса");
    root->addWidget(titleEdit);

    QLabel *errorLabel = new QLabel(card);
    errorLabel->setObjectName("CreateProjectError");
    errorLabel->setVisible(false);
    root->addWidget(errorLabel);

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();

    QPushButton *cancelBtn = new QPushButton("Отмена", card);
    cancelBtn->setObjectName("CreateProjectSecondary");
    cancelBtn->setCursor(Qt::PointingHandCursor);

    QPushButton *createBtn = new QPushButton("Создать проект", card);
    createBtn->setObjectName("CreateProjectPrimary");
    createBtn->setCursor(Qt::PointingHandCursor);

    buttons->addWidget(cancelBtn);
    buttons->addWidget(createBtn);
    root->addLayout(buttons);

    connect(xBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(createBtn, &QPushButton::clicked, &dlg, [&]() {
      const QString projectTitle = titleEdit->text().trimmed();
      if (projectTitle.isEmpty()) {
        errorLabel->setText("Введите название проекта.");
        errorLabel->setVisible(true);
        return;
      }

      const int newId = createProjectInDb(projectTitle);
      if (newId < 0) {
        errorLabel->setText("Не удалось создать проект. Проверьте базу данных.");
        errorLabel->setVisible(true);
        return;
      }

      dlg.accept();
    });

    titleEdit->setFocus();

    if (dlg.exec() == QDialog::Accepted)
      refreshProjectsUI();
  }

  void openCart()
  {
    CartDialog dlg(m_username, m_cart, this);
    dlg.exec();
  }

bool showRecommendResultsDialog(const RecommendCriteria &crit,
                  const QVector<Product> &strictList,
                  const QVector<Product> &softList)
  {
    QDialog dlg(this);
    dlg.setObjectName("RecommendResultsDialog");
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setAttribute(Qt::WA_TranslucentBackground, true);
    dlg.setModal(true);
    dlg.resize(960, 700);
    dlg.setMinimumSize(860, 620);

    dlg.setStyleSheet(
     "QDialog#RecommendResultsDialog{background:#00000000;}"
     "QFrame#RecoRoot{background:#F4EFEA;border:1px solid #E1D6CE;border-radius:22px;}"
     "QLabel{background:transparent;border:none;color:#1D1A17;}"
     "QLabel#RecoTitle{font-size:24px;font-weight:900;color:#15120F;}"
     "QLabel#RecoSubTitle{font-size:13px;color:#746A62;}"
     "QLabel#RecoBlockTitle{font-size:16px;font-weight:900;color:#171411;}"
     "QLabel#RecoStats{background:#FFF9F3;border:1px solid #E5D8CD;border-radius:12px;padding:10px 12px;font-size:13px;font-weight:800;color:#3A312B;}"
     "QPushButton#RecoX{background:transparent;border:none;color:#16120F;font-size:26px;font-weight:900;min-width:34px;max-width:34px;min-height:34px;max-height:34px;border-radius:17px;}"
     "QPushButton#RecoX:hover{background:#E8DED5;}"
     "QScrollArea#RecoScroll{background:transparent;border:none;}"
     "QWidget#RecoGridHost{background:transparent;border:none;}"
     "QFrame#RecoProductCard{background:#FFFFFF;border:1px solid #E2D8CF;border-radius:14px;}"
     "QFrame#RecoProductCard:hover{border:1px solid #C79672;background:#FFFDFB;}"
     "QLabel#RecoProductImage{background:#F8F3EE;border:1px solid #EADFD6;border-radius:10px;color:#A29A92;}"
     "QLabel#RecoProductName{font-size:13px;font-weight:900;color:#171411;}"
     "QLabel#RecoProductMeta{font-size:11px;color:#665D55;}"
     "QLabel#RecoProductPrice{font-size:14px;font-weight:900;color:#6F4B37;}"
     "QPushButton#RecoCartBtn{background:#F4EFEA;color:#6F4B37;border:1px solid #D8CBC0;border-radius:9px;min-height:30px;font-size:12px;font-weight:900;}"
     "QPushButton#RecoCartBtn:hover{background:#EEE4DC;border:1px solid #C79672;}"
     "QFrame#RecoHintBox{background:#FFF7F0;border:1px solid #E4D1C2;border-radius:14px;}"
     "QLabel#RecoHintTitle{font-size:14px;font-weight:900;color:#171411;}"
     "QLabel#RecoHintText{font-size:12px;color:#655B54;}"
     "QPushButton#RecoPrimary{background:#A96A50;color:white;border:none;border-radius:12px;min-height:44px;padding:0 22px;font-size:14px;font-weight:900;}"
     "QPushButton#RecoPrimary:hover{background:#965A42;}"
     "QPushButton#RecoSecondary{background:#FFFFFF;color:#3A332E;border:1px solid #D8CBC0;border-radius:12px;min-height:44px;padding:0 22px;font-size:14px;font-weight:800;}"
     "QPushButton#RecoSecondary:hover{background:#FBF7F3;}"
    );

    QVBoxLayout *outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(0, 0, 0, 0);

    QFrame *root = new QFrame(&dlg);
    root->setObjectName("RecoRoot");
    outer->addWidget(root);

    QVBoxLayout *main = new QVBoxLayout(root);
    main->setContentsMargins(22, 18, 22, 18);
    main->setSpacing(12);

    QHBoxLayout *header = new QHBoxLayout();
    QVBoxLayout *titleBox = new QVBoxLayout();
    titleBox->setSpacing(4);

    QLabel *title = new QLabel("Подобрано под ваши пожелания", root);
    title->setObjectName("RecoTitle");

    QString budgetText;
    if (crit.maxBudget > 0.0)
      budgetText = QString("%1–%2 ₽").arg(QString::number(crit.minBudget, 'f', 0), QString::number(crit.maxBudget, 'f', 0));
    else
      budgetText = "не указан";

    QLabel *subtitle = new QLabel(
      QString("Мы учли тип одежды, размер, вес, стиль, цвета и бюджет. Найдено: %1 товаров.")
        .arg(strictList.size()),
      root);
    subtitle->setObjectName("RecoSubTitle");
    subtitle->setWordWrap(true);

    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);
    header->addLayout(titleBox, 1);

    QPushButton *xBtn = new RoundCloseButton(root);
    xBtn->setObjectName("RecoX");
    xBtn->setCursor(Qt::PointingHandCursor);
    header->addWidget(xBtn, 0, Qt::AlignTop);
    main->addLayout(header);

    QHBoxLayout *topInfo = new QHBoxLayout();
    topInfo->setSpacing(10);

    QLabel *stats = new QLabel(
      QString("Тип: %1\nРазмер: %2 • Вес: %3 кг • Рост: %4 см\nЦвета: %5\nБюджет: %6")
        .arg(crit.category.isEmpty() ? "не выбран" : crit.category)
        .arg(crit.size.isEmpty() ? "не выбран" : crit.size)
        .arg(crit.weight > 0 ? QString::number(crit.weight) : QString("не указан"))
        .arg(crit.height > 0 ? QString::number(crit.height) : QString("не указан"))
        .arg(crit.colors.isEmpty() ? "не выбраны" : crit.colors)
        .arg(budgetText),
      root);
    stats->setObjectName("RecoStats");
    stats->setWordWrap(true);
    topInfo->addWidget(stats, 1);

    QFrame *hintBox = new QFrame(root);
    hintBox->setObjectName("RecoHintBox");
    QVBoxLayout *hintLay = new QVBoxLayout(hintBox);
    hintLay->setContentsMargins(14, 12, 14, 12);
    hintLay->setSpacing(5);

    QLabel *hintTitle = new QLabel("Как работает подбор", hintBox);
    hintTitle->setObjectName("RecoHintTitle");
    QLabel *hintText = new QLabel("Категория отбирается строго, а цвет, стиль, бюджет и вес повышают релевантность. Поэтому подбор не пропадает, если параметры слишком узкие.", hintBox);
    hintText->setObjectName("RecoHintText");
    hintText->setWordWrap(true);

    hintLay->addWidget(hintTitle);
    hintLay->addWidget(hintText);
    topInfo->addWidget(hintBox, 1);
    main->addLayout(topInfo);

    QLabel *blockTitle = new QLabel("Лучшие совпадения", root);
    blockTitle->setObjectName("RecoBlockTitle");
    main->addWidget(blockTitle);

    QScrollArea *scroll = new QScrollArea(root);
    scroll->setObjectName("RecoScroll");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *gridHost = new QWidget(scroll);
    gridHost->setObjectName("RecoGridHost");

    QGridLayout *grid = new QGridLayout(gridHost);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);

    auto makeProductCard = [&](const Product &p) -> QFrame* {
      QFrame *card = new QFrame(gridHost);
      card->setObjectName("RecoProductCard");
      card->setFixedSize(210, 276);

      QVBoxLayout *lay = new QVBoxLayout(card);
      lay->setContentsMargins(10, 10, 10, 10);
      lay->setSpacing(6);

      QLabel *image = new QLabel(card);
      image->setObjectName("RecoProductImage");
      image->setAlignment(Qt::AlignCenter);
      image->setFixedHeight(112);

      QPixmap px = cachedScaledPixmap(p.imagePath, QSize(178, 104));
      if (!px.isNull())
        image->setPixmap(px);
      else
        image->setText("фото");

      QLabel *name = new QLabel(p.name, card);
      name->setObjectName("RecoProductName");
      name->setWordWrap(true);
      name->setMaximumHeight(42);

      QLabel *meta = new QLabel(QString("%1 • %2 • %3").arg(p.category, p.color, p.size), card);
      meta->setObjectName("RecoProductMeta");
      meta->setWordWrap(true);

      QLabel *price = new QLabel(QString("%1 ₽").arg(p.price, 0, 'f', 0), card);
      price->setObjectName("RecoProductPrice");

      QPushButton *cartBtn = new QPushButton("В корзину", card);
      cartBtn->setObjectName("RecoCartBtn");
      cartBtn->setCursor(Qt::PointingHandCursor);
      connect(cartBtn, &QPushButton::clicked, this, [this, p, cartBtn]() {
        m_cart.addProduct(p, 1);
        cartBtn->setText("Добавлено");
        cartBtn->setEnabled(false);
        QTimer::singleShot(900, cartBtn, [cartBtn]() {
          cartBtn->setText("В корзину");
          cartBtn->setEnabled(true);
        });
      });

      lay->addWidget(image);
      lay->addWidget(name);
      lay->addWidget(meta);
      lay->addStretch();
      lay->addWidget(price);
      lay->addWidget(cartBtn);

      return card;
    };

    const int showLimit = qMin(12, strictList.size());
    for (int i = 0; i < showLimit; ++i) {
      grid->addWidget(makeProductCard(strictList[i]), i / 4, i % 4);
    }

    if (showLimit == 0) {
      QLabel *empty = new QLabel("Подходящие товары не найдены. Попробуйте расширить бюджет или снять часть фильтров.", gridHost);
      empty->setAlignment(Qt::AlignCenter);
      empty->setMinimumHeight(220);
      empty->setWordWrap(true);
      empty->setStyleSheet("background:#FFFFFF;border:1px dashed #D8CEC5;border-radius:12px;color:#8A8178;font-size:13px;padding:18px;");
      grid->addWidget(empty, 0, 0, 1, 4);
    }

    scroll->setWidget(gridHost);
    main->addWidget(scroll, 1);

    if (!softList.isEmpty()) {
      QLabel *soft = new QLabel(QString("В блоке «Вам может понравиться» появятся похожие варианты: %1. Основной каталог откроется только с точными совпадениями.").arg(softList.size()), root);
      soft->setObjectName("RecoSubTitle");
      soft->setWordWrap(true);
      main->addWidget(soft);
    }

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();

    QPushButton *openBtn = new QPushButton("Открыть в каталоге", root);
    openBtn->setObjectName("RecoPrimary");
    openBtn->setCursor(Qt::PointingHandCursor);

    buttons->addWidget(openBtn);
    main->addLayout(buttons);

    connect(xBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(openBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    return dlg.exec() == QDialog::Accepted;
  }

  void onRecommendClicked()
  {
    QuickRecommendDialog dlg(m_products, this);
    UserProfile profile;
    QString profileError;
    if (loadUserProfile(m_username, profile, profileError))
      dlg.applyProfile(profile);

    if (dlg.exec() != QDialog::Accepted)
      return;

    RecommendCriteria crit = dlg.criteria();

    QVector<Product> strictList = buildStrictCatalog(crit);

    if (strictList.isEmpty()) {
      QMessageBox::information(this, "Подбор",
                  "Пока не нашлось товаров даже после мягкого подбора.\n"
                  "Попробуй выбрать меньше типов одежды или расширить бюджет.");
      return;
    }

    QVector<Product> softList = buildSoftReco(crit, strictList, 8);

    // Show the result dialog first; switch to the catalog only when the user presses the Open in catalog button.
    const bool openCatalog = showRecommendResultsDialog(crit, strictList, softList);
    if (!openCatalog)
      return;

    m_softRecommendations.clear();
    m_smartCatalogActive = true;
    m_catalogBase = strictList;
    QSet<int> usedProductIds;
    for (const Product &p : m_catalogBase)
      usedProductIds.insert(p.id);
    for (const Product &p : softList) {
      if (!usedProductIds.contains(p.id)) {
        m_softRecommendations.push_back(p);
        usedProductIds.insert(p.id);
      }
    }

    openCatalogPage(false);
  }


  void saveProfileToDb(const RecommendCriteria &c)
  {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen())
      return;

    QString style = c.style.trimmed();
    if (style.isEmpty() || style == "Не важно")
      style = "Не выбран";

    QString budget;
    if (c.maxBudget > 0.0) {
      budget = QString("%1-%2 ₽")
             .arg(QString::number(c.minBudget, 'f', 0),
               QString::number(c.maxBudget, 'f', 0));
    } else {
      budget = "Не указан";
    }

    QSqlQuery q(db);
    q.prepare(
     "INSERT INTO user_profiles(username, gender, height, style, budget, favorite_colors) "
     "VALUES(:u, :g, :h, :s, :b, :c) "
     "ON CONFLICT(username) DO UPDATE SET "
     "gender=:g, style=:s, budget=:b, favorite_colors=:c;"
    );

    q.bindValue(":u", m_username);
    q.bindValue(":g", c.gender);
    q.bindValue(":h", c.height);
    q.bindValue(":s", style);
    q.bindValue(":b", budget);
    q.bindValue(":c", c.colors.trimmed());

    if (!q.exec()) {
      qDebug() << "saveProfileToDb error:" << q.lastError().text();
    }
  }
  bool productMatchesStyle(const Product &p, const QString &style) const
  {
    if (style.isEmpty() || style == "Не выбран" || style == "Не важно")
      return true;

    const QString s = normRu(style);
    const QString hay = normRu(p.name + " " + p.category + " " + p.styleTag + " " + p.season + " " + p.color + " " + p.material);

    if (s.contains("casual") || s.contains("кэжуал") || s.contains("повсед"))
      return hay.contains("casual") || hay.contains("кэж") || hay.contains("повсед") ||
          hay.contains("футбол") || hay.contains("худи") || hay.contains("джинс");

    if (s.contains("minimal") || s.contains("минимал"))
      return hay.contains("minimal") || hay.contains("миним") || hay.contains("basic") ||
          hay.contains("баз") || hay.contains("беж") || hay.contains("бел") || hay.contains("чер");

    if (s.contains("basic") || s.contains("баз"))
      return hay.contains("basic") || hay.contains("баз") || hay.contains("футбол") ||
          hay.contains("рубаш") || hay.contains("джинс") || hay.contains("бел") || hay.contains("чер");

    if (s.contains("sport") || s.contains("спорт"))
      return hay.contains("sport") || hay.contains("спорт") || hay.contains("кед") ||
          hay.contains("крос") || hay.contains("худи") || hay.contains("свитшот");

    if (s.contains("business") || s.contains("делов") || s.contains("office") || s.contains("офис") || s.contains("работ"))
      return hay.contains("business") || hay.contains("делов") || hay.contains("office") ||
          hay.contains("офис") || hay.contains("рубаш") || hay.contains("брюк") ||
          hay.contains("пальто") || hay.contains("тренч");

    if (s.contains("street") || s.contains("стрит"))
      return hay.contains("street") || hay.contains("стрит") || hay.contains("худи") ||
          hay.contains("овер") || hay.contains("футбол") || hay.contains("кед");

    if (s.contains("smart"))
      return hay.contains("smart") || hay.contains("casual") || hay.contains("рубаш") ||
          hay.contains("брюк") || hay.contains("тренч") || hay.contains("пальто");

    if (s.contains("oversize") || s.contains("овер"))
      return hay.contains("oversize") || hay.contains("овер") || hay.contains("худи") ||
          hay.contains("рубаш") || hay.contains("свитшот");

    if (s.contains("гранж") || s.contains("grunge"))
      return hay.contains("grunge") || hay.contains("гранж") || hay.contains("чер") ||
          hay.contains("графит") || hay.contains("худи") || hay.contains("овер");

    if (s.contains("винтаж") || s.contains("vintage") || s.contains("ретро"))
      return hay.contains("vintage") || hay.contains("винтаж") || hay.contains("ретро") ||
          hay.contains("террак") || hay.contains("карамел") || hay.contains("олив") || hay.contains("70");

    if (s.contains("classic") || s.contains("класс"))
      return hay.contains("classic") || hay.contains("класс") || hay.contains("рубаш") ||
          hay.contains("брюк") || hay.contains("пальто") || hay.contains("бел") || hay.contains("чер");

    if (s.contains("romantic") || s.contains("роман"))
      return hay.contains("romantic") || hay.contains("роман") || hay.contains("плать") ||
          hay.contains("юб") || hay.contains("топ") || hay.contains("персик") || hay.contains("молоч");

    if (s.contains("boho") || s.contains("бохо"))
      return hay.contains("boho") || hay.contains("бохо") || hay.contains("лен") ||
          hay.contains("песоч") || hay.contains("олив") || hay.contains("рубаш");

    if (s.contains("лисо"))
      return hay.contains("casual") || hay.contains("basic") || hay.contains("футбол") ||
          hay.contains("рубаш") || hay.contains("брюк");

    return hay.contains(s);
  }

  bool productMatchesBudget(const Product &p, const QString &budget) const
  {
    if (budget.isEmpty() || budget == "Не указан")
      return true;

    double price = p.price;
    if (budget == "Низкий")
      return price <= 2500.0;
    if (budget == "Средний")
      return price > 2500.0 && price <= 4500.0;
    if (budget == "Высокий")
      return price > 4500.0;

    return true;
  }

  int colorMatchScore(const Product &p, const QStringList &colors) const
  {
    if (colors.isEmpty())
      return 0;

    QString pc = p.color.toLower();
    int score = 0;
    for (const QString &raw : colors) {
      QString c = raw.trimmed().toLower();
      if (c.isEmpty())
        continue;
      if (pc.contains(c))
        ++score;
    }
    return score;
  }

  QVector<Product> buildRecommendations(const RecommendCriteria &crit)
  {
    QVector<Product> result;

    QString style = crit.style;
    if (style == "Не важно")
      style.clear();

    QString category = crit.category;
    if (category == "Не важно")
      category.clear();

    QStringList favColors;
    if (!crit.colors.isEmpty()) {
      QString tmp = crit.colors.toLower();
      tmp.replace(";", ",");
      favColors = tmp.split(",", Qt::SkipEmptyParts);
      for (QString &c : favColors)
        c = c.trimmed();
    }

    double maxPrice = (crit.maxBudget > 0.0) ? crit.maxBudget : 1e12;

    bool noStyle = style.isEmpty();
    bool noColors = favColors.isEmpty();
    bool noBudgetLimit = (crit.maxBudget <= 0.0);
    bool noCategory = category.isEmpty();

    // No filters are selected.
    if (noStyle && noColors && noBudgetLimit && noCategory) {
      for (int i = 0; i < m_products.size() && i < 6; ++i)
        result.push_back(m_products[i]);
      return result;
    }

    // Only the category is selected.
    if (noStyle && noColors && noBudgetLimit && !noCategory) {
      for (const Product &p : m_products) {
        if (p.category != category) continue;
        result.push_back(p);
        if (result.size() >= 6) break;
      }
      return result;
    }

    QVector<QPair<int, Product>> scored;

    for (const Product &p : m_products) {

      if (!category.isEmpty() && p.category != category)
        continue;

      if (p.price > maxPrice)
        continue;

      bool styleOk = productMatchesStyle(p, style);

      int score = 0;
      if (styleOk && !style.isEmpty())
        score += 2;

      score += colorMatchScore(p, favColors);

      if (score > 0)
        scored.append(qMakePair(score, p));
    }

    // Fallback recommendation by budget and selected category.
    if (scored.isEmpty()) {
      QVector<Product> budgetList;
      for (const Product &p : m_products) {

        if (!category.isEmpty() && p.category != category)
          continue;

        if (p.price <= maxPrice)
          budgetList.push_back(p);
      }

      std::sort(budgetList.begin(), budgetList.end(),
           [](const Product &a, const Product &b) {
             return a.price < b.price;
           });

      int maxCount = qMin(6, budgetList.size());
      for (int i = 0; i < maxCount; ++i)
        result.push_back(budgetList[i]);
      return result;
    }

    std::sort(scored.begin(), scored.end(),
         [](const QPair<int, Product> &a,
           const QPair<int, Product> &b)
         {
           return a.first > b.first;
         });

    int maxCount = qMin(6, scored.size());
    for (int i = 0; i < maxCount; ++i)
      result.push_back(scored[i].second);

    return result;
  }


  QVector<Product> buildRecommendations()
  {
    QVector<Product> result;

    UserProfile profile;
    QString err;
    if (!loadUserProfile(m_username, profile, err)) {
      QMessageBox::warning(this, "Рекомендации", err);
      return result;
    }

    QString style = profile.style;
    QString budget = profile.budget;
    QStringList favColors = profile.favoriteColors
                  .split(',', Qt::SkipEmptyParts);

    if ((style.isEmpty() || style == "Не выбран") &&
      (budget.isEmpty() || budget == "Не указан") &&
      favColors.isEmpty()) {

      for (int i = 0; i < m_products.size() && i < 6; ++i)
        result.push_back(m_products[i]);
      return result;
    }

    QVector<QPair<int, Product>> scored;

    for (const Product &p : m_products) {
      bool styleOk = productMatchesStyle(p, style);
      bool budgetOk = productMatchesBudget(p, budget);

      if (!budgetOk && !budget.isEmpty() && budget != "Не указан")
        continue;
      if (!styleOk && !style.isEmpty() && style != "Не выбран")
        continue;

      int score = 0;
      if (styleOk) score += 2;
      if (budgetOk) score += 2;
      score += colorMatchScore(p, favColors);

      if (score > 0)
        scored.append(qMakePair(score, p));
    }

    if (scored.isEmpty())
      return result;

    std::sort(scored.begin(), scored.end(),
         [](const QPair<int, Product> &a,
           const QPair<int, Product> &b)
         {
           return a.first > b.first;
         });

    int maxCount = qMin(6, scored.size());
    for (int i = 0; i < maxCount; ++i)
      result.push_back(scored[i].second);

    return result;
  }

  bool loadUserProfile(const QString &username, UserProfile &profile, QString &error) const
  {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
      error = "База данных недоступна.";
      return false;
    }

    QSqlQuery q(db);
    q.prepare(
     "SELECT gender, height, style, budget, favorite_colors "
     "FROM user_profiles WHERE username=:u;"
      );
    q.bindValue(":u", username);
    if (!q.exec()) {
      error = "Ошибка чтения профиля: " + q.lastError().text();
      return false;
    }

    if (!q.next()) {
      error = "Профиль не заполнен.";
      return false;
    }

    profile.username   = username;
    profile.gender    = q.value(0).toString();
    profile.height    = q.value(1).toInt();
    profile.style     = q.value(2).toString();
    profile.budget    = q.value(3).toString();
    profile.favoriteColors= q.value(4).toString();

    return true;
  }
};

// Applies catalog filters and rebuilds the visible product grid.
inline void CatalogWindow::applyFilter()
{
  QString season = m_seasonBox ? m_seasonBox->currentText() : "Любой";

  if (!m_categoryBox || !m_sizeBox || !m_colorBox) {
    rebuildGridChunked(m_smartCatalogActive ? m_catalogBase : m_products);
    return;
  }

  QString category = m_categoryBox->currentText();
  QString size   = m_sizeBox->currentText();
  QString color  = m_colorBox->currentText();
  QString style  = m_styleFilterBox ? m_styleFilterBox->currentText() : "Любой";
  int minPrice   = m_priceMinSpin ? m_priceMinSpin->value() : 0;
  int maxPrice   = m_priceMaxSpin ? m_priceMaxSpin->value() : 10000;

  const QVector<Product> &base = m_smartCatalogActive ? m_catalogBase : m_products;

  auto categoryFilterMatches = [](const Product &p, const QString &selected) {
    if (selected == "Все категории")
      return true;

    const QString wanted = normRu(selected);
    const QString cat = normRu(p.category);
    const QString name = normRu(p.name);
    const QString hay = cat + " " + name;

    if (wanted.contains("крос"))
      return hay.contains("крос") || hay.contains("sneaker");
    if (wanted.contains("кед"))
      return hay.contains("кед");
    if (wanted.contains("обув"))
      return hay.contains("обув") || hay.contains("крос") || hay.contains("кед") ||
          hay.contains("ботин") || hay.contains("сникер");

    return p.category.compare(selected, Qt::CaseInsensitive) == 0;
  };

  QVector<Product> list;
  list.reserve(base.size());

  for (const Product &p : base) {
    if (!categoryFilterMatches(p, category))
      continue;
    if (size   != "Любой" && p.size != size)
      continue;
    if (color  != "Любой" && p.color != color)
      continue;
    if (season  != "Любой" && p.season != season)
      continue;
    if (style  != "Любой") {
      const QString styleTag = p.styleTag.trimmed().toLower();
      const QString styleNeed = style.trimmed().toLower();
      if (!styleTag.contains(styleNeed))
        continue;
    }
    if (p.price < minPrice)
      continue;
    if (p.price > maxPrice)
      continue;

    list.push_back(p);
  }

  rebuildGridChunked(list);
}
