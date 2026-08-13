# API-документация (внутренние модули)

Документ описывает внутренние интерфейсы приложения POC Terminal Tracker (C++17, Qt 6, PostgreSQL).
Документация намеренно краткая — цель дать быстрый обзор сигнатур и контрактов для разработчиков.

## Уровни модулей

```
src/database/  — доступ к БД, пул соединений, миграции, аудит
src/ops/       — бэкап/восстановление, проверка целостности, планировщик
src/update/    — проверка обновлений, загрузка манифеста и файлов
src/utils/     — валидаторы, пароли, логирование, circuit breaker, экспорт отчётов
src/ui/        — окна, диалоги, делегаты
```

## 1. database/databasemanager.h — `DatabaseManager` (singleton)

Точка входа — глобальная функция `databaseManager()` (см. `idatabasemanager.h`),
возвращающая `DatabaseManager::instance()` в приложении и заглушку в тестах.

| Метод | Назначение |
|-------|-----------|
| `bool initialize(configPath = "config/config.json")` | Загрузка конфига + `.env`, подключение QPSQL (SSL-режимы `disable/prefer/require/verify-ca/verify-full`), применение миграций, подписка на `NOTIFY poc_data_changed` |
| `bool isConnected()` | Статус соединения |
| `void close()` | Закрытие соединения (graceful shutdown) |
| `QStringList pendingMigrations()` | Файлы `sql/migrations/`, отсутствующие в `schema_migrations` |
| `QSqlDatabase &getDatabase()` | Живое соединение |
| `QJsonObject configObject()` | Загруженный `config/config.json` |
| `QSqlQuery executeQuery(sql, showError=true)` | Выполнение запроса; **circuit breaker** + подсчёт сбоев |
| `bool executeTransaction(fn)` | Транзакция с **повтором transient-ошибок** (SQLSTATE 08*, 40P01, 40001, 57P01; до 3 попыток с backoff 100–300 мс) |
| `QString generateDocNumber(docType)` | Номер документа через функцию БД `generate_doc_number` |
| `void logAction(action, table, id, user, old, new)` | Запись в журнал аудита (таблица `tbl_audit_log`) |
| `setCurrentUser / setAuditUsername / setSessionRole / ...` | Контекст сессии (настройки `app.username`/`app.role` через `set_config`) |

Статическое управление: `DatabaseManager::setSuppressDialogs(bool)` — подавляет
модальные диалоги (headless-режим `--check-db`, фоновые операции). Ошибки из
не-GUI-потока логируются вместо `QMessageBox`.

### Соглашение о запросах
- Все данные, введённые пользователем, проходят через параметризованные запросы (`prepare`/`bindValue`).
- Критичные операции используют `FOR UPDATE NOWAIT` и/или транзакции.
- Массовые операции выполняются в одной транзакции с переиспользуемым prepared-запросом.

## 2. database/connectionpool.h — `ConnectionPool`

Per-thread пул соединений для фоновых потоков (например, `BackupWorker`).

| Метод | Назначение |
|-------|-----------|
| `static ConnectionPool &instance()` | Singleton |
| `QSqlDatabase acquire()` | Взять соединение из пула текущего потока (ожидание при превышении лимита) |
| `void release(QSqlDatabase)` | Вернуть соединение в пул |
| `int idleCount()` | Свободные соединения (диагностика) |
| `void clear()` | Закрыть и удалить все соединения |

Соединения изолированы по потокам (`QThread::currentThreadId()`), используются
уникальные имена (`QUuid`), чтобы не конфликтовать с соединением главного потока.

## 3. ops/backupmanager.h — `BackupManager`

Создание и восстановление резервных копий.

- `createBackup(filePath, password = "")` → `BackupResult`
  - Метод: `pg_dump` (если найден и подключается) или встроенный fallback-дамп.
  - При непустом пароле файл **шифруется** AES-256-CBC (`openssl enc -aes-256-cbc -pbkdf2 -iter 100000`), в начало файла пишется маркер `POCENC1\n`.
  - `BackupResult`: `ok`, `filePath`, `error`, `method`, `size`, `encrypted`.
- `restoreDatabase(filePath, password = "")` → `BackupResult`
  - Расшифровывает файл (по маркеру `POCENC1`), plain-файлы без маркера применяются как есть.

## 4. ops/backupworker.h — `BackupWorker` (асинхронный)

Работает в отдельном потоке (`QThread`), собственное соединение QPSQL из `ConnectionPool`.

- `setConnectionParams(params)` — параметры соединения (снимаются в главном потоке до `moveToThread`).
- Слоты: `createBackup(filePath, password)`, `restore(filePath, password)`.
- Сигналы: `backupFinished(BackupResult)`, `restoreFinished(ok, filePath, error)`.

## 5. ops/opsscheduler.h — `OpsScheduler`

