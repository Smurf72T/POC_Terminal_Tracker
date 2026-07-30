#include "loginform.h"
#include "ui_loginform.h"
#include "database/databasemanager.h"
#include "utils/password_utils.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QInputDialog>
#include <QDateTime>
#include <QRegularExpression>
#include <QApplication>

QMap<QString, int> LoginForm::s_globalFailedAttempts;
QMap<QString, qint64> LoginForm::s_globalLockUntil;

LoginForm::LoginForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginForm)
{
    ui->setupUi(this);
    setWindowTitle("Вход в POC Terminal Tracker");
    setFixedSize(400, 250);

    loadUsers();

    connect(ui->lineEditPass, &QLineEdit::returnPressed, this, &LoginForm::on_btnLogin_clicked);
}

LoginForm::~LoginForm()
{
    delete ui;
}

QString LoginForm::getUsername() const { return m_username; }
int LoginForm::getUserId() const { return m_userId; }
QString LoginForm::getRole() const { return m_role; }

void LoginForm::loadUsers()
{
    ui->comboBoxUser->clear();

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.exec("SELECT username, display_name FROM tbl_users WHERE is_active = TRUE ORDER BY username");

    while (query.next()) {
        QString username = query.value(0).toString();
        QString displayName = query.value(1).toString();
        QString label = displayName.isEmpty() ? username : displayName + " (" + username + ")";
        ui->comboBoxUser->addItem(label, username);
    }

    if (ui->comboBoxUser->count() > 0) {
        ui->lineEditPass->setFocus();
    }
}

void LoginForm::on_btnLogin_clicked()
{
    QString username = ui->comboBoxUser->currentData().toString();
    QString password = ui->lineEditPass->text();

    if (username.isEmpty() || password.isEmpty()) {
        ui->labelError->setText("Выберите пользователя и введите пароль!");
        return;
    }

    // Rate limiting: блокировка после 5 неудачных попыток
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (s_globalLockUntil.contains(username) && now < s_globalLockUntil[username]) {
        int secondsLeft = static_cast<int>((s_globalLockUntil[username] - now) / 1000) + 1;
        ui->labelError->setText(QString("Слишком много попыток. Повторите через %1 с.").arg(secondsLeft));
        return;
    }

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT user_id, username, display_name, role, password_hash FROM tbl_users "
                  "WHERE username = :uname AND is_active = TRUE");
    query.bindValue(":uname", username);

    if (query.exec() && query.next()) {
        QString storedHash = query.value(4).toString();
        if (!checkPassword(password, storedHash)) {
            s_globalFailedAttempts[username]++;
            if (s_globalFailedAttempts[username] >= 5) {
                s_globalLockUntil[username] = QDateTime::currentMSecsSinceEpoch() + 30000;
                s_globalFailedAttempts[username] = 0;
                ui->labelError->setText("Слишком много попыток. Повторите через 30 с.");
            } else {
                ui->labelError->setText(QString("Неверный пароль! Осталось попыток: %1")
                    .arg(5 - s_globalFailedAttempts[username]));
            }
            return;
        }

        // Сброс счётчика при успешном входе
        s_globalFailedAttempts.remove(username);
        s_globalLockUntil.remove(username);

        // Upgrade устаревшего хеша до PBKDF2 при успешном входе
        bool wasOldHash = (storedHash.count(':') != 2);
        if (wasOldHash) {
            QString newHash = hashPassword(password);
            QSqlQuery update(DatabaseManager::instance().getDatabase());
            update.prepare("UPDATE tbl_users SET password_hash = :hash WHERE user_id = :id");
            update.bindValue(":hash", newHash);
            update.bindValue(":id", query.value(0).toInt());
            update.exec();
        }

        m_userId = query.value(0).toInt();
        m_username = query.value(1).toString();
        m_role = query.value(3).toString();

        // Принудительная смена пароля для пользователей со старым хешем
        if (wasOldHash) {
            bool ok;
            QString newPass = QInputDialog::getText(this, "Смена пароля",
                "Необходимо сменить пароль по умолчанию.\n"
                "Новый пароль (мин. 8 символов, заглавная буква, цифра):",
                QLineEdit::Password, QString(), &ok);
            if (ok && !newPass.isEmpty()) {
                if (newPass.length() < 8 || !newPass.contains(QRegularExpression("[A-ZА-Я]")) || !newPass.contains(QRegularExpression("[0-9]"))) {
                    QMessageBox::warning(this, "Ошибка",
                        "Пароль должен быть минимум 8 символов, содержать заглавную букву и цифру.");
                    reject();
                    return;
                }
                QString newHash = hashPassword(newPass);
                QSqlQuery updatePwd(DatabaseManager::instance().getDatabase());
                updatePwd.prepare("UPDATE tbl_users SET password_hash = :hash WHERE user_id = :id");
                updatePwd.bindValue(":hash", newHash);
                updatePwd.bindValue(":id", m_userId);
                if (!updatePwd.exec()) {
                    QMessageBox::critical(this, "Ошибка",
                        "Не удалось обновить пароль: " + updatePwd.lastError().text());
                    reject();
                    return;
                }
            } else {
                reject();
                return;
            }
        }

        accept();
    } else {
        s_globalFailedAttempts[username]++;
        if (s_globalFailedAttempts[username] >= 5) {
            s_globalLockUntil[username] = QDateTime::currentMSecsSinceEpoch() + 30000;
            s_globalFailedAttempts[username] = 0;
            ui->labelError->setText("Слишком много попыток. Повторите через 30 с.");
        } else {
            ui->labelError->setText(QString("Неверный пароль! Осталось попыток: %1")
                .arg(5 - s_globalFailedAttempts[username]));
        }
    }
}

