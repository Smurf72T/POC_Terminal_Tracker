-- ============================================================
-- ОПЦИОНАЛЬНЫЙ DB-триггер синхронизации статусов SIM-карт
-- ============================================================
-- ВНИМАНИЕ: этот триггер НЕ входит в миграции (sql/migrations/) и применяется
-- только вручную. Логика «терминал возвращён → статус SIM сброшен» уже
-- реализована в приложении (returnform.cpp), поэтому триггер — необязательный
-- страховочный уровень на стороне БД. Перед применением ознакомьтесь с
-- таблицей pg_trigger — повторное применение безопасно (DROP TRIGGER IF EXISTS).
-- ============================================================
-- Учитывает оба слота терминала (слот 1 — currentsimcardid, слот 2 —
-- currentsimcardid2). SIM, установленная документом «Установка SIM»
-- (комплектация на складе), остаётся в свободном терминале и НЕ сбрасывается:
-- триггер сбрасывает статус только тех SIM, слот которых в ходе возврата стал
-- пустым (OLD.sim != NEW.sim).

-- Скрипт для добавления автоматического триггера синхронизации статусов
-- Триггер будет автоматически сбрасывать статус SIM-карты, если терминал возвращен
-- и SIM при этом была отвязана от терминала (освобождена)

-- Удалить старый триггер если есть
DROP TRIGGER IF EXISTS trg_sync_sim_status ON tblterminals;

-- Создать триггер
CREATE OR REPLACE FUNCTION sync_sim_status_on_terminal_change()
RETURNS TRIGGER AS $$
BEGIN
    -- Если терминал возвращен (статус изменился с 1 на 0)
    IF OLD.status = 1 AND NEW.status = 0 THEN
        -- Слот 1: сбрасываем статус SIM, если она была отвязана
        IF OLD.currentsimcardid IS NOT NULL AND OLD.currentsimcardid != NEW.currentsimcardid THEN
            UPDATE tblsimcards 
            SET status = 0 
            WHERE simcardid = OLD.currentsimcardid;
        END IF;
        -- Слот 2: сбрасываем статус SIM, если она была отвязана
        IF OLD.currentsimcardid2 IS NOT NULL AND OLD.currentsimcardid2 != NEW.currentsimcardid2 THEN
            UPDATE tblsimcards 
            SET status = 0 
            WHERE simcardid = OLD.currentsimcardid2;
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
    tgenabled AS "Включён",
    pg_get_triggerdef(oid) AS "Определение"
FROM pg_trigger
WHERE tgname = 'trg_sync_sim_status';