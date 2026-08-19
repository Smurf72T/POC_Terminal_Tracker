#include "test_repositories.h"

#include "database/repositories/clientrepository.h"
#include "database/repositories/documentrepository.h"
#include "database/repositories/simcardrepository.h"
#include "database/repositories/terminalrepository.h"

#include <QDate>
#include <QSet>
#include <QSqlError>
#include <QSqlQueryModel>
#include <QtTest/QtTest>

void TestRepositories::populateMethodsFillModels()
{
    TerminalRepository terminals(m_db);
    auto* termModel = new QSqlQueryModel(this);
    terminals.populateFreeTerminals(termModel);
    QCOMPARE(termModel->rowCount(), 2);
    QCOMPARE(termModel->columnCount(), 3);
    QCOMPARE(termModel->data(termModel->index(0, 0)).toString(), QString("SN-0001"));
    QCOMPARE(termModel->data(termModel->index(0, 1)).toString(), QString("PAX-A920"));
    QCOMPARE(termModel->data(termModel->index(1, 0)).toString(), QString("SN-0004"));

    SimCardRepository sims(m_db);
    auto* simModel = new QSqlQueryModel(this);
    sims.populateFreeSimCards(simModel);
    // Только SIM4: статус 0 и не привязана ни к одному терминалу.
    QCOMPARE(simModel->rowCount(), 1);
    QCOMPARE(simModel->data(simModel->index(0, 0)).toString(), QString("890100000000004"));

    ClientRepository clients(m_db);
    auto* rentalModel = new QSqlQueryModel(this);
    clients.populateRentedTerminals(rentalModel, 1);
    QCOMPARE(rentalModel->rowCount(), 2);
    QCOMPARE(rentalModel->data(rentalModel->index(0, 1)).toString(), QString("SN-0002"));
    QCOMPARE(rentalModel->data(rentalModel->index(1, 1)).toString(), QString("SN-0003"));
}

void TestRepositories::terminalLoadMethods()
{
    // Полноценный терминал с IMEI и моделью, свободный.
    insertTerminalFull(10, "SN-FULL-1", 0, 1, 0, "111111111111111", "222222222222222", 0);
    // Деактивированный терминал — не попадает в выбор для аренды.
    insertTerminalFull(11, "SN-DEACT", 0, 1, 0, QString(), QString(), 1);

    TerminalRepository repo(m_db);

    const models::Terminal t = repo.loadById(10);
    QCOMPARE(t.id, 10);
    QCOMPARE(t.serialNumber, QString("SN-FULL-1"));
    QCOMPARE(t.modelId, 1);
    QCOMPARE(t.modelName, QString("PAX-A920"));
    QCOMPARE(t.imei1, QString("111111111111111"));
    QCOMPARE(t.imei2, QString("222222222222222"));
    QCOMPARE(t.status, 0);
    QVERIFY(!t.deactivated);

    // Несуществующий id — invalid-модель.
    QCOMPARE(repo.loadById(999).id, 0);

    const auto byIds = repo.loadByIds(QList<int>{11, 10, 999});
    QCOMPARE(byIds.size(), 2);
    // Порядок исходного списка сохраняется.
    QCOMPARE(byIds.at(0).id, 11);
    QCOMPARE(byIds.at(1).id, 10);
    QCOMPARE(repo.loadByIds(QList<int>{}).size(), 0);

    // Свободные для выбора: все со status=0 и is_deactivated=0.
    const auto free = repo.loadFreeForSelection();
    QSet<int> ids;
    for (const auto& f : free)
        ids.insert(f.id);
    QVERIFY(ids.contains(1));
    QVERIFY(ids.contains(4));
    QVERIFY(ids.contains(10));
    QVERIFY(!ids.contains(11));
}

void TestRepositories::terminalUpdateMethod()
{
    insertTerminalFull(20, "SN-EDIT-1", 0, 2, 0, "333333333333333", QString(), 0);

    TerminalRepository repo(m_db);
    TerminalUpdate data;
    data.serialNumber = "SN-EDIT-1-RENAMED";
    data.modelId = 1;
    data.imei1 = "444444444444444";
    data.imei2 = "555555555555555";
    data.status = 2;
    data.purchaseDate = QDate(2026, 8, 1);
    data.notes = "обновлено из формы";
    data.wasRepaired = true;
    data.deactivated = true;

    QVERIFY(repo.update(20, data));

    const models::Terminal t = repo.loadById(20);
    QCOMPARE(t.serialNumber, QString("SN-EDIT-1-RENAMED"));
    QCOMPARE(t.modelId, 1);
    QCOMPARE(t.imei1, QString("444444444444444"));
    QCOMPARE(t.imei2, QString("555555555555555"));
    QCOMPARE(t.status, 2);
    QCOMPARE(t.purchaseDate, QDate(2026, 8, 1));
    QCOMPARE(t.notes, QString("обновлено из формы"));
    QVERIFY(t.wasRepaired);
    QVERIFY(t.deactivated);

    // Очистка даты покупки — invalid-дата записывается как NULL.
    TerminalUpdate clearDate;
    clearDate.serialNumber = t.serialNumber;
    clearDate.modelId = t.modelId;
    clearDate.imei1 = t.imei1;
    clearDate.imei2 = t.imei2;
    clearDate.status = t.status;
    clearDate.notes = t.notes;
    clearDate.wasRepaired = t.wasRepaired;
    clearDate.deactivated = t.deactivated;
    QVERIFY(repo.update(20, clearDate));
    QVERIFY(!repo.loadById(20).purchaseDate.isValid());
}

