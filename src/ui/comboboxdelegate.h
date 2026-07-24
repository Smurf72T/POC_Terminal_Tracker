#ifndef COMBOBOXDELEGATE_H
#define COMBOBOXDELEGATE_H

#include <QStyledItemDelegate>
#include <QComboBox>
#include <QList>
#include <QPair>

class ComboBoxDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    // Принимает список пар (ID модели, Название модели)
    explicit ComboBoxDelegate(const QList<QPair<int, QString>>& items, QObject *parent = nullptr)
        : QStyledItemDelegate(parent), m_items(items) {}

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        QComboBox *editor = new QComboBox(parent);
        for (const auto &item : m_items) {
            editor->addItem(item.second, item.first); // Текст, ID (в UserRole)
        }
        return editor;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        int value = index.model()->data(index, Qt::EditRole).toInt();
        QComboBox *comboBox = static_cast<QComboBox*>(editor);
        int comboIndex = comboBox->findData(value);
        if (comboIndex >= 0) comboBox->setCurrentIndex(comboIndex);
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        QComboBox *comboBox = static_cast<QComboBox*>(editor);
        int id = comboBox->currentData().toInt();
        model->setData(index, id, Qt::EditRole);
    }

private:
    QList<QPair<int, QString>> m_items;
};

#endif // COMBOBOXDELEGATE_H