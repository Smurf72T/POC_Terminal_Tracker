-- Скрипт для добавления автоматического триггера синхронизации статусов
-- Триггер будет автоматически сбрасывать статус SIM-карты, если терминал возвращен

-- Удалить старый триггер если есть
DROP TRIGGER IF EXISTS trg_sync_sim_status ON tblterminals;

-- Создать триггер
CREATE OR REPLACE FUNCTION sync_sim_status_on_terminal_change()
RETURNS TRIGGER AS $$
BEGIN
    -- Если терминал возвращен (статус изменился с 1 на 0), сбрасываем статус SIM
    IF OLD.status = 1 AND NEW.status = 0 THEN
        -- Сбрасываем статус SIM-карты, если она была привязана
        IF OLD.currentsimcardid IS NOT NULL AND OLD.currentsimcardid != NEW.currentsimcardid THEN
            UPDATE tblsimcards 
            SET status = 0 
            WHERE simcardid = OLD.currentsimcardid;
        END IF;
    END IF;
    
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Создать триггер
CREATE TRIGGER trg_sync_sim_status
AFTER UPDATE ON tblterminals
FOR EACH ROW
EXECUTE FUNCTION sync_sim_status_on_terminal_change();

-- Проверка: Вывести информацию о триггере
SELECT 
    tgname AS "Имя триггера",
    tgrelid::regclass AS "Таблица",
    tgwhen AS "Время срабатывания",
    tgconstraintname AS "Ограничение"
FROM pg_trigger
WHERE tgname = 'trg_sync_sim_status';
