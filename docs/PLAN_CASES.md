# План: учёт чехлов для POC-терминалов

> Цель: справочник «Чехлы», документы «Поступление чехлов», «Установка чехлов»,
> «Списание чехлов» и архив по ним. Чехол — отдельная учитываемая единица
> (как SIM-карта), каждый чехол имеет статус: **0 — на складе, 1 — установлен,
> 2 — списан**. Один чехол на терминал (под модель «галочка» в аренде).

## Концепция

Чехлы моделируются по аналогии с SIM-картами:

- Каждая единица — запись в `tblcases` со статусом `0/1/2`.
- На терминале — поле `tblterminals.currentcaseid` (как `currentsimcardid`).
- Массовое оприходование — строками «тип × количество» в документе поступления
  (на проведении создаются N отдельных единиц в `tblcases`).
- Номера документов: `ПЧ-XXXXX` (поступление чехлов), `УЧ-XXXXX` (установка),
  `СЧ-XXXXX` (списание).
- Интеграция в аренду: колонка-галочка «Чехол» в табличной части
  `RentalForm`; авто-простановка, если у выбранного терминала чехол уже
  установлен документом «Установка чехлов» (по аналогии с автозаполнением SIM).

## Оглавление

- Шаг 1. План сохранён в файл
- Шаг 2. Миграция `015_case_docs.sql`
- Шаг 3. Data-слой (модели + `CaseRepository` + `CaseService`)
- Шаг 4. Формы: справочник чехлов, поступление, установка, списание
- Шаг 5. Архив, меню, дашборд, «Последние документы»
- Шаг 6. Интеграция в аренду/возврат
- Шаг 7. Отчёты (свободные чехлы, терминалы с чехлами, справочник терминалов)
- Шаг 8. CMake, сборка, тесты, README/CHANGELOG

---

## Шаг 1. План сохранён в файл — ✅ ВЫПОЛНЕНО (16.09.2026)

Создан `docs/PLAN_CASES.md`.

---

## Шаг 2. Миграция `sql/migrations/015_case_docs.sql` — ✅ ВЫПОЛНЕНО (16.09.2026)

Создан `sql/migrations/015_case_docs.sql`: `tblcases` (склад со статусами 0/1/2),
`tblterminals.currentcaseid`, документы поступления/установки/списания чехлов
(шапки + детали), `tblrentaldetails.has_case`, три последовательности номеров,
`generate_doc_number()` расширен типами `case_income`/`case_install`/
`case_writeoff`, `vwterminalsfull` пересоздан с колонками чехла.

---

## Шаг 2. Миграция `sql/migrations/015_case_docs.sql`

**Файлы:**
- `sql/migrations/015_case_docs.sql` (новая)

**Что делает:**

1. `CREATE TABLE tblcases` — `caseid SERIAL PK`, `casetype VARCHAR(100) NOT NULL`,
   `status SMALLINT DEFAULT 0 CHECK (0,1,2)`, `terminalid INT REFERENCES
   tblterminals`, `notes TEXT`, `createdat TIMESTAMP`.
   Индексы: `status`, `casetype`, `terminalid`.
2. `ALTER TABLE tblterminals ADD COLUMN currentcaseid INT REFERENCES tblcases`
   + `idx_terminals_current_case`.
3. Документ **поступления чехлов**: `tblcaseincomedocs` (шапка id/docnumber
   UNIQUE/docdate/comments/createdby/createdat),
   `tblcaseincomedetails` (id, docid FK, `casetype VARCHAR`, `qty INT CHECK(qty>0)`).
4. Документ **установки чехлов**: `tblcaseinstalldocs` +
   `tblcaseinstalldetails` (id, docid FK, `terminalid FK`, `caseid FK tblcases`).
5. Документ **списания чехлов**: `tblcasewriteoffdocs` +
   `tblcasewriteoffdetails` (id, docid FK, `caseid FK`, `reason TEXT`).
6. Для аренды: `ALTER TABLE tblrentaldetails ADD COLUMN has_case BOOLEAN NOT NULL DEFAULT FALSE`.
7. Последовательности `seq_case_income_doc_number`, `seq_case_install_doc_number`,
   `seq_case_writeoff_doc_number`; расширение `generate_doc_number()`:
   `case_income → ПЧ-`, `case_install → УЧ-`, `case_writeoff → СЧ-`.
8. Пересоздание `vwterminalsfull` (+ `currentcaseid`, `casetype`).

**Критерии готовности:** миграция применяется на чистой БД; `generate_doc_number`
возвращает номера трёх новых типов.

---

## Шаг 3. Data-слой (модели + `CaseRepository` + `CaseService`) — ✅ ВЫПОЛНЕНО (16.09.2026)

