-- 014: документ «Установка SIM в терминал».
-- Позволяет на складе скомплектовать свободные терминалы (status = 0)
-- с SIM-картами без передачи в аренду. Статус терминала НЕ меняется,
-- меняется только статус SIM (0 → 1) и привязки currentsimcardid / currentsimcardid2.

-- Шапка документа установки SIM
CREATE TABLE IF NOT EXISTS tblsiminstalldocs (
    siminstalldocid SERIAL PRIMARY KEY,
    docnumber VARCHAR(20) NOT NULL UNIQUE,
    docdate TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    comments TEXT,
    createdby INTEGER,
    createdat TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Строки документа установки SIM
CREATE TABLE IF NOT EXISTS tblsiminstalldetails (
    siminstalldetailid SERIAL PRIMARY KEY,
    siminstalldocid INTEGER NOT NULL REFERENCES tblsiminstalldocs(siminstalldocid) ON UPDATE CASCADE ON DELETE CASCADE,
    terminalid INTEGER NOT NULL REFERENCES tblterminals(terminalid) ON UPDATE CASCADE ON DELETE RESTRICT,
    simcardid INTEGER REFERENCES tblsimcards(simcardid) ON UPDATE CASCADE ON DELETE SET NULL,
    simcardid2 INTEGER REFERENCES tblsimcards(simcardid) ON UPDATE CASCADE ON DELETE SET NULL
);

-- Последовательность для номеров документов
CREATE SEQUENCE IF NOT EXISTS seq_siminstall_doc_number START 1;

-- Индексы
CREATE INDEX IF NOT EXISTS idx_siminstall_docs_date ON tblsiminstalldocs(docdate);
CREATE INDEX IF NOT EXISTS idx_siminstall_docs_number ON tblsiminstalldocs(docnumber);
CREATE INDEX IF NOT EXISTS idx_siminstall_details_doc ON tblsiminstalldetails(siminstalldocid);
CREATE INDEX IF NOT EXISTS idx_siminstall_details_terminal ON tblsiminstalldetails(terminalid);

-- Обновляем генератор номеров документов: добавляем тип 'sim_install'
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
    ELSIF p_doc_type = 'sim_install' THEN
        v_next_val := nextval('seq_siminstall_doc_number');
        v_number := 'УС-' || LPAD(v_next_val::TEXT, 5, '0');
    ELSE
        RAISE EXCEPTION 'Неизвестный тип документа: %', p_doc_type;
    END IF;
    RETURN v_number;
END;
$$ LANGUAGE plpgsql;
