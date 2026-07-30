#include "usermanagementform.h"
#include "database/databasemanager.h"
#include "utils/password_utils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QSqlQuery>
#include <QSqlError>
#include <QLabel>

UserManagementForm::UserManagementForm(QWidget *parent)
    : QDialog(parent)
    , tableView(new QTableView(this))
    , model(new QSqlQueryModel(this))
    , btnRefresh(new QPushButton("Обновить", this))
{
    setupUI();
    loadUsers();
}

void UserManagementForm::setupUI()
{
    setWindowTitle("Управление пользователями");
    resize(900, 500);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *header = new QLabel("Управление пользователями");
    header->setStyleSheet("font-size: 18px; font-weight: bold; color: #FFFFFF; padding: 8px 0;");
    layout->addWidget(header);

    tableView->setModel(model);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setAlternatingRowColors(true);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView->horizontalHeader()->setStretchLastSection(true);
    tableView->verticalHeader()->hide();
    tableView->setSortingEnabled(true);
    layout->addWidget(tableView);

    auto *btnLayout = new QHBoxLayout();
    auto *btnRole = new QPushButton("Сменить роль", this);
    auto *btnActive = new QPushButton("Деактивировать / Активировать", this);
    auto *btnResetPass = new QPushButton("Сбросить пароль", this);
    auto *btnClose = new QPushButton("Закрыть", this);

    btnLayout->addWidget(btnRole);
    btnLayout->addWidget(btnActive);
    btnLayout->addWidget(btnResetPass);
    btnLayout->addWidget(btnRefresh);
    btnLayout->addStretch();
    btnLayout->addWidget(btnClose);
    layout->addLayout(btnLayout);

    connect(btnRole, &QPushButton::clicked, this, [this]() {
        int row = tableView->currentIndex().row();
        if (row < 0) { QMessageBox::warning(this, "Ошибка", "Выберите пользователя."); return; }
        int userId = model->data(model->index(row, 0)).toInt();
        QString currentRole = model->data(model->index(row, 3)).toString();
        QStringList roles = {"admin", "user"};
        bool ok;
        QString newRole = QInputDialog::getItem(this, "Смена роли",
            "Новая роль для " + model->data(model->index(row, 1)).toString() + ":", roles, roles.indexOf(currentRole), false, &ok);
        if (ok && !newRole.isEmpty() && newRole != currentRole) {
            onRoleChanged(row, userId, newRole);
        }
    });

    connect(btnActive, &QPushButton::clicked, this, [this]() {
        int row = tableView->currentIndex().row();
        if (row < 0) { QMessageBox::warning(this, "Ошибка", "Выберите пользователя."); return; }
        int userId = model->data(model->index(row, 0)).toInt();
        bool active = model->data(model->index(row, 4)).toInt() == 1;
        onToggleActive(row, userId, active);
    });

    connect(btnResetPass, &QPushButton::clicked, this, [this]() {
        int row = tableView->currentIndex().row();
        if (row < 0) { QMessageBox::warning(this, "Ошибка", "Выберите пользователя."); return; }
        int userId = model->data(model->index(row, 0)).toInt();
        QString username = model->data(model->index(row, 1)).toString();
        onResetPassword(userId, username);
    });

    connect(btnRefresh, &QPushButton::clicked, this, &UserManagementForm::refreshTable);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::close);
}

void UserManagementForm::loadUsers()
{
    model->setQuery(
        "SELECT user_id AS \"ID\", username AS \"Логин\", "
        "display_name AS \"Отображаемое имя\", "
        "role AS \"Роль\", "
        "CASE WHEN is_active THEN 1 ELSE 0 END AS \"Активен\", "
        "created_at::date AS \"Создан\" "
        "FROM tbl_users ORDER BY username",
        DatabaseManager::instance().getDatabase());

    tableView->hideColumn(0);
    tableView->setColumnWidth(1, 150);
    tableView->setColumnWidth(2, 200);
    tableView->setColumnWidth(3, 80);
    tableView->setColumnWidth(4, 80);
    tableView->setColumnWidth(5, 100);
}

void UserManagementForm::onRoleChanged(int row, int userId, const QString &newRole)
{
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("UPDATE tbl_users SET role = :role WHERE user_id = :id");
    query.bindValue(":role", newRole);
    query.bindValue(":id", userId);
    if (query.exec()) {
        DatabaseManager::instance().logAction("UPDATE", "tbl_users", userId,
            DatabaseManager::instance().getCurrentUser());
        loadUsers();
        QMessageBox::information(this, "Успех", "Роль изменена.");
    } else {
        QMessageBox::critical(this, "Ошибка", query.lastError().text());
    }
}

void UserManagementForm::onToggleActive(int row, int userId, bool currentlyActive)
{
    QString action = currentlyActive ? "деактивирован" : "активирован";
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("UPDATE tbl_users SET is_active = :state WHERE user_id = :id");
    query.bindValue(":state", currentlyActive ? false : true);
    query.bindValue(":id", userId);
    if (query.exec()) {
        DatabaseManager::instance().logAction("UPDATE", "tbl_users", userId,
            DatabaseManager::instance().getCurrentUser());
        loadUsers();
        QMessageBox::information(this, "Успех",
            QString("Пользователь %1.").arg(action));
    } else {
        QMessageBox::critical(this, "Ошибка", query.lastError().text());
    }
}

void UserManagementForm::onResetPassword(int userId, const QString &username)
{
    bool ok;
    QString newPass = QInputDialog::getText(this, "Сброс пароля",
        "Новый пароль для " + username + ":\n(мин. 8 символов, загл. буква, цифра)",
        QLineEdit::Password, QString(), &ok);
    if (!ok || newPass.isEmpty()) return;

    if (newPass.length() < 8) {
        QMessageBox::warning(this, "Ошибка", "Пароль должен быть минимум 8 символов.");
        return;
    }
    bool hasUpper = false, hasDigit = false;
    for (const QChar &c : newPass) {
        if (c.isUpper()) hasUpper = true;
        if (c.isDigit()) hasDigit = true;
    }
    if (!hasUpper || !hasDigit) {
        QMessageBox::warning(this, "Ошибка", "Пароль должен содержать заглавную букву и цифру.");
        return;
    }

    QString saltedHash = hashPassword(newPass);

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("UPDATE tbl_users SET password_hash = :hash WHERE user_id = :id");
    query.bindValue(":hash", saltedHash);
    query.bindValue(":id", userId);
    if (query.exec()) {
        DatabaseManager::instance().logAction("UPDATE", "tbl_users", userId,
            DatabaseManager::instance().getCurrentUser());
        QMessageBox::information(this, "Успех", "Пароль сброшен.");
    } else {
        QMessageBox::critical(this, "Ошибка", query.lastError().text());
    }
}

void UserManagementForm::refreshTable()
{
    loadUsers();
}
