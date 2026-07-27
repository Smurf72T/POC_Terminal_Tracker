#ifndef DASHBOARDFORM_H
#define DASHBOARDFORM_H

#include <QWidget>
#include <QSqlQueryModel>
#include <QTimer>

namespace Ui {
    class DashboardForm;
}

class DashboardForm : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardForm(QWidget *parent = nullptr);
    ~DashboardForm();

private slots:
    void on_btnRefresh_clicked();
    void on_btnClose_clicked();
    void autoRefresh();
    void onDataChanged();

private:
    Ui::DashboardForm *ui;
    QSqlQueryModel *topClientsModel;
    QSqlQueryModel *recentDocsModel;
    QTimer *refreshTimer;

    void setupUI();
    void loadCounters();
    void loadTopClients();
    void loadRecentDocuments();
    void updateCounterWidget(QWidget* widget, const QString& value, const QString& label, const QString& color);
};

#endif // DASHBOARDFORM_H