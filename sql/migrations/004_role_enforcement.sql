-- 004_role_enforcement.sql
-- Принудительная авторизация на уровне БД (не только на уровне UI).
-- Роль текущего пользователя передаётся в сессию через set_config('app.role').
-- Триггеры запрещают не-администраторам изменять пользователей и править/удалять
-- журнал аудита, даже если обойти ограничения интерфейса.
-- Bootstrap (миграции, прямое подключение, логин/регистрация до установки роли):
-- app.role не задан -> операции не ограничиваются.
-- Идемпотентный. Транзакция управляется миграционным раннером.

-- Защита tbl_users: только admin управляет пользователями
CREATE OR REPLACE FUNCTION enforce_user_management() RETURNS TRIGGER AS $$
DECLARE
    v_role TEXT := current_setting('app.role', TRUE);
    v_user_id INTEGER;
BEGIN
    -- Роль не установлена (миграции/bootstrap/внешнее подключение) — не ограничиваем
    IF v_role IS NULL OR v_role = 'admin' THEN
        RETURN COALESCE(NEW, OLD);
    END IF;

    -- Обычному пользователю разрешено менять только свой пароль
    IF TG_OP = 'UPDATE' THEN
        SELECT user_id INTO v_user_id FROM tbl_users
        WHERE username = current_setting('app.username', TRUE) AND is_active = TRUE;
        IF v_user_id IS NOT NULL AND NEW.user_id = v_user_id
           AND OLD.username IS NOT DISTINCT FROM NEW.username
           AND OLD.role IS NOT DISTINCT FROM NEW.role
           AND OLD.is_active IS NOT DISTINCT FROM NEW.is_active
           AND OLD.display_name IS NOT DISTINCT FROM NEW.display_name
        THEN
            RETURN NEW;
        END IF;
    END IF;

    RAISE EXCEPTION 'Доступ запрещён (app.role=%): изменение пользователей доступно только администратору', v_role;
END;
$$ LANGUAGE plpgsql;

DO $$ BEGIN DROP TRIGGER IF EXISTS trg_enforce_user_management ON tbl_users; EXCEPTION WHEN undefined_table THEN NULL; END $$;
CREATE TRIGGER trg_enforce_user_management BEFORE INSERT OR UPDATE OR DELETE ON tbl_users
FOR EACH ROW EXECUTE FUNCTION enforce_user_management();

-- Защита журнала аудита: INSERT разрешён всем (пишут триггеры и log_audit_action),
-- UPDATE/DELETE — только администратору
CREATE OR REPLACE FUNCTION enforce_audit_log_protection() RETURNS TRIGGER AS $$
DECLARE
    v_role TEXT := current_setting('app.role', TRUE);
BEGIN
    IF v_role IS NULL OR v_role = 'admin' THEN
        RETURN COALESCE(NEW, OLD);
    END IF;

    RAISE EXCEPTION 'Доступ запрещён (app.role=%): изменение журнала аудита доступно только администратору', v_role;
END;
$$ LANGUAGE plpgsql;

DO $$ BEGIN DROP TRIGGER IF EXISTS trg_audit_log_protection ON tbl_audit_log; EXCEPTION WHEN undefined_table THEN NULL; END $$;
CREATE TRIGGER trg_audit_log_protection BEFORE UPDATE OR DELETE ON tbl_audit_log
FOR EACH ROW EXECUTE FUNCTION enforce_audit_log_protection();
