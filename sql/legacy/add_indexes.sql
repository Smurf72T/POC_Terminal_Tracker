-- ============================================================
-- Индексы для производительности (1000+ записей)
-- Добавлять без риска для существующих данных (IF NOT EXISTS)
-- ============================================================

BEGIN;

-- tblterminals
CREATE INDEX IF NOT EXISTS idx_terminals_status ON tblterminals(status);
CREATE INDEX IF NOT EXISTS idx_terminals_serialnumber ON tblterminals(serialnumber);
CREATE INDEX IF NOT EXISTS idx_terminals_imei1 ON tblterminals(imei1);
CREATE INDEX IF NOT EXISTS idx_terminals_imei2 ON tblterminals(imei2);
CREATE INDEX IF NOT EXISTS idx_terminals_modelid ON tblterminals(modelid);
CREATE INDEX IF NOT EXISTS idx_terminals_currsimid ON tblterminals(currentsimcardid);

-- tblsimcards
CREATE INDEX IF NOT EXISTS idx_simcards_status ON tblsimcards(status);
CREATE INDEX IF NOT EXISTS idx_simcards_simnumber ON tblsimcards(simnumber);

-- tblclients
CREATE INDEX IF NOT EXISTS idx_clients_clientname ON tblclients(clientname);
CREATE INDEX IF NOT EXISTS idx_clients_inn ON tblclients(inn);

-- tblmanufacturers
CREATE INDEX IF NOT EXISTS idx_manufacturers_name ON tblmanufacturers(manufacturername);

-- tblmodels
CREATE INDEX IF NOT EXISTS idx_models_name ON tblmodels(modelname);
CREATE INDEX IF NOT EXISTS idx_models_manufacturerid ON tblmodels(manufacturerid);

-- tblrentaldocs
CREATE INDEX IF NOT EXISTS idx_rentaldocs_clientid ON tblrentaldocs(clientid);
CREATE INDEX IF NOT EXISTS idx_rentaldocs_docdate ON tblrentaldocs(docdate);
CREATE INDEX IF NOT EXISTS idx_rentaldocs_status ON tblrentaldocs(status);

-- tblreturndocs
CREATE INDEX IF NOT EXISTS idx_returndocs_rentaldocid ON tblreturndocs(rentaldocid);
CREATE INDEX IF NOT EXISTS idx_returndocs_docdate ON tblreturndocs(docdate);

-- tblreceiptdocs
CREATE INDEX IF NOT EXISTS idx_receiptdocs_docdate ON tblreceiptdocs(docdate);

-- tblpayments
CREATE INDEX IF NOT EXISTS idx_payments_clientid ON tblpayments(clientid);
CREATE INDEX IF NOT EXISTS idx_payments_period ON tblpayments(periodmonth, periodyear);
CREATE INDEX IF NOT EXISTS idx_payments_docdate ON tblpayments(docdate);

-- tblrentaldetails
CREATE INDEX IF NOT EXISTS idx_rentaldetails_rentaldocid ON tblrentaldetails(rentaldocid);
CREATE INDEX IF NOT EXISTS idx_rentaldetails_terminalid ON tblrentaldetails(terminalid);

-- tblreturndetails
CREATE INDEX IF NOT EXISTS idx_returndetails_returndocid ON tblreturndetails(returndocid);
CREATE INDEX IF NOT EXISTS idx_returndetails_terminalid ON tblreturndetails(terminalid);

-- tblreceiptdetails
CREATE INDEX IF NOT EXISTS idx_receiptdetails_receiptdocid ON tblreceiptdetails(receiptdocid);
CREATE INDEX IF NOT EXISTS idx_receiptdetails_terminalid ON tblreceiptdetails(terminalid);

-- tblpayment_rental_links
CREATE INDEX IF NOT EXISTS idx_payrent_paymentid ON tblpayment_rental_links(paymentid);
CREATE INDEX IF NOT EXISTS idx_payrent_rentaldocid ON tblpayment_rental_links(rentaldocid);

-- ============================================================
-- Ограничения (безрисковые, только IF NOT EXISTS)
-- ============================================================

-- CHECK на статусы терминалов (0-4)
ALTER TABLE tblterminals DROP CONSTRAINT IF EXISTS ck_terminals_status;
ALTER TABLE tblterminals ADD CONSTRAINT ck_terminals_status
    CHECK (status IN (0, 1, 2, 3, 4));

-- CHECK на статусы SIM (0-1)
ALTER TABLE tblsimcards DROP CONSTRAINT IF EXISTS ck_simcards_status;
ALTER TABLE tblsimcards ADD CONSTRAINT ck_simcards_status
    CHECK (status IN (0, 1));

-- UNIQUE на serialnumber (если нет дубликатов)
DO $$
BEGIN
    IF NOT EXISTS (
        SELECT 1 FROM pg_constraint WHERE conname = 'uq_terminals_serialnumber'
    ) THEN
        -- Проверяем, нет ли дубликатов перед созданием
        IF NOT EXISTS (
            SELECT serialnumber FROM tblterminals
            WHERE serialnumber IS NOT NULL AND serialnumber != ''
            GROUP BY serialnumber HAVING COUNT(*) > 1
        ) THEN
            ALTER TABLE tblterminals ADD CONSTRAINT uq_terminals_serialnumber
                UNIQUE (serialnumber);
        END IF;
    END IF;
END $$;

COMMIT;