Созданы `src/models/casesdocument.h` (модели `CaseItem`/`CaseIncomeRow`/
`CaseInstallRow`/`CaseWriteoffRow`), `src/database/repositories/caserepository.h/.cpp`,
`src/services/caseservice.h/.cpp` (lock/free/writeoff/assignAnyFree, `FOR UPDATE NOWAIT`).
В `models::Terminal` и `TerminalRepository` добавлен `currentCaseId`.

**Файлы:**
- `src/models/casesdocument.h` (новая) — `models::CaseItem`, `CaseIncomeRow`,
  `CaseInstallRow`, `CaseWriteoffRow`, шапки через `models::DocumentHeader`.
- `src/models/rentaldocument.h` — `RentalRow` + `bool hasCase`.
- `src/database/repositories/caserepository.h/.cpp` (новая)
- `src/services/caseservice.h/.cpp` (новая)

**CaseRepository**: `loadFreeForSelection()`, `loadById/loadByIds`,
`loadByTerminal(id)`, `countByStatus`, `summarizeFree()` (остатки по типам),
`populateFreeCases(QSqlQueryModel*)`, `createBatch(casetype, qty)`, CRUD шапок/строк
трёх документов.

**CaseService** (паттерн `SimCardService`, `FOR UPDATE NOWAIT`):
- `lock(db, caseId, terminalId, error)` — status 0→1 + `currentcaseid`;
- `free(db, caseId, error)` — status 1→0 + сброс `currentcaseid`;
- `writeoff(db, caseId, error)` — status→2 + сброс `currentcaseid`;
- `assignAnyFree(db, terminalId, error)` — авто-выдача свободного чехла.

**Критерии готовности:** репозиторий покрыт тестами на SQLite-подсхеме.

---

## Шаг 4. Формы: справочник, поступление, установка, списание — ✅ ВЫПОЛНЕНО (16.09.2026)

Созданы `casesform.{h,cpp,ui}` (справочник на `SubmitErrorRelationalTableModel` с
делегатом статуса и присоединённым серийным номером терминала),
`caseincomeform*` (строки «тип × количество», `createBatch` + сверка количеств при
редактировании), `caseinstallform*` (комбобоксы терминал/чехол, авто-подстановка
текущего чехла, пустая ячейка = снятие, `CaseService::lock/free`),
`casewriteoffform*` (чекбокс-список чехлов 0/1 + причина, `CaseService::writeoff`).
Все на базе `DocumentDialog`, проведение в `*_post.cpp` в транзакции.

Все документы на базе `DocumentDialog` (паттерн `siminstallform*`), проведение —
в отдельных `*_post.cpp`, транзакции — `TransactionGuard`.

**Файлы:**
- `src/ui/dialogs/casesform.{h,cpp,ui}` — справочник чехлов (копия `simcardsform`:
  ID, тип, статус-делегат, серийный номер терминала, примечание; удаление только
  при status=0).
- `src/ui/dialogs/caseincomeform.{h,cpp,ui}` + `caseincomeform_post.cpp` —
  табличная часть `[Тип чехла (редактируемый комбобокс), Количество]`;
  на проведении `createBatch(casetype, qty)`.
- `src/ui/dialogs/caseinstallform.{h,cpp,ui}` + `caseinstallform_post.cpp` —
  `[Терминал (свободные), Чехол (свободные)]`; авто-подстановка текущего чехла
  при выборе терминала; пустая ячейка чехла = снятие.
- `src/ui/dialogs/casewriteoffform.{h,cpp,ui}` + `casewriteoffform_post.cpp` —
  список чехлов со статусом 0/1 + чекбоксы + причина; `writeoff` по отмеченным.

**Критерии готовности:** проведение/редактирование трёх документов работает;
статусы и привязки чехлов корректны.

---

## Шаг 5. Архив, меню, дашборд, «Последние документы»

**Файлы:**
- `src/ui/dialogs/archivedocumentsform.{h,cpp}` — типы 7 (Поступление чехлов),
  8 (Установка чехлов), 9 (Списание чехлов): заголовки, запросы в `applyFilter`,
  двойной клик, экспорт.
- `src/ui/mainwindow.ui` — пункты меню: Справочники→Чехлы, Документы→3 документа,
  Архивы→3 архива, Отчёты→Терминалы с чехлами.
- `src/ui/mainwindow.cpp` — подключение действий (`openForm`).
- `src/database/repositories/documentrepository.{h,cpp}` — UNION 7/8/9 в
  `recentDocuments/populateRecentDocuments` и `loadHeader`; `onRecentDocActivated`.

