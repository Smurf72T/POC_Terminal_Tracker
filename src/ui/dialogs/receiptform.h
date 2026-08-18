#ifndef RECEIPTFORM_H
#define RECEIPTFORM_H

#include <QDialog>
#include <QStandardItemModel>
#include <QPair>

class BarcodeScanner;
class SerialScanner;

namespace Ui {
class ReceiptForm;
}

class ReceiptForm : public QDialog {
    Q_OBJECT

public:
    explicit ReceiptForm(QWidget* parent = nullptr);
    ~ReceiptForm();
    void loadForEdit(int docId);

private slots:
    void on_btnAddRow_clicked();
    void on_btnDeleteRow_clicked();
    void on_btnPost_clicked(); // Кнопка "Провести"
    void on_btnPrint_clicked();
    void on_btnClose_clicked();
    void onScanFinished(const QString& raw);
    void onTableViewDoubleClicked(const QModelIndex& index);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    enum Column { ColModel = 0, ColQty = 1, ColSerial = 2, ColImei1 = 3, ColImei2 = 4 };
    // В item колонки-списка (2..4) сам список лежит в UserRole+2.
    static constexpr int ListRole = Qt::UserRole + 2;

    Ui::ReceiptForm* ui;
    QStandardItemModel* rowsModel;

    void loadModelsToDelegate();
    // Можно ли принять скан: форма активна, фокус не в текстовом редакторе.
    bool canAcceptScan() const;
    // Чтение сканера из COM-порта (раздел "scanner" в config.json).
    void setupSerialScanner();
    // Кол-во в строке (минимум 1).
    int rowQty(int row) const;
    // Списки серийников/IMEI строки.
    QStringList listForRow(int row, int col) const;
    void setListForRow(int row, int col, const QStringList& values);
    // Краткое описание списка для ячейки таблицы.
    QString listSummary(const QStringList& values, int expected) const;
    // Пересчитать тексты колонок-списков строки.
    void refreshRow(int row);
    // Открыть окно ввода для колонки 2..4.
    void openListDialog(int row, int col);

    QList<QPair<int, QString>> m_models;
    BarcodeScanner* m_scanner = nullptr;
    SerialScanner* m_serialScanner = nullptr;
    bool m_editMode = false;
    int m_editDocId = 0;
};

#endif // RECEIPTFORM_H