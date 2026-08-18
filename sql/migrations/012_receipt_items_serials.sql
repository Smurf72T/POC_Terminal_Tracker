-- 012: хранение «исходника» документа поступления в стиле 1С.
-- До миграции документ поступления содержал только развёрнутые терминалы
-- (tblreceiptdetails -> tblterminals). Теперь дополнительно сохраняем сам
-- документ как вводили: строки (модель + кол-во) и списки серийников/IMEI.
-- На проведении из этих строк по-прежнему создаются терминалы.

-- Строки документа поступления: модель + кол-во (исходник для серийников).
CREATE TABLE IF NOT EXISTS tblreceiptitems (
    receiptitemid SERIAL PRIMARY KEY,
    receiptdocid INTEGER NOT NULL REFERENCES tblreceiptdocs(receiptdocid) ON UPDATE CASCADE ON DELETE CASCADE,
    modelid INTEGER NOT NULL REFERENCES tblmodels(modelid) ON UPDATE CASCADE ON DELETE RESTRICT,
    qty INTEGER NOT NULL DEFAULT 1 CHECK (qty > 0)
);

-- Серийные номера и IMEI строки документа поступления:
-- по одному комплекту на каждую единицу (linenum — порядковый номер).
CREATE TABLE IF NOT EXISTS tblreceiptserials (
    receiptserialid SERIAL PRIMARY KEY,
    receiptitemid INTEGER NOT NULL REFERENCES tblreceiptitems(receiptitemid) ON UPDATE CASCADE ON DELETE CASCADE,
    linenum INTEGER NOT NULL CHECK (linenum > 0),
    serialnumber VARCHAR(50) NOT NULL,
    imei1 VARCHAR(15),
    imei2 VARCHAR(15),
    UNIQUE (receiptitemid, linenum)
);

CREATE INDEX IF NOT EXISTS idx_receiptitems_doc ON tblreceiptitems(receiptdocid);
CREATE INDEX IF NOT EXISTS idx_receiptserials_item ON tblreceiptserials(receiptitemid);