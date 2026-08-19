# Ищем Qt6 (минимальная версия 6.2 для поддержки Charts и всех используемых API)
find_package(Qt6 6.2 REQUIRED COMPONENTS
        Core
        Gui
        Widgets
        Sql
        PrintSupport
        Charts
        Network
)

# Ищем PostgreSQL
find_package(PostgreSQL REQUIRED)

# Проверка внешних утилит для бэкапов: pg_dump/psql нужны для полного дампа,
# openssl — для шифрования бэкапов (AES-256-CBC). При их отсутствии работает
# встроенный fallback-дамп и бэкап без шифрования — предупреждаем явно.
find_program(POC_PG_DUMP pg_dump)
find_program(POC_PSQL psql)
find_program(POC_OPENSSL openssl)
if(NOT POC_PG_DUMP OR NOT POC_PSQL)
    message(WARNING "pg_dump/psql не найдены в PATH. Резервное копирование будет выполняться встроенным "
                    "fallback-дампом (структура + данные), что медленнее и менее надёжно, чем pg_dump. "
                    "Установите PostgreSQL client tools для продакшена.")
endif()
if(NOT POC_OPENSSL)
    message(WARNING "openssl не найден в PATH. Шифрование бэкапов (AES-256-CBC) будет недоступно. "
                    "Установите OpenSSL для продакшена.")
endif()

# Подавляем предупреждение Qt о подключении GuiPrivate (нужен QXlsx для Qt >= 6.10)
set(QT_NO_PRIVATE_MODULE_WARNING ON CACHE BOOL "" FORCE)

# Подключаем QXlsx
add_subdirectory(libs/QXlsx/QXlsx)
