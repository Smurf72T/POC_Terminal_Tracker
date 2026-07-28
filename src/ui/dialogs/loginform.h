#ifndef LOGINFORM_H
#define LOGINFORM_H

#include <QDialog>

namespace Ui {
    class LoginForm;
}

class LoginForm : public QDialog
{
    Q_OBJECT

public:
    explicit LoginForm(QWidget *parent = nullptr);
    ~LoginForm();

    QString getUsername() const;
    int getUserId() const;
    QString getRole() const;

private slots:
    void on_btnLogin_clicked();
    void on_btnCancel_clicked();
    void on_btnRegister_clicked();

private:
    void loadUsers();

private:
    Ui::LoginForm *ui;
    QString m_username;
    int m_userId = 0;
    QString m_role;
};

#endif // LOGINFORM_H
