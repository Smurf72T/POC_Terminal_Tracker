-- Скрипт для диагностики и исправления статусов SIM-карт
-- Проблема: SIM-карта может иметь status = 1 (в аренде), но терминал уже возвращен

-- 1. Проверить текущее состояние SIM-карт
SELECT 
    s.simcardid,
    s.simnumber,
    s.status AS sim_status,
    t.terminalid,
    t.serialnumber,
    t.status AS terminal_status,
    t.currentsimcardid,
    CASE 
        WHEN s.status = 0 THEN 'Свободна'
        WHEN s.status = 1 AND t.terminalid IS NOT NULL AND t.status = 1 THEN 'В аренде (терминал в аренде)'
        WHEN s.status = 1 AND (t.terminalid IS NULL OR t.status = 0) THEN 'ОШИБКА: В аренде, но терминал свободен'
        ELSE 'Неизвестно'
    END AS status_description
FROM tblsimcards s
LEFT JOIN tblterminals t ON s.simcardid = t.currentsimcardid
ORDER BY s.simcardid;

-- 2. Исправить статусы SIM-карт (если терминал свободен, сбросить статус SIM)
UPDATE tblsimcards s
SET status = 0
WHERE s.status = 1
  AND EXISTS (
      SELECT 1 FROM tblterminals t 
      WHERE t.currentsimcardid = s.simcardid 
        AND t.status = 0
  );

-- 3. Проверить результат
SELECT 
    s.simcardid,
    s.simnumber,
    s.status AS sim_status,
    t.terminalid,
    t.serialnumber,
    t.status AS terminal_status
FROM tblsimcards s
LEFT JOIN tblterminals t ON s.simcardid = t.currentsimcardid
ORDER BY s.simcardid;
