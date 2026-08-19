#include "ui/base/clientdocdialog.h"

#include "database/databasemanager.h"
#include "database/repositories/clientrepository.h"
#include "database/repositories/documentrepository.h"
#include "ui/delegates/comboboxdelegate.h"
#include "ui/delegates/comboboxmodel.h"

#include <QComboBox>
#include <QPair>

ClientDocumentDialog::ClientDocumentDialog(QWidget* parent) : DocumentDialog(parent)
{
}

void ClientDocumentDialog::loadClientsToComboBox(QComboBox* box, bool withPlaceholder)
{
    if (!box)
        return;
    box->clear();
    if (withPlaceholder)
        box->addItem("-- Выберите клиента --", 0);

    if (!DatabaseManager::instance().isConnected())
        return;

    const auto all = ClientRepository(DatabaseManager::instance().getDatabase()).loadAll();
    for (const auto& c : all)
        box->addItem(c.name, c.id);
}

void ClientDocumentDialog::loadClientsToDelegate(QComboBox* box)
{
    if (!box)
        return;

    QList<QPair<int, QString>> clients;
    const auto all = ClientRepository(DatabaseManager::instance().getDatabase()).loadAll();
    for (const auto& c : all)
        clients.append(qMakePair(c.id, c.name));

    box->setItemDelegate(new ComboBoxDelegate(clients, box));
    box->setModel(new ComboBoxModel(clients, box));
}

void ClientDocumentDialog::loadRentalDocsForClient(QComboBox* box, int clientId)
{
    if (!box)
        return;
    box->clear();
    if (clientId == 0)
        return;

    const auto docs =
        DocumentRepository(DatabaseManager::instance().getDatabase()).loadRentalDocumentsByClient(clientId);
    for (const auto& d : docs) {
        QString displayText = QString("%1 от %2").arg(d.docNumber, d.date.toString("dd.MM.yyyy"));
        box->addItem(displayText, d.id);
    }
}