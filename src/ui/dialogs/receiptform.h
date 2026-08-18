#ifndef RECEIPTFORM_H
#define RECEIPTFORM_H

#include <QDialog>
#include <QStandardItemModel>
#include <QPair>

class BarcodeScanner;
class SerialScanner;
class SerialUnitsDialog;

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
    enum Column { ColModel = 0, ColQty = 1, ColSerials = 2 };
    // В item колонки ColSerials хранятся ТРИ параллельных списка (по комплекту
    // на индекс): серийники, их IMEI 1 и их IMEI 2. Связь — по индексу списка.
    static constexpr int RoleSerials = Qt::UserRole + 2;
    static constexpr int RoleImei1 = Qt::UserRole + 3;
    static constexpr int RoleImei2 = Qt::UserRole + 4;

    Ui::ReceiptForm* ui;
    QStandardItemModel* rowsModel;

    void loadModelsToDelegate();
    // Можно ли принять скан: форма активна, фокус не в текстовом редакторе.
    bool canAcceptScan() const;
    // Чтение сканера из COM-порта (раздел "scanner" в config.json).
    void setupSerialScanner();
    // Кол-во в строке (минимум 1).
    int rowQty(int row) const;
    // Параллельные списки комплектов строки (по одному на устройство).
    QStringList serialsForRow(int row) const;
    QStringList imei1ForRow(int row) const;
    QStringList imei2ForRow(int row) const;
    void setUnitsForRow(int row, const QStringList& serials, const QStringList& imei1, const QStringList& imei2);
    // Краткое описание комплектов для ячейки таблицы.
    QString listSummary(const QStringList& values, int expected) const;
    // Пересчитать текст колонки комплектов строки.
    void refreshRow(int row);
    // Открыть окно ввода комплектов (серийник + его IMEI).
    void openUnitsDialog(int row);

    QList<QPair<int, QString>> m_models;
    BarcodeScanner* m_scanner = nullptr;
    SerialScanner* m_serialScanner = nullptr;
    SerialUnitsDialog* m_unitsDialog = nullptr;
    bool m_editMode = false;
    int m_editDocId = 0;
};

#endif // RECEIPTFORM_H