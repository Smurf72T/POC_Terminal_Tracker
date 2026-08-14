#ifndef TERMINALREPOSITORY_H
#define TERMINALREPOSITORY_H

#include <QList>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QString>
#include <QStringList>
#include <QVector>

#include <utility>

#include "models/terminal.h"

// Данные для редактирования терминала (UPDATE tblterminals).
struct TerminalUpdate {
    QString serialNumber;
    int modelId = 0;
    QString imei1;
    QString imei2;
    int status = 0;
    QDate purchaseDate;      // invalid — записать NULL
    QString notes;
    bool wasRepaired = false;
    bool deactivated = false;
};

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

    // Полная модель терминала по id (с именем модели); invalid, если нет.
    models::Terminal loadById(int terminalId) const;
    // Терминалы по списку id (порядок вставки сохраняется, отсутствующие пропускаются).
    QVector<models::Terminal> loadByIds(const QList<int>& ids) const;
    // Терминалы для выбора в аренду: свободные и не деактивированные.
    QVector<models::Terminal> loadFreeForSelection() const;

    // Сохраняет изменения существующего терминала. true при успехе.
    bool update(int terminalId, const TerminalUpdate& data) const;

    // Заполняет модель колонками [serialnumber, modelname, simstatus].
    void populateFreeTerminals(QSqlQueryModel* model) const;

private:
    QSqlQuery makeQuery() const;
    // Собирает models::Terminal из текущей строки запроса (общий формат SELECT).
    models::Terminal makeTerminal(const QSqlQuery& query, int startColumn) const;

    QSqlDatabase m_db;
};

#endif // TERMINALREPOSITORY_H