void TestRepositories::simCardLoadMethods()
{
    SimCardRepository repo(m_db);

    const models::SimCard s = repo.loadById(2);
    QCOMPARE(s.id, 2);
    QCOMPARE(s.number, QString("890100000000002"));
    QCOMPARE(s.status, 1);
    QCOMPARE(s.notes, QString("в работе"));

    QCOMPARE(repo.loadById(999).id, 0);

    const auto byIds = repo.loadByIds(QList<int>{4, 2, 999});
    QCOMPARE(byIds.size(), 2);
    QCOMPARE(byIds.at(0).id, 4);
    QCOMPARE(byIds.at(1).id, 2);

    // Выбор для аренды: status=0 или привязанные к свободному терминалу.
    const auto free = repo.loadFreeForSelection();
    QSet<int> ids;
    for (const auto& f : free)
        ids.insert(f.id);
    // SIM3 привязана к свободному терминалу (SN-0004) — должна попадать.
    QVERIFY(ids.contains(1));
    QVERIFY(ids.contains(3));
    QVERIFY(ids.contains(4));
    QVERIFY(!ids.contains(2));
}

void TestRepositories::clientLoadMethods()
{
    insertClientFull(10, "ООО «Бета»", "7700000000", "Москва", "+7 900 000-00-00", "beta@example.com");

    ClientRepository repo(m_db);

    const models::Client c = repo.loadById(10);
    QCOMPARE(c.id, 10);
    QCOMPARE(c.name, QString("ООО «Бета»"));
    QCOMPARE(c.inn, QString("7700000000"));
    QCOMPARE(c.address, QString("Москва"));
    QCOMPARE(c.contactPhone, QString("+7 900 000-00-00"));
    QCOMPARE(c.contactEmail, QString("beta@example.com"));

    QCOMPARE(repo.loadById(999).id, 0);

    const auto all = repo.loadAll();
    QCOMPARE(all.size(), 3); // Альфа, ИП Иванов, Бета
    QCOMPARE(all.at(0).name, QString("ИП Иванов"));
    QCOMPARE(all.at(1).name, QString("ООО «Альфа»"));
    QCOMPARE(all.at(2).name, QString("ООО «Бета»"));
}

void TestRepositories::documentModelMethods()
{
    // Поступление: документ 2 с терминалом 10.
    insertReceiptDoc(2, "PR-2026-00002", "2026-08-05");
    insertReceiptDetail(1, 2, 10);

    // Возврат: документ 2 по аренде 1, возвращён терминал 2.
    insertReturnDoc(2, 1, "RT-2026-00002", "2026-08-06");
    insertReturnDetail(1, 2, 2);
    insertReturnDetail(2, 2, 3);

    DocumentRepository repo(m_db);

    // Шапка возврата.
    const models::DocumentHeader h = repo.loadHeader(DocumentRepository::Return, 2);
    QCOMPARE(h.id, 2);
    QCOMPARE(h.docNumber, QString("RT-2026-00002"));
    QCOMPARE(h.date, QDate(2026, 8, 6));
    QCOMPARE(h.clientId, 1);

    // Шапка аренды.
    const models::RentalDocument rd = repo.loadRentalDocument(1);
    QCOMPARE(rd.docNumber, QString("AR-2026-00001"));
    QCOMPARE(rd.clientId, 1);

    // Строки аренды 1: терминалы 2 (SIM2) и 3 (без SIM), сортировка по serial.
    const auto rows = repo.loadRentalRows(1);
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).terminalId, 2);
    QCOMPARE(rows.at(0).simNumber, QString("890100000000002"));
    QCOMPARE(rows.at(0).terminalStatus, 1);
    QCOMPARE(rows.at(1).terminalId, 3);
    QCOMPARE(rows.at(1).simCardId, 0);
    QCOMPARE(rows.at(1).simNumber, QString());

    // Документы аренды клиента 1.
    const auto docs = repo.loadRentalDocumentsByClient(1);
    QCOMPARE(docs.size(), 1);
    QCOMPARE(docs.at(0).id, 1);

    // Строки поступления: терминал 10 с моделью и IMEI.
    const auto receiptRows = repo.loadReceiptRows(2);
    QCOMPARE(receiptRows.size(), 1);
    QCOMPARE(receiptRows.at(0).terminalId, 10);
    QCOMPARE(receiptRows.at(0).serialNumber, QString("SN-FULL-1"));
    QCOMPARE(receiptRows.at(0).modelName, QString("PAX-A920"));
    QCOMPARE(receiptRows.at(0).imei1, QString("111111111111111"));

    // Связь возврата с арендой и возвращённые терминалы.
    QCOMPARE(repo.rentalDocIdForReturn(2), 1);
    QCOMPARE(repo.rentalDocIdForReturn(999), -1);
    const auto returned = repo.returnedTerminalIds(2);
    QCOMPARE(returned.size(), 2);
    QVERIFY(returned.contains(2));
    QVERIFY(returned.contains(3));
}