#include "ui/mainwindow.h"
#include "database/databasemanager.h"
#include <QApplication>
#include <QFile>
#include <QMessageBox>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

static void applyStyle(QApplication& app)
{
    QFile styleFile(":/styles/modern.qss");
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString style = QString::fromUtf8(styleFile.readAll());
        app.setStyleSheet(style);
        styleFile.close();
    } else {
        QMessageBox::warning(nullptr, "Стиль", "Не удалось загрузить modern.qss");
    }
}

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

    // Применяем тему
    applyStyle(a);

    MainWindow w;
    w.show();

    return a.exec();
}