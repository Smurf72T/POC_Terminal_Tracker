#include "ui/mainwindow.h"
#include "ui/dialogs/loginform.h"
#include "database/databasemanager.h"
#include <QApplication>
#include <QIcon>
#include <QFile>
#include <QMessageBox>
#include <QSettings>
#include <QWidget>
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
        QMessageBox::warning(&app, "Стиль", "Не удалось загрузить modern.qss");
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setWindowIcon(QIcon(":/media/70x70.png"));

#ifdef Q_OS_WIN
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (!DatabaseManager::instance().initialize()) {
        QWidget splash;
        splash.setWindowTitle("POC Terminal Tracker");
        splash.resize(400, 100);
        QMessageBox::critical(&splash, "Критическая ошибка",
                             "Не удалось подключиться к базе данных.\n"
                             "Проверьте конфигурационный файл config/config.json\n"
                             "или переменную окружения POC_DB_PASSWORD");
        return -1;
    }

    applyStyle(a);

    LoginForm loginDialog;
    if (loginDialog.exec() != QDialog::Accepted) {
        return 0;
    }

    DatabaseManager::instance().setCurrentUser(loginDialog.getUsername());
    DatabaseManager::instance().setCurrentUserRole(loginDialog.getRole());
    DatabaseManager::instance().setAuditUsername(loginDialog.getUsername());
    DatabaseManager::instance().setSessionRole(loginDialog.getRole());

    // Применяем сохранённую тему (по умолчанию тёмная)
    QSettings settings("POC", "TerminalTracker");
    if (!settings.value("darkTheme", true).toBool()) {
        QFile lightFile(":/styles/light.qss");
        if (lightFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            a.setStyleSheet(QString::fromUtf8(lightFile.readAll()));
            lightFile.close();
        }
    }

    MainWindow w;
    w.setWindowTitle(QString("POC Terminal Tracker — %1").arg(loginDialog.getUsername()));
    w.show();

    return a.exec();
}
