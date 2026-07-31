-- 002_status_change_docs.sql
-- Документы изменения статуса терминалов:
-- в ремонт, возврат из ремонта, списание, утеря
-- Идемпотентный (IF NOT EXISTS)
-- Транзакция управляется миграционным раннером

-- Флаг «ремонтировался» в карточке терминала
ALTER TABLE tblterminals ADD COLUMN IF NOT EXISTS was_repaired BOOLEAN NOT NULL DEFAULT FALSE;

-- Шапка документа изменения статуса
-- actiontype: repair | repair_return | writeoff | lost
CREATE TABLE IF NOT EXISTS tblstatuschangedocs (
    statuschangedocid SERIAL PRIMARY KEY,
    docnumber VARCHAR(50) NOT NULL UNIQUE,
    docdate TIMESTAMP NOT NULL DEFAULT NOW(),
    actiontype VARCHAR(20) NOT NULL,
    comment TEXT,
    basedocid INTEGER REFERENCES tblstatuschangedocs(statuschangedocid) ON DELETE SET NULL
);

-- Строки документа изменения статуса
CREATE TABLE IF NOT EXISTS tblstatuschangedetails (
    statuschangedetailid SERIAL PRIMARY KEY,
    statuschangedocid INTEGER NOT NULL REFERENCES tblstatuschangedocs(statuschangedocid) ON DELETE CASCADE,
    terminalid INTEGER NOT NULL REFERENCES tblterminals(terminalid)
);

-- Индексы
CREATE INDEX IF NOT EXISTS idx_statuschangedocs_docdate ON tblstatuschangedocs(docdate);
CREATE INDEX IF NOT EXISTS idx_statuschangedocs_actiontype ON tblstatuschangedocs(actiontype);
CREATE INDEX IF NOT EXISTS idx_statuschangedocs_basedocid ON tblstatuschangedocs(basedocid);
CREATE INDEX IF NOT EXISTS idx_statuschangedetails_docid ON tblstatuschangedetails(statuschangedocid);
CREATE INDEX IF NOT EXISTS idx_statuschangedetails_terminalid ON tblstatuschangedetails(terminalid);

-- Последовательность и генератор номеров документов изменения статусов
CREATE SEQUENCE IF NOT EXISTS seq_statuschange_doc_number START 1;

-- Параметр функции в старых версиях назывался doc_type — PostgreSQL не позволяет
-- переименовывать параметры в CREATE OR REPLACE, поэтому сбрасываем функцию.
DROP FUNCTION IF EXISTS generate_doc_number(TEXT);

CREATE FUNCTION generate_doc_number(p_doc_type TEXT)
RETURNS TEXT AS $$
DECLARE
    v_next_val BIGINT;
    v_number TEXT;
BEGIN
    IF p_doc_type = 'receipt' THEN
        v_next_val := nextval('seq_receipt_doc_number');
        v_number := 'ПП-' || LPAD(v_next_val::TEXT, 5, '0');
    ELSIF p_doc_type = 'rental' THEN
        v_next_val := nextval('seq_rental_doc_number');
        v_number := 'АР-' || LPAD(v_next_val::TEXT, 5, '0');
    ELSIF p_doc_type = 'return' THEN
        v_next_val := nextval('seq_return_doc_number');
        v_number := 'ВР-' || LPAD(v_next_val::TEXT, 5, '0');
    ELSIF p_doc_type = 'payment' THEN
        v_next_val := nextval('seq_payment_doc_number');
        v_number := 'ОП-' || LPAD(v_next_val::TEXT, 5, '0');
    ELSIF p_doc_type = 'statuschange' THEN
        v_next_val := nextval('seq_statuschange_doc_number');
        v_number := 'ИС-' || LPAD(v_next_val::TEXT, 5, '0');
    ELSE
        RAISE EXCEPTION 'Неизвестный тип документа: %', p_doc_type;
    END IF;
    RETURN v_number;
END;
$$ LANGUAGE plpgsql;
