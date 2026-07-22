#include "ui/mainwindow.h"
#include "database/databasemanager.h"
#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Инициализация подключения к БД
    if (!DatabaseManager::instance().initialize()) {
        QMessageBox::critical(nullptr, "Критическая ошибка",
                             "Не удалось подключиться к базе данных.\n"
                             "Проверьте конфигурационный файл config/config.json");
        return -1;
    }

    MainWindow w;
    w.show();

    return a.exec();
}