#include "ui/mainwindow.h"
#include "database/databasemanager.h"
#include <QApplication>
#include <QMessageBox>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

#ifdef Q_OS_WIN
    // Устанавливаем UTF-8 для консоли Windows
    SetConsoleOutputCP(CP_UTF8);
#endif

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