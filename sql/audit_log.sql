-- ============================================================
-- Логирование действий пользователей (Audit Log)
-- ============================================================

-- Таблица пользователей
CREATE TABLE IF NOT EXISTS tbl_users (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(100) UNIQUE NOT NULL,
    display_name VARCHAR(100),
    password_hash VARCHAR(256),
    role VARCHAR(50) DEFAULT 'user',
    is_active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT NOW()
);

-- ВНИМАНИЕ: Пароль по умолчанию хранится в старом формате (salt+SHA256).
-- При первом входе он будет автоматически обновлён до PBKDF2-HMAC-SHA256 (100000 итераций).
-- Пароль: Admin123!   |   Соль: a1b2c3d4e5f6a7b8
-- В ПРОДУКШЕНЕ СМЕНИТЕ ПАРОЛЬ СРАЗУ ПОСЛЕ УСТАНОВКИ!
INSERT INTO tbl_users (username, display_name, password_hash, role, is_active)
VALUES ('admin', 'Администратор',
        'a1b2c3d4e5f6a7b8e4e0e1680400b99f8d57de3bf3abec3f6c9ad99440eae0721df5d3fca66a2597',
        'admin', TRUE)
ON CONFLICT (username) DO NOTHING;

-- Таблица аудита
CREATE TABLE IF NOT EXISTS tbl_audit_log (
    audit_log_id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES tbl_users(user_id),
    username VARCHAR(100),
    action VARCHAR(50) NOT NULL,
    table_name VARCHAR(100),
    record_id INTEGER,
    old_values JSONB,
    new_values JSONB,
    performed_at TIMESTAMP DEFAULT NOW()
);

-- Индексы
CREATE INDEX IF NOT EXISTS idx_audit_log_table_date
ON tbl_audit_log(table_name, performed_at DESC);

CREATE INDEX IF NOT EXISTS idx_audit_log_user
ON tbl_audit_log(user_id);

CREATE INDEX IF NOT EXISTS idx_audit_log_performed_at
ON tbl_audit_log(performed_at DESC);

-- Функция для упрощённой записи в аудит
CREATE OR REPLACE FUNCTION log_audit_action(
    p_action TEXT,
    p_table_name TEXT,
    p_record_id INTEGER,
    p_username TEXT DEFAULT NULL,
    p_old_values JSONB DEFAULT NULL,
    p_new_values JSONB DEFAULT NULL
)
RETURNS VOID AS $$
DECLARE
    v_user_id INTEGER;
BEGIN
    SELECT user_id INTO v_user_id FROM tbl_users
    WHERE username = COALESCE(p_username, 'system') AND is_active = TRUE;

    INSERT INTO tbl_audit_log (user_id, username, action, table_name, record_id, old_values, new_values, performed_at)
    VALUES (v_user_id, COALESCE(p_username, 'system'), p_action, p_table_name, p_record_id, p_old_values, p_new_values, NOW());
END;
$$ LANGUAGE plpgsql;

-- Триггер для автоматического логирования изменений в tblterminals
CREATE OR REPLACE FUNCTION audit_terminals_trigger()
RETURNS TRIGGER AS $$
BEGIN
    IF TG_OP = 'INSERT' THEN
        INSERT INTO tbl_audit_log (username, action, table_name, record_id, new_values, performed_at)
        VALUES (COALESCE(current_setting('app.username', TRUE), 'system'), 'CREATE', 'tblterminals',
                NEW.terminalid, row_to_json(NEW)::jsonb, NOW());
        RETURN NEW;
    ELSIF TG_OP = 'UPDATE' THEN
        INSERT INTO tbl_audit_log (username, action, table_name, record_id, old_values, new_values, performed_at)
        VALUES (COALESCE(current_setting('app.username', TRUE), 'system'), 'UPDATE', 'tblterminals',
                NEW.terminalid, row_to_json(OLD)::jsonb, row_to_json(NEW)::jsonb, NOW());
        RETURN NEW;
    ELSIF TG_OP = 'DELETE' THEN
        INSERT INTO tbl_audit_log (username, action, table_name, record_id, old_values, performed_at)
        VALUES (COALESCE(current_setting('app.username', TRUE), 'system'), 'DELETE', 'tblterminals',
                OLD.terminalid, row_to_json(OLD)::jsonb, NOW());
        RETURN OLD;
    END IF;
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_audit_terminals ON tblterminals;
CREATE TRIGGER trg_audit_terminals
    AFTER INSERT OR UPDATE OR DELETE ON tblterminals
    FOR EACH ROW EXECUTE FUNCTION audit_terminals_trigger();

-- Триггер для логирования изменений в tblclients
CREATE OR REPLACE FUNCTION audit_clients_trigger()
RETURNS TRIGGER AS $$
BEGIN
    IF TG_OP = 'INSERT' THEN
        INSERT INTO tbl_audit_log (username, action, table_name, record_id, new_values, performed_at)
        VALUES (COALESCE(current_setting('app.username', TRUE), 'system'), 'CREATE', 'tblclients',
                NEW.clientid, row_to_json(NEW)::jsonb, NOW());
        RETURN NEW;
    ELSIF TG_OP = 'UPDATE' THEN
        INSERT INTO tbl_audit_log (username, action, table_name, record_id, old_values, new_values, performed_at)
        VALUES (COALESCE(current_setting('app.username', TRUE), 'system'), 'UPDATE', 'tblclients',
                NEW.clientid, row_to_json(OLD)::jsonb, row_to_json(NEW)::jsonb, NOW());
        RETURN NEW;
    ELSIF TG_OP = 'DELETE' THEN
        INSERT INTO tbl_audit_log (username, action, table_name, record_id, old_values, performed_at)
        VALUES (COALESCE(current_setting('app.username', TRUE), 'system'), 'DELETE', 'tblclients',
                OLD.clientid, row_to_json(OLD)::jsonb, NOW());
        RETURN OLD;
    END IF;
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_audit_clients ON tblclients;
CREATE TRIGGER trg_audit_clients
    AFTER INSERT OR UPDATE OR DELETE ON tblclients
    FOR EACH ROW EXECUTE FUNCTION audit_clients_trigger();
