# POC Terminal Tracker

Система учёта POC-терминалов и SIM-карт. Разработана на Qt6 (C++17) с PostgreSQL.

## Функциональность

- **Справочники:** Производители, Модели, Клиенты, SIM-карты, Терминалы
- **Документы:**
  - Поступление терминалов на склад
  - Передача в аренду (с автоматическим обновлением статусов)
  - Возврат из аренды (с автоматическим сбросом статусов SIM и терминалов)
  - Отметка оплаты
- **Архивы:** Просмотр и фильтрация документов по датам и клиентам
- **Дашборд:** Счётчики свободных/занятых терминалов и SIM-карт, топ клиентов, последние документы

## Архитектура открытия форм

Все формы открываются через централизованный метод `MainWindow::openForm()`:

```cpp
void MainWindow::openForm(QWidget *form)
{
    form->setAttribute(Qt::WA_DeleteOnClose);
    form->setWindowModality(Qt::WindowModal);
    form->setWindowFlags(form->windowFlags() | Qt::Window);
    form->show();
    // Центрирование относительно главного окна
    QRect mainRect = this->geometry();
    QRect formRect = form->frameGeometry();
    form->move(mainRect.center() - formRect.center());
}
```

Это обеспечивает:
- Автоматическое удаление формы при закрытии (без утечек памяти)
- Модальность относительно главного окна
- Центрирование по центру главного окна

## Требования

- Qt 6.11.1+
- PostgreSQL
- CMake 3.20+

## Сборка

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Конфигурация

Файл `config/config.json` содержит настройки подключения к БД:

```json
{
  "database": {
    "host": "localhost",
    "port": 5432,
    "database": "pocbase",
    "username": "postgres",
    "password": "postgres"
  }
}
```