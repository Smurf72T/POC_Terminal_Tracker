#ifndef SUBMITERRORTABLEMODEL_H
#define SUBMITERRORTABLEMODEL_H

#include <QSqlTableModel>
#include <QSqlRelationalTableModel>

class SubmitErrorTableModel : public QSqlTableModel
{
    Q_OBJECT
public:
    using QSqlTableModel::QSqlTableModel;

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

signals:
    void submitFailed(const QString &error);
};

class SubmitErrorRelationalTableModel : public QSqlRelationalTableModel
{
    Q_OBJECT
public:
    using QSqlRelationalTableModel::QSqlRelationalTableModel;

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

signals:
    void submitFailed(const QString &error);
};

#endif // SUBMITERRORTABLEMODEL_H
