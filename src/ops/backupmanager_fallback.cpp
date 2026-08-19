#include "backupmanager.h"

#include "utils/logging.h"

#include <QDateTime>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTextStream>
#include <QVariant>

namespace {

QString escapeSqlLiteral(const QString& value)
{
    QString escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace("'", "''");
    return "'" + escaped + "'";
}

QString formatSqlValue(const QVariant& val)
{
    if (val.isNull())
        return "NULL";

    switch (static_cast<QMetaType::Type>(val.typeId())) {
        case QMetaType::QDateTime:
            return escapeSqlLiteral(val.toDateTime().toString(Qt::ISODateWithMs));
        case QMetaType::QDate:
            return escapeSqlLiteral(val.toDate().toString(Qt::ISODate));
        case QMetaType::QTime:
            return escapeSqlLiteral(val.toTime().toString("HH:mm:ss"));
        case QMetaType::Bool:
            return val.toBool() ? "TRUE" : "FALSE";
        case QMetaType::QByteArray:
            return "'\\x" + QString::fromLatin1(val.toByteArray().toHex()) + "'";
        case QMetaType::Double:
            return QString::number(val.toDouble(), 'g', 17);
        case QMetaType::QString:
        case QMetaType::Char:
        case QMetaType::QChar:
        case QMetaType::QStringList:
        case QMetaType::QJsonObject:
        case QMetaType::QJsonArray:
        case QMetaType::QJsonValue:
            return escapeSqlLiteral(val.toString());
        default:
            return val.toString();
    }
}

} // namespace