**Критерии готовности:** документы видны в архивах и «Последних документах»,
открываются на редактирование.

---

## Шаг 6. Интеграция в аренду/возврат — ✅ ВЫПОЛНЕНО (16.09.2026)

**Файлы:**
- `src/ui/dialogs/rentalform.{h,cpp,ui}` — колонка «Чехол» (галочка,
  `CheckBoxDelegate`, ширина 70); карта `m_installedCase` (terminalId → {caseId, тип});
  авто-простановка в `autoFillCaseForTerminal`/`onTableViewDataChanged`;
  восстановление галочки из `has_case` в `loadSpecificEditData`.
- `src/ui/dialogs/rentalform_post.cpp`:
  - галочка есть, чехла нет → `CaseService::assignAnyFree` (ошибка, если пусто);
  - галочка есть, чехол установлен → `has_case = TRUE`;
  - галочки нет, а чехол не был установлен «Установкой чехлов» → освобождение
    (вспомогательная `wasCaseInstalledByDoc`);
  - запись `has_case` в `tblrentaldetails`; изъятие терминала из документа тоже
    возвращает автоматически выданный чехол на склад.
- `src/models/rentaldocument.h` — `RentalRow::hasCase`; `documentrepository.cpp`
  загружает `has_case`.
- `src/ui/dialogs/returnform_post.cpp` — при возврате чехол остаётся установленным
  (как SIM из «Установка SIM»).

**Критерии готовности:** выбор терминала с чехлом проставляет галочку; проведение
аренды корректно распределяет/освобождает чехлы; возврат сохраняет чехол.

---

## Шаг 7. Отчёты (свободные чехлы, терминалы с чехлами, справочник терминалов) — ✅ ВЫПОЛНЕНО (16.09.2026)

**Файлы:**
- `src/ui/dialogs/freedevicesreportdialog.{h,cpp}` — панель «Свободные чехлы»
  (остатки по типам + итог) и кнопка экспорта.
- `src/ui/dialogs/caseterminalsreportdialog.{h,cpp}` (новая) — отчёт
  «Терминалы с чехлами»: терминал, модель, статус, наличие чехла, тип чехла;
  фильтр «все/с чехлом/без»; экспорт.
- `src/ui/dialogs/terminalsform.cpp` — колонка «Чехол» в справочнике терминалов
  (LEFT JOIN `tblcases c ON t.currentcaseid = c.caseid`).

**Критерии готовности:** отчёты показывают корректные остатки и присутствие чехлов.

---

## Шаг 8. CMake, сборка, тесты, README/CHANGELOG — ✅ ВЫПОЛНЕНО (16.09.2026)

**Файлы:**
- `cmake/sources.cmake` — `casesform/caseincomeform/caseinstallform/casewriteoffform`
  (+`*_post.cpp`), `caserepository`, `caseservice`, `casesdocument.h`,
  `CheckBoxDelegate.h`, `caseterminalsreportdialog`.
- `cmake/tests.cmake` — `caserepository.cpp` добавлен к `test_repositories`.
- `tests/test_repositories.{cpp,_seed,_query,_load}` — SQLite-схема дополнена
  таблицами чехлов, `tblterminals.currentcaseid`, `tblrentaldetails.has_case`;
  новый тест `caseOperations` (поступление `createBatch`, остатки, установка/снятие,
  строки трёх документов, отсутствие двойного нахождения в свободных).
- `tests/test_db_integration_schema.cpp`, `test_db_integration_backup.cpp`,
  `test_concurrency_numbers.cpp` — число миграций 16, новые последовательности и
  префиксы номеров (`ПЧ-`/`УЧ-`/`СЧ-`).
- `README.md`, `CHANGELOG.md` — описание учёта чехлов (v1.8.0).
- FiX: недостающие include `ui_case*form.h` в `case*form_post.cpp`,
  `<QSqlRelationalDelegate>` в `casesform.cpp`.

**Критерии готовности:** проект собирается, `ctest` зелёные (15/15), `clang-format`
— таргет `lint` пропущен (clang-format не установлен в окружении).

---

## Важные решения

- Галочка в аренде **без выбора конкретного чехла** → авто-выдача любого
  свободного. При необходимости выбора — отдельная колонка-список.
- «Автоматически проведётся по всем нужным документам» — реализовано как
  проведение операций в той же транзакции аренды (статусы + привязки), без
  генерации дополнительных физических документов (консистентно с SIM).
- Единицы чехлов учитываются **поштучно**; массовое поступление — строкой
  «тип × количество».

## Методы проверки

- Сборка: `cmake --build build`
- Тесты: `ctest --output-on-failure`
- Линт: `cmake --build build --target lint`