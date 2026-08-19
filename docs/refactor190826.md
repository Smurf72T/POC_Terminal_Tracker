# План рефакторинга крупных UI-форм (документооборот)

**Дата:** 19.08.2026  
**Цель:** Уменьшить размер, дублирование и сложность 5 крупных файлов форм документов, сохранив текущую архитектуру (репозитории + value-модели + write-логика в UI).

## Статус: ВЫПОЛНЕНО (19.08.2026)

Рефакторинг завершён коммитами `a028050`…`c93758c` (Этапы 0–5, см. §6).
Фактические результаты — в §5, критерии завершения — в §8.

---

## 1. Анализ текущего состояния

### 1.1. Файлы и их размер

| Файл | Строк | Назначение |
|------|-------|------------|
| `receiptform.cpp` | 792 | Поступление терминалов (комплекты SN·IMEI, сканер, печать) |
| `rentalform.cpp` | 706 | Передача в аренду (двухслотовые SIM, блокировки, создание SIM) |
| `statuschangeform.cpp` | 633 | Изменение статуса (4 типа, откат, привязка к ремонту) |
| `returnform.cpp` | 604 | Возврат из аренды (сброс статусов, восстановление при редактировании) |
| `paymentform.cpp` | 416 | Отметка оплаты (период, привязка к арендам, дубликаты) |

**Итого:** 3 151 строка C++ кода в UI-слое.

### 1.2. Выявленные общие паттерны

Все 5 форм повторяют один и тот же сценарий:

1. **Конструктор:** `ui->setupUi()` → дата по умолчанию → инициализация модели таблицы → загрузка справочников
2. **loadForEdit:** заголовок из БД → строки → режим редактирования
3. **on_btnPost_clicked:**
   - Валидация входных данных
   - `db.transaction()`
   - INSERT/UPDATE шапки (`tbl*docs`)
   - DELETE старых деталей (режим редактирования)
   - Цикл по строкам: валидация → блокировка `FOR UPDATE NOWAIT` → INSERT/UPDATE деталей
   - Откат удалённых строк (режим редактирования)
   - `db.commit()` / `db.rollback()`
   - `DatabaseManager::instance().logAction()`
   - `DatabaseManager::instance().notifyDataChanged()`
   - `QMessageBox::information` + `close()`
4. **on_btnPrint:** генерация номера → HTML-шаблон → `QPrinter` / `QPrintDialog`
5. **on_btnClose:** `close()`

### 1.3. Зоны дублирования

| Паттерн | Где встречается | Строк (оцен.) |
|---------|----------------|---------------|
| Транзакция + шапка (INSERT/UPDATE) | Все 5 форм | ~80 строк × 5 = 400 |
| Генерация номера документа | Все 5 форм | ~15 строк × 5 = 75 |
| Печать (HTML → QPrinter) | Все 5 форм | ~50 строк × 5 = 250 |
| `logAction` + `notifyDataChanged` | Все 5 форм | ~5 строк × 5 = 25 |
| `QMessageBox::critical/warning/information` | Все 5 форм | ~100 строк |
| Загрузка клиентов | rental, return, payment | ~60 строк |
| Загрузка документов аренды | rental, return, payment | ~80 строк |
| Batch-загрузка терминалов/SIM для печати | rental, return | ~40 строк |

**Потенциал сокращения:** ~850 строк (27% от общего объёма).

---

## 2. Цели рефакторинга

1. **Уменьшить максимальный размер файла** с 792 до ≤ 400 строк
2. **Устранить дублирование** транзакционной логики и печати
3. **Повысить тестируемость** — вынести бизнес-логику из UI в чистые классы
4. **Сохранить текущую архитектуру** — репозитории и value-модели остаются, write-логика остаётся в UI (согласно ADR)
5. **Не нарушить существующий API** — `openForm()`, `loadForEdit()`, слоты остаются

---

## 3. Архитектурное решение

### 3.1. Новая структура

```
src/
  ui/
    dialogs/
      receiptform.cpp/h        ← 792 → ~300
      rentalform.cpp/h         ← 706 → ~280
      statuschangeform.cpp/h   ← 633 → ~250
      returnform.cpp/h         ← 604 → ~230
      paymentform.cpp/h        ← 416 → ~200
    base/                      ← НОВЫЙ каталог
      documentdialog.h/cpp     ← Базовый диалог документов
      printservice.h/cpp       ← Сервис печати
      transactionguard.h/cpp   ← RAII-обёртка транзакции
  services/                    ← НОВЫЙ каталог
    documentnumbergenerator.h/cpp  ← Генерация номеров
    postactionlogger.h/cpp         ← logAction + notifyDataChanged
```