bool BackupManager::createFallbackBackup(const QSqlDatabase& db, const QString& filePath, const QString& dbname,
                                         QString* error, std::atomic<bool>* cancelRequested)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = "Не удалось создать файл резервной копии: " + filePath;
        return false;
    }

    QTextStream out(&file);

    out << "-- Резервная копия БД " << dbname << "\n";
    out << "-- Создана: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
    out << "-- Восстановление: psql -U <user> -d <dbname> -f <file>\n\n";

    auto cancelled = [cancelRequested]() { return cancelRequested && cancelRequested->load(); };

    QSqlQuery tableQuery(db);
    if (!tableQuery.exec("SELECT tablename FROM pg_tables WHERE schemaname = 'public' ORDER BY tablename")) {
        if (error)
            *error = "Не удалось получить список таблиц: " + tableQuery.lastError().text();
        qCWarning(logSQL) << "Failed to list tables:" << tableQuery.lastError().text();
        return false;
    }
    QStringList tables;
    while (tableQuery.next())
        tables.append(tableQuery.value(0).toString());

    out << "-- 1. Удаление старых таблиц\n";
    for (const QString& table : tables)
        out << "DROP TABLE IF EXISTS \"" << table << "\" CASCADE;\n";
    out << "\n";

    out << "-- 2. Последовательности (после DROP, т.к. owned-последовательности удаляются вместе с таблицей)\n";
    QSqlQuery seqQuery(db);
    if (seqQuery.exec("SELECT schemaname, sequencename FROM pg_sequences "
                      "WHERE schemaname = 'public' ORDER BY sequencename")) {
        while (seqQuery.next()) {
            QString seq = seqQuery.value(1).toString();
            QSqlQuery stateQuery(db);
            if (stateQuery.exec("SELECT last_value, is_called FROM \"" + seq + "\"") && stateQuery.next()) {
                qint64 lastValue = stateQuery.value(0).toLongLong();
                bool isCalled = stateQuery.value(1).toBool();
                out << "CREATE SEQUENCE IF NOT EXISTS \"" << seq << "\" START 1;\n";
                out << "SELECT setval('" << seq << "', " << lastValue << ", " << (isCalled ? "TRUE" : "FALSE")
                    << ");\n";
            }
        }
    } else {
        qCWarning(logSQL) << "Failed to list sequences:" << seqQuery.lastError().text();
    }
    out << "\n";

    out << "-- 3. Создание таблиц (FK добавляются в конце)\n";
    for (const QString& table : tables) {
        if (cancelled()) {
            file.close();
            QFile::remove(filePath);
            if (error)
                *error = "Операция отменена пользователем";
            return false;
        }
        QSqlQuery colQuery(db);
        colQuery.prepare("SELECT column_name, data_type, is_nullable, column_default "
                         "FROM information_schema.columns "
                         "WHERE table_schema = 'public' AND table_name = :tbl "
                         "ORDER BY ordinal_position");
        colQuery.bindValue(":tbl", table);
        if (!colQuery.exec()) {
            qCWarning(logSQL) << "Failed to load columns for table" << table << ":" << colQuery.lastError().text();
            continue;
        }

        QStringList columnDefs;
        while (colQuery.next()) {
            QString name = colQuery.value(0).toString();
            QString type = colQuery.value(1).toString();
            QString nullable = colQuery.value(2).toString();
            QString defaultValue = colQuery.value(3).toString();
            if (type.toUpper().startsWith("INT"))
                type = "INTEGER";
            QString def = "    \"" + name + "\" " + type + (nullable == "YES" ? " NULL" : " NOT NULL");
            if (!defaultValue.isEmpty())
                def += " DEFAULT " + defaultValue;
            columnDefs.append(def);
        }
        out << "CREATE TABLE \"" << table << "\" (\n" << columnDefs.join(",\n") << "\n);\n\n";
    }

    out << "-- 4. Данные\n";
    for (const QString& table : tables) {
        if (cancelled()) {
            file.close();
            QFile::remove(filePath);
            if (error)
                *error = "Операция отменена пользователем";
            return false;
        }
        QSqlQuery colQuery(db);
        colQuery.prepare("SELECT column_name FROM information_schema.columns "
                         "WHERE table_schema = 'public' AND table_name = :tbl "
                         "ORDER BY ordinal_position");
        colQuery.bindValue(":tbl", table);
        QStringList columnNames;
        if (colQuery.exec()) {
            while (colQuery.next())
                columnNames.append("\"" + colQuery.value(0).toString() + "\"");
        }
        if (columnNames.isEmpty()) {
            qCWarning(logSQL) << "No columns for table" << table;
            continue;
        }

        QSqlQuery dataQuery(db);
        dataQuery.prepare(QString("SELECT * FROM \"%1\"").arg(table));
        if (!dataQuery.exec()) {
            qCWarning(logSQL) << "Failed to load data from table" << table << ":" << dataQuery.lastError().text();
            continue;
        }

        while (dataQuery.next()) {
            out << "INSERT INTO \"" << table << "\" (" << columnNames.join(", ") << ") VALUES (";
            for (int i = 0; i < dataQuery.record().count(); ++i) {
                if (i > 0)
                    out << ", ";
                out << formatSqlValue(dataQuery.value(i));
            }
            out << ");\n";
        }
        out << "\n";
    }

    out << "-- 5. Функции\n";
    QSqlQuery funcQuery(db);
    if (funcQuery.exec("SELECT pg_get_functiondef(p.oid) FROM pg_proc p "
                       "JOIN pg_namespace n ON n.oid = p.pronamespace "
                       "WHERE n.nspname = 'public' ORDER BY p.oid")) {
        while (funcQuery.next())
            out << funcQuery.value(0).toString() << "\n";
    }
    out << "\n";

    out << "-- 6. Триггеры\n";
    QSqlQuery triggerQuery(db);
    if (triggerQuery.exec("SELECT pg_get_triggerdef(t.oid) FROM pg_trigger t "
                          "JOIN pg_class c ON c.oid = t.tgrelid "
                          "JOIN pg_namespace n ON n.oid = c.relnamespace "
                          "WHERE NOT t.tgisinternal AND n.nspname = 'public'")) {
        while (triggerQuery.next())
            out << triggerQuery.value(0).toString() << ";\n";
    }
    out << "\n";

    out << "-- 7. Индексы (индексы ограничений создаются вместе с ограничениями)\n";
    QSqlQuery indexQuery(db);
    if (indexQuery.exec("SELECT pg_get_indexdef(i.indexrelid) FROM pg_index i "
                        "JOIN pg_class c ON c.oid = i.indexrelid "
                        "JOIN pg_namespace n ON n.oid = c.relnamespace "
                        "LEFT JOIN pg_constraint con ON con.conindid = i.indexrelid "
                        "WHERE n.nspname = 'public' AND con.oid IS NULL "
                        "ORDER BY c.relname")) {
        while (indexQuery.next())
            out << indexQuery.value(0).toString() << ";\n";
    }
    out << "\n";

    out << "-- 8. Ограничения (FK в конце)\n";
    QSqlQuery conQuery(db);
    if (conQuery.exec("SELECT c.conrelid::regclass::text, c.conname, pg_get_constraintdef(c.oid) "
                      "FROM pg_constraint c WHERE c.connamespace = 'public'::regnamespace "
                      "AND c.contype IN ('p','u','c','f') "
                      "ORDER BY (c.contype = 'f'), c.conname")) {
        while (conQuery.next()) {
            out << "ALTER TABLE " << conQuery.value(0).toString() << " ADD CONSTRAINT " << conQuery.value(1).toString()
                << " " << conQuery.value(2).toString() << ";\n";
        }
    }
    out << "\n";

    file.close();
    return true;
}