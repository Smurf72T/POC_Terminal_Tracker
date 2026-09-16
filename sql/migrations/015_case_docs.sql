-- 015: учёт чехлов для POC-терминалов.
-- Справочник чехлов (склад) + 3 документа: «Поступление чехлов» (ПЧ-),
-- «Установка чехлов» (УЧ-), «Списание чехлов» (СЧ-).
-- Статусы чехла: 0 — на складе, 1 — установлен на терминале, 2 — списан.
-- Один чехол на терминал (tblterminals.currentcaseid), по аналогии с SIM.

-- Справочник/склад чехлов
CREATE TABLE IF NOT EXISTS tblcases (
    caseid SERIAL PRIMARY KEY,
    casetype VARCHAR(100) NOT NULL,
    status SMALLINT NOT NULL DEFAULT 0 CHECK (status IN (0, 1, 2)),
    terminalid INTEGER REFERENCES tblterminals(terminalid) ON UPDATE CASCADE ON DELETE SET NULL,
    notes TEXT,
    createdat TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_cases_status ON tblcases(status);
CREATE INDEX IF NOT EXISTS idx_cases_casetype ON tblcases(casetype);
CREATE INDEX IF NOT EXISTS idx_cases_terminal ON tblcases(terminalid);

-- Текущий чехол терминала
ALTER TABLE tblterminals ADD COLUMN IF NOT EXISTS currentcaseid INTEGER
    REFERENCES tblcases(caseid) ON UPDATE CASCADE ON DELETE SET NULL;

CREATE INDEX IF NOT EXISTS idx_terminals_current_case ON tblterminals(currentcaseid);

-- Документы поступления чехлов
CREATE TABLE IF NOT EXISTS tblcaseincomedocs (
    caseincomedocid SERIAL PRIMARY KEY,
    docnumber VARCHAR(20) NOT NULL UNIQUE,
    docdate TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    comments TEXT,
    createdby INTEGER,
    createdat TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Строки поступления: тип чехла + количество оприходованных единиц.
CREATE TABLE IF NOT EXISTS tblcaseincomedetails (
    caseincomedetailid SERIAL PRIMARY KEY,
    caseincomedocid INTEGER NOT NULL REFERENCES tblcaseincomedocs(caseincomedocid) ON UPDATE CASCADE ON DELETE CASCADE,
    casetype VARCHAR(100) NOT NULL,
    qty INTEGER NOT NULL CHECK (qty > 0)
);

CREATE INDEX IF NOT EXISTS idx_caseincome_docs_date ON tblcaseincomedocs(docdate);
CREATE INDEX IF NOT EXISTS idx_caseincome_docs_number ON tblcaseincomedocs(docnumber);
CREATE INDEX IF NOT EXISTS idx_caseincome_details_doc ON tblcaseincomedetails(caseincomedocid);

-- Документы установки чехлов
CREATE TABLE IF NOT EXISTS tblcaseinstalldocs (
    caseinstalldocid SERIAL PRIMARY KEY,
    docnumber VARCHAR(20) NOT NULL UNIQUE,
    docdate TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    comments TEXT,
    createdby INTEGER,
    createdat TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Строки установки: терминал + установленный чехол.
CREATE TABLE IF NOT EXISTS tblcaseinstalldetails (
    caseinstalldetailid SERIAL PRIMARY KEY,
    caseinstalldocid INTEGER NOT NULL REFERENCES tblcaseinstalldocs(caseinstalldocid) ON UPDATE CASCADE ON DELETE CASCADE,
    terminalid INTEGER NOT NULL REFERENCES tblterminals(terminalid) ON UPDATE CASCADE ON DELETE RESTRICT,
    caseid INTEGER REFERENCES tblcases(caseid) ON UPDATE CASCADE ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_caseinstall_docs_date ON tblcaseinstalldocs(docdate);
CREATE INDEX IF NOT EXISTS idx_caseinstall_docs_number ON tblcaseinstalldocs(docnumber);
CREATE INDEX IF NOT EXISTS idx_caseinstall_details_doc ON tblcaseinstalldetails(caseinstalldocid);
CREATE INDEX IF NOT EXISTS idx_caseinstall_details_terminal ON tblcaseinstalldetails(terminalid);
CREATE INDEX IF NOT EXISTS idx_caseinstall_details_case ON tblcaseinstalldetails(caseid);

-- Документы списания чехлов
CREATE TABLE IF NOT EXISTS tblcasewriteoffdocs (
    casewriteoffdocid SERIAL PRIMARY KEY,
    docnumber VARCHAR(20) NOT NULL UNIQUE,
    docdate TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    comments TEXT,
    createdby INTEGER,
    createdat TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Строки списания: списываемый чехол + причина.
CREATE TABLE IF NOT EXISTS tblcasewriteoffdetails (
    casewriteoffdetailid SERIAL PRIMARY KEY,
    casewriteoffdocid INTEGER NOT NULL REFERENCES tblcasewriteoffdocs(casewriteoffdocid) ON UPDATE CASCADE ON DELETE CASCADE,
    caseid INTEGER NOT NULL REFERENCES tblcases(caseid) ON UPDATE CASCADE ON DELETE RESTRICT,
    reason TEXT
);

CREATE INDEX IF NOT EXISTS idx_casewriteoff_docs_date ON tblcasewriteoffdocs(docdate);
CREATE INDEX IF NOT EXISTS idx_casewriteoff_docs_number ON tblcasewriteoffdocs(docnumber);
CREATE INDEX IF NOT EXISTS idx_casewriteoff_details_doc ON tblcasewriteoffdetails(casewriteoffdocid);

-- Признак «чехол едет с терминалом» в строке аренды — нужен для корректного
-- редактирования проведённых аренд и возврата.
ALTER TABLE tblrentaldetails ADD COLUMN IF NOT EXISTS has_case BOOLEAN NOT NULL DEFAULT FALSE;

-- Последовательности для номеров документов по чехлам
CREATE SEQUENCE IF NOT EXISTS seq_case_income_doc_number START 1;
CREATE SEQUENCE IF NOT EXISTS seq_case_install_doc_number START 1;
CREATE SEQUENCE IF NOT EXISTS seq_case_writeoff_doc_number START 1;

-- Обновляем генератор номеров документов: добавляем типы по чехлам
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
    ELSIF p_doc_type = 'case_income' THEN
        v_next_val := nextval('seq_case_income_doc_number');
        v_number := 'ПЧ-' || LPAD(v_next_val::TEXT, 5, '0');
    ELSIF p_doc_type = 'case_install' THEN
        v_next_val := nextval('seq_case_install_doc_number');
        v_number := 'УЧ-' || LPAD(v_next_val::TEXT, 5, '0');
    ELSIF p_doc_type = 'case_writeoff' THEN
        v_next_val := nextval('seq_case_writeoff_doc_number');
        v_number := 'СЧ-' || LPAD(v_next_val::TEXT, 5, '0');
    ELSE
        RAISE EXCEPTION 'Неизвестный тип документа: %', p_doc_type;
    END IF;
    RETURN v_number;
END;
$$ LANGUAGE plpgsql;

-- Представление полной информации о терминалах с учётом чехла.
-- CREATE OR REPLACE VIEW не может изменить список колонок существующего
-- представления, поэтому оно пересоздаётся.
DROP VIEW IF EXISTS vwterminalsfull;
CREATE VIEW vwterminalsfull AS
SELECT t.terminalid,
       t.serialnumber,
       t.imei1,
       t.imei2,
       t.status AS terminalstatus,
       CASE t.status
           WHEN 0 THEN 'Свободен'::TEXT
           WHEN 1 THEN 'В аренде'::TEXT
           WHEN 2 THEN 'В ремонте/списан'::TEXT
           ELSE NULL::TEXT
       END AS terminalstatusname,
       m.modelname,
       mf.manufacturername,
       t.currentsimcardid,
       s.simnumber AS currentsimnumber,
       t.currentsimcardid2,
       s2.simnumber AS currentsimnumber2,
       t.currentcaseid,
       c.casetype AS currentcasetype,
       t.purchasedate,
       t.notes,
       t.createdat
FROM tblterminals t
LEFT JOIN tblmodels m ON t.modelid = m.modelid
LEFT JOIN tblmanufacturers mf ON m.manufacturerid = mf.manufacturerid
LEFT JOIN tblsimcards s ON t.currentsimcardid = s.simcardid
LEFT JOIN tblsimcards s2 ON t.currentsimcardid2 = s2.simcardid
LEFT JOIN tblcases c ON t.currentcaseid = c.caseid;