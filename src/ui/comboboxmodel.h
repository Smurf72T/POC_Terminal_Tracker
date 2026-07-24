#ifndef COMBOBOXMODEL_H
#define COMBOBOXMODEL_H

#include <QAbstractItemModel>
#include <QList>
#include <QPair>
#include <QHash>

class ComboBoxModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit ComboBoxModel(const QList<QPair<int, QString>>& items, QObject *parent = nullptr)
        : QAbstractItemModel(parent), m_items(items) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return m_items.size(); }
    int columnCount(const QModelIndex &parent = QModelIndex()) const override { return 1; }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole && index.row() < m_items.size()) {
            return m_items[index.row()].second;
        }
        return QVariant();
    }

    QHash<int, QVariant> itemData(const QModelIndex &index, const QList<int> &roles = QList<int>()) const override {
        QHash<int, QVariant> hash;
        if (index.row() < m_items.size()) {
            for (int role : roles) {
                if (role == Qt::DisplayRole) hash[role] = m_items[index.row()].second;
                if (role == Qt::UserRole) hash[role] = m_items[index.row()].first;
            }
            if (roles.isEmpty()) {
                hash[Qt::DisplayRole] = m_items[index.row()].second;
                hash[Qt::UserRole] = m_items[index.row()].first;
            }
        }
        return hash;
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