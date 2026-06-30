#pragma once

/*
  File: src/dialogs/PaymentDialog.h
  Purpose: Simple standalone card payment dialog used by older checkout flows.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "../payment/LocalPayServer.h"

class PaymentDialog : public QDialog
{
public:
  QString address() const { return m_addressEdit->toPlainText().trimmed(); }
  QString paymentMethod() const { return m_paymentMethod; }

  explicit PaymentDialog(int totalCents, QWidget *parent = nullptr)
    : QDialog(parent), m_totalCents(totalCents)
  {
    setWindowTitle("Оплата по QR");
    setModal(true);
    resize(560, 620);

    auto *mainL = new QVBoxLayout(this);

    // ===== Recipient data for demo payment =====
    // Payment requisites are used to build the transfer link.
    m_requisiteNumber = QString::fromLocal8Bit(qgetenv("PAYMENT_REQUISITE_NUMBER"));
    m_bankCode = QString::fromLocal8Bit(qgetenv("PAYMENT_BANK_CODE"));

    if (m_requisiteNumber.isEmpty())
      m_requisiteNumber = "70000000000";
    if (m_bankCode.isEmpty())
      m_bankCode = "100000000000";

    m_baseUrl = QString("https://www.sberbank.ru/ru/choise_bank?requisiteNumber=%1&bankCode=%2")
            .arg(m_requisiteNumber, m_bankCode);

    // Amount.
    const double rub = m_totalCents / 100.0;

    QLabel *sumLbl = new QLabel(this);
    sumLbl->setText(QString("<b>Сумма к оплате:</b> %1 ₽")
              .arg(QString::number(rub, 'f', 2)));
    mainL->addWidget(sumLbl);

    // Show recipient requisites to the user.
    QLabel *toLbl = new QLabel(this);
    toLbl->setWordWrap(true);
    toLbl->setText(QString("<b>Получатель:</b> +%1<br><b>BankCode:</b> %2")
              .arg(m_requisiteNumber, m_bankCode));
    mainL->addWidget(toLbl);

    // ===== QR code from a file located next to the executable =====
    m_qrLabel = new QLabel(this);
    m_qrLabel->setAlignment(Qt::AlignCenter);
    m_qrLabel->setMinimumSize(260, 260);
    m_qrLabel->setStyleSheet("border:1px solid #ddd; background:#fff; padding:10px;");
    mainL->addWidget(m_qrLabel, 0, Qt::AlignCenter);

    QString qrPath = QCoreApplication::applicationDirPath() + "/qr.png";
    QPixmap px(qrPath);
    if (px.isNull()) {
      m_qrLabel->setText("QR не найден.\nПроверьте, что qr.png лежит рядом с exe-файлом.");
    } else {
      m_qrLabel->setPixmap(px.scaled(300, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    // ===== Payment buttons =====
    auto *btnRow = new QHBoxLayout();

    QPushButton *openBtn = new QPushButton("Открыть перевод", this);
    QPushButton *copyAmt = new QPushButton("Скопировать сумму", this);
    QPushButton *copyLink = new QPushButton("Скопировать ссылку", this);

    btnRow->addWidget(openBtn);
    btnRow->addWidget(copyAmt);
    btnRow->addWidget(copyLink);
    mainL->addLayout(btnRow);

    m_payUrl = m_baseUrl + "&amount=" + QString::number(rub, 'f', 2);

    connect(openBtn, &QPushButton::clicked, this, [this](){
      // Open the transfer link with the specified amount.
      QDesktopServices::openUrl(QUrl(m_payUrl));
    });

    connect(copyAmt, &QPushButton::clicked, this, [this, rub](){
      QGuiApplication::clipboard()->setText(QString::number(rub, 'f', 2));
      QMessageBox::information(this, "Сумма", "Сумма скопирована в буфер обмена.");
    });

    connect(copyLink, &QPushButton::clicked, this, [this](){
      QGuiApplication::clipboard()->setText(m_baseUrl);
      QMessageBox::information(this, "Ссылка", "Ссылка скопирована в буфер обмена.");
    });

    QLabel *hint = new QLabel(
     "1) Отсканируйте QR или нажмите «Открыть перевод».\n"
     "2) Проверьте сумму. При необходимости вставьте ее из кнопки «Скопировать сумму».\n"
     "3) После перевода отметьте «Я оплатил» и нажмите «Оплатить».", this);
    hint->setWordWrap(true);
    mainL->addWidget(hint);

    // Delivery address.
    QLabel *addrLbl = new QLabel("Адрес доставки:", this);
    mainL->addWidget(addrLbl);

    m_addressEdit = new QTextEdit(this);
    m_addressEdit->setPlaceholderText("Город, улица, дом, квартира...");
    m_addressEdit->setFixedHeight(90);
    mainL->addWidget(m_addressEdit);

    // Confirmation checkbox.
    m_paidCheck = new QCheckBox("Я оплатил (перевод выполнен)", this);
    mainL->addWidget(m_paidCheck);

    // OK/Cancel buttons.
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okBtn = buttons->button(QDialogButtonBox::Ok);
    m_okBtn->setText("Оплатить");
    buttons->button(QDialogButtonBox::Cancel)->setText("Отмена");
    mainL->addWidget(buttons);

    m_okBtn->setEnabled(false);

    connect(m_paidCheck, &QCheckBox::toggled, this, [this](bool on){
      m_okBtn->setEnabled(on);
    });

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this](){
      onPayClicked();
    });
  }

private:
  int m_totalCents = 0;
  QString m_paymentMethod = "Перевод по QR";
  QLabel *m_qrLabel = nullptr;
  QTextEdit *m_addressEdit = nullptr;
  QCheckBox *m_paidCheck = nullptr;
  QPushButton *m_okBtn = nullptr;

  // Data used to build the transfer link.
  QString m_requisiteNumber;
  QString m_bankCode;
  QString m_baseUrl;
  QString m_payUrl;

  void onPayClicked()
  {
    QString addr = m_addressEdit->toPlainText().trimmed();
    if (addr.size() < 10) {
      QMessageBox::warning(this, "Оплата", "Введите адрес доставки (минимум 10 символов).");
      return;
    }
    if (!m_paidCheck->isChecked()) {
      QMessageBox::warning(this, "Оплата", "Отметьте пункт «Я оплатил».");
      return;
    }

    QMessageBox::information(this, "Оплата", "Оплата подтверждена.");
    accept();
  }
};
