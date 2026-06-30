#pragma once

/*
  File: src/auth/AuthDialogs.h
  Purpose: Login, registration, and phone-confirmation dialogs.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "../widgets/RoundCloseButton.h"
#include "../api/ApiClient.h"

#include <QCheckBox>
#include <QColor>
#include <QCloseEvent>
#include <QDialog>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

// ================= Common window helper functions ================

class AuthBaseDialog : public QDialog
{
public:
  explicit AuthBaseDialog(QWidget *parent = nullptr)
    : QDialog(parent)
  {
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setWindowModality(Qt::ApplicationModal);
    setModal(true);
  }

protected:
  QPoint m_dragPos;
  bool m_authPositioned = false;

  void keepAuthDialogInFront(QWidget *focusWidget = nullptr)
  {
    if (!isVisible())
      show();

    if (isMinimized())
      showNormal();

    raise();
    activateWindow();

    if (focusWidget)
      focusWidget->setFocus(Qt::OtherFocusReason);
  }

  void keepAuthDialogInFrontQueued(QWidget *focusWidget = nullptr)
  {
    QTimer::singleShot(0, this, [this, focusWidget]() {
      keepAuthDialogInFront(focusWidget);
    });
    QTimer::singleShot(80, this, [this, focusWidget]() {
      keepAuthDialogInFront(focusWidget);
    });
  }

  void showEvent(QShowEvent *event) override
  {
    QDialog::showEvent(event);

    if (m_authPositioned)
      return;

    m_authPositioned = true;

    // Login and registration windows are not placed exactly in the center,
    // but are shifted slightly downward for a better visual balance.
    const int verticalOffset = -35;

    if (QWidget *parent = parentWidget()) {
      QRect parentRect = parent->frameGeometry();
      if (parentRect.isValid() && parentRect.width() > 0 && parentRect.height() > 0) {
        move(parentRect.center().x() - width() / 2,
           parentRect.center().y() - height() / 2 + verticalOffset);
        keepAuthDialogInFrontQueued();
        return;
      }
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
      return;

    QRect available = screen->availableGeometry();
    move(available.center().x() - width() / 2,
       available.center().y() - height() / 2 + verticalOffset);
    keepAuthDialogInFrontQueued();
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
};

static QString authWindowStyle()
{
  return QString(
   "QDialog{background:#00000000;}"
   "QFrame#AuthRoot{background:transparent;border:none;border-radius:28px;}"
   "QFrame#AuthCard{background:#F6F1EC;border:1px solid #E7DDD4;border-radius:24px;}"
   "QFrame#AuthHero{background:#FCF9F5;border:1px solid #E9DFD6;border-radius:18px;}"
   "QFrame#AuthSmallCard{background:#FFF9F4;border:1px solid #E7DCD2;border-radius:16px;}"
   "QLabel{background:transparent;border:none;color:#1D1A17;}"
   "QLabel#AuthBrand{font-size:14px;font-weight:900;color:#6F4B37;}"
   "QLabel#AuthTitle{font-size:26px;font-weight:900;color:#15120F;}"
   "QLabel#AuthSubtitle{font-size:13px;color:#756B63;}"
   "QLabel#AuthFieldLabel{font-size:12px;font-weight:800;color:#2E2925;}"
   "QLabel#AuthStatus{font-size:12px;font-weight:800;color:#B33A32;}"
   "QLabel#AuthStatusOk{font-size:12px;font-weight:800;color:#2E8B47;}"
   "QLabel#AuthCode{background:#FFFFFF;border:1px solid #E0D5CC;border-radius:14px;padding:12px;font-size:28px;font-weight:900;color:#6F4B37;letter-spacing:4px;}"
   "QLineEdit#AuthEdit{background:#FFFFFF;border:1px solid #D9CEC5;border-radius:12px;min-height:42px;padding:0 14px;font-size:14px;color:#211D1A;selection-background-color:#C9964E;}"
   "QLineEdit#AuthEdit:focus{border:1px solid #C9964E;}"
   "QCheckBox{background:transparent;color:#5C524B;font-size:12px;font-weight:700;spacing:8px;}"
   "QCheckBox::indicator{width:18px;height:18px;border:1px solid #CFC3B8;border-radius:5px;background:#FFFFFF;}"
   "QCheckBox::indicator:checked{background:#C9964E;border:1px solid #C9964E;}"
   "QPushButton#AuthClose{background:transparent;border:none;color:#1F1A17;font-size:24px;font-weight:900;min-width:34px;max-width:34px;min-height:34px;max-height:34px;border-radius:17px;padding:0;}"
   "QPushButton#AuthClose:hover{background:#E8DED5;}"
   "QPushButton#AuthMin{background:transparent;border:none;color:#1F1A17;font-size:20px;font-weight:900;min-width:34px;max-width:34px;min-height:34px;max-height:34px;border-radius:17px;padding:0;}"
   "QPushButton#AuthMin:hover{background:#E8DED5;}"
   "QPushButton#AuthPrimary{background:#7A4F39;color:white;border:none;border-radius:12px;min-height:44px;padding:0 22px;font-size:14px;font-weight:900;}"
   "QPushButton#AuthPrimary:hover{background:#68412F;}"
   "QPushButton#AuthPrimary:pressed{background:#7A4F39;}"
   "QPushButton#AuthSecondary{background:#FFFFFF;color:#3A342F;border:1px solid #D8CEC5;border-radius:12px;min-height:44px;padding:0 18px;font-size:14px;font-weight:800;}"
   "QPushButton#AuthSecondary:hover{background:#FBF7F3;}"
   "QPushButton#AuthLink{background:transparent;border:none;color:#8A624A;text-decoration:underline;font-size:13px;font-weight:800;padding:0;}"
   "QPushButton#AuthLink:hover{color:#6F4B37;}"
  );
}

static QLabel *makeAuthLabel(const QString &text, QWidget *parent)
{
  QLabel *label = new QLabel(text, parent);
  label->setObjectName("AuthFieldLabel");
  return label;
}

static bool isValidEmail(const QString &email)
{
  static const QRegularExpression re(
    R"(^[A-Za-z0-9._%+\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}$)"
  );
  return re.match(email).hasMatch();
}

static bool isValidPhone(const QString &phone)
{
  QString digits;
  for (QChar ch : phone) {
    if (ch.isDigit())
      digits += ch;
  }

  if (digits.size() != 11)
    return false;

  return digits.startsWith('7') || digits.startsWith('8');
}

static QString normalizePhone(const QString &phone)
{
  QString digits;
  for (QChar ch : phone) {
    if (ch.isDigit())
      digits += ch;
  }

  if (digits.size() == 11 && digits.startsWith('8'))
    digits[0] = '7';

  if (digits.size() == 11 && digits.startsWith('7'))
    return "+" + digits;

  return phone.trimmed();
}

// ================= CODE DISPLAY DIALOG =================

class CodeShowDialog : public AuthBaseDialog
{
public:
  explicit CodeShowDialog(const QString &code, QWidget *parent = nullptr)
    : AuthBaseDialog(parent)
  {
    setWindowTitle("Код подтверждения");
    setWindowModality(Qt::ApplicationModal);
    resize(400, 280);
    setStyleSheet(authWindowStyle());

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(18, 18, 18, 18);

    QFrame *root = new QFrame(this);
    root->setObjectName("AuthRoot");
    outer->addWidget(root);
    auto *shadow = new QGraphicsDropShadowEffect(root);
    shadow->setBlurRadius(34);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(35, 28, 22, 90));
    root->setGraphicsEffect(shadow);


    QVBoxLayout *rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(8, 8, 8, 8);

    QFrame *card = new QFrame(root);
    card->setObjectName("AuthCard");
    rootLay->addWidget(card);

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(24, 22, 24, 22);
    lay->setSpacing(12);

    QLabel *brand = new QLabel("ClothingCatalog", card);
    brand->setObjectName("AuthBrand");
    brand->setAlignment(Qt::AlignCenter);

    QLabel *title = new QLabel("Код подтверждения", card);
    title->setObjectName("AuthTitle");
    title->setAlignment(Qt::AlignCenter);

    QLabel *hint = new QLabel("Код пришёл в приложение.\nСкопируйте его в окно ввода.", card);
    hint->setObjectName("AuthSubtitle");
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);

    QLabel *codeLbl = new QLabel(code, card);
    codeLbl->setObjectName("AuthCode");
    codeLbl->setAlignment(Qt::AlignCenter);

    lay->addWidget(brand);
    lay->addWidget(title);
    lay->addWidget(hint);
    lay->addWidget(codeLbl);
    lay->addStretch();
  }

protected:
  void closeEvent(QCloseEvent *event) override
  {
    event->ignore();
  }

  void keyPressEvent(QKeyEvent *event) override
  {
    if (event->key() == Qt::Key_Escape) {
      event->ignore();
      return;
    }

    AuthBaseDialog::keyPressEvent(event);
  }
};

// ================= CODE INPUT DIALOG =================

class CodeInputDialog : public AuthBaseDialog
{
  Q_OBJECT

public:
  explicit CodeInputDialog(QWidget *parent = nullptr)
    : AuthBaseDialog(parent)
  {
    setWindowTitle("Подтверждение телефона");
    resize(430, 290);
    setStyleSheet(authWindowStyle());

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(18, 18, 18, 18);

    QFrame *root = new QFrame(this);
    root->setObjectName("AuthRoot");
    outer->addWidget(root);
    auto *shadow = new QGraphicsDropShadowEffect(root);
    shadow->setBlurRadius(34);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(35, 28, 22, 90));
    root->setGraphicsEffect(shadow);


    QVBoxLayout *rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(8, 8, 8, 8);

    QFrame *card = new QFrame(root);
    card->setObjectName("AuthCard");
    rootLay->addWidget(card);

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(12);

    QHBoxLayout *head = new QHBoxLayout();
    QLabel *title = new QLabel("Подтверждение телефона", card);
    title->setObjectName("AuthTitle");
    title->setStyleSheet("font-size:20px;font-weight:900;color:#15120F;");

    QPushButton *closeBtn = new RoundCloseButton(card);
    closeBtn->setObjectName("AuthClose");
    closeBtn->setCursor(Qt::PointingHandCursor);

    head->addWidget(title);
    head->addStretch();
    head->addWidget(closeBtn);
    lay->addLayout(head);

    QLabel *hint = new QLabel("Введите шестизначный код", card);
    hint->setObjectName("AuthSubtitle");
    lay->addWidget(hint);

    m_edit = new QLineEdit(card);
    m_edit->setObjectName("AuthEdit");
    m_edit->setPlaceholderText("например 123456");
    m_edit->setMaxLength(6);
    m_edit->setAlignment(Qt::AlignCenter);
    m_edit->setValidator(new QRegularExpressionValidator(QRegularExpression(R"(\d{0,6})"), m_edit));
    lay->addWidget(m_edit);

    QHBoxLayout *btns = new QHBoxLayout();
    QPushButton *cancel = new QPushButton("Отмена", card);
    cancel->setObjectName("AuthSecondary");
    cancel->setCursor(Qt::PointingHandCursor);

    QPushButton *ok = new QPushButton("Подтвердить", card);
    ok->setObjectName("AuthPrimary");
    ok->setCursor(Qt::PointingHandCursor);

    btns->addWidget(cancel);
    btns->addStretch();
    btns->addWidget(ok);
    lay->addLayout(btns);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_edit, &QLineEdit::returnPressed, this, &QDialog::accept);
  }

  QString code() const
  {
    return m_edit->text().trimmed();
  }

private:
  QLineEdit *m_edit = nullptr;
};

// ================= REGISTRATION DIALOG =================

// Registration dialog validates user input and starts the phone-confirmation flow.
class RegistrationDialog : public AuthBaseDialog
{
  Q_OBJECT

public:
  explicit RegistrationDialog(QWidget *parent = nullptr)
    : AuthBaseDialog(parent)
  {
    setWindowTitle("Регистрация");
    resize(600, 700);
    setMinimumSize(560, 660);
    setStyleSheet(authWindowStyle());

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(18, 18, 18, 18);

    QFrame *root = new QFrame(this);
    root->setObjectName("AuthRoot");
    outer->addWidget(root);
    auto *shadow = new QGraphicsDropShadowEffect(root);
    shadow->setBlurRadius(34);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(35, 28, 22, 90));
    root->setGraphicsEffect(shadow);


    QVBoxLayout *rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(8, 8, 8, 8);

    QFrame *card = new QFrame(root);
    card->setObjectName("AuthCard");
    rootLay->addWidget(card);

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(28, 22, 28, 24);
    layout->setSpacing(12);

    QHBoxLayout *header = new QHBoxLayout();
    QVBoxLayout *titleBox = new QVBoxLayout();
    titleBox->setSpacing(3);

    QLabel *brand = new QLabel("ClothingCatalog", card);
    brand->setObjectName("AuthBrand");

    QLabel *title = new QLabel("Создание аккаунта", card);
    title->setObjectName("AuthTitle");

    QLabel *subtitle = new QLabel("Заполните данные, подтвердите телефон и войдите в приложение.", card);
    subtitle->setObjectName("AuthSubtitle");
    subtitle->setWordWrap(true);

    titleBox->addWidget(brand);
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);

    QPushButton *closeBtn = new RoundCloseButton(card);
    closeBtn->setObjectName("AuthClose");
    closeBtn->setCursor(Qt::PointingHandCursor);

    header->addLayout(titleBox, 1);
    header->addWidget(closeBtn, 0, Qt::AlignTop);
    layout->addLayout(header);

    QFrame *formCard = new QFrame(card);
    formCard->setObjectName("AuthHero");
    QVBoxLayout *form = new QVBoxLayout(formCard);
    form->setContentsMargins(18, 16, 18, 16);
    form->setSpacing(9);

    auto addField = [&](const QString &caption, QLineEdit *edit) {
      form->addWidget(makeAuthLabel(caption, formCard));
      edit->setObjectName("AuthEdit");
      form->addWidget(edit);
    };

    m_first = new QLineEdit(formCard);
    m_last = new QLineEdit(formCard);
    m_email = new QLineEdit(formCard);
    m_phone = new QLineEdit(formCard);
    m_pass = new QLineEdit(formCard);
    m_pass2 = new QLineEdit(formCard);

    m_first->setPlaceholderText("Алексей");
    m_last->setPlaceholderText("Петров");
    m_email->setPlaceholderText("example@mail.com");
    m_phone->setPlaceholderText("+79991234567");
    m_pass->setPlaceholderText("Минимум 8 символов");
    m_pass2->setPlaceholderText("Повторите пароль");

    m_phone->setMaxLength(16);
    m_phone->setValidator(new QRegularExpressionValidator(QRegularExpression(R"(^\+?\d{0,15}$)"), m_phone));

    m_pass->setEchoMode(QLineEdit::Password);
    m_pass2->setEchoMode(QLineEdit::Password);

    QHBoxLayout *nameRow = new QHBoxLayout();
    QVBoxLayout *firstCol = new QVBoxLayout();
    QVBoxLayout *lastCol = new QVBoxLayout();

    firstCol->setSpacing(6);
    lastCol->setSpacing(6);

    firstCol->addWidget(makeAuthLabel("Имя", formCard));
    m_first->setObjectName("AuthEdit");
    firstCol->addWidget(m_first);

    lastCol->addWidget(makeAuthLabel("Фамилия", formCard));
    m_last->setObjectName("AuthEdit");
    lastCol->addWidget(m_last);

    nameRow->addLayout(firstCol, 1);
    nameRow->addLayout(lastCol, 1);
    form->addLayout(nameRow);

    addField("Почта", m_email);
    addField("Телефон", m_phone);
    addField("Пароль", m_pass);
    addField("Повтор пароля", m_pass2);

    m_showPassword = new QCheckBox("Показать пароль", formCard);
    form->addWidget(m_showPassword);

    layout->addWidget(formCard);

    m_status = new QLabel(card);
    m_status->setObjectName("AuthStatus");
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    QHBoxLayout *btns = new QHBoxLayout();

    QPushButton *cancelBtn = new QPushButton("Назад", card);
    cancelBtn->setObjectName("AuthSecondary");
    cancelBtn->setCursor(Qt::PointingHandCursor);

    QPushButton *okBtn = new QPushButton("Зарегистрироваться", card);
    okBtn->setObjectName("AuthPrimary");
    okBtn->setCursor(Qt::PointingHandCursor);

    btns->addWidget(cancelBtn);
    btns->addStretch();
    btns->addWidget(okBtn);
    layout->addLayout(btns);

    okBtn->setDefault(false);
    okBtn->setAutoDefault(false);
    cancelBtn->setDefault(false);
    cancelBtn->setAutoDefault(false);
    closeBtn->setDefault(false);
    closeBtn->setAutoDefault(false);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, this, &RegistrationDialog::doRegister);
    connect(m_first, &QLineEdit::returnPressed, this, &RegistrationDialog::doRegister);
    connect(m_last, &QLineEdit::returnPressed, this, &RegistrationDialog::doRegister);
    connect(m_email, &QLineEdit::returnPressed, this, &RegistrationDialog::doRegister);
    connect(m_phone, &QLineEdit::returnPressed, this, &RegistrationDialog::doRegister);
    connect(m_pass, &QLineEdit::returnPressed, this, &RegistrationDialog::doRegister);
    connect(m_pass2, &QLineEdit::returnPressed, this, &RegistrationDialog::doRegister);

    connect(m_showPassword, &QCheckBox::toggled, this, [this](bool checked) {
      m_pass->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
      m_pass2->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });
  }

  QString email() const
  {
    return m_emailValue;
  }

protected:
  void keyPressEvent(QKeyEvent *event) override
  {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
      doRegister();
      event->accept();
      return;
    }

    AuthBaseDialog::keyPressEvent(event);
  }

private slots:
  void doRegister()
  {
    setStatus(QString(), false);

    QString first = m_first->text().trimmed();
    QString last = m_last->text().trimmed();
    QString email = m_email->text().trimmed().toLower();
    QString phone = m_phone->text().trimmed();
    QString p1  = m_pass->text();
    QString p2  = m_pass2->text();

    if (first.isEmpty() || last.isEmpty() || email.isEmpty() ||
      phone.isEmpty() || p1.isEmpty() || p2.isEmpty()) {
      setStatus("Заполните все поля.", false);
      return;
    }

    if (!isValidEmail(email)) {
      setStatus("Введите корректную почту.", false);
      return;
    }

    if (!isValidPhone(phone)) {
      setStatus("Введите корректный номер телефона.", false);
      return;
    }

    if (p1.length() < 8) {
      setStatus("Пароль должен содержать минимум 8 символов.", false);
      return;
    }

    if (p1 != p2) {
      setStatus("Пароли не совпадают.", false);
      return;
    }

    phone = normalizePhone(phone);

    QString err;
    QString phoneCode;
    if (!ApiClient::instance().registerUserEx(first, last, email, phone, p1, &phoneCode, &err)) {
      setStatus(err.isEmpty() ? "Ошибка регистрации." : err, false);
      return;
    }

    CodeShowDialog showDlg(phoneCode, this);
    showDlg.show();

    CodeInputDialog inputDlg(this);
    const int result = inputDlg.exec();

    showDlg.done(0);

    if (result != QDialog::Accepted) {
      setStatus("Ввод кода отменён. Нажмите «Зарегистрироваться» снова, чтобы получить новый код.", false);
      return;
    }

    const QString input = inputDlg.code();
    if (input.isEmpty()) {
      setStatus("Введите код подтверждения.", false);
      return;
    }

    QString confirmErr;
    if (!ApiClient::instance().confirmPhone(email, input, &confirmErr)) {
      setStatus(confirmErr.isEmpty() ? "Неверный код." : confirmErr, false);
      return;
    }

    m_emailValue = email;
    accept();
  }

private:
  QLineEdit *m_first = nullptr;
  QLineEdit *m_last = nullptr;
  QLineEdit *m_email = nullptr;
  QLineEdit *m_phone = nullptr;
  QLineEdit *m_pass = nullptr;
  QLineEdit *m_pass2 = nullptr;
  QCheckBox *m_showPassword = nullptr;
  QLabel *m_status = nullptr;

  QString m_emailValue;

  void setStatus(const QString &text, bool ok)
  {
    m_status->setText(text);
    m_status->setObjectName(ok ? "AuthStatusOk" : "AuthStatus");
    m_status->style()->unpolish(m_status);
    m_status->style()->polish(m_status);
    keepAuthDialogInFrontQueued();
  }
};

// ================= LOGIN DIALOG =================

// Login dialog authenticates regular users and administrators.
class LoginDialog : public AuthBaseDialog
{
  Q_OBJECT

public:
  explicit LoginDialog(QWidget *parent = nullptr)
    : AuthBaseDialog(parent)
  {
    setWindowTitle("Вход в систему");
    resize(560, 560);
    setMinimumSize(520, 520);
    setStyleSheet(authWindowStyle());

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(18, 18, 18, 18);

    QFrame *root = new QFrame(this);
    root->setObjectName("AuthRoot");
    outer->addWidget(root);
    auto *shadow = new QGraphicsDropShadowEffect(root);
    shadow->setBlurRadius(34);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(35, 28, 22, 90));
    root->setGraphicsEffect(shadow);


    QVBoxLayout *rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(8, 8, 8, 8);

    QFrame *card = new QFrame(root);
    card->setObjectName("AuthCard");
    rootLay->addWidget(card);

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(28, 22, 28, 24);
    layout->setSpacing(14);

    QHBoxLayout *header = new QHBoxLayout();

    QVBoxLayout *titleBox = new QVBoxLayout();
    titleBox->setSpacing(3);

    QLabel *brand = new QLabel("ClothingCatalog", card);
    brand->setObjectName("AuthBrand");

    QLabel *title = new QLabel("Вход в систему", card);
    title->setObjectName("AuthTitle");

    QLabel *subtitle = new QLabel("Введите почту/логин и пароль, чтобы открыть каталог.", card);
    subtitle->setObjectName("AuthSubtitle");
    subtitle->setWordWrap(true);

    titleBox->addWidget(brand);
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);

    QHBoxLayout *winBtns = new QHBoxLayout();
    winBtns->setSpacing(4);

    QPushButton *minBtn = new QPushButton("−", card);
    minBtn->setObjectName("AuthMin");
    minBtn->setCursor(Qt::PointingHandCursor);

    QPushButton *closeBtn = new RoundCloseButton(card);
    closeBtn->setObjectName("AuthClose");
    closeBtn->setCursor(Qt::PointingHandCursor);

    winBtns->addWidget(minBtn);
    winBtns->addWidget(closeBtn);

    header->addLayout(titleBox, 1);
    header->addLayout(winBtns);
    layout->addLayout(header);

    QFrame *formCard = new QFrame(card);
    formCard->setObjectName("AuthHero");

    QVBoxLayout *form = new QVBoxLayout(formCard);
    form->setContentsMargins(18, 16, 18, 16);
    form->setSpacing(9);

    m_login = new QLineEdit(formCard);
    m_login->setObjectName("AuthEdit");
    m_login->setPlaceholderText("example@mail.com или admin");

    m_password = new QLineEdit(formCard);
    m_password->setObjectName("AuthEdit");
    m_password->setPlaceholderText("Пароль");
    m_password->setEchoMode(QLineEdit::Password);

    form->addWidget(makeAuthLabel("Логин / почта", formCard));
    form->addWidget(m_login);
    form->addWidget(makeAuthLabel("Пароль", formCard));
    form->addWidget(m_password);

    m_showPassword = new QCheckBox("Показать пароль", formCard);
    form->addWidget(m_showPassword);

    layout->addWidget(formCard);

    m_statusLabel = new QLabel(card);
    m_statusLabel->setObjectName("AuthStatus");
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    QFrame *hintCard = new QFrame(card);
    hintCard->setObjectName("AuthSmallCard");
    QVBoxLayout *hintLay = new QVBoxLayout(hintCard);
    hintLay->setContentsMargins(14, 12, 14, 12);
    hintLay->setSpacing(4);

    QLabel *hintTitle = new QLabel("Нет аккаунта?", hintCard);
    hintTitle->setStyleSheet("font-size:13px;font-weight:900;color:#171411;");

    QLabel *hintText = new QLabel("Создайте профиль, подтвердите телефон и возвращайтесь ко входу.", hintCard);
    hintText->setObjectName("AuthSubtitle");
    hintText->setWordWrap(true);

    hintLay->addWidget(hintTitle);
    hintLay->addWidget(hintText);
    layout->addWidget(hintCard);

    QHBoxLayout *btnLayout = new QHBoxLayout();

    QPushButton *regBtn = new QPushButton("Регистрация", card);
    regBtn->setObjectName("AuthSecondary");
    regBtn->setCursor(Qt::PointingHandCursor);

    QPushButton *exitBtn = new QPushButton("Выход", card);
    exitBtn->setObjectName("AuthSecondary");
    exitBtn->setCursor(Qt::PointingHandCursor);

    QPushButton *loginBtn = new QPushButton("Войти", card);
    loginBtn->setObjectName("AuthPrimary");
    loginBtn->setCursor(Qt::PointingHandCursor);

    btnLayout->addWidget(exitBtn);
    btnLayout->addWidget(regBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(loginBtn);
    layout->addLayout(btnLayout);

    loginBtn->setDefault(false);
    loginBtn->setAutoDefault(false);
    regBtn->setDefault(false);
    regBtn->setAutoDefault(false);
    exitBtn->setDefault(false);
    exitBtn->setAutoDefault(false);
    closeBtn->setDefault(false);
    closeBtn->setAutoDefault(false);
    minBtn->setDefault(false);
    minBtn->setAutoDefault(false);

    connect(minBtn, &QPushButton::clicked, this, &QDialog::showMinimized);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(exitBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(loginBtn, &QPushButton::clicked, this, &LoginDialog::onLogin);
    connect(regBtn, &QPushButton::clicked, this, &LoginDialog::onRegister);
    connect(m_password, &QLineEdit::returnPressed, this, &LoginDialog::onLogin);
    connect(m_login, &QLineEdit::returnPressed, this, &LoginDialog::onLogin);

    connect(m_showPassword, &QCheckBox::toggled, this, [this](bool checked) {
      m_password->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });
  }

  QString username() const
  {
    return m_username;
  }

  bool isAdmin() const
  {
    return m_isAdmin;
  }

protected:
  void keyPressEvent(QKeyEvent *event) override
  {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
      onLogin();
      event->accept();
      return;
    }

    AuthBaseDialog::keyPressEvent(event);
  }

private slots:
  void onLogin()
  {
    m_statusLabel->clear();

    QString user = m_login->text().trimmed();
    QString password = m_password->text();

    if (user.isEmpty() || password.isEmpty()) {
      setStatus("Введите логин и пароль.", false);
      return;
    }

    QString err;
    if (!ApiClient::instance().login(user, password, &err)) {
      setStatus(err.isEmpty() ? "Ошибка входа." : err, false);
      return;
    }

    m_username = ApiClient::instance().username();
    m_isAdmin = ApiClient::instance().isAdmin();

    accept();
  }

  void onRegister()
  {
    // The login dialog is not hidden with hide(), because that would finish QDialog::exec().
    // Instead, the dialog is temporarily made transparent and disabled.
    RegistrationDialog dlg(nullptr);

    dlg.move(this->geometry().center().x() - dlg.width() / 2,
         this->geometry().center().y() - dlg.height() / 2 - 35);

    const qreal oldOpacity = this->windowOpacity();

    this->setEnabled(false);
    this->setWindowOpacity(0.0);  // The dialog stays active for QDialog::exec(), but is not visible.

    const int result = dlg.exec();

    // Restore the login dialog after registration is finished.
    this->setWindowOpacity(oldOpacity <= 0.0 ? 1.0 : oldOpacity);
    this->setEnabled(true);
    this->showNormal();
    this->raise();
    this->activateWindow();

    if (result == QDialog::Accepted) {
      m_login->setText(dlg.email());
      m_password->clear();
      m_password->setFocus();
      setStatus("Регистрация успешна. Введите пароль и войдите.", true);
    } else {
      setStatus(QString(), false);
      m_login->setFocus();
    }
  }

private:
  QLineEdit *m_login = nullptr;
  QLineEdit *m_password = nullptr;
  QCheckBox *m_showPassword = nullptr;
  QLabel *m_statusLabel = nullptr;

  QString m_username;
  bool m_isAdmin = false;

  void setStatus(const QString &text, bool ok)
  {
    m_statusLabel->setText(text);
    m_statusLabel->setObjectName(ok ? "AuthStatusOk" : "AuthStatus");
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
    keepAuthDialogInFrontQueued();
  }
};
