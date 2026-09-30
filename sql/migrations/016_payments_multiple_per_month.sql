-- 016: несколько оплат за один период (клиент платит дважды в месяце).
--
-- Раньше tblpayments имела UNIQUE (clientid, periodmonth, periodyear), поэтому
-- в форме «Отметка оплаты за аренду» повторная оплата за тот же месяц
-- приводила к диалогу «Заменить её (включая привязанные документы)?» и
-- удалению прежней записи вместе с её связями — платёж за аренду терялся.
-- Ограничение снимается: оплаты накапливаются, их суммы складываются в отчётах.

-- Имя UNIQUE-ограничения зависит от того, создана ли таблица в 000_base_schema.sql
-- (тогда это tblpayments_clientid_periodmonth_periodyear_key) или в более ранней
-- версии БД, поэтому ищем его по определению, а не по имени.
DO $$
DECLARE
    v_constraint TEXT;
BEGIN
    SELECT conname INTO v_constraint
    FROM pg_constraint
    WHERE conrelid = 'tblpayments'::regclass
      AND contype = 'u'
      AND pg_get_constraintdef(oid) ILIKE '%(clientid, periodmonth, periodyear)%'
    LIMIT 1;

    IF v_constraint IS NOT NULL THEN
        EXECUTE format('ALTER TABLE tblpayments DROP CONSTRAINT %I', v_constraint);
    END IF;
END $$;

-- Взамен уникальности — индекс для выборок «оплаты клиента за период»
-- (форма оплаты, отчёты по аренде).
CREATE INDEX IF NOT EXISTS idx_payments_client_period
    ON tblpayments(clientid, periodyear, periodmonth);
