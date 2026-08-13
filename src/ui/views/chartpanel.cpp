#include "views/chartpanel.h"

#include "database/databasemanager.h"
#include "utils/terminal_status.h"

#include <QChart>
#include <QChartView>
#include <QHBoxLayout>
#include <QPainter>
#include <QSqlQuery>
#include <QValueAxis>
#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QPieSeries>

ChartPanel::ChartPanel(QWidget* parent) : QGroupBox(parent)
{
    setTitle("Аналитика");
    setStyleSheet("QGroupBox { font-size: 14px; font-weight: bold; color: #CCCCCC; "
                  "border: 1px solid #3C3C3C; border-radius: 8px; margin-top: 8px; padding-top: 18px; }"
                  "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }");

    auto* layout = new QHBoxLayout(this);

    auto* pieChart = new QChart();
    pieChart->setTheme(QChart::ChartThemeDark);
    pieChart->setAnimationOptions(QChart::SeriesAnimations);
    pieChart->legend()->setAlignment(Qt::AlignBottom);
    m_statusView = new QChartView(pieChart, this);
    m_statusView->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(m_statusView);

    auto* barChart = new QChart();
    barChart->setTheme(QChart::ChartThemeDark);
    barChart->setAnimationOptions(QChart::SeriesAnimations);
    barChart->legend()->setAlignment(Qt::AlignBottom);
    m_revenueView = new QChartView(barChart, this);
    m_revenueView->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(m_revenueView);
}

void ChartPanel::refresh()
{
    rebuildPie();
    rebuildBar();
}

void ChartPanel::applyDarkTheme(bool dark)
{
    if (m_statusView && m_statusView->chart())
        m_statusView->chart()->setTheme(dark ? QChart::ChartThemeDark : QChart::ChartThemeLight);
    if (m_revenueView && m_revenueView->chart())
        m_revenueView->chart()->setTheme(dark ? QChart::ChartThemeDark : QChart::ChartThemeLight);
}

void ChartPanel::rebuildPie()
{
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    auto* pieChart = qobject_cast<QChart*>(m_statusView->chart());
    if (!pieChart)
        return;

    QString signature;
    if (query.exec("SELECT " + TerminalStatus::sqlCaseExpression("status") +
                   " AS status_name, COUNT(*) FROM tblterminals GROUP BY status ORDER BY status")) {
        while (query.next())
            signature += query.value(0).toString() + "=" + query.value(1).toString() + ";";
    }
    if (signature == m_signature.value("pie"))
        return;

    m_signature["pie"] = signature;
    pieChart->removeAllSeries();
    auto* pieSeries = new QPieSeries();
    if (query.exec("SELECT " + TerminalStatus::sqlCaseExpression("status") +
                   " AS status_name, COUNT(*) FROM tblterminals GROUP BY status ORDER BY status")) {
        while (query.next())
            pieSeries->append(query.value(0).toString(), query.value(1).toInt());
    }
    pieChart->addSeries(pieSeries);
}

void ChartPanel::rebuildBar()
{
    QSqlQuery query(DatabaseManager::instance().getDatabase());
    auto* barChart = qobject_cast<QChart*>(m_revenueView->chart());
    if (!barChart)
        return;

    QString signature;
    QStringList categories;
    QList<double> totals;
    if (query.exec("SELECT to_char(periodyear || '-' || LPAD(periodmonth::text, 2, '0'), 'YYYY-MM') AS month, "
                   "COALESCE(SUM(amount), 0) AS total "
                   "FROM tblpayments "
                   "WHERE (periodyear * 12 + periodmonth) >= (EXTRACT(YEAR FROM CURRENT_DATE) * 12 + EXTRACT(MONTH "
                   "FROM CURRENT_DATE) - 5) "
                   "GROUP BY periodyear, periodmonth ORDER BY periodyear, periodmonth")) {
        while (query.next()) {
            categories << query.value(0).toString();
            totals << query.value(1).toDouble();
            signature += query.value(0).toString() + "=" + query.value(1).toString() + ";";
        }
    }
    if (signature == m_signature.value("bar"))
        return;

    m_signature["bar"] = signature;
    barChart->removeAllSeries();
    const auto oldAxes = barChart->axes();
    for (auto* axis : oldAxes)
        barChart->removeAxis(axis);

    auto* barSet = new QBarSet("Оплаты");
    barSet->setColor("#1976D2");
    for (double total : totals)
        *barSet << total;

    auto* barSeries = new QBarSeries();
    barSeries->append(barSet);
    barChart->addSeries(barSeries);

    auto* axisX = new QBarCategoryAxis();
    axisX->append(categories);
    barChart->addAxis(axisX, Qt::AlignBottom);
    barSeries->attachAxis(axisX);

    auto* axisY = new QValueAxis();
    axisY->setTitleText("Сумма, руб.");
    barChart->addAxis(axisY, Qt::AlignLeft);
    barSeries->attachAxis(axisY);
}