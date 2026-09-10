-- Скрипт для диагностики несоответствий между терминалами и SIM-картами
-- Этот скрипт покажет все несогласованности в базе данных
-- Учитывает оба слота SIM терминала (слот 1 — currentsimcardid, слот 2 — currentsimcardid2)
-- и SIM, установленные документом «Установка SIM» (комплектация на складе): для них
-- штатное состояние «SIM status = 1 при свободном терминале».

-- ==================== ДИАГНОСТИКА ====================

-- 1. Показать все терминалы и их текущие SIM-карты (оба слота)
SELECT 
    t.terminalid AS "ID Терминала",
    t.serialnumber AS "Серийный номер",
    t.status AS "Статус терминала",
    CASE 
        WHEN t.status = 0 THEN 'Свободен'
        WHEN t.status = 1 THEN 'В аренде'
        ELSE 'Неизвестно'
    END AS "Статус терминала (текст)",
    t.currentsimcardid AS "ID SIM слот 1",
    s1.simnumber AS "Номер SIM 1",
    s1.status AS "Статус SIM 1",
    t.currentsimcardid2 AS "ID SIM слот 2",
    s2.simnumber AS "Номер SIM 2",
    s2.status AS "Статус SIM 2"
FROM tblterminals t
LEFT JOIN tblsimcards s1 ON t.currentsimcardid = s1.simcardid
LEFT JOIN tblsimcards s2 ON t.currentsimcardid2 = s2.simcardid
ORDER BY t.terminalid;

-- ==================== Выявить проблемные записи ====================

-- 2. Найти терминалы со статусом "свободен" (0), у которых есть привязанная SIM со статусом "в аренде" (1)
-- Исключаются SIM, установленные документом «Установка SIM» (комплектация на складе):
-- для них статус 1 при свободном терминале — штатное состояние
SELECT 
    'ПРОБЛЕМА: Терминал свободен, но SIM в аренде' AS "Тип проблемы",
    t.terminalid AS "ID Терминала",
    t.serialnumber AS "Серийный номер терминала",
    s.simcardid AS "ID SIM",
    s.simnumber AS "Номер SIM"
FROM tblterminals t
JOIN tblsimcards s ON (t.currentsimcardid = s.simcardid OR t.currentsimcardid2 = s.simcardid)
WHERE t.status = 0 
  AND s.status = 1
  AND NOT EXISTS (
      SELECT 1 FROM tblsiminstalldetails d
      WHERE d.terminalid = t.terminalid
        AND (d.simcardid = s.simcardid OR d.simcardid2 = s.simcardid)
  );

-- 3. Найти SIM-карты со статусом "в аренде" (1), которые не привязаны ни к одному терминалу
SELECT 
    'ПРОБЛЕМА: SIM в аренде, но не привязана к терминалу' AS "Тип проблемы",
    s.simcardid AS "ID SIM",
    s.simnumber AS "Номер SIM"
FROM tblsimcards s
LEFT JOIN tblterminals t ON (s.simcardid = t.currentsimcardid OR s.simcardid = t.currentsimcardid2)
WHERE s.status = 1 
  AND t.terminalid IS NULL;

-- 4. Найти терминалы со статусом "в аренде" (1), у которых нет привязанной SIM
SELECT 
    'ПРЕДУПРЕЖДЕНИЕ: Терминал в аренде, но нет SIM' AS "Тип проблемы",
    t.terminalid AS "ID Терминала",
    t.serialnumber AS "Серийный номер терминала"
FROM tblterminals t
WHERE t.status = 1 
  AND t.currentsimcardid IS NULL
  AND t.currentsimcardid2 IS NULL;

-- ==================== ИСПРАВЛЕНИЕ ====================

-- 5. ИСПРАВИТЬ: Сбросить статус SIM-карт, которые не привязаны к терминалам, но имеют статус "в аренде"
UPDATE tblsimcards
SET status = 0
WHERE status = 1
  AND simcardid NOT IN (
      SELECT currentsimcardid FROM tblterminals WHERE currentsimcardid IS NOT NULL
      UNION
      SELECT currentsimcardid2 FROM tblterminals WHERE currentsimcardid2 IS NOT NULL
  );

-- 6. ИСПРАВИТЬ: Сбросить статус SIM-карт, если терминал, к которому они привязаны, свободен
-- SIM, установленные документом «Установка SIM», НЕ сбрасываются (комплектация на складе)
UPDATE tblsimcards s
SET status = 0
WHERE s.status = 1
  AND EXISTS (
      SELECT 1
      FROM tblterminals t
      WHERE (t.currentsimcardid = s.simcardid OR t.currentsimcardid2 = s.simcardid)
        AND t.status = 0
        AND NOT EXISTS (
            SELECT 1 FROM tblsiminstalldetails d
            WHERE d.terminalid = t.terminalid
              AND (d.simcardid = s.simcardid OR d.simcardid2 = s.simcardid)
        )
  );

-- 7. Проверить результат после исправления (оба слота)
SELECT 
    t.terminalid AS "ID Терминала",
    t.serialnumber AS "Серийный номер",
    t.status AS "Статус терминала",
    s1.simcardid AS "ID SIM 1",
    s1.simnumber AS "Номер SIM 1",
    s1.status AS "Статус SIM 1",
    s2.simcardid AS "ID SIM 2",
    s2.simnumber AS "Номер SIM 2",
    s2.status AS "Статус SIM 2"
FROM tblterminals t
LEFT JOIN tblsimcards s1 ON t.currentsimcardid = s1.simcardid
LEFT JOIN tblsimcards s2 ON t.currentsimcardid2 = s2.simcardid
ORDER BY t.terminalid;