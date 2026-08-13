#ifndef DOCUMENTREPOSITORY_H
#define DOCUMENTREPOSITORY_H

#include <QSqlDatabase>
#include <QSqlQueryModel>
#include <QString>
#include <QVector>

// Доступ к таблицам документов (поступление/аренда/возврат/изменение статуса)
// без SQL в UI-слое.
class DocumentRepository {
public:
    explicit DocumentRepository(const QSqlDatabase& db);

    // Типы документов (doctype) совпадают с кодами в recentDocuments:
    // 1 — поступление, 2 — аренда, 3 — возврат, 5 — изменение статуса.
    enum DocType : int { Receipt = 1, Rental = 2, Return = 3, StatusChange = 5 };

    struct RecentDocument {
        int docType = Receipt;
        int docId = 0;
        QString number;
        QString date;
        QString typeName;
    };

    // Последние документы всех типов, сортировка по дате (DESC), лимит.
    QVector<RecentDocument> recentDocuments(int limit = 15) const;
    void populateRecentDocuments(QSqlQueryModel* model, int limit = 15) const;

private:
    QSqlDatabase m_db;
};

#endif // DOCUMENTREPOSITORY_H