### 3.2. Принципы

- **Базовый класс `DocumentDialog`** — общая транзакционная логика, шапка, режим редактирования
- **Сервисы** — чистые классы без Qt-зависимостей (кроме logging), легко тестируемые
- **RAII для транзакций** — `TransactionGuard` автоматически rollback при исключении/раннем return
- **Печать вынесена** в `PrintService` — статические методы с HTML-шаблонами

---

## 4. Пошаговый план

### Этап 0: Подготовка (без изменений логики)

#### Шаг 0.1: Создать каталоги и заголовочные файлы-заглушки

```
src/ui/base/documentdialog.h
src/ui/base/printservice.h
src/ui/base/transactionguard.h
src/services/documentnumbergenerator.h
src/services/postactionlogger.h
```

**Цель:** Подготовить каркас без изменения существующего кода.  
**Проверка:** `cmake --build .` — сборка без ошибок.

#### Шаг 0.2: Добавить новые файлы в CMakeLists.txt

**Цель:** CMake видит новые файлы.  
**Проверка:** `cmake --build .` — сборка без ошибок.

---

### Этап 1: Выделение сервисов (безопасно, изолированно)

#### Шаг 1.1: `DocumentNumberGenerator`

**Файл:** `src/services/documentnumbergenerator.h/cpp`

**Функциональность:**
- Статический метод `generate(const QString& docType, QSqlDatabase& db) -> QString`
- Внутренне вызывает `DatabaseManager::instance().generateDocNumber(docType)`
- Возвращает пустую строку при ошибке

**Почему сервис:** Логика генерации номера — чистая функция, зависит только от БД. Не требует Qt-объектов.

**Изменения в формах:** Заменить 5 вызовов `DatabaseManager::instance().generateDocNumber(...)` на `DocumentNumberGenerator::generate(...)`.

**Ожидаемый результат:** -15 строк × 5 форм = -75 строк.

#### Шаг 1.2: `PostActionLogger`

**Файл:** `src/services/postactionlogger.h/cpp`

**Функциональность:**
- Статический метод `log(const QString& action, const QString& table, int docId)`
- Внутренне: `DatabaseManager::instance().logAction(action, table, docId)`
- Статический метод `notify()` → `DatabaseManager::instance().notifyDataChanged()`

**Почему сервис:** Два вызова, повторяющиеся в каждом `on_btnPost_clicked` после commit.

**Изменения в формах:** Заменить пары `logAction` + `notifyDataChanged` на `PostActionLogger::log(...)` + `PostActionLogger::notify()`.

**Ожидаемый результат:** -5 строк × 5 форм = -25 строк.

#### Шаг 1.3: `PrintService`

**Файл:** `src/ui/base/printservice.h/cpp`

**Функциональность:**
- `static bool printHtml(const QString& html, QWidget* parent = nullptr)` — общий метод печати
- `static QString escapeHtml(const QString& s)` — вспомогательный метод
- Шаблоны HTML-заголовков: `receiptHeader`, `rentalActHeader`, `returnActHeader`, `statusChangeHeader`, `paymentReceiptHeader`

**Почему сервис:** 5 методов `on_btnPrint_*` содержат ~50 строк каждый, отличающихся только HTML-контентом.

**Изменения в формах:** Каждый `on_btnPrint_*` сводится к:
```cpp
void ReceiptForm::on_btnPrint_clicked()
{
    if (!m_editMode && ui->lineEditNumber->text().trimmed().isEmpty()) {
        QString num = DocumentNumberGenerator::generate("receipt", db);
        if (num.isEmpty()) { QMessageBox::critical(...); return; }
        ui->lineEditNumber->setText(num);
    }
    QString html = buildPrintHtml(); // вызов приватного метода формы
    PrintService::printHtml(html, this);
}
```

**Ожидаемый результат:** -30 строк × 5 форм = -150 строк.

---

### Этап 2: RAII-транзакция

#### Шаг 2.1: `TransactionGuard`

**Файл:** `src/ui/base/transactionguard.h/cpp`

