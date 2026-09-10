#ifndef SIMINSTALLFORM_H
#define SIMINSTALLFORM_H

#include "ui/base/documentdialog.h"
#include <QMap>
#include <QPair>

namespace Ui {
class SimInstallForm;
}

class QSqlDatabase;

class SimInstallForm : public DocumentDialog {
    Q_OBJECT

public:
    explicit SimInstallForm(QWidget* parent = nullptr);
    ~SimInstallForm();

private slots:
    void on_btnAddRow_clicked();
    void on_btnDeleteRow_clicked();
    void on_btnPost_clicked();
    void on_btnClose_clicked();
    void onTableViewDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);

private:
    Ui::SimInstallForm* ui;

    // Снимок деталей документа из БД (terminalid -> {sim слота 1, sim слота 2})
    QMap<int, QPair<int, int>> m_originalDetails;

    // Уже установленные в свободные терминалы SIM (terminalId -> {simId, номер}).
    QMap<int, QPair<int, QString>> m_installedSim1;
    QMap<int, QPair<int, QString>> m_installedSim2;

    // --- DocumentDialog ---
    QString docType() const override;
    QLineEdit* headerNumberEdit() const override;
    QDateEdit* headerDateEdit() const override;
    QTextEdit* headerCommentEdit() const override;
    QTableView* tableView() const override;
    bool validateBeforePost() override;
    int postHeader(QSqlDatabase& db) override;
    bool postDetails(QSqlDatabase& db, int docId) override;
    void onPostSuccess(int docId) override;
    void loadSpecificEditData(int docId) override;

    void loadFreeTerminalsToDelegate();
    void loadFreeSIMsToDelegate();
    // Автозаполнение SIM-карт при выборе терминала в колонке 0.
    void autoFillSimForTerminal(int row, int terminalId);
};

#endif // SIMINSTALLFORM_H
