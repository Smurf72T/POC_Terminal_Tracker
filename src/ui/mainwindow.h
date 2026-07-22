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

private:
    Ui::MainWindow *ui;
    void setupUI();
    void updateStatusBar();
};

#endif // MAINWINDOW_H