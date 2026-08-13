#ifndef CHECKBOXDELEGATE_H
#define CHECKBOXDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QStyleOptionButton>
#include <QStyleOptionViewItem>
#include <QModelIndex>
#include <QApplication>
#include <QEvent>
#include <QMouseEvent>

class CheckBoxDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit CheckBoxDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
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
        checkBoxStyle.rect.setWidth(20);
        checkBoxStyle.rect.moveCenter(option.rect.center());

        QApplication::style()->drawControl(QStyle::CE_CheckBox, &checkBoxStyle, painter);
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        Q_UNUSED(index);
        return QSize(20, option.rect.height());
    }

    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override
    {
        if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            QRect checkBoxRect = option.rect;
            checkBoxRect.setWidth(20);
            checkBoxRect.moveCenter(option.rect.center());

            if (checkBoxRect.contains(mouseEvent->pos())) {
                bool current = index.data(Qt::DisplayRole).toBool();
                model->setData(index, !current, Qt::DisplayRole);
                return true;
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }
};

#endif // CHECKBOXDELEGATE_H
