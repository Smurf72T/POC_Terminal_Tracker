#ifndef LOGINFORM_H
#define LOGINFORM_H

#include <QDialog>
#include <QVector>

namespace Ui {
class LoginForm;
}

class LoginForm : public QDialog {
    Q_OBJECT

public:
    explicit LoginForm(QWidget* parent = nullptr);
    ~LoginForm();

    QString getUsername() const;
    int getUserId() const;
    QString getRole() const;

    // Ограничение саморегистрации: не более kMaxRegistrations попыток
    // за kRateLimitWindowMs с одного клиента (в памяти).
    static constexpr int kMaxRegistrations = 3;
    static constexpr qint64 kRateLimitWindowMs = 10 * 60 * 1000; // 10 минут

private slots:
    void on_btnLogin_clicked();
    void on_btnCancel_clicked();
    void on_btnRegister_clicked();

private:
    void loadUsers();
    // Регистрация разрешена? Иначе возвращает сообщение ожидания.
    bool registrationAllowed(QString* blockMessage);

private:
    Ui::LoginForm* ui;
    QString m_username;
    int m_userId = 0;
    QString m_role;

    // Метки времени успешных заявок на регистрацию (монотонное время).
    QVector<qint64> m_registerAttempts;
};

#endif // LOGINFORM_H
