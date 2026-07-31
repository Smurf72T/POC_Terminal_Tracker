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
#include <QDebug>

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

    // Rate limiting на уровне БД (переживает перезапуск приложения)
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT user_id, username, display_name, role, password_hash, "
                  "failed_login_attempts, locked_until FROM tbl_users "
                  "WHERE username = :uname AND is_active = TRUE");
    query.bindValue(":uname", username);

    if (query.exec() && query.next()) {
        QDateTime lockedUntil = query.value(6).toDateTime();
        if (lockedUntil.isValid() && lockedUntil > QDateTime::currentDateTime()) {
            int secondsLeft = static_cast<int>(QDateTime::currentDateTime().secsTo(lockedUntil)) + 1;
            ui->labelError->setText(QString("Слишком много попыток. Повторите через %1 с.").arg(secondsLeft));
            return;
        }

        if (lockedUntil.isValid()) {
            // Блокировка истекла — открываем новое окно из 5 попыток
            QSqlQuery reset(DatabaseManager::instance().getDatabase());
            reset.prepare("UPDATE tbl_users SET failed_login_attempts = 0, locked_until = NULL "
                          "WHERE username = :uname");
            reset.bindValue(":uname", username);
            if (!reset.exec()) {
                qWarning() << "Не удалось сбросить блокировку:" << reset.lastError().text();
            }
        }

        QString storedHash = query.value(4).toString();
        if (!checkPassword(password, storedHash)) {
            QSqlQuery upd(DatabaseManager::instance().getDatabase());
            upd.prepare("UPDATE tbl_users SET failed_login_attempts = failed_login_attempts + 1, "
                        "locked_until = CASE WHEN failed_login_attempts + 1 >= 5 "
                        "THEN NOW() + INTERVAL '30 seconds' ELSE locked_until END "
                        "WHERE username = :uname RETURNING failed_login_attempts");
            upd.bindValue(":uname", username);

            int attempts = 5;
            if (upd.exec() && upd.next()) {
                attempts = upd.value(0).toInt();
            }

            if (attempts >= 5) {
                ui->labelError->setText("Слишком много попыток. Повторите через 30 с.");
            } else {
                ui->labelError->setText(QString("Неверный пароль! Осталось попыток: %1")
                    .arg(5 - attempts));
            }
            return;
        }

        // Успешный вход: сбрасываем счётчик и блокировку
        QSqlQuery clear(DatabaseManager::instance().getDatabase());
        clear.prepare("UPDATE tbl_users SET failed_login_attempts = 0, locked_until = NULL "
                      "WHERE username = :uname");
        clear.bindValue(":uname", username);
        if (!clear.exec()) {
            qWarning() << "Не удалось сбросить счётчик попыток:" << clear.lastError().text();
        }

        m_userId = query.value(0).toInt();
        m_username = query.value(1).toString();
        m_role = query.value(3).toString();

        // Принудительная смена пароля для пользователей со старым хешем.
        // Апгрейд хеша выполняем ТОЛЬКО после успешной смены пароля:
        // если пользователь отменит диалог, старый хеш останется в БД,
        // и при следующем входе смена пароля будет запрошена снова.
        bool wasOldHash = (storedHash.count(':') != 2);
        if (wasOldHash) {
            bool ok;
            QString newPass = QInputDialog::getText(this, "Смена пароля",
                "Необходимо сменить пароль по умолчанию.\n"
                "Новый пароль (мин. 8 символов, заглавная буква, цифра):",
                QLineEdit::Password, QString(), &ok);
            if (!ok || newPass.isEmpty()) {
                reject();
                return;
            }
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
        }

        accept();
    } else {
        ui->labelError->setText("Неверный пароль или пользователь не активен.");
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

    // Саморегистрация создаёт неактивную учётную запись:
    // доступ появляется только после активации администратором
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("INSERT INTO tbl_users (username, display_name, password_hash, role, is_active) "
                  "VALUES (:uname, :dname, :hash, 'user', FALSE)");
    query.bindValue(":uname", username.trimmed());
    query.bindValue(":dname", displayName.trimmed());
    query.bindValue(":hash", storedHash);

    if (query.exec()) {
        QMessageBox::information(this, "Успех",
            "Заявка на регистрацию '" + username.trimmed() + "' отправлена.\n"
            "Учётная запись будет активирована администратором.");
        loadUsers();
        ui->lineEditPass->setFocus();
    } else {
        QMessageBox::warning(this, "Ошибка", "Не удалось создать пользователя.\nВозможно, такой логин уже существует.");
    }
}
