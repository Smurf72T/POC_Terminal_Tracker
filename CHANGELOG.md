# Changelog

## [1.0.0] — 2026-07-30

### Security
- **SQL-инъекции устранены**: все LIKE-фильтры переписаны через `QSqlField::formatValue()` (clientsform, modelsform, simcardsform, manufacturersform)
- **PBKDF2-HMAC-SHA256** с 100k итераций для хранения паролей вместо SHA-256+salt; авто-апгрейд при логине
- **Rate limiting**: блокировка входа на 30 сек после 5 неудачных попыток
- **SSL принудительно**: удалён fallback на unprotected-соединение с PostgreSQL
- **Больше никаких паролей на диске**: бекап через `PGPASSWORD` (env var), `.env` исключён из сборки

### Features
- **Система миграций БД**: авто-применение `sql/migrations/*.sql` при запуске
- **Ролевая защита UI**: скрытие admin-меню для обычных пользователей
- **Принудительная смена пароля** при первом входе
- **QLoggingCategory**: логи через `logDB`, `logAudit`, `logMigration`, `logSQL`, `logApp` вместо `qDebug()`

### Performance & Stability
- **Database indexes**: 25 индексов + CHECK-ограничения + UNIQUE на serialnumber
- **Утечка памяти в графиках**: оси удаляются перед обновлением `updateCharts()`
- **Проверка `query.exec()`**: 11 вызовов обёрнуты в `if (!exec()) { qDebug() << error; }`

### Quality
- **Unit-тесты**: `test_password_utils` (6 тестов для PBKDF2, обратная совместимость), `test_validator` (15 тестов для IMEI, INN, Luhn, серийных номеров)
- **CMake**: смягчена версия Qt до 6.2, проверка наличия DLL-путей, опция `BUILD_TESTS`

### Bugfixes
- **`QMessageBox::critical(nullptr,...)`**: заменён на parent-окно или `QApplication::activeWindow()`
- **Беказ без `.pgpass`**: пароль через PGPASSWORD вместо временного файла
- **Мусор из индекса удалён**: `stderr.txt`, `stdout.txt`, `test_result.txt`
