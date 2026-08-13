-- 000_base_schema.sql
-- Базовая схема бизнес-таблиц, представлений и индексов.
-- Ранее схема существовала только в живой БД и нигде в миграциях не создавалась,
-- из-за чего чистая установка (новая пустая БД) была невозможна:
-- 002_status_change_docs.sql ссылался на tblterminals, которой не существовало.
-- Идемпотентный (IF NOT EXISTS / CREATE OR REPLACE). Транзакция управляется раннером.

-- Справочник клиентов
CREATE TABLE IF NOT EXISTS tblclients (
    clientid SERIAL PRIMARY KEY,
    clientname VARCHAR(150) NOT NULL,
    inn VARCHAR(20),
    address TEXT,
    contactphone VARCHAR(50),
    contactemail VARCHAR(100),
    createdat TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Справочник производителей терминалов
CREATE TABLE IF NOT EXISTS tblmanufacturers (
    manufacturerid SERIAL PRIMARY KEY,
    manufacturername VARCHAR(100) NOT NULL UNIQUE
);

-- Справочник моделей терминалов
CREATE TABLE IF NOT EXISTS tblmodels (
    modelid SERIAL PRIMARY KEY,
    manufacturerid INTEGER NOT NULL REFERENCES tblmanufacturers(manufacturerid) ON UPDATE CASCADE ON DELETE RESTRICT,
    modelname VARCHAR(100) NOT NULL,
    UNIQUE (manufacturerid, modelname)
);

-- Справочник SIM-карт
CREATE TABLE IF NOT EXISTS tblsimcards (
    simcardid SERIAL PRIMARY KEY,
    simnumber VARCHAR(19) NOT NULL UNIQUE,
    status SMALLINT NOT NULL DEFAULT 0 CHECK (status IN (0, 1)),
    notes TEXT,
    createdat TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Справочник POC-терминалов
-- status: 0 — свободен, 1 — в аренде, 2 — в ремонте, 3 — списан, 4 — утерян.
--   CHECK в исходной схеме ограничен 0..2; полный словарь 0..4 вводит
--   миграция 006_terminal_status_check.sql. Единый C++-источник имён:
--   src/utils/terminal_status.h (TerminalStatus).
CREATE TABLE IF NOT EXISTS tblterminals (
    terminalid SERIAL PRIMARY KEY,
    serialnumber VARCHAR(50) NOT NULL UNIQUE,
    modelid INTEGER NOT NULL REFERENCES tblmodels(modelid) ON UPDATE CASCADE ON DELETE RESTRICT,
    imei1 VARCHAR(15) UNIQUE,
    imei2 VARCHAR(15) UNIQUE,
    status SMALLINT NOT NULL DEFAULT 0 CHECK (status IN (0, 1, 2)),
    currentsimcardid INTEGER REFERENCES tblsimcards(simcardid) ON UPDATE CASCADE ON DELETE SET NULL,
    purchasedate DATE,
    notes TEXT,
    createdat TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    was_repaired BOOLEAN NOT NULL DEFAULT FALSE
);

-- Документы поступления терминалов
CREATE TABLE IF NOT EXISTS tblreceiptdocs (
    receiptdocid SERIAL PRIMARY KEY,
    docnumber VARCHAR(20) NOT NULL UNIQUE,
    docdate TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    comments TEXT,
    createdby INTEGER,
    createdat TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Строки документов поступления
CREATE TABLE IF NOT EXISTS tblreceiptdetails (
    receiptdetailid SERIAL PRIMARY KEY,
    receiptdocid INTEGER NOT NULL REFERENCES tblreceiptdocs(receiptdocid) ON UPDATE CASCADE ON DELETE CASCADE,
    terminalid INTEGER NOT NULL REFERENCES tblterminals(terminalid) ON UPDATE CASCADE ON DELETE RESTRICT
);

-- Документы передачи терминалов в аренду
CREATE TABLE IF NOT EXISTS tblrentaldocs (
    rentaldocid SERIAL PRIMARY KEY,
    docnumber VARCHAR(20) NOT NULL UNIQUE,
    docdate TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    clientid INTEGER NOT NULL REFERENCES tblclients(clientid) ON UPDATE CASCADE ON DELETE RESTRICT,
    comments TEXT,
    createdby INTEGER,
    createdat TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Строки документов аренды
CREATE TABLE IF NOT EXISTS tblrentaldetails (
    rentaldetailid SERIAL PRIMARY KEY,
    rentaldocid INTEGER NOT NULL REFERENCES tblrentaldocs(rentaldocid) ON UPDATE CASCADE ON DELETE CASCADE,
    terminalid INTEGER NOT NULL REFERENCES tblterminals(terminalid) ON UPDATE CASCADE ON DELETE RESTRICT,
    simcardid INTEGER REFERENCES tblsimcards(simcardid) ON UPDATE CASCADE ON DELETE SET NULL,
    comment TEXT
);

-- Документы возврата терминалов из аренды
CREATE TABLE IF NOT EXISTS tblreturndocs (
    returndocid SERIAL PRIMARY KEY,
    docnumber VARCHAR(20) NOT NULL UNIQUE,
    docdate TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    clientid INTEGER NOT NULL REFERENCES tblclients(clientid) ON UPDATE CASCADE ON DELETE RESTRICT,
    comments TEXT,
    createdby INTEGER,
    createdat TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Строки документов возврата
CREATE TABLE IF NOT EXISTS tblreturndetails (
    returndetailid SERIAL PRIMARY KEY,
    returndocid INTEGER NOT NULL REFERENCES tblreturndocs(returndocid) ON UPDATE CASCADE ON DELETE CASCADE,
    terminalid INTEGER NOT NULL REFERENCES tblterminals(terminalid) ON UPDATE CASCADE ON DELETE RESTRICT
);

-- Отметки об оплате аренды
CREATE TABLE IF NOT EXISTS tblpayments (
    paymentid SERIAL PRIMARY KEY,
    clientid INTEGER NOT NULL REFERENCES tblclients(clientid) ON UPDATE CASCADE ON DELETE RESTRICT,
    paymentdate TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    periodmonth INTEGER NOT NULL CHECK (periodmonth BETWEEN 1 AND 12),
    periodyear INTEGER NOT NULL CHECK (periodyear BETWEEN 2000 AND 2100),
    amount NUMERIC(12, 2) DEFAULT 0,
    comment TEXT,
    createdby INTEGER,
    createdat TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (clientid, periodmonth, periodyear)
);

-- Связь документов оплаты с документами аренды
CREATE TABLE IF NOT EXISTS tblpayment_rental_links (
    linkid SERIAL PRIMARY KEY,
    paymentid INTEGER NOT NULL REFERENCES tblpayments(paymentid) ON DELETE CASCADE,
    rentaldocid INTEGER NOT NULL REFERENCES tblrentaldocs(rentaldocid) ON DELETE CASCADE,
    UNIQUE (paymentid, rentaldocid)
);

-- История привязок SIM-карт к терминалам
CREATE TABLE IF NOT EXISTS tblsimassignments (
    assignmentid SERIAL PRIMARY KEY,
    terminalid INTEGER NOT NULL REFERENCES tblterminals(terminalid) ON UPDATE CASCADE ON DELETE CASCADE,
    simcardid INTEGER NOT NULL REFERENCES tblsimcards(simcardid) ON UPDATE CASCADE ON DELETE RESTRICT,
    datefrom TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    dateto TIMESTAMP,
    notes TEXT
);

-- Документы изменения статуса терминалов
-- actiontype: repair | repair_return | writeoff | lost
CREATE TABLE IF NOT EXISTS tblstatuschangedocs (
    statuschangedocid SERIAL PRIMARY KEY,
    docnumber VARCHAR(50) NOT NULL UNIQUE,
    docdate TIMESTAMP NOT NULL DEFAULT NOW(),
    actiontype VARCHAR(20) NOT NULL,
    comment TEXT,
    basedocid INTEGER REFERENCES tblstatuschangedocs(statuschangedocid) ON DELETE SET NULL
);

-- Строки документов изменения статуса
CREATE TABLE IF NOT EXISTS tblstatuschangedetails (
    statuschangedetailid SERIAL PRIMARY KEY,
    statuschangedocid INTEGER NOT NULL REFERENCES tblstatuschangedocs(statuschangedocid) ON DELETE CASCADE,
    terminalid INTEGER NOT NULL REFERENCES tblterminals(terminalid)
);

-- Представление SIM-карт со строковым статусом
CREATE OR REPLACE VIEW vsimcards AS
SELECT simcardid,
       simnumber,
       CASE status
           WHEN 0 THEN 'Свободна'::TEXT
           WHEN 1 THEN 'Установлена'::TEXT
           ELSE 'Неизвестно'::TEXT
       END AS status_text,
       status AS status_code,
       notes,
       createdat
FROM tblsimcards;

-- Представление полной информации о терминалах
CREATE OR REPLACE VIEW vwterminalsfull AS
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
       t.purchasedate,
       t.notes,
       t.createdat
FROM tblterminals t
LEFT JOIN tblmodels m ON t.modelid = m.modelid
LEFT JOIN tblmanufacturers mf ON m.manufacturerid = mf.manufacturerid
LEFT JOIN tblsimcards s ON t.currentsimcardid = s.simcardid;

-- Представление терминалов, находящихся в аренде в данный момент
CREATE OR REPLACE VIEW vwcurrentrentals AS
SELECT t.terminalid,
       t.serialnumber,
       t.imei1,
       c.clientname,
       rd.docdate AS rentaldate,
       rd.docnumber AS rentaldocnumber,
       s.simnumber AS currentsimnumber
FROM tblterminals t
JOIN tblrentaldetails rdl ON t.terminalid = rdl.terminalid
JOIN tblrentaldocs rd ON rdl.rentaldocid = rd.rentaldocid
JOIN tblclients c ON rd.clientid = c.clientid
LEFT JOIN tblsimcards s ON t.currentsimcardid = s.simcardid
WHERE t.status = 1;

-- Индексы
CREATE INDEX IF NOT EXISTS idx_clients_name ON tblclients(clientname);
CREATE INDEX IF NOT EXISTS idx_models_manufacturer ON tblmodels(manufacturerid);
CREATE INDEX IF NOT EXISTS idx_simcards_number ON tblsimcards(simnumber);
CREATE INDEX IF NOT EXISTS idx_simcards_status ON tblsimcards(status);

CREATE INDEX IF NOT EXISTS idx_terminals_serial ON tblterminals(serialnumber);
CREATE INDEX IF NOT EXISTS idx_terminals_imei1 ON tblterminals(imei1);
CREATE INDEX IF NOT EXISTS idx_terminals_imei2 ON tblterminals(imei2);
CREATE INDEX IF NOT EXISTS idx_terminals_model ON tblterminals(modelid);
CREATE INDEX IF NOT EXISTS idx_terminals_status ON tblterminals(status);
CREATE INDEX IF NOT EXISTS idx_terminals_current_sim ON tblterminals(currentsimcardid);

CREATE INDEX IF NOT EXISTS idx_receipt_docs_date ON tblreceiptdocs(docdate);
CREATE INDEX IF NOT EXISTS idx_receipt_docs_number ON tblreceiptdocs(docnumber);
CREATE INDEX IF NOT EXISTS idx_receipt_details_doc ON tblreceiptdetails(receiptdocid);
CREATE INDEX IF NOT EXISTS idx_receipt_details_terminal ON tblreceiptdetails(terminalid);

CREATE INDEX IF NOT EXISTS idx_rental_docs_date ON tblrentaldocs(docdate);
CREATE INDEX IF NOT EXISTS idx_rental_docs_number ON tblrentaldocs(docnumber);
CREATE INDEX IF NOT EXISTS idx_rental_docs_client ON tblrentaldocs(clientid);
CREATE INDEX IF NOT EXISTS idx_rental_details_doc ON tblrentaldetails(rentaldocid);
CREATE INDEX IF NOT EXISTS idx_rental_details_terminal ON tblrentaldetails(terminalid);
CREATE INDEX IF NOT EXISTS idx_rental_details_sim ON tblrentaldetails(simcardid);

CREATE INDEX IF NOT EXISTS idx_return_docs_date ON tblreturndocs(docdate);
CREATE INDEX IF NOT EXISTS idx_return_docs_number ON tblreturndocs(docnumber);
CREATE INDEX IF NOT EXISTS idx_return_docs_client ON tblreturndocs(clientid);
CREATE INDEX IF NOT EXISTS idx_return_details_doc ON tblreturndetails(returndocid);
CREATE INDEX IF NOT EXISTS idx_return_details_terminal ON tblreturndetails(terminalid);

CREATE INDEX IF NOT EXISTS idx_payments_client ON tblpayments(clientid);
CREATE INDEX IF NOT EXISTS idx_payments_date ON tblpayments(paymentdate);
CREATE INDEX IF NOT EXISTS idx_payments_period ON tblpayments(periodyear, periodmonth);
CREATE INDEX IF NOT EXISTS idx_pr_links_payment ON tblpayment_rental_links(paymentid);
CREATE INDEX IF NOT EXISTS idx_pr_links_rental ON tblpayment_rental_links(rentaldocid);

CREATE INDEX IF NOT EXISTS idx_sim_assignments_terminal ON tblsimassignments(terminalid);
CREATE INDEX IF NOT EXISTS idx_sim_assignments_sim ON tblsimassignments(simcardid);
CREATE INDEX IF NOT EXISTS idx_sim_assignments_dates ON tblsimassignments(datefrom, dateto);

CREATE INDEX IF NOT EXISTS idx_statuschangedocs_docdate ON tblstatuschangedocs(docdate);
CREATE INDEX IF NOT EXISTS idx_statuschangedocs_actiontype ON tblstatuschangedocs(actiontype);
CREATE INDEX IF NOT EXISTS idx_statuschangedocs_basedocid ON tblstatuschangedocs(basedocid);
CREATE INDEX IF NOT EXISTS idx_statuschangedetails_docid ON tblstatuschangedetails(statuschangedocid);
CREATE INDEX IF NOT EXISTS idx_statuschangedetails_terminalid ON tblstatuschangedetails(terminalid);
