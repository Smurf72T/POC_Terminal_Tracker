#ifndef USERMANAGEMENTFORM_H
#define USERMANAGEMENTFORM_H

#include <QDialog>
#include <QSqlQueryModel>
#include <QTableView>
#include <QPushButton>
#include <QComboBox>

class UserManagementForm : public QDialog
{
    Q_OBJECT

public:
    explicit UserManagementForm(QWidget *parent = nullptr);

private slots:
    void onRoleChanged(int row, int userId, const QString &newRole);
    void onToggleActive(int row, int userId, bool currentlyActive);
    void onResetPassword(int userId, const QString &username);
    void refreshTable();

private:
    QTableView *tableView;
    QSqlQueryModel *model;
    QPushButton *btnRefresh;

    void setupUI();
    void loadUsers();
};

#endif // USERMANAGEMENTFORM_H
