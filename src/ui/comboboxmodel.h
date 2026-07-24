#ifndef COMBOBOXMODEL_H
#define COMBOBOXMODEL_H

#include <QAbstractItemModel>
#include <QList>
#include <QPair>

class ComboBoxModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit ComboBoxModel(const QList<QPair<int, QString>>& items, QObject *parent = nullptr)
        : QAbstractItemModel(parent), m_items(items) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return m_items.size(); }
    int columnCount(const QModelIndex &parent = QModelIndex()) const override { return 1; }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (index.row() >= m_items.size()) return QVariant();
        if (role == Qt::DisplayRole) return m_items[index.row()].second;
        if (role == Qt::UserRole) return m_items[index.row()].first;
        return QVariant();
    }

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        if (row < m_items.size() && column == 0) return createIndex(row, column);
        return QModelIndex();
    }

    QModelIndex parent(const QModelIndex &index) const override { Q_UNUSED(index); return QModelIndex(); }

private:
    QList<QPair<int, QString>> m_items;
};

#endif // COMBOBOXMODEL_H