void LoginForm::on_btnCancel_clicked()
{
    reject();
}

void LoginForm::on_btnRegister_clicked()
{
    bool ok;
    QString username = QInputDialog::getText(this, "Регистрация", "Логин:", QLineEdit::Normal, QString(), &ok);
    if (!ok || username.trimmed().isEmpty()) return;

    QString displayName = QInputDialog::getText(this, "Регистрация", "Отображаемое имя:", QLineEdit::Normal, username.trimmed(), &ok);
    if (!ok || displayName.trimmed().isEmpty()) return;

    QString password = QInputDialog::getText(this, "Регистрация", "Пароль:", QLineEdit::Password, QString(), &ok);
    if (!ok || password.isEmpty()) return;

    QString confirmPassword = QInputDialog::getText(this, "Регистрация", "Подтвердите пароль:", QLineEdit::Password, QString(), &ok);
    if (!ok) return;

    if (password != confirmPassword) {
        QMessageBox::warning(this, "Ошибка", "Пароли не совпадают!");
        return;
    }

    if (password.length() < 8) {
        QMessageBox::warning(this, "Ошибка", "Пароль должен содержать минимум 8 символов!");
        return;
    }
    if (!password.contains(QRegularExpression("[A-ZА-Я]"))) {
        QMessageBox::warning(this, "Ошибка", "Пароль должен содержать хотя бы одну заглавную букву!");
        return;
    }
    if (!password.contains(QRegularExpression("[0-9]"))) {
        QMessageBox::warning(this, "Ошибка", "Пароль должен содержать хотя бы одну цифру!");
        return;
    }

    QString storedHash = hashPassword(password);

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("INSERT INTO tbl_users (username, display_name, password_hash, role, is_active) "
                  "VALUES (:uname, :dname, :hash, 'user', TRUE)");
    query.bindValue(":uname", username.trimmed());
    query.bindValue(":dname", displayName.trimmed());
    query.bindValue(":hash", storedHash);

    if (query.exec()) {
        QMessageBox::information(this, "Успех", "Пользователь '" + username.trimmed() + "' зарегистрирован!\nТеперь войдите в систему.");
        loadUsers();
        int idx = ui->comboBoxUser->findData(username.trimmed());
        if (idx >= 0) ui->comboBoxUser->setCurrentIndex(idx);
        ui->lineEditPass->setFocus();
    } else {
        QMessageBox::warning(this, "Ошибка", "Не удалось создать пользователя.\nВозможно, такой логин уже существует.");
    }
}
