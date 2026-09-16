#ifndef CASEREPOSITORY_H
#define CASEREPOSITORY_H

#include <QList>
#include <QPair>
#include <QSqlDatabase>
#include <QSqlQueryModel>
#include <QString>
#include <QVector>

#include "models/casesdocument.h"
#include "models/document.h"

// Доступ к таблицам чехлов (tblcases) и документам по чехлам (поступление,
// установка, списание) без SQL в UI-слое.
class CaseRepository {
public:
    explicit CaseRepository(const QSqlDatabase& db);

    // --- Справочник/склад ---
    int countAll() const;
    int countByStatus(int status) const;
    models::CaseItem loadById(int caseId) const;
    QVector<models::CaseItem> loadByIds(const QList<int>& ids) const;
    // Чехол, установленный на терминале (tblterminals.currentcaseid); invalid, если нет.
    models::CaseItem loadByTerminal(int terminalId) const;
    // Свободные чехлы (status = 0) для выбора в UI.
    QVector<models::CaseItem> loadFreeForSelection() const;
    // Остатки свободных чехлов по типам: пары (casetype, count).
    QVector<QPair<QString, int>> summarizeFree() const;
    // Заполняет модель колонками [Тип чехла, Количество] — свободные остатки.
    void populateFreeCasesSummary(QSqlQueryModel* model) const;
    // Создаёт qty единиц типа caseType (status = 0). true при успехе.
    bool createBatch(const QString& caseType, int qty) const;

    // --- Документ «Поступление чехлов» ---
    models::DocumentHeader loadIncomeHeader(int docId) const;
    QVector<models::CaseIncomeRow> loadIncomeRows(int docId) const;
    bool deleteIncomeDetails(int docId) const;
    bool deleteIncomeHeader(int docId) const;

    // --- Документ «Установка чехлов» ---
    models::DocumentHeader loadInstallHeader(int docId) const;
    QVector<models::CaseInstallRow> loadInstallRows(int docId) const;
    bool deleteInstallDetails(int docId) const;
    bool deleteInstallHeader(int docId) const;

    // --- Документ «Списание чехлов» ---
    models::DocumentHeader loadWriteoffHeader(int docId) const;
    QVector<models::CaseWriteoffRow> loadWriteoffRows(int docId) const;
    bool deleteWriteoffDetails(int docId) const;
    bool deleteWriteoffHeader(int docId) const;

private:
    QSqlDatabase m_db;
};

#endif // CASEREPOSITORY_H