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
    void on_btnGenerate_clicked();
    void on_comboModel_currentIndexChanged(int index);
    void on_btnPost_clicked(); // Кнопка "Провести"
    void on_btnPrint_clicked();
    void on_btnClose_clicked();
    void onScanFinished(const QString& raw);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    Ui::ReceiptForm* ui;
    QStandardItemModel* rowsModel;

    void loadModelsToDelegate();
    // Можно ли принять скан: форма активна, фокус не в текстовом редакторе.
    bool canAcceptScan() const;
    // Чтение сканера из COM-порта (раздел "scanner" в config.json).
    void setupSerialScanner();
    // Текущий выбранный в комбобоксе ID модели (0, если не выбран).
    int selectedModelId() const;
    // Применить текущую модель комбобокса ко всем строкам таблицы.
    void applyModelToAllRows();
    // Целевая строка для серийного номера: первая с пустым серийником, иначе новая.
    int targetRowForSerial() const;
    // Целевая строка для IMEI: первая, где серийник заполнен, а эта колонка пуста.
    int targetRowForImei(int imeiCol) const;

    QList<QPair<int, QString>> m_models;
    BarcodeScanner* m_scanner = nullptr;
    SerialScanner* m_serialScanner = nullptr;
    bool m_editMode = false;
    int m_editDocId = 0;
};

#endif // RECEIPTFORM_H