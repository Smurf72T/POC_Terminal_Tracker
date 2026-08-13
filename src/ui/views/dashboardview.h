#ifndef DASHBOARDVIEW_H
#define DASHBOARDVIEW_H

#include <QObject>
#include <QString>

class QLabel;
class QTableView;
class QTimer;
class QSqlQueryModel;
class QModelIndex;

class ChartPanel;

namespace Ui {
class MainWindow;
}

// Управляет содержимым дашборда в centralwidget главного окна:
// счётчики, таблицы «Терминалы в аренде по клиентам» и «Последние документы»,
// панель «Аналитика» (ChartPanel) и периодическое автообновление.
// Подключает сигнал dataChanged от DatabaseManager и сообщает о действиях
// пользователя (двойной клик) сигналами, чтобы MainWindow оставался тонким.
class DashboardView : public QObject {
    Q_OBJECT

public:
    explicit DashboardView(Ui::MainWindow* ui, QWidget* centralWidget, QObject* parent = nullptr);

    void refreshAll();
    void refreshCharts();

    void applyDarkTheme(bool dark);

    int recentDocTypeAt(const QModelIndex& index) const;
    int recentDocIdAt(const QModelIndex& index) const;
    int topClientIdAt(const QModelIndex& index) const;
    QString topClientNameAt(const QModelIndex& index) const;

signals:
    void recentDocActivated(int docType, int docId);
    void topClientActivated(int clientId, const QString& clientName);

private slots:
    void onDatabaseDataChanged();
    void onRecentDocDoubleClicked(const QModelIndex& index);
    void onTopClientDoubleClicked(const QModelIndex& index);

private:
    void setupTables();
    void setupCharts(QWidget* centralWidget);
    void loadCounters();
    void loadTopClients();
    void loadRecentDocuments();
    void updateLastUpdateLabel();
    void updateCounterWidget(QLabel* valueLabel, QLabel* nameLabel, const QString& value, const QString& label,
                             const QString& color);

    Ui::MainWindow* m_ui = nullptr;
    ChartPanel* m_charts = nullptr;
    QSqlQueryModel* m_topClientsModel = nullptr;
    QSqlQueryModel* m_recentDocsModel = nullptr;
    QTimer* m_refreshTimer = nullptr;
};

#endif // DASHBOARDVIEW_H