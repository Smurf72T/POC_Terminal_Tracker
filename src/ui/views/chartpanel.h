#ifndef CHARTPANEL_H
#define CHARTPANEL_H

#include <QGroupBox>
#include <QMap>
#include <QString>

class QChartView;

// Панель «Аналитика»: круговая диаграмма статусов терминалов и
// столбчатая диаграмма выручки за последние 6 месяцев.
// Перерисовывает серии только при изменении данных (по подписи результатов).
class ChartPanel : public QGroupBox {
    Q_OBJECT

public:
    explicit ChartPanel(QWidget* parent = nullptr);

    void refresh();
    void applyDarkTheme(bool dark);

private slots:
    void rebuildPie();
    void rebuildBar();

private:
    QChartView* m_statusView = nullptr;
    QChartView* m_revenueView = nullptr;
    QMap<QString, QString> m_signature;
};

#endif // CHARTPANEL_H