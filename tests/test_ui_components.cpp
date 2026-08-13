#include <QtTest>
#include <QStandardItemModel>
#include <QComboBox>
#include <QMouseEvent>

#include "ui/delegates/CheckBoxDelegate.h"
#include "ui/delegates/comboboxdelegate.h"
#include "ui/delegates/comboboxmodel.h"
#include "ui/delegates/readonlydelegate.h"

class TestUiComponents : public QObject {
    Q_OBJECT

private slots:
    void checkboxDelegateTogglesOnClick();
    void comboboxDelegateWritesUserAndDisplayRoles();
    void comboboxDelegateInitialisesEditor();
    void comboboxModelReturnsDisplayAndUserRoles();
    void readonlyDelegateDoesNotCreateEditor();
    void checkboxDelegateSizeHint();
    void editableComboboxWritesNewTextAsZeroId();
    void editableComboboxResolvesTextToExistingItem();
};

void TestUiComponents::checkboxDelegateTogglesOnClick()
{
    QStandardItemModel model;
    model.setColumnCount(1);
    model.setRowCount(1);
    const QModelIndex index = model.index(0, 0);
    model.setData(index, false);

    CheckBoxDelegate delegate;
    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 40, 24);
    const QPoint center = option.rect.center();

    QMouseEvent click(QEvent::MouseButtonRelease, QPointF(center), QPointF(center), Qt::LeftButton, Qt::NoButton,
                      Qt::NoModifier);
    QVERIFY(delegate.editorEvent(&click, &model, option, index));
    QCOMPARE(model.data(index, Qt::DisplayRole).toBool(), true);

    QMouseEvent secondClick(QEvent::MouseButtonRelease, QPointF(center), QPointF(center), Qt::LeftButton, Qt::NoButton,
                            Qt::NoModifier);
    QVERIFY(delegate.editorEvent(&secondClick, &model, option, index));
    QCOMPARE(model.data(index, Qt::DisplayRole).toBool(), false);

    // Клик вне чекбокса не должен менять значение
    QMouseEvent miss(QEvent::MouseButtonRelease, QPointF(39, 12), QPointF(39, 12), Qt::LeftButton, Qt::NoButton,
                     Qt::NoModifier);
    QVERIFY(!delegate.editorEvent(&miss, &model, option, index));
    QCOMPARE(model.data(index, Qt::DisplayRole).toBool(), false);
}

void TestUiComponents::comboboxDelegateWritesUserAndDisplayRoles()
{
    QList<QPair<int, QString>> items = {{1, "Активен"}, {2, "Списан"}};
    ComboBoxDelegate delegate(items);

    QStandardItemModel model;
    model.setColumnCount(1);
    model.setRowCount(1);
    const QModelIndex index = model.index(0, 0);

    QComboBox editor;
    for (const auto& item : items)
        editor.addItem(item.second, item.first);
    editor.setCurrentIndex(1);

    delegate.setModelData(&editor, &model, index);
    QCOMPARE(model.data(index, Qt::UserRole).toInt(), 2);
    QCOMPARE(model.data(index, Qt::DisplayRole).toString(), QString("Списан"));
}

void TestUiComponents::comboboxDelegateInitialisesEditor()
{
    QList<QPair<int, QString>> items = {{1, "Активен"}, {2, "Списан"}};
    ComboBoxDelegate delegate(items);

    QStandardItemModel model;
    model.setColumnCount(1);
    model.setRowCount(1);
    const QModelIndex index = model.index(0, 0);
    model.setData(index, 2, Qt::UserRole);

    QWidget parent;
    QStyleOptionViewItem option;
    QWidget* editorWidget = delegate.createEditor(&parent, option, index);
    QVERIFY(editorWidget != nullptr);
    QComboBox* editor = qobject_cast<QComboBox*>(editorWidget);
    QVERIFY(editor != nullptr);

    delegate.setEditorData(editor, index);
    QCOMPARE(editor->currentData().toInt(), 2);
    QCOMPARE(editor->currentText(), QString("Списан"));
    delete editorWidget;
}

void TestUiComponents::comboboxModelReturnsDisplayAndUserRoles()
{
    QList<QPair<int, QString>> items = {{1, "Один"}, {2, "Два"}};
    ComboBoxModel model(items);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.columnCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), Qt::DisplayRole).toString(), QString("Один"));
    QCOMPARE(model.data(model.index(1, 0), Qt::UserRole).toInt(), 2);
    QCOMPARE(model.data(model.index(5, 0)), QVariant());
    QVERIFY(model.parent(model.index(0, 0)) == QModelIndex());
}

void TestUiComponents::readonlyDelegateDoesNotCreateEditor()
{
    ReadOnlyDelegate delegate;
    QWidget parent;
    QStyleOptionViewItem option;
    QStandardItemModel model;
    QWidget* editor = delegate.createEditor(&parent, option, model.index(0, 0));
    QVERIFY(editor == nullptr);
}

void TestUiComponents::checkboxDelegateSizeHint()
{
    CheckBoxDelegate delegate;
    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 40, 24);
    QStandardItemModel model;
    const QSize size = delegate.sizeHint(option, model.index(0, 0));
    QVERIFY(size.width() > 0);
    QVERIFY(size.height() > 0);
}

void TestUiComponents::editableComboboxWritesNewTextAsZeroId()
{
    QList<QPair<int, QString>> items = {{1, "Активен"}, {2, "Списан"}};
    ComboBoxDelegate delegate(items, nullptr, true);

    QStandardItemModel model;
    model.setColumnCount(1);
    model.setRowCount(1);
    const QModelIndex index = model.index(0, 0);

    QComboBox editor;
    for (const auto& item : items)
        editor.addItem(item.second, item.first);
    editor.setEditable(true);
    editor.setInsertPolicy(QComboBox::NoInsert);
    editor.setCurrentText("79991234567");

    delegate.setModelData(&editor, &model, index);
    QCOMPARE(model.data(index, Qt::UserRole).toInt(), 0);
    QCOMPARE(model.data(index, Qt::DisplayRole).toString(), QString("79991234567"));
}

void TestUiComponents::editableComboboxResolvesTextToExistingItem()
{
    QList<QPair<int, QString>> items = {{1, "Активен"}, {2, "Списан"}};
    ComboBoxDelegate delegate(items, nullptr, true);

    QStandardItemModel model;
    model.setColumnCount(1);
    model.setRowCount(1);
    const QModelIndex index = model.index(0, 0);

    QComboBox editor;
    for (const auto& item : items)
        editor.addItem(item.second, item.first);
    editor.setEditable(true);
    editor.setInsertPolicy(QComboBox::NoInsert);
    editor.setCurrentText("Списан");

    delegate.setModelData(&editor, &model, index);
    QCOMPARE(model.data(index, Qt::UserRole).toInt(), 2);
    QCOMPARE(model.data(index, Qt::DisplayRole).toString(), QString("Списан"));
}

QTEST_MAIN(TestUiComponents)
#include "test_ui_components.moc"
