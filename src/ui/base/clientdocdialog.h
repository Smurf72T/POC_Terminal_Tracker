#ifndef CLIENTDOCDIALOG_H
#define CLIENTDOCDIALOG_H

#include "ui/base/documentdialog.h"

class QComboBox;

// Базовый диалог документов, работающих с клиентами и документами аренды
// (аренда, возврат, оплата). Содержит общие методы загрузки справочников.
class ClientDocumentDialog : public DocumentDialog {
    Q_OBJECT

public:
    explicit ClientDocumentDialog(QWidget* parent = nullptr);

protected:
    // Заполняет выпадающий список клиентами (ClientRepository::loadAll()).
    // withPlaceholder — добавить первую запись «-- Выберите клиента --» (id 0).
    void loadClientsToComboBox(QComboBox* box, bool withPlaceholder = false);
    // Заполняет выпадающий список клиентами через ComboBoxDelegate/ComboBoxModel.
    void loadClientsToDelegate(QComboBox* box);
    // Заполняет выпадающий список документами аренды клиента (номер + дата).
    void loadRentalDocsForClient(QComboBox* box, int clientId);
};

#endif // CLIENTDOCDIALOG_H