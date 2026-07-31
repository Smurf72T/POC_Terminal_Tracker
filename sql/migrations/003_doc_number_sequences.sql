-- 003_doc_number_sequences.sql
-- Последовательности для генерации номеров документов.
-- Ранее создавались только вручную (sql/doc_sequences.sql), из-за чего на
-- чистой установке generate_doc_number() завершался ошибкой (последовательности
-- не существовали), а формы подставляли статический номер "ПП-00001".
-- Идемпотентный (IF NOT EXISTS). Транзакция управляется миграционным раннером.

CREATE SEQUENCE IF NOT EXISTS seq_receipt_doc_number START 1;
CREATE SEQUENCE IF NOT EXISTS seq_rental_doc_number START 1;
CREATE SEQUENCE IF NOT EXISTS seq_return_doc_number START 1;
CREATE SEQUENCE IF NOT EXISTS seq_payment_doc_number START 1;
CREATE SEQUENCE IF NOT EXISTS seq_statuschange_doc_number START 1;
