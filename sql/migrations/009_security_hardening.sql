-- 009_security_hardening.sql
-- Усиление безопасности учётных записей.
-- 1. Флаг must_change_password: пароль должен быть сменён при следующем входе.
-- 2. Дефолтный admin (пароль захардкожен в 001_initial.sql) — принудительная смена.
-- 3. Пользователи с legacy-хешами (SHA-256, 64/80 символов) — принудительная смена,
--    чтобы в БД оставались только хеши PBKDF2-HMAC-SHA256 формата «iter:salt:hash».
-- Идемпотентный (ADD COLUMN IF NOT EXISTS). Транзакция управляется миграционным раннером.

ALTER TABLE tbl_users ADD COLUMN IF NOT EXISTS must_change_password BOOLEAN NOT NULL DEFAULT FALSE;

-- Дефолтный admin: пароль известен публично из 001_initial.sql
UPDATE tbl_users SET must_change_password = TRUE WHERE username = 'admin';

-- Любые оставшиеся legacy-хеши (не формата PBKDF2) — принудительная смена пароля
UPDATE tbl_users
SET must_change_password = TRUE
WHERE must_change_password = FALSE
  AND password_hash IS NOT NULL
  AND password_hash !~ '^[0-9]+:[0-9a-f]+:[0-9a-f]+$';
