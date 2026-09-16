#include "database/repositories/caserepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariantList>

CaseRepository::CaseRepository(const QSqlDatabase& db) : m_db(db) {}

int CaseRepository::countByStatus(int status) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM tblcases WHERE status = :status");
    query.bindValue(":status", status);
    if (!query.exec() || !query.next())
        return 0;
    return query.value(0).toInt();
}

int CaseRepository::countAll() const
{
    QSqlQuery query(m_db);
    if (!query.exec("SELECT COUNT(*) FROM tblcases") || !query.next())
        return 0;
    return query.value(0).toInt();
}

models::CaseItem CaseRepository::loadById(int caseId) const
{
    models::CaseItem c;
    QSqlQuery query(m_db);
    query.prepare("SELECT c.caseid, c.casetype, c.status, "
                  "COALESCE(c.terminalid, 0), COALESCE(t.serialnumber, ''), COALESCE(c.notes, '') "
                  "FROM tblcases c "
                  "LEFT JOIN tblterminals t ON c.terminalid = t.terminalid "
                  "WHERE c.caseid = :id");
    query.bindValue(":id", caseId);
    if (!query.exec() || !query.next())
        return c;
    c.id = query.value(0).toInt();
    c.caseType = query.value(1).toString();
    c.status = query.value(2).toInt();
    c.terminalId = query.value(3).toInt();
    c.terminalSerial = query.value(4).toString();
    c.notes = query.value(5).toString();
    return c;
}

QVector<models::CaseItem> CaseRepository::loadByIds(const QList<int>& ids) const
{
    QVector<models::CaseItem> result;
    if (ids.isEmpty())
        return result;
    QStringList placeholders;
    QVariantList values;
    for (int i = 0; i < ids.size(); ++i) {
        placeholders << QString(":id%1").arg(i);
        values << ids.at(i);
    }
    QSqlQuery query(m_db);
    query.prepare(QString("SELECT c.caseid, c.casetype, c.status, "
                          "COALESCE(c.terminalid, 0), COALESCE(t.serialnumber, ''), COALESCE(c.notes, '') "
                          "FROM tblcases c "
                          "LEFT JOIN tblterminals t ON c.terminalid = t.terminalid "
                          "WHERE c.caseid IN (%1)")
                      .arg(placeholders.join(", ")));
    for (int i = 0; i < ids.size(); ++i)
        query.bindValue(placeholders.at(i), values.at(i));
    if (!query.exec())
        return result;
    while (query.next()) {
        models::CaseItem c;
        c.id = query.value(0).toInt();
        c.caseType = query.value(1).toString();
        c.status = query.value(2).toInt();
        c.terminalId = query.value(3).toInt();
        c.terminalSerial = query.value(4).toString();
        c.notes = query.value(5).toString();
        result.append(c);
    }
    return result;
}

models::CaseItem CaseRepository::loadByTerminal(int terminalId) const
{
    models::CaseItem c;
    QSqlQuery query(m_db);
    query.prepare("SELECT c.caseid, c.casetype, c.status, "
                  "COALESCE(c.terminalid, 0), COALESCE(t.serialnumber, ''), COALESCE(c.notes, '') "
                  "FROM tblcases c "
                  "JOIN tblterminals t ON c.terminalid = t.terminalid "
                  "WHERE t.terminalid = :tid AND t.currentcaseid = c.caseid LIMIT 1");
    query.bindValue(":tid", terminalId);
    if (!query.exec() || !query.next())
        return c;
    c.id = query.value(0).toInt();
    c.caseType = query.value(1).toString();
    c.status = query.value(2).toInt();
    c.terminalId = query.value(3).toInt();
    c.terminalSerial = query.value(4).toString();
    c.notes = query.value(5).toString();
    return c;
}

QVector<models::CaseItem> CaseRepository::loadFreeForSelection() const
{
    QVector<models::CaseItem> result;
    QSqlQuery query(m_db);
    if (query.exec("SELECT caseid, casetype, status, COALESCE(terminalid, 0), '', COALESCE(notes, '') "
                   "FROM tblcases WHERE status = 0 ORDER BY casetype, caseid")) {
        while (query.next()) {
            models::CaseItem c;
            c.id = query.value(0).toInt();
            c.caseType = query.value(1).toString();
            c.status = query.value(2).toInt();
            c.terminalId = query.value(3).toInt();
            c.terminalSerial = query.value(4).toString();
            c.notes = query.value(5).toString();
            result.append(c);
        }
    }
    return result;
}

QVector<QPair<QString, int>> CaseRepository::summarizeFree() const
{
    QVector<QPair<QString, int>> result;
    QSqlQuery query(m_db);
    if (query.exec("SELECT casetype, COUNT(*) AS cnt FROM tblcases WHERE status = 0 "
                   "GROUP BY casetype ORDER BY casetype")) {
        while (query.next())
            result.append(qMakePair(query.value(0).toString(), query.value(1).toInt()));
    }
    return result;
}

void CaseRepository::populateFreeCasesSummary(QSqlQueryModel* model) const
{
    const QString queryStr = "SELECT casetype AS \"Тип чехла\", COUNT(*) AS \"Количество\" "
                             "FROM tblcases WHERE status = 0 "
                             "GROUP BY casetype ORDER BY casetype";
    model->setQuery(queryStr, m_db);
}

bool CaseRepository::createBatch(const QString& caseType, int qty) const
{
    QSqlQuery insert(m_db);
    insert.prepare("INSERT INTO tblcases (casetype, status) VALUES (:t, 0)");
    insert.bindValue(":t", caseType);
    for (int i = 0; i < qty; ++i) {
        if (!insert.exec())
            return false;
    }
    return true;
}

