# POC Terminal Tracker

Система учёта POC-терминалов и SIM-карт. Qt 6 (C++17) + PostgreSQL.

## Функциональность

- **Справочники:** Производители, Модели, Клиенты, SIM-карты, Терминалы, Пользователи
- **Документы:**
  - Поступление терминалов на склад
  - Передача в аренду (с автоматическим обновлением статусов)
  - Возврат из аренды (с автоматическим сбросом статусов SIM и терминалов)
  - Отметка оплаты (с печатью квитанции)
- **Архивы:** Просмотр и фильтрация документов по датам и клиентам, экспорт в Excel/PDF
- **Дашборд:** Счётчики свободных/занятых терминалов и SIM-карт, графики (pie — статусы, bar — выручка), топ клиентов, последние документы
- **Уведомления о просрочке:** Аренды >30 дней, неоплаченные периоды
- **Журнал аудита:** Фильтры по дате/действию/таблице, экспорт в Excel
- **Массовое обновление статусов:** Множественный выбор терминалов для смены статуса (свободен/в аренде/в ремонте/списан/утерян)
- **Отчёты по периодам:** Выручка по клиентам, загрузка терминалов, конвертация аренды, использование SIM-карт, задолженность клиентов
- **Вход/регистрация:** Аутентификация (PBKDF2-HMAC-SHA256, 100k итераций), rate limiting (5 попыток → 30 сек блокировка), принудительная смена пароля при первом входе
- **Ролевая модель:** admin (полный доступ), user (ограниченное меню, без аудита/бекапа/управления пользователями)
- **Управление пользователями:** Смена ролей, деактивация, сброс пароля (Сервис → Управление пользователями)
- **Печать документов:** Квитанция поступления, акт возврата, квитанция об оплате (HTML → QPrinter)
- **Глобальный поиск:** Ctrl+K, поиск по всем справочникам (ILIKE), открытие найденной формы
- **Переключалка темы:** Тёмная/светлая тема (кнопка в статусбаре, `modern.qss` ↔ `light.qss`), выбор сохраняется в QSettings
- **Эксплуатация (v1.3.0):** автоматические бэкапы по расписанию (интервал + retention), журнал операций `ops.log`, автоматическая проверка целостности БД (терминалы/SIM), статус бэкапа в статусбаре — подробности в [docs/OPS.md](docs/OPS.md)

## Требования

- Qt 6.2+ (с модулем Charts)
- PostgreSQL 14+
- CMake 3.20+
- MinGW (сборка через CLion) или любой C++17 компилятор

## Сборка

```bash
mkdir build && cd build
cmake .. -G Ninja -DBUILD_TESTS=ON
cmake --build .

# Запуск тестов
ctest --output-on-failure
```

## Конфигурация

### Пароль базы данных

Пароль передаётся через переменную окружения или `.env` в корне проекта (не коммитится):

```env
POC_DB_HOST=localhost
POC_DB_PORT=5432
POC_DB_NAME=pocbase
POC_DB_USER=postgres
POC_DB_PASSWORD=postgres
```

Приоритет: `.env` > переменная окружения `POC_DB_PASSWORD` > `config/config.json`.

### Эксплуатация и автобэкапы

Автоматические бэкапы, журнал операций и проверка целостности БД настраиваются
в `config/config.json` (секции `backup` и `monitoring`):

```json
"backup": {
  "enabled": true,
  "interval_hours": 24,
  "directory": "backups",
  "retention_count": 14
},
"monitoring": {
  "integrity_enabled": true,
  "integrity_interval_hours": 24
}
```

Подробное руководство по эксплуатации — [docs/OPS.md](docs/OPS.md).

### Начальная настройка БД

1. Создайте базу данных `pocbase`
2. Запустите приложение — миграции применятся автоматически из `sql/migrations/`
3. Дефолтный пользователь: `admin` / `admin123`

Если авто-миграции не сработали (нет прав на создание таблиц), выполните вручную в порядке нумерации:

