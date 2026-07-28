-- ============================================================
-- Логирование действий пользователей (Audit Log)
-- ============================================================

-- Таблица пользователей
CREATE TABLE IF NOT EXISTS tbl_users (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(100) UNIQUE NOT NULL,
    display_name VARCHAR(100),
    is_active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT NOW()
);

-- Вставляем дефолтного пользователя
INSERT INTO tbl_users (username, display_name, is_active)
VALUES ('admin', 'Администратор', TRUE)
ON CONFLICT (username) DO NOTHING;

-- Таблица аудита
CREATE TABLE IF NOT EXISTS tbl_audit_log (
    audit_log_id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES tbl_users(user_id),
    username VARCHAR(100),
    action VARCHAR(50) NOT NULL, -- CREATE, UPDATE, DELETE, POST, LOGIN
    table_name VARCHAR(100),
    record_id INTEGER,
    old_values JSONB,
    new_values JSONB,
    performed_at TIMESTAMP DEFAULT NOW()
);

-- Индекс для быстрого поиска по таблице и дате
CREATE INDEX IF NOT EXISTS idx_audit_log_table_date 
ON tbl_audit_log(table_name, performed_at DESC);

-- Индекс по пользователю
CREATE INDEX IF NOT EXISTS idx_audit_log_user 
ON tbl_audit_log(user_id);

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
    -- Получаем ID пользователя по имени
    SELECT user_id INTO v_user_id FROM tbl_users 
    WHERE username = COALESCE(p_username, 'system') AND is_active = TRUE;
    
    INSERT INTO tbl_audit_log (user_id, username, action, table_name, record_id, old_values, new_values, performed_at)
    VALUES (v_user_id, COALESCE(p_username, 'system'), p_action, p_table_name, p_record_id, p_old_values, p_new_values, NOW());
END;
$$ LANGUAGE plpgsql;

-- Пример использования
-- SELECT log_audit_action('POST', 'tblrentaldocs', 1, 'admin', NULL, '{"docnumber": "АР-00001"}');