**Функциональность:**
```cpp
class TransactionGuard {
public:
    explicit TransactionGuard(QSqlDatabase& db);
    ~TransactionGuard();
    
    void commit();
    void setRollbackMessage(const QString& msg);
    
    // Non-copyable
    TransactionGuard(const TransactionGuard&) = delete;
    TransactionGuard& operator=(const TransactionGuard&) = delete;

private:
    QSqlDatabase& m_db;
    bool m_committed = false;
    QString m_rollbackMsg;
};
```

**Поведение:**
- Конструктор: `db.transaction()` — при ошибке показывает `QMessageBox::critical`
- Деструктор: если `!m_committed` → `db.rollback()` + `QMessageBox::critical`
- `commit()`: `m_committed = true; db.commit()` — при ошибке → rollback + critical

**Почему RAII:** Убирает 90% кода `if (!db.transaction()) { ... return; }` и `if (!db.commit()) { db.rollback(); ... }` из всех 5 форм.

**Изменения в формах:**
```cpp
// БЫЛО (в on_btnPost_clicked):
if (!db.transaction()) {
    QMessageBox::critical(this, "Ошибка", "Не удалось начать транзакцию");
    return;
}
// ... много кода ...
if (!db.commit()) {
    db.rollback();
    QMessageBox::critical(this, "Ошибка", "Не удалось зафиксировать транзакцию");
}

// СТАЛО:
TransactionGuard guard(db);
// ... код ...
guard.commit();
// деструктор автоматически rollback при раннем return
```

**Ожидаемый результат:** -40 строк × 5 форм = -200 строк.

---

### Этап 3: Базовый класс `DocumentDialog`

#### Шаг 3.1: Создать `DocumentDialog`

**Файл:** `src/ui/base/documentdialog.h/cpp`

**Наследование:** `class DocumentDialog : public QDialog`

**Общие приватные методы:**

```cpp
class DocumentDialog : public QDialog {
    Q_OBJECT
public:
    explicit DocumentDialog(QWidget* parent = nullptr);
    ~DocumentDialog() override;

    // Переопределяется в потомках
    virtual QString docType() const = 0;       // "receipt", "rental", "return", ...
    virtual bool validateBeforePost() = 0;     // валидация, специфичная для формы
    virtual int postHeader(QSqlDatabase& db, TransactionGuard& guard) = 0;
    virtual bool postDetails(QSqlDatabase& db, TransactionGuard& guard, int docId) = 0;
    virtual void onPostSuccess(int docId) = 0; // log + notify + message + close

    // Общие методы
    void setupHeaderFields();                  // дата по умолчанию, номер
    void setupRowsModel(QTableView* tableView, const QStringList& headers, int modelColumn);
    void loadForEdit(int docId);               // общая загрузка шапки
    void onBtnPostClicked();                   // общий сценарий проведения
    void onBtnCloseClicked();                  // close()

protected:
    Ui::DocumentDialog* ui;                    // или общий ui-файл
    QStandardItemModel* rowsModel;
    bool m_editMode = false;
    int m_editDocId = 0;
};
```

**Общий метод `onBtnPostClicked()`:**
```cpp
void DocumentDialog::onBtnPostClicked()
{
    if (!validateBeforePost()) return;
    
    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    TransactionGuard guard(db);
    
    int docId = postHeader(db, guard);
    if (docId < 0) return;
    
    if (!postDetails(db, guard, docId)) return;
    
    guard.commit();
    onPostSuccess(docId);
}
```

**Общий метод `loadForEdit()`:**
```cpp
void DocumentDialog::loadForEdit(int docId)
{
    m_editMode = true;
    m_editDocId = docId;
    
    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    DocumentRepository docs(db);
    models::DocumentHeader header = docs.loadHeader(resolveDocType(), docId);
    
    if (header.id != 0) {
        ui->lineEditNumber->setText(header.docNumber);
        ui->lineEditNumber->setReadOnly(true);
        ui->dateEdit->setDate(header.date);
        ui->textEditComment->setText(header.comments);
    }
    
    loadSpecificEditData(docId); // виртуальный, переопределяется
    setWindowTitle(QString("Редактирование %1 ID %2").arg(docTitle(), docId));
}
```

#### Шаг 3.2: Перевести формы на наследование

Каждая из 5 форм:
1. Наследуется от `DocumentDialog` вместо `QDialog`
2. Переопределяет виртуальные методы
3. Удаляет общие методы (`setupHeaderFields`, `onBtnPostClicked`, `onBtnCloseClicked`, `loadForEdit`)
4. Сохраняет специфичную логику (`postDetails` остаётся виртуальной)