```bash
psql -U postgres -d pocbase -f sql/migrations/000_base_schema.sql
psql -U postgres -d pocbase -f sql/migrations/001_initial.sql
psql -U postgres -d pocbase -f sql/migrations/002_status_change_docs.sql
psql -U postgres -d pocbase -f sql/migrations/003_doc_number_sequences.sql
psql -U postgres -d pocbase -f sql/migrations/004_role_enforcement.sql
psql -U postgres -d pocbase -f sql/migrations/005_login_security.sql
```

## Логирование

Приложение использует `QLoggingCategory`. Для включения отладки установите переменную окружения:

```env
QT_LOGGING_RULES="app.sql=true;app.migration=true"
```

Категории:
| Категория    | Назначение                          |
|-------------|-------------------------------------|
| `app.database` | Ошибки подключения/запросов к БД |
| `app.audit`    | Сбои логирования аудита           |
| `app.migration`| Ход выполнения миграций БД        |
| `app.sql`      | Ошибки SQL-запросов в UI-формах   |
| `app.general`  | Общие ошибки приложения           |

## Структура проекта

```
src/
  main.cpp                    — Точка входа, показ формы входа
  database/databasemanager.*  — Подключение к БД, миграции, аудит, .env
  ui/
    mainwindow.*              — Главное окно, дашборд
    dialogs/
      loginform.*             — Вход/регистрация с rate limiting
      terminalsform.*         — Справочник терминалов
      clientsform.*           — Справочник клиентов
      manufacturersform.*     — Справочник производителей
      modelsform.*            — Справочник моделей
      simcardsform.*          — Справочник SIM-карт
      receiptform.*           — Поступление терминалов
      rentalform.*            — Передача в аренду
      returnform.*            — Возврат из аренды
      paymentform.*           — Отметка оплаты
      archivedocumentsform.*  — Архив документов
      terminalhistoryform.*   — История терминала
      bulkimportform.*        — Массовый импорт
      auditlogform.*          — Журнал аудита
      expirynotificationsform.* — Уведомления о просрочке
      batchstatusform.*       — Массовая смена статусов
      usermanagementform.*    — Управление пользователями
      reportsform.*           — Отчёты по периодам
  utils/
    password_utils.h          — PBKDF2-HMAC-SHA256 (100k итераций), constant-time проверка, обратная совместимость
    validator.*               — Валидация ИНН, IMEI (Luhn), данных
    reportexporter.*          — Экспорт в Excel (QXlsx) и PDF
    logging.h                 — QLoggingCategory: app.database, app.audit, app.migration, app.sql, app.general
  ops/
    backupmanager.*           — Бэкап (pg_dump + fallback) и восстановление (psql), без UI
    opslog.*                  — Журнал операций logs/ops.log (ротация 1 МБ)
    opsscheduler.*            — Планировщик автобэкапов и проверки целостности БД
styles/
  modern.qss / light.qss      — Тёмная/светлая тема
sql/
  migrations/               — Миграции БД 001/002/003 (применяются автоматически)
  add_indexes.sql           — Индексы и ограничения
libs/
  QXlsx/                      — Git submodule (QtExcel/QXlsx)
config/
  config.json                 — Настройки БД (пароль через .env)
tests/
  test_password_utils.cpp     — Unit-тесты PBKDF2, обратная совместимость
  test_validator.cpp          — Unit-тесты IMEI, INN, Luhn, серийных номеров
  stub_databasemanager.cpp    — Стаб DatabaseManager для тестов
```

## Архитектура открытия форм

Все формы открываются через централизованный метод `MainWindow::openForm()`:

```cpp
void MainWindow::openForm(QWidget *form)
{
    form->setAttribute(Qt::WA_DeleteOnClose);
    form->setWindowModality(Qt::WindowModal);
    form->setWindowFlags(form->windowFlags() | Qt::Window);
    form->show();
    QRect mainRect = this->geometry();
    QRect formRect = form->frameGeometry();
    form->move(mainRect.center() - formRect.center());
}
```

Это обеспечивает:
- Автоматическое удаление формы при закрытии (без утечек памяти)
- Модальность относительно главного окна
- Центрирование по центру главного окна
