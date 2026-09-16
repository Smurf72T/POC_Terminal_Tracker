#ifndef CASESDOCUMENT_H
#define CASESDOCUMENT_H

#include <QString>

// Value-модели учёта чехлов (tblcases и документов по чехлам).
// Наполняются через CaseRepository.
namespace models {

// Единица-чехол на складе/в установке (tblcases).
// status: 0 — на складе, 1 — установлен на терминале, 2 — списан.
struct CaseItem {
    int id = 0;
    QString caseType;
    int status = 0;
    int terminalId = 0;
    QString terminalSerial;
    QString notes;
};

// Строка поступления чехлов (tblcaseincomedetails): тип + количество.
struct CaseIncomeRow {
    int detailId = 0;
    QString caseType;
    int qty = 0;
};

// Строка установки чехлов (tblcaseinstalldetails).
struct CaseInstallRow {
    int detailId = 0;
    int terminalId = 0;
    QString terminalSerial;
    int caseId = 0;
    QString caseType;
};

// Строка списания чехлов (tblcasewriteoffdetails).
struct CaseWriteoffRow {
    int detailId = 0;
    int caseId = 0;
    QString caseType;
    QString reason;
};

} // namespace models

#endif // CASESDOCUMENT_H