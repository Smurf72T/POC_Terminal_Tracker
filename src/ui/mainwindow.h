#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QStatusBar>

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onActionAbout_triggered();
    void onActionExit_triggered();
    void onActionManufacturers_triggered();
    void onActionModels_triggered();
    void onActionClients_triggered();
    void onActionSIMCards_triggered();
    void onActionTerminals_triggered();
    void onActionReceipt_triggered();
    void onActionRental_triggered();
    void onActionReturn_triggered();
    void onActionArchiveReceipt_triggered();
    void onActionArchiveRental_triggered();
    void onActionArchiveReturn_triggered();

private:
    Ui::MainWindow *ui;
    void setupUI();
    void updateStatusBar();
    void centerWindow(QWidget *widget);
};

#endif // MAINWINDOW_H