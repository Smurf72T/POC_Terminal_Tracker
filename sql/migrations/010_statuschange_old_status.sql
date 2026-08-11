-- 010: снимок прежнего статуса терминала в деталях документа изменения статуса.
-- Нужен для корректного отката статуса при редактировании проведённого документа.
ALTER TABLE tblstatuschangedetails ADD COLUMN IF NOT EXISTS old_status INTEGER;

-- Для уже проведённых документов заполняем снимок наилучшим образом:
-- прежний статус восстанавливается обратным отображением типа операции.
UPDATE tblstatuschangedetails scd
SET old_status = CASE d.actiontype
                     WHEN 'repair' THEN 0
                     WHEN 'repair_return' THEN 2
                     ELSE NULL
                 END
FROM tblstatuschangedocs d
WHERE d.statuschangedocid = scd.statuschangedocid
  AND scd.old_status IS NULL;
