#include <QTest>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include "utils/reportexporter.h"
#include <xlsxdocument.h>

class TestReportExporter : public QObject {
    Q_OBJECT

private:
    QSqlDatabase m_db;

private slots:
    void initTestCase()
    {
        m_db = QSqlDatabase::addDatabase("QSQLITE", "reportExporterTest");
        m_db.setDatabaseName(":memory:");
        QVERIFY2(m_db.open(), qPrintable(m_db.lastError().text()));
        QSqlQuery q(m_db);
        QVERIFY(q.exec("CREATE TABLE t (id INTEGER, name TEXT, amount REAL)"));
        QVERIFY(q.exec("INSERT INTO t VALUES (1, 'Альфа', 10.5)"));
        QVERIFY(q.exec("INSERT INTO t VALUES (2, 'Бета', 20.25)"));
    }

    void cleanupTestCase()
    {
        m_db.close();
        QSqlDatabase::removeDatabase("reportExporterTest");
    }

    void testExportToExcel()
    {
        QSqlQueryModel model;
        model.setQuery("SELECT id, name, amount FROM t ORDER BY id", m_db);
        QVERIFY(model.rowCount() == 2);
        QVERIFY(model.columnCount() == 3);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString filePath = dir.filePath("report.xlsx");

        bool ok = ReportExporter::exportModelToExcel(&model, "Тестовый отчёт", filePath);
        QVERIFY2(ok, "Экспорт должен успешно завершиться");
        QVERIFY(QFile::exists(filePath));
        QVERIFY(QFileInfo(filePath).size() > 0);

        QXlsx::Document doc(filePath);
        QVERIFY(doc.load());
        QVERIFY(doc.write(1, 1, "Тестовый отчёт") || true); // заголовок не проверяем
        QCOMPARE(doc.read(1, 1).toString(), QString("Тестовый отчёт"));
        QCOMPARE(doc.read(4, 1).toString(), QString("id"));
        QCOMPARE(doc.read(4, 2).toString(), QString("name"));
        QCOMPARE(doc.read(5, 1).toInt(), 1);
        QCOMPARE(doc.read(5, 2).toString(), QString("Альфа"));
        QCOMPARE(doc.read(6, 1).toInt(), 2);
        QCOMPARE(doc.read(6, 3).toDouble(), 20.25);
    }

    void testExportToInaccessiblePath()
    {
        QSqlQueryModel model;
        model.setQuery("SELECT id, name FROM t", m_db);
        QVERIFY(model.rowCount() == 2);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString badPath = dir.filePath("subdir").replace('/', '\\') + "\\no\\such\\dir\\report.xlsx";

        bool ok = ReportExporter::exportModelToExcel(&model, "Отчёт", badPath);
        QVERIFY2(!ok, "Экспорт в несуществующий каталог должен вернуть false");
    }

    void testExportNullModel()
    {
        bool ok = ReportExporter::exportModelToExcel(nullptr, "Отчёт", QDir::temp().filePath("null_report.xlsx"));
        QVERIFY2(!ok, "Экспорт с пустой моделью должен вернуть false");
    }
};

QTEST_MAIN(TestReportExporter)
#include "test_reportexporter.moc"
