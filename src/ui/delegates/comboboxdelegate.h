#ifndef COMBOBOXDELEGATE_H
#define COMBOBOXDELEGATE_H

#include <QStyledItemDelegate>
#include <QComboBox>
#include <QList>
#include <QPair>
#include <QMap>

class ComboBoxDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ComboBoxDelegate(const QList<QPair<int, QString>>& items, QObject *parent = nullptr,
                              bool editable = false)
        : QStyledItemDelegate(parent), m_editable(editable)
    {
        for (const auto &item : items) {
            m_idToText[item.first] = item.second;
        }
    }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        QComboBox *editor = new QComboBox(parent);
        for (auto it = m_idToText.begin(); it != m_idToText.end(); ++it) {
            editor->addItem(it.value(), it.key());
        }
        if (m_editable) {
            editor->setEditable(true);
            editor->setInsertPolicy(QComboBox::NoInsert);
        }
        return editor;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        int id = index.model()->data(index, Qt::UserRole).toInt();
        QComboBox *comboBox = static_cast<QComboBox*>(editor);
        int comboIndex = comboBox->findData(id);
        if (comboIndex >= 0) {
            comboBox->setCurrentIndex(comboIndex);
        } else if (comboBox->isEditable()) {
            comboBox->setCurrentText(index.model()->data(index, Qt::DisplayRole).toString());
        }
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        QComboBox *comboBox = static_cast<QComboBox*>(editor);
        QString text = comboBox->currentText().trimmed();
        int id = 0;

        if (comboBox->isEditable()) {
            // В редактируемом режиме currentIndex не отражает введённый текст:
            // ищем точное совпадение в списке, иначе считаем значение новым (id = 0).
            int comboIndex = comboBox->findText(text);
            if (comboIndex >= 0) {
                id = comboBox->itemData(comboIndex).toInt();
            }
        } else {
            id = comboBox->currentData().toInt();
        }

        // Сохраняем ID в UserRole и текст в DisplayRole
        model->setData(index, id, Qt::UserRole);
        model->setData(index, text, Qt::DisplayRole);
    }

private:
    QMap<int, QString> m_idToText;
    bool m_editable;
};

#endif // COMBOBOXDELEGATE_H