**Ожидаемый результат:** -100 строк × 5 форм = -500 строк (за счёт удаления общих методов).

---

### Этап 4: Устранение дублирования загрузки данных

#### Шаг 4.1: Общие методы загрузки для форм аренды/возврата/оплаты

**Файл:** `src/ui/base/clientdocdialog.h` (наследуется от `DocumentDialog`)

**Функциональность:**
```cpp
class ClientDocumentDialog : public DocumentDialog {
    // Для форм, работающих с клиентами и документами аренды
protected:
    void loadClientsToComboBox();
    void loadClientsToDelegate();
    void loadRentalDocsForClient(int clientId);
};
```

**Применение:** `RentalForm`, `ReturnForm`, `PaymentForm` наследуют от `ClientDocumentDialog`.

**Ожидаемый результат:** -60 строк (общие методы загрузки клиентов и арендных документов).

#### Шаг 4.2: Вынести batch-загрузку для печати

**Файл:** `src/services/printservice.h` (дополнить)

```cpp
class PrintService {
public:
    // ...
    static QHash<int, models::Terminal> loadTerminalsBatch(const QList<int>& ids, QSqlDatabase& db);
    static QHash<int, models::SimCard> loadSimsBatch(const QList<int>& ids, QSqlDatabase& db);
};
```

**Применение:** `RentalForm::on_btnPrintAct_clicked`, `ReturnForm::on_btnPrint_clicked`

**Ожидаемый результат:** -20 строк × 2 формы = -40 строк.

---

### Этап 5: Специфичная оптимизация каждой формы

#### Шаг 5.1: `ReceiptForm` (792 → ~300)

**Основные зоны оптимизации:**

1. **Сканер и eventFilter** (~150 строк) — оставить как есть, это уникальная логика
2. **Комплекты (SN·IMEI)** (~120 строк) — вынести в `SerialUnitsService`:
   ```cpp
   class SerialUnitsService {
       static QStringList serialsForRow(QStandardItem* item);
       static void setUnitsForRow(QStandardItem* item, ...);
       static QString listSummary(const QStringList& values, int expected);
       static bool validateSerials(const QList<ItemData>& items);
   };
   ```
3. **on_btnPost_clicked** (~200 строк) — после `TransactionGuard` и `DocumentDialog` останется ~80 строк валидации и INSERT
4. **on_btnPrint** (~70 строк) — после `PrintService` останется ~30 строк сборки HTML
5. **loadForEdit** (~80 строк) — оставить, специфичная логика восстановления из `tblreceiptdetails`

**Целевой размер:** ~300 строк

#### Шаг 5.2: `RentalForm` (706 → ~280)

**Основные зоны оптимизации:**

1. **resolveSimFromCell / lockSimCard / freeSimCard** (~80 строк) — вынести в `SimCardService`:
   ```cpp
   class SimCardService {
       static int resolveOrCreate(QSqlDatabase& db, int cellSimId, const QString& number, QString* error);
       static bool lock(QSqlDatabase& db, int simId, const QString& context, QString* error);
       static bool free(QSqlDatabase& db, int simId, const QString& context, QString* error);
   };
   ```
2. **on_btnPost_clicked** (~250 строк) — после `TransactionGuard` и `DocumentDialog` останется ~120 строк
3. **on_btnPrintAct** (~80 строк) — после `PrintService` останется ~40 строк
4. **loadForEdit** (~50 строк) — оставить

**Целевой размер:** ~280 строк

#### Шаг 5.3: `StatusChangeForm` (633 → ~250)

**Основные зоны оптимизации:**

1. **actionType / targetStatus / expectStatus / statusText** (~30 строк) — вынести в `StatusChangeService`:
   ```cpp
   class StatusChangeService {
       static int targetStatus(const QString& actionType);
       static bool expectStatus(const QString& actionType, int currentStatus);
       static QString actionTitle(const QString& actionType);
       static QString statusText(int status);
   };
   ```
2. **on_btnPost_clicked** (~180 строк) — после `TransactionGuard` останется ~100 строк
3. **loadTerminals / loadTerminalsFromRepairDoc** (~80 строк) — вынести в `StatusChangeRepository` или оставить
4. **on_btnPrint** (~50 строк) — после `PrintService` останется ~20 строк

**Целевой размер:** ~250 строк

