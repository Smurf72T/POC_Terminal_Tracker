-- 007_data_change_notify.sql
-- Межэкземплярное обновление: после изменения ключевых таблиц посылаем
-- NOTIFY 'poc_data_changed'. Другие запущенные экземпляры приложения,
-- подписанные на канал, обновляют дашборд. NOTIFY доставляется только
-- после COMMIT, поэтому уведомления приходят исключительно о зафиксированных
-- изменениях. Транзакция управляется миграционным раннером.

CREATE OR REPLACE FUNCTION notify_data_changed() RETURNS TRIGGER AS $$
BEGIN
    PERFORM pg_notify('poc_data_changed', TG_TABLE_NAME);
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

-- Идемпотентное создание триггеров (CREATE TRIGGER IF NOT EXISTS в PostgreSQL нет)
DO $$
DECLARE
    t TEXT;
BEGIN
    FOREACH t IN ARRAY ARRAY[
        'tblclients', 'tblmanufacturers', 'tblmodels',
        'tblsimcards', 'tblterminals',
        'tblreceiptdocs', 'tblreceiptdetails',
        'tblrentaldocs', 'tblrentaldetails',
        'tblreturndocs', 'tblreturndetails',
        'tblpayments', 'tblpayment_rental_links',
        'tblstatuschangedocs', 'tblstatuschangedetails',
        'tbl_users'
    ] LOOP
        IF NOT EXISTS (SELECT 1 FROM pg_trigger
                       WHERE tgname = 'trg_data_change_notify_' || t
                         AND NOT tgisinternal) THEN
            EXECUTE format(
                'CREATE TRIGGER trg_data_change_notify_%s ' ||
                'AFTER INSERT OR UPDATE OR DELETE ON %I ' ||
                'FOR EACH ROW EXECUTE FUNCTION notify_data_changed()',
                t, t);
        END IF;
    END LOOP;
END $$;
