-- 006_terminal_status_check.sql
-- Расширение CHECK-ограничения статуса терминала: в 000_base_schema.sql
-- статусы ограничены (0,1,2), но приложение использует 3 (списан) и 4 (утерян).
-- На свежей БД (собранной целиком из миграций) списание/утеря падали с
-- CHECK-ошибкой. Транзакция управляется миграционным раннером.

-- Идемпотентно снимаем старое ограничение (имя по умолчанию от inline-CHECK).
DO $$
BEGIN
    IF EXISTS (SELECT 1 FROM pg_constraint
               WHERE conname = 'tblterminals_status_check'
                 AND conrelid = 'tblterminals'::regclass) THEN
        ALTER TABLE tblterminals DROP CONSTRAINT tblterminals_status_check;
    END IF;
END $$;

-- Добавляем расширенное ограничение, если его ещё нет.
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_constraint
                   WHERE conname = 'tblterminals_status_check'
                     AND conrelid = 'tblterminals'::regclass) THEN
        ALTER TABLE tblterminals ADD CONSTRAINT tblterminals_status_check
            CHECK (status IN (0, 1, 2, 3, 4));
    END IF;
END $$;
