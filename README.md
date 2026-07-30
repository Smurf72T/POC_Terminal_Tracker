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
- **Вход/регистрация:** Аутентификация пользователей (SHA-256), роли admin/user
- **Управление пользователями:** Смена ролей, деактивация, сброс пароля (Сервис → Управление пользователями)
- **Печать документов:** Квитанция поступления, акт возврата, квитанция об оплате (HTML → QPrinter)
- **Глобальный поиск:** Ctrl+K, поиск по всем справочникам (ILIKE), открытие найденной формы
- **Переключалка темы:** Тёмная/светлая тема (кнопка в статусбаре, `modern.qss` ↔ `light.qss`)

## Требования

- Qt 6.11.1+ (с модулем Charts)
- PostgreSQL 17+
- CMake 3.20+
- MinGW (сборка через CLion)

## Сборка

```bash
mkdir build && cd build
cmake .. -G Ninja
cmake --build .
```

## Конфигурация

### Пароль базы данных

Пароль хранится в `.env` в корне проекта (не коммитится в репозиторий):

```env
POC_DB_HOST=localhost
POC_DB_PORT=5432
POC_DB_NAME=pocbase
POC_DB_USER=postgres
POC_DB_PASSWORD=postgres
```

Приоритет: `.env` > переменная окружения `POC_DB_PASSWORD` > `config/config.json`.

### Начальная настройка БД

1. Создайте базу данных `pocbase`
2. Выполните миграции из `sql/`:
   ```bash
   psql -U postgres -d pocbase -f sql/audit_log.sql
   ```
3. Дефолтный пользователь: `admin` / `admin123`

## Структура проекта

```
src/
  main.cpp                    — Точка входа, показ формы входа
  database/databasemanager.*  — Подключение к БД, выполнение запросов, .env
  ui/
    mainwindow.*              — Главное окно, дашборд
    dialogs/
      loginform.*             — Вход/регистрация
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
    validator.*               — Валидация ИНН, IMEI (Luhn), данных
    reportexporter.*          — Экспорт в Excel (QXlsx) и PDF
styles/
  light.qss                   — Светлая тема
sql/
  audit_log.sql               — Таблицы аудита, триггеры, tbl_users
libs/
  QXlsx/                      — Git submodule (QtExcel/QXlsx)
config/
  config.json                 — Настройки БД (пароль пустой, см. .env)
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