Планировщик автоматических операций.

- `start()` — запуск; первая проверка через 10 с, далее точное перепланирование.
- `checkSchedule()` — запуск просроченного бэкапа/проверки целостности и перепланировка таймера.
- Сигналы: `backupFinished(bool, filePath, message)`, `integrityFinished(bool, summary)`, `backupRequested(filePath, password)`.
- Ретенция: удаление старых `backup_poc_*.sql` сверх `retention_count`.

## 6. utils/circuitbreaker.h — `CircuitBreaker`

Защита от каскадных сбоев внешних зависимостей.

- `isAllowed()` — можно ли выполнять запрос (закрыт → да; открыт → нет; полуоткрыт → ограниченное число проб).
- `onSuccess()` / `onFailure()` — учёт результата.
- Параметры: `failureThreshold` (по умолчанию 5), `cooldownMs` (30 с), `halfOpenTries` (1).
- Состояния: `Closed` → `Open` (после N сбоев) → `HalfOpen` (после cooldown) → `Closed` (успех) или снова `Open`.

Использован в `DatabaseManager::executeQuery/executeTransaction` (fail-fast при отказе БД).

## 7. utils/password_utils.h

- `hashPassword(password)` — PBKDF2-HMAC-SHA256, 100 000 итераций, соль 16 байт; формат `iter:salt:hash` (hex).
- `checkPassword(password, storedHash)` — constant-time сравнение; поддержка legacy-форматов.
- `validatePasswordStrength(password)` → `PasswordStrengthResult` — мин. 8 символов, заглавная буква, цифра.
- `isLegacyPasswordHash(hash)` — хеш не в формате `iter:salt:hash` (требует смены пароля).

## 8. utils/validator.h

- `validateIMEI` (15 цифр, Luhn), `validateINN` (10/12 цифр, контрольные суммы),
  `validateSerial`, `validateClientName`, `validateModelName` и др. — чистые функции без побочных эффектов.

## 8а. utils/terminal_status.h — словарь статусов терминалов

- `TerminalStatus::Value` — `Available=0`, `Rented=1`, `Repair=2`, `WrittenOff=3`, `Lost=4`.
- `name(int)` — название статуса; `sqlCaseExpression(column)` — фрагмент `CASE … END` для SQL UI.
- **ВНИМАНИЕ (расхождение схем):** в `sql/migrations/000_base_schema.sql` у `tblterminals.status`
  стоит `CHECK (status IN (0,1,2))`, полный словарь 0..4 вводит
  `sql/migrations/006_terminal_status_check.sql`. На новой БД, собранной целиком из миграций,
  значение ограничение применяется в версии 006. Единый источник словаря — `terminal_status.h`,
  не дублируйте строки имён в SQL-запросах UI.

## 9. utils/reportexporter.h — `ReportExporter`

- `exportXlsx(...)` — экспорт таблицы в `.xlsx` (QXlsx), `exportPdf(...)`/`exportCsv(...)`.

## 10. update/updatemanager.h — `UpdateManager`

- `currentVersion()` — версия из `config.json`.
- Сигналы: `updateAvailable(version, notes, url)`, `noUpdateAvailable()`, `checkFailed(error)`,
  `downloadProgress(received, total)`, `downloadFinished(filePath)`, `downloadFailed(error)`.
- Безопасность: манифест и бинарник проверяются по `sha256` из манифеста,
  при заданном `update.pinned_sha256` — SPKI-пиннинг сертификата (base64).

## 11. CLI-режимы приложения

| Флаг | Поведение | Код возврата |
|------|-----------|--------------|
| `--check-db` | Проверка подключения и применённых миграций (без GUI) | 0 — OK; 1 — нет соединения; 2 — есть незавершённые миграции |
| `--version` | Вывод версии из `config.json` | 0 |
| `-h` / `--help` | Справка | 0 |

## Тестирование

| Тест | Что покрывает |
|------|---------------|
| `test_password_utils` | Хеширование PBKDF2, constant-time сравнение, сложность пароля |
| `test_validator` | Валидаторы + fuzz-наборы (случайные входы, детерминированный seed) |
| `test_update_utils` | Парсинг/сравнение версий |
| `test_db_integration` | Миграции, аудит, роли, rate limiting, бизнес-поток, бэкап (в т.ч. шифрование) |
| `test_concurrency` | Гонки: номера документов, выдача SIM, массовая смена статуса, rate limiting |
| `test_ui_components` | Делегаты и модели UI |
| `test_connectionpool` | Пул соединений (QSQLITE) |
| `test_circuitbreaker` | Переходы Closed/Open/HalfOpen |

Запуск: `ctest --test-dir cmake-build-debug --output-on-failure`
(для интеграционных тестов требуется доступный PostgreSQL; без него тесты пропускаются через QSKIP).
