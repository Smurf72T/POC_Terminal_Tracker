-- 005_login_security.sql
-- Блокировка перебора пароля на уровне БД (переживает перезапуск приложения
-- и действует для всех клиентов), а не только в памяти процесса.
-- Идемпотентный. Транзакция управляется миграционным раннером.

ALTER TABLE tbl_users ADD COLUMN IF NOT EXISTS failed_login_attempts INTEGER NOT NULL DEFAULT 0;
ALTER TABLE tbl_users ADD COLUMN IF NOT EXISTS locked_until TIMESTAMP;
