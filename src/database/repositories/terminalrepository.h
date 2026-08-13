#ifndef TERMINALREPOSITORY_H
#define TERMINALREPOSITORY_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QString>
#include <QStringList>
#include <QVector>

#include <utility>

// Доступ к данным таблицы tblterminals без SQL в UI-слое.
// Методы с префиксом `load` возвращают готовые структуры данных,
// методы с префиксом `populate` заполняют переданную модель.
class TerminalRepository {
public:
    explicit TerminalRepository(const QSqlDatabase& db);

    struct StatusCount {
        int status = 0;
        int count = 0;
    };

    struct FreeTerminal {
        QString serialNumber;
        QString modelName;
        QString simStatus;
    };

    int countAll() const;
    int countByStatus(int status) const;
    QVector<StatusCount> statusCounts() const;
    QVector<FreeTerminal> loadFreeTerminals() const;

    // id по серийному номеру; -1, если не найден.
    int findIdBySerial(const QString& serialNumber) const;
    // Пары (serial, id) для выбора терминала в UI.
    QVector<QPair<QString, int>> loadSerialsWithIds() const;

    // Заполняет модель колонками [serialnumber, modelname, simstatus].
    void populateFreeTerminals(QSqlQueryModel* model) const;

private:
    QSqlQuery makeQuery() const;

    QSqlDatabase m_db;
};

#endif // TERMINALREPOSITORY_H