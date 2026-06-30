#pragma once

/*
  File: src/dialogs/UserProfileDialog.h
  Purpose: User profile editor used by the recommendation logic.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "../widgets/RoundCloseButton.h"
#include "../api/ApiClient.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDateEdit>
#include <QDebug>
#include <QDialog>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QButtonGroup>

class UserProfileDialog : public QDialog
{
public:
  UserProfileDialog(const QString &username, QWidget *parent = nullptr)
    : QDialog(parent)
    , m_username(username)
  {
    setWindowTitle("Профиль пользователя");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setModal(true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    resize(700, 820);
    setMinimumSize(660, 700);

    setStyleSheet(
     "QDialog{background:transparent;}"
     "QFrame#OuterShade{background:#00000012;border:none;border-radius:24px;}"
     "QFrame#RootCard{background:#F8F4EE;border:1px solid #E2D6CA;border-radius:24px;}"
     "QLabel{background:transparent;border:none;color:#1A1714;}"
     "QLabel#HeaderTitle{font-size:15px;font-weight:900;color:#181512;}"
     "QPushButton#CloseBtn{background:transparent;border:none;color:#1A1714;font-size:24px;font-weight:900;min-width:34px;max-width:34px;min-height:34px;max-height:34px;border-radius:17px;}"
     "QPushButton#CloseBtn:hover{background:#EDE4DB;}"
     "QFrame#HeroPanel{background:#F3EDE5;border:none;border-radius:20px;}"
     "QLabel#AvatarLabel{background:#E2A07C;color:#FFF7F1;border:none;border-radius:32px;font-size:24px;font-weight:300;}"
     "QLabel#UsernameLabel{font-size:14px;font-weight:900;color:#111111;}"
     "QLabel#UserTypeLabel{font-size:11px;color:#2D2926;}"
     "QPushButton#AvatarBtn{background:#E2A07C;color:#FFF8F2;border:none;border-radius:9px;min-height:34px;max-height:34px;padding:0 14px;font-size:11px;font-weight:800;}"
     "QPushButton#AvatarBtn:hover{background:#D38E6A;}"
     "QPushButton#AvatarLink{background:transparent;border:none;color:#B18A72;text-decoration:underline;font-size:12px;font-weight:700;padding:0 2px;}"
     "QPushButton#AvatarLink:hover{color:#936F5B;}"
     "QFrame#SectionCard{background:#FBF8F4;border:1px solid #B6A596;border-radius:18px;}"
     "QLabel#SectionTitle{font-size:13px;font-weight:900;color:#171411;}"
     "QLabel#SmallSectionTitle{font-size:11px;font-weight:900;color:#171411;}"
     "QLabel#FieldLabel{font-size:10px;font-weight:800;color:#1C1916;}"
     "QLineEdit#FieldEdit,QComboBox#FieldCombo,QDateEdit#FieldDateEdit,QSpinBox#FieldSpin{background:#FFFFFF;border:1px solid #B7A79A;border-radius:9px;min-height:26px;max-height:26px;padding:0 10px;color:#23201D;font-size:10px;}"
     "QLineEdit#FieldEdit:focus,QComboBox#FieldCombo:focus,QDateEdit#FieldDateEdit:focus,QSpinBox#FieldSpin:focus{border:1px solid #D3906E;}"
     "QComboBox#FieldCombo::drop-down{border:none;width:26px;}"
     "QComboBox#FieldCombo::down-arrow{image:none;width:10px;height:10px;border:none;}"
     "QFrame#PhoneFrame{background:#FFFFFF;border:2px solid #4AA357;border-radius:11px;}"
     "QLineEdit#PhoneEdit{background:transparent;border:none;min-height:32px;max-height:32px;padding:0 12px;color:#23201D;font-size:11px;}"
     "QLabel#PhoneVerified{color:#2D9344;font-size:11px;font-weight:800;padding-right:12px;}"
     "QPushButton#ChoiceBtn{background:#FFFFFF;border:1px solid #B7A79A;border-radius:9px;min-height:28px;max-height:28px;padding:0 10px;color:#23201D;font-size:10px;font-weight:700;text-align:left;}"
     "QPushButton#ChoiceBtn:checked{background:#F2E7DD;border:2px solid #8B6A58;color:#1B1714;}"
     "QPushButton#ChoiceBtn:hover{background:#F8F2EC;}"
     "QPushButton#ColorPill{border-radius:12px;min-height:24px;max-height:24px;padding:0 10px;font-size:10px;font-weight:700;}"
     "QToolButton#PaletteChip{background:#FFFFFF;border:1px solid #D0C4B8;border-radius:14px;min-width:28px;max-width:28px;min-height:28px;max-height:28px;}"
     "QToolButton#PaletteChip:checked{border:2px solid #6D4F3A;}"
     "QPushButton#SecurityBtn{background:#FFFFFF;border:1px solid #B7A79A;border-radius:9px;min-height:30px;max-height:30px;padding:0 14px;color:#1D1916;font-size:10px;font-weight:800;}"
     "QPushButton#SecurityBtn:hover{background:#F8F2EC;}"
     "QCheckBox#TwoFactor{spacing:10px;font-size:11px;font-weight:700;color:#2C2825;}"
     "QCheckBox#TwoFactor::indicator{width:42px;height:24px;}"
     "QCheckBox#TwoFactor::indicator:unchecked{border:none;border-radius:12px;background:#CFC8C1;}"
     "QCheckBox#TwoFactor::indicator:checked{border:none;border-radius:12px;background:#CFA27D;}"
     "QPushButton#SaveBtn{background:#E2A07C;border:none;border-radius:10px;min-height:36px;max-height:36px;padding:0 16px;color:#FFF8F2;font-size:11px;font-weight:800;}"
     "QPushButton#SaveBtn:hover{background:#D38E6A;}"
     "QPushButton#CancelBtn{background:#B5ADA4;border:none;border-radius:10px;min-height:36px;max-height:36px;padding:0 16px;color:#FFF8F2;font-size:11px;font-weight:800;}"
     "QPushButton#CancelBtn:hover{background:#A29A92;}"
     "QLabel#StatusOk{font-size:11px;color:#2E8B47;font-weight:800;}"
     "QLabel#StatusError{font-size:11px;color:#BA3B33;font-weight:800;}"
    );

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(10, 10, 10, 10);

    QFrame *outerShade = new QFrame(this);
    outerShade->setObjectName("OuterShade");
    outer->addWidget(outerShade);

    QVBoxLayout *shadeLay = new QVBoxLayout(outerShade);
    shadeLay->setContentsMargins(6, 6, 6, 6);

    QFrame *rootCard = new QFrame(outerShade);
    rootCard->setObjectName("RootCard");
    shadeLay->addWidget(rootCard);

    QVBoxLayout *root = new QVBoxLayout(rootCard);
    root->setContentsMargins(14, 10, 14, 12);
    root->setSpacing(7);

    QHBoxLayout *header = new QHBoxLayout();
    QLabel *headerTitle = new QLabel("Профиль пользователя", rootCard);
    headerTitle->setObjectName("HeaderTitle");
    header->addWidget(headerTitle);
    header->addStretch();

    QPushButton *closeBtn = new RoundCloseButton(rootCard);
    closeBtn->setObjectName("CloseBtn");
    closeBtn->setCursor(Qt::PointingHandCursor);
    header->addWidget(closeBtn);
    root->addLayout(header);

    QFrame *hero = new QFrame(rootCard);
    hero->setObjectName("HeroPanel");
    QHBoxLayout *heroLay = new QHBoxLayout(hero);
    heroLay->setContentsMargins(10, 8, 10, 8);
    heroLay->setSpacing(10);

    m_avatarLabel = new QLabel(hero);
    m_avatarLabel->setObjectName("AvatarLabel");
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setFixedSize(58, 58);
    heroLay->addWidget(m_avatarLabel, 0, Qt::AlignTop);

    QVBoxLayout *heroInfo = new QVBoxLayout();
    heroInfo->setSpacing(6);

    m_usernameLabel = new QLabel(m_username, hero);
    m_usernameLabel->setObjectName("UsernameLabel");
    heroInfo->addWidget(m_usernameLabel);

    m_userTypeLabel = new QLabel(hero);
    m_userTypeLabel->setObjectName("UserTypeLabel");
    heroInfo->addWidget(m_userTypeLabel);

    QHBoxLayout *avatarBtns = new QHBoxLayout();
    avatarBtns->setSpacing(8);

    QPushButton *changeAvatarBtn = new QPushButton("Сменить аватар", hero);
    changeAvatarBtn->setObjectName("AvatarBtn");
    changeAvatarBtn->setCursor(Qt::PointingHandCursor);
    changeAvatarBtn->setFixedWidth(145);

    QPushButton *deleteAvatarBtn = new QPushButton("Удалить фото", hero);
    deleteAvatarBtn->setObjectName("AvatarLink");
    deleteAvatarBtn->setCursor(Qt::PointingHandCursor);

    avatarBtns->addWidget(changeAvatarBtn, 0, Qt::AlignLeft);
    avatarBtns->addWidget(deleteAvatarBtn, 0, Qt::AlignLeft);
    avatarBtns->addStretch();
    heroInfo->addLayout(avatarBtns);

    heroInfo->addStretch();
    heroLay->addLayout(heroInfo, 1);
    root->addWidget(hero);

    QFrame *contactsCard = new QFrame(rootCard);
    contactsCard->setObjectName("SectionCard");
    QVBoxLayout *contactsLay = new QVBoxLayout(contactsCard);
    contactsLay->setContentsMargins(12, 10, 12, 10);
    contactsLay->setSpacing(6);

    QLabel *contactsTitle = new QLabel("Контактная информация", contactsCard);
    contactsTitle->setObjectName("SectionTitle");
    contactsLay->addWidget(contactsTitle);

    QGridLayout *contactsGrid = new QGridLayout();
    contactsGrid->setHorizontalSpacing(8);
    contactsGrid->setVerticalSpacing(5);

    m_accFirst = new QLineEdit(contactsCard);
    m_accLast = new QLineEdit(contactsCard);
    m_accEmail = new QLineEdit(contactsCard);
    m_accPhone = new QLineEdit(contactsCard);

    for (QLineEdit *e : {m_accFirst, m_accLast, m_accEmail}) {
      e->setObjectName("FieldEdit");
    }
    m_accPhone->setObjectName("PhoneEdit");

    contactsGrid->addWidget(makeFieldLabel("Имя:", contactsCard), 0, 0);
    contactsGrid->addWidget(m_accFirst, 0, 1);
    contactsGrid->addWidget(makeFieldLabel("Фамилия:", contactsCard), 1, 0);
    contactsGrid->addWidget(m_accLast, 1, 1);
    contactsGrid->addWidget(makeFieldLabel("Почта:", contactsCard), 2, 0);
    contactsGrid->addWidget(m_accEmail, 2, 1);
    contactsGrid->addWidget(makeFieldLabel("Телефон:", contactsCard), 3, 0);

    QFrame *phoneFrame = new QFrame(contactsCard);
    phoneFrame->setObjectName("PhoneFrame");
    QHBoxLayout *phoneLay = new QHBoxLayout(phoneFrame);
    phoneLay->setContentsMargins(0, 0, 0, 0);
    phoneLay->setSpacing(0);
    phoneLay->addWidget(m_accPhone, 1);

    m_phoneVerifiedLabel = new QLabel("Номер подтверждён", phoneFrame);
    m_phoneVerifiedLabel->setObjectName("PhoneVerified");
    phoneLay->addWidget(m_phoneVerifiedLabel, 0, Qt::AlignVCenter);

    contactsGrid->addWidget(phoneFrame, 3, 1);
    contactsGrid->setColumnStretch(1, 1);
    contactsLay->addLayout(contactsGrid);
    root->addWidget(contactsCard);

    QFrame *prefCard = new QFrame(rootCard);
    prefCard->setObjectName("SectionCard");
    QVBoxLayout *prefLay = new QVBoxLayout(prefCard);
    prefLay->setContentsMargins(12, 10, 12, 10);
    prefLay->setSpacing(6);

    QLabel *prefTitle = new QLabel("Личные предпочтения и параметры", prefCard);
    prefTitle->setObjectName("SmallSectionTitle");
    prefLay->addWidget(prefTitle);

    QGridLayout *prefGrid = new QGridLayout();
    prefGrid->setHorizontalSpacing(8);
    prefGrid->setVerticalSpacing(6);

    m_gender = new QComboBox(prefCard);
    m_gender->setObjectName("FieldCombo");
    m_gender->addItems({"Не указано", "Мужской", "Женский"});

    m_height = new QSpinBox(prefCard);
    m_height->setObjectName("FieldSpin");
    m_height->setRange(120, 230);
    m_height->setButtonSymbols(QAbstractSpinBox::NoButtons);

    prefGrid->addWidget(makeFieldLabel("Пол:", prefCard), 0, 0);
    prefGrid->addWidget(m_gender, 0, 1);
    prefGrid->addWidget(makeFieldLabel("Рост:", prefCard), 0, 2);
    prefGrid->addWidget(m_height, 0, 3);

    prefGrid->addWidget(makeFieldLabel("Стиль:", prefCard), 1, 0);
    m_style = new QComboBox(prefCard);
    m_style->setObjectName("FieldCombo");
    m_style->addItems({"Кэжуал", "Минималистичный", "Базовый", "Спортивный", "Деловой", "Streetwear", "Smart Casual", "Oversize", "Гранж", "Винтаж", "Классика", "Романтический", "Бохо", "Офисный"});
    prefGrid->addWidget(m_style, 1, 1, 1, 3);

    prefGrid->addWidget(makeFieldLabel("Бюджет:", prefCard), 2, 0);
    m_budget = new QComboBox(prefCard);
    m_budget->setObjectName("FieldCombo");
    m_budget->addItems({"До 5000 ₽", "5000-15000 ₽", "15000-30000 ₽", "30000 ₽ +"});
    prefGrid->addWidget(m_budget, 2, 1, 1, 3);

    prefGrid->addWidget(makeFieldLabel("Любимые цвета:", prefCard), 3, 0);
    m_colorInput = new QLineEdit(prefCard);
    m_colorInput->setObjectName("FieldEdit");
    m_colorInput->setPlaceholderText("Добавить цвет (например, hex #RRGGBB или название)");
    prefGrid->addWidget(m_colorInput, 3, 1, 1, 3);

    QWidget *selectedRow = new QWidget(prefCard);
    QHBoxLayout *selectedLay = new QHBoxLayout(selectedRow);
    selectedLay->setContentsMargins(0, 0, 0, 0);
    selectedLay->setSpacing(6);

    QLabel *selectedLabel = makeFieldLabel("Selected Colors:", prefCard);
    selectedLabel->setStyleSheet("font-size:11px;font-weight:800;color:#1C1916;");
    selectedLay->addWidget(selectedLabel, 0, Qt::AlignVCenter);

    QWidget *selectedHost = new QWidget(selectedRow);
    m_selectedColorsLay = new QHBoxLayout(selectedHost);
    m_selectedColorsLay->setContentsMargins(0, 0, 0, 0);
    m_selectedColorsLay->setSpacing(6);
    selectedLay->addWidget(selectedHost, 1);

    m_selectedCountLabel = new QLabel(selectedRow);
    m_selectedCountLabel->setStyleSheet("font-size:11px;color:#6C635C;font-weight:700;");
    selectedLay->addWidget(m_selectedCountLabel, 0, Qt::AlignVCenter);

    prefGrid->addWidget(selectedRow, 4, 0, 1, 4);

    QWidget *paletteRow = new QWidget(prefCard);
    m_paletteLay = new QHBoxLayout(paletteRow);
    m_paletteLay->setContentsMargins(0, 0, 0, 0);
    m_paletteLay->setSpacing(8);
    prefGrid->addWidget(paletteRow, 5, 1, 1, 3);

    QLabel *extraTitle = new QLabel("Дополнительные предпочтения", prefCard);
    extraTitle->setObjectName("SmallSectionTitle");
    prefGrid->addWidget(extraTitle, 6, 0, 1, 4);

    prefGrid->addWidget(makeFieldLabel("Предпочитаемая посадка:", prefCard), 7, 0);
    m_fit = new QComboBox(prefCard);
    m_fit->setObjectName("FieldCombo");
    m_fit->addItems({"Приталенная", "Свободная", "Прямая", "Oversize"});
    prefGrid->addWidget(m_fit, 7, 1);

    prefGrid->addWidget(makeFieldLabel("Размерный\nстандарт:", prefCard), 7, 2);
    m_sizeStandard = new QComboBox(prefCard);
    m_sizeStandard->setObjectName("FieldCombo");
    m_sizeStandard->addItems({"EU", "US", "UK", "RU"});
    m_sizeStandard->setFixedWidth(90);
    prefGrid->addWidget(m_sizeStandard, 7, 3);

    prefGrid->addWidget(makeFieldLabel("Дата рождения:", prefCard), 8, 0);
    m_birthDate = new QDateEdit(prefCard);
    m_birthDate->setObjectName("FieldDateEdit");
    m_birthDate->setDisplayFormat("dd.MM.yyyy");
    m_birthDate->setCalendarPopup(true);
    m_birthDate->setDate(QDate(1990, 1, 1));
    prefGrid->addWidget(m_birthDate, 8, 1, 1, 3);

    prefGrid->addWidget(makeFieldLabel("Параметры\nбезопасности:", prefCard), 9, 0);
    QWidget *secWrap = new QWidget(prefCard);
    QHBoxLayout *secLay = new QHBoxLayout(secWrap);
    secLay->setContentsMargins(0, 0, 0, 0);
    secLay->setSpacing(12);

    QPushButton *changePassBtn = new QPushButton("Смена пароля", secWrap);
    changePassBtn->setObjectName("SecurityBtn");
    changePassBtn->setCursor(Qt::PointingHandCursor);

    m_twoFactor = new QCheckBox("Двухфакторная\nаутентификация", secWrap);
    m_twoFactor->setObjectName("TwoFactor");
    m_twoFactor->setCursor(Qt::PointingHandCursor);

    secLay->addWidget(changePassBtn, 0, Qt::AlignLeft);
    secLay->addWidget(m_twoFactor, 0, Qt::AlignLeft);
    secLay->addStretch();
    prefGrid->addWidget(secWrap, 9, 1, 1, 3);

    prefGrid->setColumnStretch(1, 2);
    prefGrid->setColumnStretch(3, 1);
    prefLay->addLayout(prefGrid);

    root->addWidget(prefCard, 1);

    m_status = new QLabel(rootCard);
    m_status->hide();
    root->addWidget(m_status);

    QHBoxLayout *bottom = new QHBoxLayout();
    bottom->setSpacing(12);
    QPushButton *saveBtn = new QPushButton("Сохранить изменения", rootCard);
    saveBtn->setObjectName("SaveBtn");
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setFixedWidth(220);

    QPushButton *cancelBtn = new QPushButton("Отмена", rootCard);
    cancelBtn->setObjectName("CancelBtn");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setFixedWidth(120);

    bottom->addWidget(saveBtn, 0, Qt::AlignLeft);
    bottom->addStretch();
    bottom->addWidget(cancelBtn, 0, Qt::AlignRight);
    root->addLayout(bottom);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, this, &UserProfileDialog::onSave);
    connect(changeAvatarBtn, &QPushButton::clicked, this, &UserProfileDialog::changeAvatar);
    connect(deleteAvatarBtn, &QPushButton::clicked, this, &UserProfileDialog::clearAvatar);
    connect(changePassBtn, &QPushButton::clicked, this, &UserProfileDialog::openPasswordDialog);
    connect(m_colorInput, &QLineEdit::returnPressed, this, &UserProfileDialog::addManualColors);
    connect(m_accFirst, &QLineEdit::textChanged, this, &UserProfileDialog::updateAvatarView);
    connect(m_accLast, &QLineEdit::textChanged, this, &UserProfileDialog::updateAvatarView);

    loadAccount();
    loadProfile();
    refreshHeader();
    refreshSelectedColors();
    refreshPaletteChips();
    updateAvatarView();
  }

private:
  QString m_username;
  QString m_avatarPath;
  QPoint m_dragPos;

  QLabel *m_avatarLabel = nullptr;
  QLabel *m_usernameLabel = nullptr;
  QLabel *m_userTypeLabel = nullptr;
  QLabel *m_phoneVerifiedLabel = nullptr;
  QLabel *m_status = nullptr;
  QLabel *m_selectedCountLabel = nullptr;

  QLineEdit *m_accFirst = nullptr;
  QLineEdit *m_accLast = nullptr;
  QLineEdit *m_accEmail = nullptr;
  QLineEdit *m_accPhone = nullptr;
  QComboBox *m_gender = nullptr;
  QSpinBox *m_height = nullptr;
  QComboBox *m_style = nullptr;
  QComboBox *m_budget = nullptr;
  QLineEdit *m_colorInput = nullptr;
  QHBoxLayout *m_selectedColorsLay = nullptr;
  QHBoxLayout *m_paletteLay = nullptr;
  QStringList m_selectedColors;
  QComboBox *m_fit = nullptr;
  QComboBox *m_sizeStandard = nullptr;
  QDateEdit *m_birthDate = nullptr;
  QCheckBox *m_twoFactor = nullptr;

  QLabel *makeFieldLabel(const QString &text, QWidget *parent) const
  {
    QLabel *label = new QLabel(text, parent);
    label->setObjectName("FieldLabel");
    return label;
  }

  void addChoiceButton(QHBoxLayout *layout, QButtonGroup *group, const QString &value, const QString &title)
  {
    QPushButton *btn = new QPushButton(title);
    btn->setObjectName("ChoiceBtn");
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setProperty("value", value);
    group->addButton(btn);
    layout->addWidget(btn);
  }

  void mousePressEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton)
      m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
    QDialog::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override
  {
    if (event->buttons() & Qt::LeftButton)
      move(event->globalPosition().toPoint() - m_dragPos);
    QDialog::mouseMoveEvent(event);
  }

  QString initials() const
  {
    QString a = m_accFirst ? m_accFirst->text().trimmed() : QString();
    QString b = m_accLast ? m_accLast->text().trimmed() : QString();
    QString out;
    if (!a.isEmpty()) out += a.left(1).toUpper();
    if (!b.isEmpty()) out += b.left(1).toUpper();
    if (out.isEmpty() && !m_username.trimmed().isEmpty())
      out = m_username.left(1).toUpper();
    if (out.isEmpty())
      out = "A";
    return out.left(2);
  }

  QPixmap circularPixmap(const QPixmap &src, int size) const
  {
    QPixmap scaled = src.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QPixmap out(size, size);
    out.fill(Qt::transparent);

    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addEllipse(0, 0, size, size);
    p.setClipPath(path);
    p.drawPixmap(0, 0, scaled);
    p.setClipping(false);
    p.setPen(QPen(QColor("#DCCFC6"), 1));
    p.drawEllipse(0, 0, size - 1, size - 1);
    return out;
  }

  void updateAvatarView()
  {
    if (!m_avatarLabel)
      return;

    QPixmap avatar;
    if (!m_avatarPath.isEmpty() && QFileInfo::exists(m_avatarPath))
      avatar.load(m_avatarPath);

    if (!avatar.isNull()) {
      m_avatarLabel->setText(QString());
      m_avatarLabel->setPixmap(circularPixmap(avatar, m_avatarLabel->width()));
      return;
    }

    m_avatarLabel->setPixmap(QPixmap());
    m_avatarLabel->setText(initials());
  }

  void refreshHeader()
  {
    m_usernameLabel->setText(m_username);
    const bool admin = ApiClient::instance().isAdmin() || m_username.toLower().contains("admin");
    m_userTypeLabel->setText(QString("Тип пользователя: %1").arg(admin ? "Администратор" : "Пользователь"));
  }

  QString passwordHash(const QString &password) const
  {
    return QString::fromLatin1(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
  }

  bool ensureUserAccountColumns(QString *err = nullptr)
  {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
      if (err) *err = "База данных недоступна.";
      return false;
    }

    auto hasCol = [&](const QString &table, const QString &col) {
      QSqlQuery info(db);
      if (!info.exec(QString("PRAGMA table_info(%1);").arg(table)))
        return false;
      while (info.next()) {
        if (info.value(1).toString() == col)
          return true;
      }
      return false;
    };
    auto addCol = [&](const QString &table, const QString &col, const QString &type) {
      if (hasCol(table, col))
        return true;
      QSqlQuery q(db);
      if (!q.exec(QString("ALTER TABLE %1 ADD COLUMN %2 %3;").arg(table, col, type))) {
        if (err) *err = q.lastError().text();
        return false;
      }
      return true;
    };

    return addCol("users", "first_name", "TEXT")
      && addCol("users", "last_name", "TEXT")
      && addCol("users", "email", "TEXT")
      && addCol("users", "phone", "TEXT")
      && addCol("users", "phone_verified", "INTEGER DEFAULT 0")
      && addCol("users", "avatar_path", "TEXT");
  }

  bool ensureUserProfileColumns(QString *err = nullptr)
  {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
      if (err) *err = "База данных недоступна.";
      return false;
    }

    auto hasCol = [&](const QString &table, const QString &col) {
      QSqlQuery info(db);
      if (!info.exec(QString("PRAGMA table_info(%1);").arg(table)))
        return false;
      while (info.next()) {
        if (info.value(1).toString() == col)
          return true;
      }
      return false;
    };
    auto addCol = [&](const QString &table, const QString &col, const QString &type) {
      if (hasCol(table, col))
        return true;
      QSqlQuery q(db);
      if (!q.exec(QString("ALTER TABLE %1 ADD COLUMN %2 %3;").arg(table, col, type))) {
        if (err) *err = q.lastError().text();
        return false;
      }
      return true;
    };

    return addCol("user_profiles", "birth_date", "TEXT")
      && addCol("user_profiles", "preferred_fit", "TEXT")
      && addCol("user_profiles", "size_standard", "TEXT")
      && addCol("user_profiles", "two_factor", "INTEGER DEFAULT 0");
  }

  QString stripIconText(const QString &text) const
  {
    QString s = text.trimmed();
    if (s.contains(' ')) {
      QString first = s.section(' ', 0, 0);
      if (first.size() <= 3)
        return s.section(' ', 1);
    }
    return s;
  }

  QString selectedGroupValue(QButtonGroup *group) const
  {
    if (!group || !group->checkedButton())
      return QString();
    return group->checkedButton()->property("value").toString();
  }

  void selectGroupValue(QButtonGroup *group, const QString &value)
  {
    if (!group)
      return;
    const QString needle = value.trimmed();
    for (QAbstractButton *btn : group->buttons()) {
      if (btn->property("value").toString().compare(needle, Qt::CaseInsensitive) == 0) {
        btn->setChecked(true);
        return;
      }
    }
    if (!group->buttons().isEmpty())
      group->buttons().first()->setChecked(true);
  }

  QStringList paletteNames() const
  {
    return {"Dark Blue", "White", "Black", "Gray", "Beige", "Brown", "Terracotta", "Olive", "Milk", "Graphite"};
  }

  QColor colorFromName(const QString &name) const
  {
    const QString n = name.trimmed().toLower();
    if (n.contains("dark blue") || n.contains("темно-син") || n.contains("син")) return QColor("#123F83");
    if (n.contains("white") || n.contains("бел")) return QColor("#F2EFE6");
    if (n.contains("black") || n.contains("чер")) return QColor("#171A1F");
    if (n.contains("gray") || n.contains("grey") || n.contains("сер")) return QColor("#7B848E");
    if (n.contains("beige") || n.contains("беж") || n.contains("пес")) return QColor("#D8C39B");
    if (n.contains("brown") || n.contains("корич")) return QColor("#7B5A44");
    if (n.contains("terracotta") || n.contains("террак")) return QColor("#C96549");
    if (n.contains("olive") || n.contains("олив") || n.contains("зел")) return QColor("#7D965D");
    if (n.contains("milk") || n.contains("молоч")) return QColor("#EAE4D8");
    if (n.contains("graphite") || n.contains("графит")) return QColor("#56606B");
    QColor c(name);
    if (c.isValid()) return c;
    return QColor("#C8BFB5");
  }

  bool isDark(const QColor &c) const
  {
    const int y = (c.red() * 299 + c.green() * 587 + c.blue() * 114) / 1000;
    return y < 145;
  }

  QString normalizeColorToken(QString token) const
  {
    token = token.trimmed();
    if (token.isEmpty())
      return QString();
    if (token.startsWith("#")) {
      QColor c(token);
      if (c.isValid())
        return c.name().toUpper();
    }
    return token;
  }

  void addColor(const QString &token)
  {
    QString t = normalizeColorToken(token);
    if (t.isEmpty())
      return;
    for (const QString &c : m_selectedColors) {
      if (c.compare(t, Qt::CaseInsensitive) == 0)
        return;
    }
    if (m_selectedColors.size() >= 10) {
      showStatus("Можно выбрать максимум 10 цветов.", false);
      return;
    }
    m_selectedColors << t;
  }

  void addManualColors()
  {
    QString text = m_colorInput ? m_colorInput->text() : QString();
    text.replace(";", ",");
    text.replace("/", ",");
    text.replace("\n", ",");
    QStringList list = text.split(",", Qt::SkipEmptyParts);
    for (QString token : list)
      addColor(token);
    if (m_colorInput)
      m_colorInput->clear();
    refreshSelectedColors();
    refreshPaletteChips();
  }

  void removeColor(const QString &token)
  {
    for (int i = m_selectedColors.size() - 1; i >= 0; --i) {
      if (m_selectedColors[i].compare(token, Qt::CaseInsensitive) == 0)
        m_selectedColors.removeAt(i);
    }
    refreshSelectedColors();
    refreshPaletteChips();
  }

  void clearLayout(QLayout *layout)
  {
    if (!layout) return;
    while (QLayoutItem *item = layout->takeAt(0)) {
      if (QWidget *w = item->widget())
        w->deleteLater();
      delete item;
    }
  }

  void refreshSelectedColors()
  {
    clearLayout(m_selectedColorsLay);
    for (const QString &name : m_selectedColors) {
      QColor c = colorFromName(name);
      QPushButton *pill = new QPushButton(name + "×");
      pill->setObjectName("ColorPill");
      pill->setCursor(Qt::PointingHandCursor);
      pill->setStyleSheet(QString(
       "QPushButton#ColorPill{background:%1;border:1px solid #BFAF9F;color:%2;border-radius:12px;min-height:24px;max-height:24px;padding:0 10px;font-size:10px;font-weight:700;}"
       "QPushButton#ColorPill:hover{border:2px solid #6D4F3A;}"
      ).arg(c.name(), isDark(c) ? "#FFFFFF" : "#1A1714"));
      connect(pill, &QPushButton::clicked, this, [this, name]() { removeColor(name); });
      m_selectedColorsLay->addWidget(pill);
    }
    m_selectedColorsLay->addStretch();
    if (m_selectedCountLabel)
      m_selectedCountLabel->setText(QString("Выбрано: %1/10 цветов").arg(m_selectedColors.size()));
  }

  void refreshPaletteChips()
  {
    clearLayout(m_paletteLay);
    for (const QString &name : paletteNames()) {
      QToolButton *chip = new QToolButton(this);
      chip->setObjectName("PaletteChip");
      chip->setCheckable(true);
      chip->setChecked(containsColor(name));
      chip->setToolTip(name);
      chip->setCursor(Qt::PointingHandCursor);
      QColor c = colorFromName(name);
      chip->setStyleSheet(QString(
       "QToolButton#PaletteChip{background:%1;border:1px solid #D0C4B8;border-radius:14px;min-width:28px;max-width:28px;min-height:28px;max-height:28px;}"
       "QToolButton#PaletteChip:checked{border:2px solid #6D4F3A;}"
       "QToolButton#PaletteChip:hover{border:2px solid #6D4F3A;}"
      ).arg(c.name()));
      connect(chip, &QToolButton::clicked, this, [this, name]() {
        if (containsColor(name))
          removeColor(name);
        else {
          addColor(name);
          refreshSelectedColors();
          refreshPaletteChips();
        }
      });
      m_paletteLay->addWidget(chip);
    }
    m_paletteLay->addStretch();
  }

  bool containsColor(const QString &token) const
  {
    for (const QString &c : m_selectedColors) {
      if (c.compare(token, Qt::CaseInsensitive) == 0)
        return true;
    }
    return false;
  }

  void showStatus(const QString &text, bool ok)
  {
    m_status->setObjectName(ok ? "StatusOk" : "StatusError");
    m_status->setText(text);
    m_status->style()->unpolish(m_status);
    m_status->style()->polish(m_status);
    m_status->show();
  }

  void loadAccount()
  {
    m_accFirst->setText("Алексей");
    m_accLast->setText("Петров");
    m_accEmail->setText(m_username + "@corp.io");
    m_accPhone->setText("+7 (901) 234-56-78");
    m_phoneVerifiedLabel->setText("Номер подтверждён");

    // Use the API when it is available.
    QJsonObject me;
    QString apiErr;
    if (ApiClient::instance().getMe(&me, &apiErr)) {
      if (!me.value("first_name").toString().isEmpty())
        m_accFirst->setText(me.value("first_name").toString());
      if (!me.value("last_name").toString().isEmpty())
        m_accLast->setText(me.value("last_name").toString());
      if (!me.value("email").toString().isEmpty())
        m_accEmail->setText(me.value("email").toString());
      if (!me.value("phone").toString().isEmpty())
        m_accPhone->setText(me.value("phone").toString());
      if (!me.value("phone_verified").toBool())
        m_phoneVerifiedLabel->setText("Номер не подтверждён");
    }

    // Local users table fields.
    QString err;
    if (!ensureUserAccountColumns(&err))
      return;

    QSqlQuery q(QSqlDatabase::database());
    q.prepare("SELECT IFNULL(first_name,''), IFNULL(last_name,''), IFNULL(email,''), IFNULL(phone,''), IFNULL(phone_verified,0), IFNULL(avatar_path,'') FROM users WHERE username=:u;");
    q.bindValue(":u", m_username);
    if (q.exec() && q.next()) {
      if (!q.value(0).toString().isEmpty()) m_accFirst->setText(q.value(0).toString());
      if (!q.value(1).toString().isEmpty()) m_accLast->setText(q.value(1).toString());
      if (!q.value(2).toString().isEmpty()) m_accEmail->setText(q.value(2).toString());
      if (!q.value(3).toString().isEmpty()) m_accPhone->setText(q.value(3).toString());
      if (!q.value(4).toBool()) m_phoneVerifiedLabel->setText("Номер не подтверждён");
      m_avatarPath = q.value(5).toString();
    }
  }

  void loadProfile()
  {
    QString err;
    ensureUserProfileColumns(&err);

    m_gender->setCurrentIndex(0);
    m_height->setValue(182);
    m_style->setCurrentIndex(0);
    m_budget->setCurrentText("5000-15000 ₽");
    m_fit->setCurrentIndex(0);
    m_sizeStandard->setCurrentText("EU");
    m_birthDate->setDate(QDate(1990, 1, 1));
    m_twoFactor->setChecked(false);
    m_selectedColors = {"Dark Blue", "White", "Black"};

    QSqlQuery q(QSqlDatabase::database());
    q.prepare("SELECT IFNULL(gender,''), IFNULL(height,0), IFNULL(style,''), IFNULL(budget,''), IFNULL(favorite_colors,''), IFNULL(birth_date,''), IFNULL(preferred_fit,''), IFNULL(size_standard,''), IFNULL(two_factor,0) FROM user_profiles WHERE username=:u;");
    q.bindValue(":u", m_username);
    if (q.exec() && q.next()) {
      const QString gender = q.value(0).toString();
      const int height = q.value(1).toInt();
      const QString style = q.value(2).toString();
      const QString budget = q.value(3).toString();
      const QString colors = q.value(4).toString();
      const QString birth = q.value(5).toString();
      const QString fit = q.value(6).toString();
      const QString sizeStd = q.value(7).toString();
      const bool two = q.value(8).toBool();

      if (!gender.isEmpty()) {
        for (int i = 0; i < m_gender->count(); ++i) {
          if (stripIconText(m_gender->itemText(i)).compare(gender, Qt::CaseInsensitive) == 0 || m_gender->itemText(i).compare(gender, Qt::CaseInsensitive) == 0) {
            m_gender->setCurrentIndex(i);
            break;
          }
        }
      }
      if (height > 0) m_height->setValue(height);
      if (!style.isEmpty()) {
        for (int i = 0; i < m_style->count(); ++i) {
          if (stripIconText(m_style->itemText(i)).compare(style, Qt::CaseInsensitive) == 0 ||
            m_style->itemText(i).compare(style, Qt::CaseInsensitive) == 0) {
            m_style->setCurrentIndex(i);
            break;
          }
        }
      }
      if (!budget.isEmpty()) {
        int bidx = m_budget->findText(budget, Qt::MatchFixedString);
        if (bidx >= 0)
          m_budget->setCurrentIndex(bidx);
      }

      if (!colors.trimmed().isEmpty()) {
        m_selectedColors.clear();
        QStringList list = colors.split(",", Qt::SkipEmptyParts);
        for (QString c : list)
          addColor(c.trimmed());
      }

      if (!birth.isEmpty()) {
        QDate d = QDate::fromString(birth, "dd.MM.yyyy");
        if (!d.isValid()) d = QDate::fromString(birth, Qt::ISODate);
        if (d.isValid()) m_birthDate->setDate(d);
      }

      if (!fit.isEmpty()) {
        for (int i = 0; i < m_fit->count(); ++i) {
          if (stripIconText(m_fit->itemText(i)).compare(fit, Qt::CaseInsensitive) == 0 || m_fit->itemText(i).compare(fit, Qt::CaseInsensitive) == 0) {
            m_fit->setCurrentIndex(i);
            break;
          }
        }
      }

      if (!sizeStd.isEmpty()) {
        int idx = m_sizeStandard->findText(sizeStd, Qt::MatchFixedString);
        if (idx >= 0) m_sizeStandard->setCurrentIndex(idx);
      }

      m_twoFactor->setChecked(two);
    }
  }

  bool saveAccountLocal(QString *err = nullptr)
  {
    if (!ensureUserAccountColumns(err))
      return false;

    QSqlQuery q(QSqlDatabase::database());
    q.prepare("UPDATE users SET first_name=:fn, last_name=:ln, email=:em, phone=:ph, avatar_path=:av WHERE username=:u;");
    q.bindValue(":fn", m_accFirst->text().trimmed());
    q.bindValue(":ln", m_accLast->text().trimmed());
    q.bindValue(":em", m_accEmail->text().trimmed());
    q.bindValue(":ph", m_accPhone->text().trimmed());
    q.bindValue(":av", m_avatarPath);
    q.bindValue(":u", m_username);

    if (!q.exec()) {
      if (err) *err = q.lastError().text();
      return false;
    }
    return true;
  }

  bool saveProfileLocal(QString *err = nullptr)
  {
    if (!ensureUserProfileColumns(err))
      return false;

    const QString gender = stripIconText(m_gender->currentText());
    const int height = m_height->value();
    const QString style = stripIconText(m_style->currentText());
    const QString budget = m_budget->currentText();
    const QString colors = m_selectedColors.join(", ");
    const QString birth = m_birthDate->date().toString("dd.MM.yyyy");
    const QString fit = stripIconText(m_fit->currentText());
    const QString sizeStd = m_sizeStandard->currentText();
    const int twoFactor = m_twoFactor->isChecked() ? 1 : 0;

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery upd(db);
    upd.prepare("UPDATE user_profiles SET gender=:g, height=:h, style=:s, budget=:b, favorite_colors=:c, birth_date=:bd, preferred_fit=:pf, size_standard=:ss, two_factor=:tf WHERE username=:u;");
    upd.bindValue(":g", gender);
    upd.bindValue(":h", height);
    upd.bindValue(":s", style);
    upd.bindValue(":b", budget);
    upd.bindValue(":c", colors);
    upd.bindValue(":bd", birth);
    upd.bindValue(":pf", fit);
    upd.bindValue(":ss", sizeStd);
    upd.bindValue(":tf", twoFactor);
    upd.bindValue(":u", m_username);

    if (!upd.exec()) {
      if (err) *err = upd.lastError().text();
      return false;
    }
    if (upd.numRowsAffected() > 0)
      return true;

    QSqlQuery ins(db);
    ins.prepare("INSERT INTO user_profiles(username, gender, height, style, budget, favorite_colors, birth_date, preferred_fit, size_standard, two_factor) VALUES(:u,:g,:h,:s,:b,:c,:bd,:pf,:ss,:tf);");
    ins.bindValue(":u", m_username);
    ins.bindValue(":g", gender);
    ins.bindValue(":h", height);
    ins.bindValue(":s", style);
    ins.bindValue(":b", budget);
    ins.bindValue(":c", colors);
    ins.bindValue(":bd", birth);
    ins.bindValue(":pf", fit);
    ins.bindValue(":ss", sizeStd);
    ins.bindValue(":tf", twoFactor);

    if (!ins.exec()) {
      if (err) *err = ins.lastError().text();
      return false;
    }
    return true;
  }

  bool changePasswordLocal(const QString &currentPassword, const QString &newPassword, QString *err = nullptr)
  {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
      if (err) *err = "База данных недоступна.";
      return false;
    }

    QSqlQuery q(db);
    q.prepare("SELECT password_hash FROM users WHERE username=:u;");
    q.bindValue(":u", m_username);
    if (!q.exec() || !q.next()) {
      if (err) *err = "Пользователь не найден.";
      return false;
    }

    if (q.value(0).toString() != passwordHash(currentPassword)) {
      if (err) *err = "Текущий пароль указан неверно.";
      return false;
    }

    QSqlQuery upd(db);
    upd.prepare("UPDATE users SET password_hash=:h WHERE username=:u;");
    upd.bindValue(":h", passwordHash(newPassword));
    upd.bindValue(":u", m_username);
    if (!upd.exec()) {
      if (err) *err = upd.lastError().text();
      return false;
    }
    return true;
  }

  void changeAvatar()
  {
    const QString file = QFileDialog::getOpenFileName(
      this, "Выберите аватар", QString(), "Images (*.png *.jpg *.jpeg *.bmp *.webp)"
    );
    if (file.isEmpty())
      return;
    m_avatarPath = file;
    QString err;
    saveAccountLocal(&err);
    updateAvatarView();
    showStatus("Аватар обновлён.", true);
  }

  void clearAvatar()
  {
    m_avatarPath.clear();
    QString err;
    saveAccountLocal(&err);
    updateAvatarView();
    showStatus("Фото удалено. Теперь отображаются инициалы.", true);
  }

  void openPasswordDialog()
  {
    QDialog dlg(this);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setAttribute(Qt::WA_TranslucentBackground, true);
    dlg.setModal(true);
    dlg.resize(420, 290);
    dlg.setStyleSheet(
     "QDialog{background:transparent;}"
     "QFrame#PassCard{background:#F8F4EE;border:1px solid #E2D6CA;border-radius:18px;}"
     "QLabel{background:transparent;border:none;color:#1A1714;}"
     "QLabel#PassTitle{font-size:15px;font-weight:900;}"
     "QLineEdit{background:#FFFFFF;border:1px solid #B7A79A;border-radius:10px;min-height:34px;padding:0 12px;font-size:11px;}"
     "QPushButton#PassPrimary{background:#E2A07C;color:#FFF8F2;border:none;border-radius:10px;min-height:36px;padding:0 16px;font-size:11px;font-weight:800;}"
     "QPushButton#PassSecondary{background:#B5ADA4;color:#FFF8F2;border:none;border-radius:10px;min-height:36px;padding:0 16px;font-size:11px;font-weight:800;}"
     "QPushButton#PassX{background:transparent;border:none;color:#1A1714;font-size:18px;font-weight:900;}"
     "QLabel#PassStatus{font-size:10px;color:#BA3B33;font-weight:700;}"
    );

    QVBoxLayout *ov = new QVBoxLayout(&dlg);
    ov->setContentsMargins(0, 0, 0, 0);
    QFrame *card = new QFrame(&dlg);
    card->setObjectName("PassCard");
    ov->addWidget(card);

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(16, 14, 16, 16);
    lay->setSpacing(10);

    QHBoxLayout *head = new QHBoxLayout();
    QLabel *title = new QLabel("Смена пароля", card);
    title->setObjectName("PassTitle");
    QPushButton *x = new RoundCloseButton(card);
    x->setObjectName("PassX");
    x->setFixedSize(24, 24);
    head->addWidget(title);
    head->addStretch();
    head->addWidget(x);
    lay->addLayout(head);

    QLineEdit *cur = new QLineEdit(card);
    cur->setPlaceholderText("Текущий пароль");
    cur->setEchoMode(QLineEdit::Password);
    QLineEdit *nw = new QLineEdit(card);
    nw->setPlaceholderText("Новый пароль");
    nw->setEchoMode(QLineEdit::Password);
    QLineEdit *rep = new QLineEdit(card);
    rep->setPlaceholderText("Повторите новый пароль");
    rep->setEchoMode(QLineEdit::Password);

    QLabel *status = new QLabel(card);
    status->setObjectName("PassStatus");
    status->hide();

    lay->addWidget(cur);
    lay->addWidget(nw);
    lay->addWidget(rep);
    lay->addWidget(status);

    QHBoxLayout *btns = new QHBoxLayout();
    QPushButton *cancel = new QPushButton("Отмена", card);
    cancel->setObjectName("PassSecondary");
    QPushButton *ok = new QPushButton("Сохранить", card);
    ok->setObjectName("PassPrimary");
    btns->addWidget(cancel);
    btns->addStretch();
    btns->addWidget(ok);
    lay->addLayout(btns);

    QObject::connect(x, &QPushButton::clicked, &dlg, &QDialog::reject);
    QObject::connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    QObject::connect(ok, &QPushButton::clicked, &dlg, [&]() {
      if (cur->text().isEmpty() || nw->text().isEmpty() || rep->text().isEmpty()) {
        status->setText("Заполните все поля.");
        status->show();
        return;
      }
      if (nw->text() != rep->text()) {
        status->setText("Новый пароль и повтор не совпадают.");
        status->show();
        return;
      }
      QString err;
      if (!changePasswordLocal(cur->text(), nw->text(), &err)) {
        status->setText(err);
        status->show();
        return;
      }
      dlg.accept();
      showStatus("Пароль успешно изменён.", true);
    });

    dlg.exec();
  }

  void onSave()
  {
    addManualColors();

    if (m_accFirst->text().trimmed().isEmpty()) {
      showStatus("Укажите имя.", false);
      return;
    }
    if (m_accLast->text().trimmed().isEmpty()) {
      showStatus("Укажите фамилию.", false);
      return;
    }

    QString accountErr, profileErr;
    const bool ok1 = saveAccountLocal(&accountErr);
    const bool ok2 = saveProfileLocal(&profileErr);

    if (!ok1 || !ok2) {
      showStatus(QString("Ошибка сохранения: %1 %2").arg(accountErr, profileErr).trimmed(), false);
      return;
    }

    updateAvatarView();
    showStatus("Профиль сохранён. Аватар обновлён.", true);
  }
};
