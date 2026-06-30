#pragma once

/*
  File: src/dialogs/MyOrdersDialog.h
  Purpose: User order-history dialog with automatic status refresh.

  Developer notes:
  - This comment block is documentation only; it does not affect compilation.
  - Keep business rules close to the module that owns the data.
  - Prefer small helper functions instead of duplicating SQL, UI, or network logic.
*/
#include "../api/ApiClient.h"

class MyOrdersDialog : public QDialog
{
public:
  MyOrdersDialog(const QString &username, QWidget *parent = nullptr)
    : QDialog(parent)
    , m_username(username)
  {
    setWindowTitle("Мои заказы");
    resize(700, 400);

    QVBoxLayout *root = new QVBoxLayout(this);

    QLabel *title = new QLabel("История заказов пользователя: " + m_username, this);
    QFont f = title->font();
    f.setPointSize(12);
    f.setBold(true);
    title->setFont(f);
    root->addWidget(title);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    QStringList headers;
    headers << "№ заказа" << "Дата" << "Статус" << "Кол-во" << "Оплата" << "Сумма, ₽";
    m_table->setHorizontalHeaderLabels(headers);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    root->addWidget(m_table);

    // Automatically refresh order statuses.
    QTimer *timer = new QTimer(this);
    timer->setInterval(3000);
    connect(timer, &QTimer::timeout, this, [this]() { loadOrders(); });
    timer->start();

    QHBoxLayout *btns = new QHBoxLayout();
    QPushButton *refreshBtn = new QPushButton("Обновить", this);

    QPushButton *closeBtn = new QPushButton("Закрыть", this);
    btns->addWidget(refreshBtn);
    btns->addStretch();
    btns->addWidget(closeBtn);
    root->addLayout(btns);

    connect(refreshBtn, &QPushButton::clicked, this, [this]() { loadOrders(); });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    loadOrders();
  }

private:
  QString    m_username;
  QTableWidget *m_table = nullptr;

  static QString normalizeOrderStatus(QString s)
  {
    s = s.trimmed().toLower();
    s.replace("ё", "е");
    return s;
  }

  static QStringList orderStatusVisual(const QString &status)
  {
    const QString n = normalizeOrderStatus(status);
    if (n == "оформлен") return {"Оформлен", "#EFE7DF", "#6B5444", "#D8C9BE"};
    if (n == "оплачен") return {"Оплачен", "#E5F4E8", "#2E7D46", "#B7DFC0"};
    if (n.contains("формир")) return {"Формируется", "#FFF1D7", "#9A641F", "#F0D09B"};
    if (n == "собран") return {"Собран", "#EAE6FF", "#5B4BB2", "#C9C1F4"};
    if (n.contains("ожидает отправ")) return {"Ожидает отправки", "#E5EDFF", "#325EA8", "#BFD0F5"};
    if (n.contains("в пути") || n.contains("отправ")) return {"В пути", "#DCEEFF", "#2D6FA8", "#BBD8F3"};
    if (n.contains("достав")) return {"Доставлен", "#DDF4E3", "#2E7D46", "#B7DFC0"};
    if (n.contains("отмен")) return {"Отменён", "#F9DCDD", "#B64B53", "#E8B5BB"};
    if (n.contains("возврат")) return {"↩ Возврат", "#F4E1F4", "#8C4A86", "#D8B7D8"};
    return {"◔ В обработке", "#EEE7E1", "#5F5750", "#D8CBC0"};
  }

  QWidget *makeStatusChip(const QString &status)
  {
    const QStringList st = orderStatusVisual(status);
    QLabel *lab = new QLabel(st.value(0), m_table);
    lab->setAlignment(Qt::AlignCenter);
    lab->setMinimumHeight(28);
    lab->setStyleSheet(QString("background:%1;color:%2;border:1px solid %3;border-radius:10px;padding:0 12px;font-weight:800;").arg(st.value(1), st.value(2), st.value(3)));
    QWidget *wrap = new QWidget(m_table);
    QHBoxLayout *lay = new QHBoxLayout(wrap);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(lab, 0, Qt::AlignLeft | Qt::AlignVCenter);
    lay->addStretch();
    return wrap;
  }

  void loadOrders()
  {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen())
      return;

    QSqlQuery q(db);
    q.prepare("SELECT id, created_at, status, items_count, payment_method, total FROM orders "
        "WHERE username=:u ORDER BY id DESC;");
    q.bindValue(":u", m_username);

    if (!q.exec()) {
      qDebug() << "Load orders error:" << q.lastError().text();
      return;
    }

    m_table->setRowCount(0);

    int row = 0;
    while (q.next()) {
      m_table->insertRow(row);
      m_table->setItem(row, 0, new QTableWidgetItem(q.value(0).toString())); // number
      m_table->setItem(row, 1, new QTableWidgetItem(q.value(1).toString())); // date
      m_table->setCellWidget(row, 2, makeStatusChip(q.value(2).toString())); // status
      m_table->setItem(row, 3, new QTableWidgetItem(q.value(3).toString())); // quantity
      m_table->setItem(row, 4, new QTableWidgetItem(q.value(4).toString())); // payment
      m_table->setItem(row, 5, new QTableWidgetItem(
                 QString::number(q.value(5).toDouble(), 'f', 2))); // total amount
      ++row;
    }
  }
};

// ================== Advanced recommendation flow ==================
