#ifndef READONLYDELEGATE_H
#define READONLYDELEGATE_H

#include <QStyledItemDelegate>

class ReadOnlyDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ReadOnlyDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {}

    // Запрещаем создание редактора — колонка будет только для чтения
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        Q_UNUSED(parent);
        Q_UNUSED(option);
        Q_UNUSED(index);
        return nullptr;
    }
};

#endif // READONLYDELEGATE_H