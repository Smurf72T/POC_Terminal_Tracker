#include "ui/mainwindow.h"
#include "ui/dialogs/loginform.h"
#include "database/databasemanager.h"
#include "utils/logging.h"
#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMessageBox>
#include <QSettings>
#include <QSqlQuery>
#include <QWidget>
#include <cstdio>
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

static QString readAppVersion(const QString &configPath)
{
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull())
        return QString();
    return doc.object()["application"].toObject()["version"].toString();
}

// Путь к config.json независимо от рабочего каталога: рядом с exe (портативная
// сборка), уровнем/двумя уровнями выше (разработка, CMake build) и CWD как fallback.
static QString appConfigPath()
{
    const QString base = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        base + "/config/config.json",
        base + "/../config/config.json",
        base + "/../../config/config.json",
        QStringLiteral("config/config.json")
    };
    for (const QString &c : candidates) {
        if (QFileInfo::exists(c))
            return c;
    }
    return candidates.last();
}

// Ищет значение ключа в .env рядом с exe, рядом с config.json и в корне проекта —
// тем же порядком, что и DatabaseManager (чтобы предупреждение не было ложным).
static QString envValueFromDotEnv(const QString &configPath, const QString &key)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QFileInfo cfgInfo(configPath);
    const QStringList candidates = {
        appDir + "/.env",
        appDir + "/../.env",
        cfgInfo.absolutePath() + "/.env",
        cfgInfo.absolutePath() + "/../../.env"
    };
    for (const QString &candidate : candidates) {
        QFile f(candidate);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        while (!f.atEnd()) {
            QString line = QString::fromUtf8(f.readLine()).trimmed();
            if (line.isEmpty() || line.startsWith('#'))
                continue;
            const int eq = line.indexOf('=');
            if (eq <= 0)
                continue;
            if (line.left(eq).trimmed() == key)
                return line.mid(eq + 1).trimmed();
        }
    }
    return QString();
}

static int runHealthCheck()
{
    // Без GUI-диалогов: вывод результата в stdout, код возврата 0/1/2.
    DatabaseManager::setSuppressDialogs(true);

    if (!DatabaseManager::instance().initialize(appConfigPath())) {
        std::printf("DB_ERROR: не удалось подключиться к базе данных или применить миграции\n");
        DatabaseManager::instance().close();
        return 1;
    }

    QSqlQuery q(DatabaseManager::instance().getDatabase());
    if (!q.exec("SELECT 1") || !q.next()) {
        std::printf("DB_ERROR: %s\n", q.lastError().text().toUtf8().constData());
        DatabaseManager::instance().close();
        return 1;
    }

    QStringList pending = DatabaseManager::instance().pendingMigrations();
    if (!pending.isEmpty()) {
        std::printf("DB_WARN: не применены миграции: %s\n", pending.join(", ").toUtf8().constData());
        DatabaseManager::instance().close();
        return 2;
    }

    std::printf("DB_OK\n");
    DatabaseManager::instance().close();
    return 0;
}

static void printUsage(const char *appName)
{
    std::printf("POC Terminal Tracker\n");
    std::printf("Использование: %s [опции]\n", appName);
    std::printf("  --check-db   проверка подключения к БД и применённых миграций (без GUI)\n");
    std::printf("  --version    вывод версии приложения\n");
    std::printf("  -h, --help   этот экран\n");
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setWindowIcon(QIcon(":/media/70x70.png"));

#ifdef Q_OS_WIN
    SetConsoleOutputCP(CP_UTF8);
#endif

    const QStringList args = QCoreApplication::arguments();
    const QString appName = args.isEmpty() ? "POC Terminal Tracker" : QFileInfo(args[0]).fileName();

    if (args.contains("--check-db"))
        return runHealthCheck();

    if (args.contains("-h") || args.contains("--help")) {
        printUsage(appName.toUtf8().constData());
        return 0;
    }

    if (args.contains("--version")) {
        QString version = readAppVersion(appConfigPath());
        std::printf("%s\n", (version.isEmpty() ? QString("unknown") : version).toUtf8().constData());
        return 0;
    }

    // Частая причина «не подключается к БД»: пустой пароль при отсутствии .env/POC_DB_PASSWORD.
    {
        QFile cfgFile(appConfigPath());
        if (cfgFile.open(QIODevice::ReadOnly)) {
            QJsonDocument cfgDoc = QJsonDocument::fromJson(cfgFile.readAll());
            const QJsonObject dbCfg = cfgDoc.object()["database"].toObject();
            const QString envPassword = envValueFromDotEnv(appConfigPath(), "POC_DB_PASSWORD");
            if (dbCfg["password"].toString().isEmpty()
                && qEnvironmentVariableIsEmpty("POC_DB_PASSWORD")
                && envPassword.isEmpty())
                qCWarning(logApp) << "config.json: database.password пуст и POC_DB_PASSWORD не задан — "
                                  << "подключение к БД, скорее всего, не удастся. Настройте .env или config.json";
        }
    }

    if (!DatabaseManager::instance().initialize(appConfigPath())) {
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
        DatabaseManager::instance().close();
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

    // Graceful shutdown: закрываем соединение с БД после завершения цикла событий
    QObject::connect(&a, &QCoreApplication::aboutToQuit, []() {
        qCInfo(logApp) << "Приложение завершает работу, закрываем соединение с БД";
        DatabaseManager::instance().close();
    });

    return a.exec();
}
