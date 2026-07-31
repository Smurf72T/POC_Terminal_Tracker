#include "submiterrortablemodel.h"
#include <QSqlError>

bool SubmitErrorTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    bool ok = QSqlTableModel::setData(index, value, role);
    if (!ok && lastError().isValid()) {
        emit submitFailed(lastError().text());
    }
    return ok;
}

bool SubmitErrorRelationalTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    bool ok = QSqlRelationalTableModel::setData(index, value, role);
    if (!ok && lastError().isValid()) {
        emit submitFailed(lastError().text());
    }
    return ok;
}
