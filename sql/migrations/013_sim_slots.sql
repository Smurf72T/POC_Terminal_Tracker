-- 013: поддержка двух SIM-карт у терминала (слот 1 — imei1, слот 2 — imei2).
-- Раньше у терминала была одна привязка SIM (currentsimcardid) и одна SIM
-- в строке аренды (tblrentaldetails.simcardid) — всегда слот №1 (imei1).
-- Теперь терминалы с двумя IMEI (два слота) могут получать в аренду две SIM:
-- слот 1 (imei1) — существующие поля, слот 2 (imei2) — новые simcardid2.
-- Миграция данных не требуется: все текущие SIM установлены в первый слот,
-- т.е. автоматически считаются принадлежащими imei1.

-- Текущая SIM в слоте 2 терминала.
ALTER TABLE tblterminals ADD COLUMN IF NOT EXISTS currentsimcardid2 INTEGER
    REFERENCES tblsimcards(simcardid) ON UPDATE CASCADE ON DELETE SET NULL;

CREATE INDEX IF NOT EXISTS idx_terminals_current_sim2 ON tblterminals(currentsimcardid2);

-- SIM слота 2 в строке документа аренды.
ALTER TABLE tblrentaldetails ADD COLUMN IF NOT EXISTS simcardid2 INTEGER
    REFERENCES tblsimcards(simcardid) ON UPDATE CASCADE ON DELETE SET NULL;

CREATE INDEX IF NOT EXISTS idx_rental_details_sim2 ON tblrentaldetails(simcardid2);

-- Слот в истории привязок SIM к терминалам (1 — imei1, 2 — imei2).
ALTER TABLE tblsimassignments ADD COLUMN IF NOT EXISTS simslot SMALLINT NOT NULL DEFAULT 1
    CHECK (simslot IN (1, 2));

-- Представление полной информации о терминалах: оба слота SIM.
-- CREATE OR REPLACE VIEW не может изменить список колонок существующего
-- представления, поэтому при переходе с 000–012 представление пересоздаётся.
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
       t.purchasedate,
       t.notes,
       t.createdat
FROM tblterminals t
LEFT JOIN tblmodels m ON t.modelid = m.modelid
LEFT JOIN tblmanufacturers mf ON m.manufacturerid = mf.manufacturerid
LEFT JOIN tblsimcards s ON t.currentsimcardid = s.simcardid
LEFT JOIN tblsimcards s2 ON t.currentsimcardid2 = s2.simcardid;

-- Представление терминалов, находящихся в аренде в данный момент,
-- с SIM обоих слотов.
DROP VIEW IF EXISTS vwcurrentrentals;
CREATE VIEW vwcurrentrentals AS
SELECT t.terminalid,
       t.serialnumber,
       t.imei1,
       t.imei2,
       c.clientname,
       rd.docdate AS rentaldate,
       rd.docnumber AS rentaldocnumber,
       s.simnumber AS currentsimnumber,
       s2.simnumber AS currentsimnumber2
FROM tblterminals t
JOIN tblrentaldetails rdl ON t.terminalid = rdl.terminalid
JOIN tblrentaldocs rd ON rdl.rentaldocid = rd.rentaldocid
JOIN tblclients c ON rd.clientid = c.clientid
LEFT JOIN tblsimcards s ON t.currentsimcardid = s.simcardid
LEFT JOIN tblsimcards s2 ON t.currentsimcardid2 = s2.simcardid
WHERE t.status = 1;