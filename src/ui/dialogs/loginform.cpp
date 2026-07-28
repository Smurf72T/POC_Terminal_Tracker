#include "loginform.h"
#include "ui_loginform.h"
#include "database/databasemanager.h"
#include <QMessageBox>
#include <QSqlQuery>
#include <QCryptographicHash>
#include <QInputDialog>

LoginForm::LoginForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginForm)
{
    ui->setupUi(this);
    setWindowTitle("Вход в POC Terminal Tracker");
    setFixedSize(400, 250);

    connect(ui->lineEditPass, &QLineEdit::returnPressed, this, &LoginForm::on_btnLogin_clicked);
}

LoginForm::~LoginForm()
{
    delete ui;
}

QString LoginForm::getUsername() const { return m_username; }
int LoginForm::getUserId() const { return m_userId; }
QString LoginForm::getRole() const { return m_role; }

void LoginForm::on_btnLogin_clicked()
{
    QString username = ui->lineEditUser->text().trimmed();
    QString password = ui->lineEditPass->text();

    if (username.isEmpty() || password.isEmpty()) {
        ui->labelError->setText("Введите логин и пароль!");
        return;
    }

    QByteArray hash = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex();

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("SELECT user_id, username, display_name, role FROM tbl_users "
                  "WHERE username = :uname AND password_hash = :hash AND is_active = TRUE");
    query.bindValue(":uname", username);
    query.bindValue(":hash", QString(hash));

    if (query.exec() && query.next()) {
        m_userId = query.value(0).toInt();
        m_username = query.value(2).toString();
        m_role = query.value(3).toString();
        accept();
    } else {
        ui->labelError->setText("Неверный логин или пароль!");
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

    if (password.length() < 4) {
        QMessageBox::warning(this, "Ошибка", "Пароль должен содержать минимум 4 символа!");
        return;
    }

    QByteArray hash = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex();

    QSqlQuery query(DatabaseManager::instance().getDatabase());
    query.prepare("INSERT INTO tbl_users (username, display_name, password_hash, role, is_active) "
                  "VALUES (:uname, :dname, :hash, 'user', TRUE)");
    query.bindValue(":uname", username.trimmed());
    query.bindValue(":dname", displayName.trimmed());
    query.bindValue(":hash", QString(hash));

    if (query.exec()) {
        QMessageBox::information(this, "Успех", "Пользователь '" + username.trimmed() + "' зарегистрирован!\nТеперь войдите в систему.");
        ui->lineEditUser->setText(username.trimmed());
        ui->lineEditPass->setFocus();
    } else {
        QMessageBox::warning(this, "Ошибка", "Не удалось создать пользователя.\nВозможно, такой логин уже существует.");
    }
}
