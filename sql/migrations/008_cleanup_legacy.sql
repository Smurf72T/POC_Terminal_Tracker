-- 008_cleanup_legacy.sql
-- Удаление устаревшей последовательности seq_doc_numbers.
-- Была создана в 001_initial.sql для старой версии generate_doc_number()
-- (формат «ТИП-YYYY-000001»). С 002_status_change_docs.sql генератор переписан
-- на отдельные последовательности (seq_receipt/rental/return/payment/
-- statuschange_doc_number), и seq_doc_numbers нигде не используется.
-- Идемпотентный (DROP IF EXISTS). Транзакция управляется миграционным раннером.

DROP SEQUENCE IF EXISTS seq_doc_numbers;