models::DocumentHeader CaseRepository::loadIncomeHeader(int docId) const
{
    models::DocumentHeader h;
    QSqlQuery query(m_db);
    query.prepare("SELECT docnumber, docdate, comments FROM tblcaseincomedocs WHERE caseincomedocid = :id");
    query.bindValue(":id", docId);
    if (!query.exec() || !query.next())
        return h;
    h.id = docId;
    h.docNumber = query.value(0).toString();
    h.date = query.value(1).toDateTime().date();
    h.comments = query.value(2).toString();
    return h;
}

QVector<models::CaseIncomeRow> CaseRepository::loadIncomeRows(int docId) const
{
    QVector<models::CaseIncomeRow> result;
    QSqlQuery query(m_db);
    query.prepare("SELECT caseincomedetailid, casetype, qty FROM tblcaseincomedetails "
                  "WHERE caseincomedocid = :id ORDER BY caseincomedetailid");
    query.bindValue(":id", docId);
    if (!query.exec())
        return result;
    while (query.next()) {
        models::CaseIncomeRow r;
        r.detailId = query.value(0).toInt();
        r.caseType = query.value(1).toString();
        r.qty = query.value(2).toInt();
        result.append(r);
    }
    return result;
}

bool CaseRepository::deleteIncomeDetails(int docId) const
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM tblcaseincomedetails WHERE caseincomedocid = :id");
    query.bindValue(":id", docId);
    return query.exec();
}

bool CaseRepository::deleteIncomeHeader(int docId) const
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM tblcaseincomedocs WHERE caseincomedocid = :id");
    query.bindValue(":id", docId);
    return query.exec();
}

models::DocumentHeader CaseRepository::loadInstallHeader(int docId) const
{
    models::DocumentHeader h;
    QSqlQuery query(m_db);
    query.prepare("SELECT docnumber, docdate, comments FROM tblcaseinstalldocs WHERE caseinstalldocid = :id");
    query.bindValue(":id", docId);
    if (!query.exec() || !query.next())
        return h;
    h.id = docId;
    h.docNumber = query.value(0).toString();
    h.date = query.value(1).toDateTime().date();
    h.comments = query.value(2).toString();
    return h;
}

QVector<models::CaseInstallRow> CaseRepository::loadInstallRows(int docId) const
{
    QVector<models::CaseInstallRow> result;
    QSqlQuery query(m_db);
    query.prepare("SELECT d.caseinstalldetailid, d.terminalid, t.serialnumber, "
                  "COALESCE(d.caseid, 0), COALESCE(c.casetype, '') "
                  "FROM tblcaseinstalldetails d "
                  "JOIN tblterminals t ON d.terminalid = t.terminalid "
                  "LEFT JOIN tblcases c ON d.caseid = c.caseid "
                  "WHERE d.caseinstalldocid = :docid "
                  "ORDER BY t.serialnumber");
    query.bindValue(":docid", docId);
    if (!query.exec())
        return result;
    while (query.next()) {
        models::CaseInstallRow r;
        r.detailId = query.value(0).toInt();
        r.terminalId = query.value(1).toInt();
        r.terminalSerial = query.value(2).toString();
        r.caseId = query.value(3).toInt();
        r.caseType = query.value(4).toString();
        result.append(r);
    }
    return result;
}

bool CaseRepository::deleteInstallDetails(int docId) const
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM tblcaseinstalldetails WHERE caseinstalldocid = :id");
    query.bindValue(":id", docId);
    return query.exec();
}

bool CaseRepository::deleteInstallHeader(int docId) const
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM tblcaseinstalldocs WHERE caseinstalldocid = :id");
    query.bindValue(":id", docId);
    return query.exec();
}

models::DocumentHeader CaseRepository::loadWriteoffHeader(int docId) const
{
    models::DocumentHeader h;
    QSqlQuery query(m_db);
    query.prepare("SELECT docnumber, docdate, comments FROM tblcasewriteoffdocs WHERE casewriteoffdocid = :id");
    query.bindValue(":id", docId);
    if (!query.exec() || !query.next())
        return h;
    h.id = docId;
    h.docNumber = query.value(0).toString();
    h.date = query.value(1).toDateTime().date();
    h.comments = query.value(2).toString();
    return h;
}

QVector<models::CaseWriteoffRow> CaseRepository::loadWriteoffRows(int docId) const
{
    QVector<models::CaseWriteoffRow> result;
    QSqlQuery query(m_db);
    query.prepare("SELECT d.casewriteoffdetailid, d.caseid, COALESCE(c.casetype, ''), COALESCE(d.reason, '') "
                  "FROM tblcasewriteoffdetails d "
                  "LEFT JOIN tblcases c ON d.caseid = c.caseid "
                  "WHERE d.casewriteoffdocid = :docid "
                  "ORDER BY c.casetype, c.caseid");
    query.bindValue(":docid", docId);
    if (!query.exec())
        return result;
    while (query.next()) {
        models::CaseWriteoffRow r;
        r.detailId = query.value(0).toInt();
        r.caseId = query.value(1).toInt();
        r.caseType = query.value(2).toString();
        r.reason = query.value(3).toString();
        result.append(r);
    }
    return result;
}

bool CaseRepository::deleteWriteoffDetails(int docId) const
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM tblcasewriteoffdetails WHERE casewriteoffdocid = :id");
    query.bindValue(":id", docId);
    return query.exec();
}

bool CaseRepository::deleteWriteoffHeader(int docId) const
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM tblcasewriteoffdocs WHERE casewriteoffdocid = :id");
    query.bindValue(":id", docId);
    return query.exec();
}