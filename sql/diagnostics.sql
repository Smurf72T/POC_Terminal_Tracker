-- Скрипт для диагностики несоответствий между терминалами и SIM-картами
-- Этот скрипт покажет все несогласованности в базе данных

-- ==================== ДИАГНОСТИКА ====================

-- 1. Показать все терминалы и их текущие SIM-карты
SELECT 
    t.terminalid AS "ID Терминала",
    t.serialnumber AS "Серийный номер",
    t.status AS "Статус терминала",
    CASE 
        WHEN t.status = 0 THEN 'Свободен'
        WHEN t.status = 1 THEN 'В аренде'
        ELSE 'Неизвестно'
    END AS "Статус терминала (текст)",
    t.currentsimcardid AS "ID Текущей SIM",
    s.simnumber AS "Номер SIM",
    s.status AS "Статус SIM",
    CASE 
        WHEN s.status = 0 THEN 'Свободна'
        WHEN s.status = 1 THEN 'В аренде'
        ELSE 'Неизвестно'
    END AS "Статус SIM (текст)"
FROM tblterminals t
LEFT JOIN tblsimcards s ON t.currentsimcardid = s.simcardid
ORDER BY t.terminalid;

-- ==================== ВЫявить проблемные записи ====================

-- 2. Найти терминалы со статусом "свободен" (0), у которых есть привязанная SIM с статусом "в аренде" (1)
-- Это НЕВОЗМОЖНО по логике: если терминал свободен, SIM не может быть в аренде
SELECT 
    'ПРОБЛЕМА: Терминал свободен, но SIM в аренде' AS "Тип проблемы",
    t.terminalid AS "ID Терминала",
    t.serialnumber AS "Серийный номер терминала",
    s.simcardid AS "ID SIM",
    s.simnumber AS "Номер SIM"
FROM tblterminals t
JOIN tblsimcards s ON t.currentsimcardid = s.simcardid
WHERE t.status = 0 
  AND s.status = 1;

-- 3. Найти SIM-карты со статусом "в аренде" (1), которые не привязаны ни к одному терминалу
SELECT 
    'ПРОБЛЕМА: SIM в аренде, но не привязана к терминалу' AS "Тип проблемы",
    s.simcardid AS "ID SIM",
    s.simnumber AS "Номер SIM"
FROM tblsimcards s
LEFT JOIN tblterminals t ON s.simcardid = t.currentsimcardid
WHERE s.status = 1 
  AND t.terminalid IS NULL;

-- 4. Найти терминалы со статусом "в аренде" (1), у которых нет привязанной SIM
SELECT 
    'ПРЕДУПРЕЖДЕНИЕ: Терминал в аренде, но нет SIM' AS "Тип проблемы",
    t.terminalid AS "ID Терминала",
    t.serialnumber AS "Серийный номер терминала"
FROM tblterminals t
WHERE t.status = 1 
  AND t.currentsimcardid IS NULL;

-- ==================== ИСПРАВЛЕНИЕ ====================

-- 5. ИСПРАВИТЬ: Сбросить статус SIM-карт, которые не привязаны к терминалам, но имеют статус "в аренде"
UPDATE tblsimcards
SET status = 0
WHERE status = 1
  AND simcardid NOT IN (
      SELECT currentsimcardid FROM tblterminals WHERE currentsimcardid IS NOT NULL
  );

-- 6. ИСПРАВИТЬ: Сбросить статус SIM-карт, если терминал, к которому они привязаны, свободен
UPDATE tblsimcards s
SET status = 0
WHERE s.status = 1
  AND EXISTS (
      SELECT 1 FROM tblterminals t 
      WHERE t.currentsimcardid = s.simcardid 
        AND t.status = 0
  );

-- 7. Проверить результат после исправления
SELECT 
    t.terminalid AS "ID Терминала",
    t.serialnumber AS "Серийный номер",
    t.status AS "Статус терминала",
    s.simcardid AS "ID SIM",
    s.simnumber AS "Номер SIM",
    s.status AS "Статус SIM"
FROM tblterminals t
LEFT JOIN tblsimcards s ON t.currentsimcardid = s.simcardid
ORDER BY t.terminalid;