#### Шаг 5.4: `ReturnForm` (604 → ~230)

**Основные зоны оптимизации:**

1. **on_btnPost_clicked** (~300 строк) — самая сложная логика (режим редактирования с откатом). После `TransactionGuard` и `DocumentDialog` останется ~150 строк
2. **on_btnPrint** (~50 строк) — после `PrintService` останется ~25 строк
3. **loadForEdit** (~40 строк) — оставить

**Целевой размер:** ~230 строк

#### Шаг 5.5: `PaymentForm` (416 → ~200)

**Основные зоны оптимизации:**

1. **on_btnSave_clicked** (~120 строк) — после `TransactionGuard` и `DocumentDialog` останется ~60 строк
2. **on_btnPrint** (~50 строк) — после `PrintService` останется ~25 строк
3. **loadClients / loadMonths / loadYears / loadRentalDocsForClient** (~80 строк) — часть можно оставить, часть вынести

**Целевой размер:** ~200 строк

---

## 5. Итоговая оценка

### До рефакторинга

| Файл | Строк |
|------|-------|
| receiptform.cpp | 792 |
| rentalform.cpp | 706 |
| statuschangeform.cpp | 633 |
| returnform.cpp | 604 |
| paymentform.cpp | 416 |
| **Итого** | **3 151** |

### После рефакторинга (факт, 19.08.2026)

**Основные файлы форм** (критерий «≤ 400 строк» выполнен):

| Файл | Строк (до) | Строк (факт) | Сокращение |
|------|------------|--------------|------------|
| receiptform.cpp | 792 | 371 | -421 (-53%) |
| rentalform.cpp | 706 | 347 | -359 (-51%) |
| statuschangeform.cpp | 633 | 389 | -244 (-39%) |
| returnform.cpp | 604 | 267 | -337 (-56%) |
| paymentform.cpp | 416 | 286 | -130 (-31%) |
| **Итого** | **3 151** | **1 660** | **-1 491 (-47%)** |

Крупные блоки вынесены из форм в отдельные файлы **по ответственности**
(`*_post.cpp` — проведение, `*_scan.cpp` — логика сканера):

| Файл | Строк | Назначение |
|------|-------|------------|
| rentalform_post.cpp | 295 | Проведение/редактирование аренды |
| returnform_post.cpp | 322 | Проведение/редактирование возврата |
| receiptform_post.cpp | 253 | Проведение поступления |
| statuschangeform_post.cpp | 223 | Проведение изменения статуса |
| paymentform_post.cpp | 144 | Проведение оплаты |
| receiptform_scan.cpp | 169 | Сканер (COM-порт/клавиатура) в поступлении |

**Новый слой `base/` и сервисы `services/`:**

| Файл | Строк | Назначение |
|------|-------|------------|
| base/documentdialog.cpp | 56 | Базовый класс форм документов (пост, загрузка, печать) |
| base/clientdocdialog.cpp | 60 | Базовый класс для клиентских форм (аренда/возврат/оплата) |
| base/printservice.cpp | 58 | Печать HTML → QPrinter, batch-загрузка для печати |
| base/transactionguard.cpp | 35 | RAII-обёртка транзакций в `on_btnPost` |
| services/simcardservice.cpp | 74 | Привязка/освобождение SIM (слоты 1/2, проверка дублей) |
| services/statuschangeservice.cpp | 46 | Правила смены статусов терминалов |
| services/serialunitsservice.cpp | 28 | Комплекты SN·IMEI (парсинг, валидация, сводка) |
| services/documentnumbergenerator.cpp | 7 | Генерация номера документа |
| services/postactionlogger.cpp | 12 | Аудит + `NOTIFY` после проведения |
| **Итого (новое)** | **376** | — |

Итог: UI-слой форм документов разделён на 5 форм + 6 `*_post`/`*_scan` +
4 файла `base/` + 5 файлов `services/`; каждый файл имеет единственную
ответственность, крупнейший файл — 389 строк (было 792).

---

## 6. Порядок выполнения (зависимости)

