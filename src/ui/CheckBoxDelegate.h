#ifndef CHECKBOXDELEGATE_H
#define CHECKBOXDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QStyleOptionButton>
#include <QStyleOptionViewItem>
#include <QModelIndex>
#include <QApplication>

class CheckBoxDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit CheckBoxDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        bool checked = index.data(Qt::DisplayRole).toBool();

        QStyleOptionButton checkBoxStyle;
        checkBoxStyle.state = QStyle::State_Enabled;
        if (checked) {
            checkBoxStyle.state |= QStyle::State_On;
        } else {
            checkBoxStyle.state |= QStyle::State_Off;
        }
        checkBoxStyle.rect = option.rect;
        checkBoxStyle.rect.setWidth(20); // Фиксированная ширина чекбокса
        checkBoxStyle.rect.moveCenter(option.rect.center()); // Центрируем

        QApplication::style()->drawControl(QStyle::CE_CheckBox, &checkBoxStyle, painter);
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        Q_UNUSED(index);
        return QSize(20, option.rect.height());
    }
};

#endif // CHECKBOXDELEGATE_H