#include "views/chartpanel.h"

#include "database/databasemanager.h"
#include "database/repositories/paymentrepository.h"
#include "database/repositories/terminalrepository.h"
#include "utils/terminal_status.h"

#include <QChart>
#include <QChartView>
#include <QHBoxLayout>
#include <QPainter>
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
    auto* pieChart = qobject_cast<QChart*>(m_statusView->chart());
    if (!pieChart)
        return;

    TerminalRepository repo(DatabaseManager::instance().getDatabase());
    const auto counts = repo.statusCounts();

    QString signature;
    for (const auto& c : counts)
        signature += TerminalStatus::name(c.status) + "=" + QString::number(c.count) + ";";
    if (signature == m_signature.value("pie"))
        return;

    m_signature["pie"] = signature;
    pieChart->removeAllSeries();
    auto* pieSeries = new QPieSeries();
    for (const auto& c : counts)
        pieSeries->append(TerminalStatus::name(c.status), c.count);
    pieChart->addSeries(pieSeries);
}

void ChartPanel::rebuildBar()
{
    auto* barChart = qobject_cast<QChart*>(m_revenueView->chart());
    if (!barChart)
        return;

    PaymentRepository repo(DatabaseManager::instance().getDatabase());
    const auto revenues = repo.revenueByMonth(6);

    QString signature;
    QStringList categories;
    QList<double> totals;
    for (const auto& r : revenues) {
        categories << r.month;
        totals << r.total;
        signature += r.month + "=" + QString::number(r.total) + ";";
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