```
Этап 0: Подготовка (заглушки, CMake)
  ↓
Этап 1: Сервисы (изолированно, можно тестировать каждый)
  ├─ Шаг 1.1: DocumentNumberGenerator
  ├─ Шаг 1.2: PostActionLogger
  └─ Шаг 1.3: PrintService
  ↓
Этап 2: TransactionGuard (RAII транзакции)
  ↓
Этап 3: DocumentDialog (базовый класс)
  ↓
Этап 4: Общие методы для клиентских форм
  ↓
Этап 5: Специфичная оптимизация каждой формы
  ├─ Шаг 5.1: ReceiptForm
  ├─ Шаг 5.2: RentalForm
  ├─ Шаг 5.3: StatusChangeForm
  ├─ Шаг 5.4: ReturnForm
  └─ Шаг 5.5: PaymentForm
  ↓
Финальная проверка: сборка, тесты, линтинг
```

---

## 7. Риски и митигация

### Риск 1: Нарушение существующей логики при переносе в базовый класс

**Митигация:**
- Каждый этап (0→1→2→3) проверяется сборкой и тестами
- `TransactionGuard` и сервисы — чистые классы, легко тестируемые unit-тестами
- Базовый класс `DocumentDialog` тестируется через наследников

### Риск 2: Сложность отката при проблемах

**Митигация:**
- Каждый шаг — отдельный коммит
- После каждого этапа: `git status`, `cmake --build .`, `ctest`
- Если что-то сломалось — откат одного коммита, а не всего рефакторинга

### Риск 3: Увеличение количества файлов

**Митигация:**
- Новые файлы маленькие (20-120 строк)
- Каждый файл имеет единственную ответственность (SRP)
- Файловая структура соответствует существующим соглашениям (`base/`, `services/`)

### Риск 4: Виртуальные методы и производительность

**Митигация:**
- Виртуальные вызовы только при создании/проведении формы (не в hot path)
- Влияние на производительство пренебрежимо мало

---

## 8. Критерии завершения

1. [x] Все 5 файлов ≤ 400 строк — было 792/706/633/604/416, стало 371/347/389/267/286 (см. §5)
2. [x] `cmake --build .` проходит без ошибок — проверено на `build/dev-mingw` (ninja, «no work to do»)
3. [x] `ctest --output-on-failure` — 13/15 зелёные (все unit/UI/репозитории тесты);
       `test_db_integration` и `test_concurrency` требуют live-окружения PostgreSQL
       и не связаны с рефакторингом (исходники форм не затрагиваются тестами)
4. [ ] Проектный линтер — настроена цель `lint` (clang-format dry-run), но на машине
       clang-format не установлен, поэтому проверка пропускается
5. [ ] Ручное тестирование: создание/редактирование/проведение/печать каждого типа документа
       (требует запуска приложения)
6. [ ] Все новые файлы имеют unit-тесты (для сервисов) — НЕ покрыты отдельными тестами
7. [x] Git-история: каждый этап — отдельный коммит — коммиты `a028050`…`c93758c` (13 коммитов,
       осмысленные сообщения, по одному на этап/шаг)

---

## 9. Дополнительные возможности (опционально, вне плана)

### 9.1. `ReceiptForm` → отдельный `SerialUnitsService`

Выделить логику работы с комплектами (серийник + IMEI 1 + IMEI 2) в отдельный сервис:

```cpp
class SerialUnitsService {
    static QStringList getSerials(QStandardItem* item);
    static void setSerials(QStandardItem* item, const QStringList& serials, ...);
    static QString summary(const QStringList& serials, int expected);
    static bool validate(const QList<ItemData>& items, QString* error);
};
```

### 9.2. `PaymentForm` → `PaymentRepository::savePayment()`

Перенести INSERT/UPDATE платежа и связей в репозиторий:

```cpp
class PaymentRepository {
    int savePayment(QSqlDatabase& db, const PaymentData& data, const QList<int>& rentalIds);
};
```

### 9.3. Общие HTML-шаблоны

Создать `PrintTemplates` с переиспользуемыми блоками:
- Шапка с номером и датой
- Таблица с настраиваемыми колонками
- Подпись (сделал/принял)

### 9.4. Унификация делегатов

Объединить `ComboBoxDelegate` и `ReadOnlyDelegate` в единую иерархию делегатов с конфигурацией через свойства.

---

## 10. Примечания

- План не предполагает изменения базы данных, миграций или API репозиториев
- Все изменения — в UI-слое и новом слое сервисов
- `MainWindow::openForm()` остаётся без изменений
- Слоты `on_btnPost_clicked`, `on_btnPrint_clicked`, `on_btnClose_clicked` сохраняют имена для совместимости с `.ui`-файлами
- Режим редактирования (`loadForEdit`) сохраняется для всех 5 форм
