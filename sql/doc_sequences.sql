-- ============================================================
-- Последовательности для генерации номеров документов
-- ============================================================

-- Последовательности
CREATE SEQUENCE IF NOT EXISTS seq_receipt_doc_number START 1;
CREATE SEQUENCE IF NOT EXISTS seq_rental_doc_number START 1;
CREATE SEQUENCE IF NOT EXISTS seq_return_doc_number START 1;
CREATE SEQUENCE IF NOT EXISTS seq_payment_doc_number START 1;

-- Функция генерации номера документа
CREATE OR REPLACE FUNCTION generate_doc_number(p_doc_type TEXT)
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
    ELSE
        RAISE EXCEPTION 'Неизвестный тип документа: %', p_doc_type;
    END IF;
    
    RETURN v_number;
END;
$$ LANGUAGE plpgsql;

-- Проверка
SELECT generate_doc_number('receipt');
SELECT generate_doc_number('rental');
SELECT generate_doc_number('return');
SELECT generate_doc_number('payment');
