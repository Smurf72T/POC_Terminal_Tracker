-- 011: флаг деактивации терминалов.
-- Терминалы, на которые ссылаются документы, нельзя удалить физически
-- (ON DELETE RESTRICT). Вместо DELETE помечаем is_deactivated = TRUE —
-- терминал скрывается из выбора в новых документах, но история сохраняется.
ALTER TABLE tblterminals ADD COLUMN IF NOT EXISTS is_deactivated BOOLEAN NOT NULL DEFAULT FALSE;

CREATE INDEX IF NOT EXISTS idx_terminals_is_deactivated ON tblterminals(is_deactivated);
