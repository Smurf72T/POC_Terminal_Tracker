cmake_minimum_required(VERSION 3.20)
project(POC_Terminal_Tracker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# DashboardView работает с виджетами из mainwindow.ui по их objectName —
# включаем каталог .ui в пути поиска uic для дочерних подкаталогов (views/).
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/src/ui)

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

# Исходные файлы
set(SOURCES
        src/main.cpp
        src/database/databasemanager.cpp
        src/database/connectionpool.cpp
        src/database/submiterrortablemodel.cpp
        src/database/repositories/terminalrepository.cpp
        src/database/repositories/clientrepository.cpp
        src/database/repositories/simcardrepository.cpp
        src/database/repositories/documentrepository.cpp
        src/database/repositories/paymentrepository.cpp
        src/ui/mainwindow.cpp
        src/ui/views/dashboardview.cpp
        src/ui/views/chartpanel.cpp
        src/ui/panels/backuppanel.cpp
        src/ui/panels/maintenancepanel.cpp
        src/ui/dialogs/manufacturersform.cpp
        src/ui/dialogs/modelsform.cpp
        src/ui/dialogs/clientsform.cpp
        src/ui/dialogs/simcardsform.cpp
        src/ui/dialogs/terminalsform.cpp
        src/ui/dialogs/terminaleditform.cpp
        resources.qrc
        src/ui/dialogs/receiptform.cpp
        src/ui/dialogs/serialunitsdialog.cpp
        src/ui/dialogs/rentalform.cpp
        src/ui/dialogs/returnform.cpp
        src/ui/dialogs/statuschangeform.cpp
        src/ui/dialogs/archivedocumentsform.cpp
        src/ui/dialogs/paymentform.cpp
        src/ui/dialogs/terminalhistoryform.cpp
        src/ui/dialogs/bulkimportform.cpp
        src/ui/dialogs/auditlogform.cpp
        src/ui/dialogs/loginform.cpp
        src/ui/dialogs/expirynotificationsform.cpp
        src/ui/dialogs/batchstatusform.cpp
        src/ui/dialogs/usermanagementform.cpp
        src/ui/dialogs/reportsform.cpp
        src/ui/dialogs/globalsearchdialog.cpp
        src/ui/dialogs/terminalhistorypickerdialog.cpp
        src/ui/dialogs/freedevicesreportdialog.cpp
        src/ui/dialogs/clientrentalreportdialog.cpp
        src/ui/dialogs/updatesettingsdialog.cpp
        src/utils/reportexporter.cpp
        src/utils/validator.cpp
        src/utils/registration_ratelimiter.cpp
        src/utils/logging.cpp
        src/utils/circuitbreaker.cpp
        src/utils/barcodeparser.cpp
        src/utils/barcodescanner.cpp
        src/utils/serialscanner.cpp
        src/ops/backupmanager.cpp
        src/ops/backupworker.cpp
        src/ops/opslog.cpp
        src/ops/opsscheduler.cpp
        src/update/updatemanager.cpp
        src/ui/base/printservice.cpp
        src/ui/base/transactionguard.cpp
        src/services/documentnumbergenerator.cpp
        src/services/postactionlogger.cpp
)

set(HEADERS
        src/models/terminal.h
        src/models/client.h
        src/models/simcard.h
        src/models/rentaldocument.h
        src/models/document.h
        src/database/databasemanager.h
        src/database/idatabasemanager.h
        src/database/connectionpool.h
        src/database/submiterrortablemodel.h
        src/database/repositories/terminalrepository.h
        src/database/repositories/clientrepository.h
        src/database/repositories/simcardrepository.h
        src/database/repositories/documentrepository.h
        src/database/repositories/paymentrepository.h
        src/ui/mainwindow.h
        src/ui/views/dashboardview.h
        src/ui/views/chartpanel.h
        src/ui/panels/backuppanel.h
        src/ui/panels/maintenancepanel.h
        src/ui/dialogs/manufacturersform.h
        src/ui/dialogs/modelsform.h
        src/ui/dialogs/clientsform.h
        src/ui/dialogs/simcardsform.h
        src/ui/dialogs/terminalsform.h
        src/ui/dialogs/terminaleditform.h
        src/ui/delegates/readonlydelegate.h
        src/ui/dialogs/receiptform.h
        src/ui/dialogs/serialunitsdialog.h
        src/ui/delegates/comboboxdelegate.h
        src/ui/delegates/comboboxmodel.h
        src/ui/delegates/CheckBoxDelegate.h
        src/ui/dialogs/rentalform.h
        src/ui/dialogs/returnform.h
        src/ui/dialogs/statuschangeform.h
        src/ui/dialogs/archivedocumentsform.h
        src/ui/dialogs/paymentform.h
        src/ui/dialogs/terminalhistoryform.h
        src/ui/dialogs/bulkimportform.h
        src/ui/dialogs/auditlogform.h
        src/ui/dialogs/loginform.h
        src/ui/dialogs/expirynotificationsform.h
        src/ui/dialogs/batchstatusform.h
        src/ui/dialogs/usermanagementform.h
        src/ui/dialogs/reportsform.h
        src/ui/dialogs/globalsearchdialog.h
        src/ui/dialogs/terminalhistorypickerdialog.h
        src/ui/dialogs/freedevicesreportdialog.h
        src/ui/dialogs/clientrentalreportdialog.h
        src/ui/dialogs/updatesettingsdialog.h
        src/ui/base/documentdialog.h
        src/ui/base/printservice.h
        src/ui/base/transactionguard.h
        src/services/documentnumbergenerator.h
        src/services/postactionlogger.h
        src/utils/reportexporter.h
        src/utils/validator.h
        src/utils/registration_ratelimiter.h
        src/utils/logging.h
        src/utils/circuitbreaker.h
        src/utils/barcodeparser.h
        src/utils/serialscanner.h
        src/utils/barcodescanner.h
        src/utils/password_utils.h
        src/ops/backupmanager.h
        src/ops/backupworker.h
        src/ops/opslog.h
        src/ops/opsscheduler.h
        src/update/updatemanager.h
        src/update/version.h
)

set(UI_FILES
        src/ui/mainwindow.ui
        src/ui/dialogs/manufacturersform.ui
        src/ui/dialogs/modelsform.ui
        src/ui/dialogs/clientsform.ui
        src/ui/dialogs/simcardsform.ui
        src/ui/dialogs/terminalsform.ui
        src/ui/dialogs/terminaleditform.ui
        src/ui/dialogs/receiptform.ui
        src/ui/dialogs/serialunitsdialog.ui
        src/ui/dialogs/rentalform.ui
        src/ui/dialogs/returnform.ui
        src/ui/dialogs/statuschangeform.ui
        src/ui/dialogs/archivedocumentsform.ui
        src/ui/dialogs/paymentform.ui
        src/ui/dialogs/terminalhistoryform.ui
        src/ui/dialogs/bulkimportform.ui
        src/ui/dialogs/auditlogform.ui
        src/ui/dialogs/loginform.ui
        src/ui/dialogs/expirynotificationsform.ui
        src/ui/dialogs/batchstatusform.ui
        src/ui/dialogs/reportsform.ui
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${UI_FILES})

# Линковка Qt6 и PostgreSQL
target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Sql
        Qt6::PrintSupport
        Qt6::Charts
        Qt6::Network
        PostgreSQL::PostgreSQL
        QXlsx::QXlsx
)

# Диагностика сканера штрих-кода: показывает сырые KeyPress/KeyRelease,
# задержки между нажатиями и результат BarcodeScanner+BarcodeParser.
add_executable(scanner_probe
        tools/scanner_probe/scanner_probe.cpp
        src/utils/barcodescanner.cpp
        src/utils/barcodeparser.cpp
        src/utils/serialscanner.cpp
)
target_include_directories(scanner_probe PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(scanner_probe PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

# Копируем config.json в папку сборки. config.json не коммитится (см. .gitignore,
# версия живёт в config/config.json.example) — при его отсутствии берём шаблон.
if(EXISTS ${CMAKE_SOURCE_DIR}/config/config.json)
    set(POC_CONFIG_TEMPLATE ${CMAKE_SOURCE_DIR}/config/config.json)
else()
    set(POC_CONFIG_TEMPLATE ${CMAKE_SOURCE_DIR}/config/config.json.example)
endif()
configure_file(
        ${POC_CONFIG_TEMPLATE}
        ${CMAKE_BINARY_DIR}/config/config.json
        COPYONLY
)

# Копируем SQL-миграции в папку сборки
file(COPY ${CMAKE_SOURCE_DIR}/sql/migrations/ DESTINATION ${CMAKE_BINARY_DIR}/sql/migrations/)

# Добавляем include директории
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/ui
        ${PostgreSQL_INCLUDE_DIRS}
)

# Копируем DLL PostgreSQL в папку сборки
if(DEFINED PostgreSQL_ROOT AND EXISTS "${PostgreSQL_ROOT}/bin")
    set(PG_DLL_DIR "${PostgreSQL_ROOT}/bin")
    foreach(_pg_dll libpq.dll libintl-9.dll libssl-3-x64.dll libcrypto-3-x64.dll)
        if(EXISTS "${PG_DLL_DIR}/${_pg_dll}")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PG_DLL_DIR}/${_pg_dll}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${_pg_dll}"
                    COMMENT "Copying ${_pg_dll}"
            )
        else()
            message(WARNING "PostgreSQL DLL not found: ${PG_DLL_DIR}/${_pg_dll}")
        endif()
    endforeach()
else()
    message(WARNING "PostgreSQL_ROOT/bin not found, DLLs will not be copied")
endif()

# Копируем Qt Charts DLL
if(DEFINED CMAKE_PREFIX_PATH AND EXISTS "${CMAKE_PREFIX_PATH}/bin")
    set(QT_DLL_DIR "${CMAKE_PREFIX_PATH}/bin")
    foreach(_qt_dll Qt6Charts.dll Qt6OpenGL.dll Qt6OpenGLWidgets.dll)
        if(EXISTS "${QT_DLL_DIR}/${_qt_dll}")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${QT_DLL_DIR}/${_qt_dll}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${_qt_dll}"
                    COMMENT "Copying ${_qt_dll}"
            )
        else()
            message(WARNING "Qt DLL not found: ${QT_DLL_DIR}/${_qt_dll}")
        endif()
    endforeach()
else()
    message(WARNING "CMAKE_PREFIX_PATH/bin not found, Qt DLLs will not be copied")
endif()

# Опциональная сборка тестов
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)

    find_package(Threads)

    add_executable(test_password_utils tests/test_password_utils.cpp)
    target_include_directories(test_password_utils PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_password_utils PRIVATE Qt6::Test Qt6::Core)
    add_test(NAME test_password_utils COMMAND test_password_utils)

    add_executable(test_validator tests/test_validator.cpp src/utils/validator.cpp tests/stub_databasemanager.cpp)
    target_include_directories(test_validator PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/database)
    target_link_libraries(test_validator PRIVATE Qt6::Test Qt6::Core Qt6::Sql)
    add_test(NAME test_validator COMMAND test_validator)

    add_executable(test_loginform tests/test_loginform.cpp src/utils/registration_ratelimiter.cpp)
    target_include_directories(test_loginform PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_loginform PRIVATE Qt6::Test Qt6::Core)
    add_test(NAME test_loginform COMMAND test_loginform)

    add_executable(test_update_utils tests/test_update_utils.cpp)
    target_include_directories(test_update_utils PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_update_utils PRIVATE Qt6::Test Qt6::Core)
    add_test(NAME test_update_utils COMMAND test_update_utils)

    add_executable(test_barcodeparser tests/test_barcodeparser.cpp src/utils/barcodeparser.cpp)
    target_include_directories(test_barcodeparser PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_barcodeparser PRIVATE Qt6::Test Qt6::Core)
    add_test(NAME test_barcodeparser COMMAND test_barcodeparser)

    add_executable(test_barcodescanner tests/test_barcodescanner.cpp src/utils/barcodescanner.cpp src/utils/barcodeparser.cpp)
    target_include_directories(test_barcodescanner PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_barcodescanner PRIVATE Qt6::Test Qt6::Core)
    add_test(NAME test_barcodescanner COMMAND test_barcodescanner)

    add_executable(test_updatemanager tests/test_updatemanager.cpp
            src/update/updatemanager.cpp
            src/ops/opslog.cpp
            src/utils/logging.cpp)
    target_include_directories(test_updatemanager PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_updatemanager PRIVATE Qt6::Test Qt6::Core Qt6::Network)
    add_test(NAME test_updatemanager COMMAND test_updatemanager)

    add_executable(test_opsscheduler tests/test_opsscheduler.cpp
            src/ops/opsscheduler.cpp
            src/ops/backupmanager.cpp
            src/ops/backupworker.cpp
            src/ops/opslog.cpp
            src/database/databasemanager.cpp
            src/database/connectionpool.cpp
            src/utils/circuitbreaker.cpp
            src/utils/logging.cpp)
    target_include_directories(test_opsscheduler PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_opsscheduler PRIVATE Qt6::Test Qt6::Core Qt6::Sql Qt6::Widgets Threads::Threads)
    add_test(NAME test_opsscheduler COMMAND test_opsscheduler)

    add_executable(test_db_integration tests/test_db_integration.cpp
            src/ops/backupmanager.cpp
            src/ops/opslog.cpp
            src/utils/logging.cpp)
    target_include_directories(test_db_integration PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_db_integration PRIVATE Qt6::Test Qt6::Core Qt6::Sql)
    add_test(NAME test_db_integration COMMAND test_db_integration)

    add_executable(test_concurrency tests/test_concurrency.cpp)
    target_include_directories(test_concurrency PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_concurrency PRIVATE Qt6::Test Qt6::Core Qt6::Sql Threads::Threads)
    add_test(NAME test_concurrency COMMAND test_concurrency)

    add_executable(test_ui_components tests/test_ui_components.cpp
            src/ui/delegates/CheckBoxDelegate.h
            src/ui/delegates/comboboxdelegate.h
            src/ui/delegates/comboboxmodel.h
            src/ui/delegates/readonlydelegate.h)
    target_include_directories(test_ui_components PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ui)
    target_link_libraries(test_ui_components PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
    add_test(NAME test_ui_components COMMAND test_ui_components)

    add_executable(test_connectionpool tests/test_connectionpool.cpp src/database/connectionpool.cpp)
    target_include_directories(test_connectionpool PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/database)
    target_link_libraries(test_connectionpool PRIVATE Qt6::Test Qt6::Core Qt6::Sql Threads::Threads)
    add_test(NAME test_connectionpool COMMAND test_connectionpool)

    add_executable(test_circuitbreaker tests/test_circuitbreaker.cpp src/utils/circuitbreaker.cpp)
    target_include_directories(test_circuitbreaker PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_circuitbreaker PRIVATE Qt6::Test Qt6::Core)
    add_test(NAME test_circuitbreaker COMMAND test_circuitbreaker)

    add_executable(test_reportexporter tests/test_reportexporter.cpp
            src/utils/reportexporter.cpp)
    target_include_directories(test_reportexporter PRIVATE
            ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/libs/QXlsx/QXlsx/header)
    target_link_libraries(test_reportexporter PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
            Qt6::Sql Qt6::PrintSupport QXlsx::QXlsx)
    add_test(NAME test_reportexporter COMMAND test_reportexporter)
    set_tests_properties(test_reportexporter PROPERTIES TIMEOUT 60)

    add_executable(test_repositories tests/test_repositories.cpp
            src/database/repositories/terminalrepository.cpp
            src/database/repositories/clientrepository.cpp
            src/database/repositories/simcardrepository.cpp
            src/database/repositories/documentrepository.cpp
            src/database/repositories/paymentrepository.cpp)
    target_include_directories(test_repositories PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_repositories PRIVATE Qt6::Test Qt6::Core Qt6::Sql)
    add_test(NAME test_repositories COMMAND test_repositories)

    if(DEFINED CMAKE_PREFIX_PATH AND EXISTS "${CMAKE_PREFIX_PATH}/plugins/sqldrivers")
        foreach(_sql_driver_pair "test_db_integration;qsqlpsql.dll" "test_concurrency;qsqlpsql.dll" "test_connectionpool;qsqlite.dll" "test_validator;qsqlite.dll" "test_repositories;qsqlite.dll")
            list(GET _sql_driver_pair 0 _test)
            list(GET _sql_driver_pair 1 _driver)
            if(EXISTS "${CMAKE_PREFIX_PATH}/plugins/sqldrivers/${_driver}")
                add_custom_command(TARGET ${_test} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${_test}>/sqldrivers"
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${CMAKE_PREFIX_PATH}/plugins/sqldrivers/${_driver}"
                        "$<TARGET_FILE_DIR:${_test}>/sqldrivers/${_driver}"
                        COMMENT "Copying ${_driver} for ${_test}"
                )
            endif()
        endforeach()
    endif()

    # QPA-платформа (qwindows.dll) для GUI-тестов. Qt DLL копируются рядом с
    # exe-тестами, поэтому Qt вычисляет каталог плагинов относительно exe и
    # не находит «windows»-плагин — QApplication падает на qFatal
    # (0xc0000602). Копируем плагины в стандартную раскладку <exe>/plugins/.
    if(DEFINED CMAKE_PREFIX_PATH AND EXISTS "${CMAKE_PREFIX_PATH}/plugins/platforms")
        foreach(_test test_ui_components test_reportexporter)
            add_custom_command(TARGET ${_test} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_PREFIX_PATH}/plugins/platforms"
                    "$<TARGET_FILE_DIR:${_test}>/plugins/platforms"
                    COMMENT "Copying Qt platform plugins for ${_test}"
            )
        endforeach()
    endif()

    # Копируем DLL Qt и PostgreSQL рядом с тестами, чтобы ctest работал без настройки PATH
    foreach(_test test_password_utils test_validator test_loginform test_update_utils test_updatemanager
            test_opsscheduler test_db_integration test_concurrency
            test_ui_components test_connectionpool test_circuitbreaker test_reportexporter test_repositories)
        foreach(_qt_dll Qt6Test.dll Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll Qt6Sql.dll Qt6PrintSupport.dll)
            if(DEFINED QT_DLL_DIR AND EXISTS "${QT_DLL_DIR}/${_qt_dll}")
                add_custom_command(TARGET ${_test} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${QT_DLL_DIR}/${_qt_dll}"
                        "$<TARGET_FILE_DIR:${_test}>/${_qt_dll}"
                        COMMENT "Copying ${_qt_dll}"
                )
            endif()
        endforeach()
        foreach(_pg_dll libpq.dll libintl-9.dll libssl-3-x64.dll libcrypto-3-x64.dll)
            if(DEFINED PG_DLL_DIR AND EXISTS "${PG_DLL_DIR}/${_pg_dll}")
                add_custom_command(TARGET ${_test} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${PG_DLL_DIR}/${_pg_dll}"
                        "$<TARGET_FILE_DIR:${_test}>/${_pg_dll}"
                        COMMENT "Copying ${_pg_dll}"
                )
            endif()
        endforeach()
    endforeach()
endif()

# ============================================================
# Доставка: портативный дистрибутив (windeployqt + CPack ZIP)
# ============================================================
if(WIN32)
    find_program(WINDEPLOYQT_EXE windeployqt HINTS "${CMAKE_PREFIX_PATH}/bin" DOC "Qt windeployqt")
    find_program(MAKENSIS_EXE makensis DOC "NSIS makensis (для CPack NSIS-установщика)")

    if(WINDEPLOYQT_EXE)
        set(DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy")
        set(DEPLOY_CMDS)

        list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E rm -rf "${DEPLOY_DIR}")
        list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/config")
        list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/sql/migrations")
        list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/docs")

        # Сам exe, затем Qt runtime + плагины через windeployqt
        list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E copy
                "$<TARGET_FILE:${PROJECT_NAME}>" "${DEPLOY_DIR}/POC_Terminal_Tracker.exe")
        list(APPEND DEPLOY_CMDS COMMAND "${WINDEPLOYQT_EXE}" --no-translations
                "${DEPLOY_DIR}/POC_Terminal_Tracker.exe")

        # Драйвер QPSQL (windeployqt его не определяет — плагин грузится динамически)
        if(EXISTS "${CMAKE_PREFIX_PATH}/plugins/sqldrivers/qsqlpsql.dll")
            list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_PREFIX_PATH}/plugins/sqldrivers/qsqlpsql.dll"
                    "${DEPLOY_DIR}/sqldrivers/qsqlpsql.dll")
        endif()

        # PostgreSQL DLL (libpq и зависимости)
        if(DEFINED PostgreSQL_ROOT AND EXISTS "${PostgreSQL_ROOT}/bin")
            foreach(_pg_dll libpq.dll libintl-9.dll libssl-3-x64.dll libcrypto-3-x64.dll)
                if(EXISTS "${PostgreSQL_ROOT}/bin/${_pg_dll}")
                    list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${PostgreSQL_ROOT}/bin/${_pg_dll}" "${DEPLOY_DIR}/${_pg_dll}")
                endif()
            endforeach()
        endif()

        # MinGW runtime (libgcc/libstdc++/libwinpthread), если не попал в deploy через windeployqt
        get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        foreach(_rt_dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
            if(EXISTS "${MINGW_BIN_DIR}/${_rt_dll}")
                list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${MINGW_BIN_DIR}/${_rt_dll}" "${DEPLOY_DIR}/${_rt_dll}")
            endif()
        endforeach()

        # Конфиг, миграции, документация
        list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${POC_CONFIG_TEMPLATE}" "${DEPLOY_DIR}/config/config.json")
        list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/sql/migrations" "${DEPLOY_DIR}/sql/migrations")
        list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/docs/OPS.md" "${DEPLOY_DIR}/docs/OPS.md")
        list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/docs/API.md" "${DEPLOY_DIR}/docs/API.md")
        list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/docs/adr" "${DEPLOY_DIR}/docs/adr")
        list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/README.md" "${DEPLOY_DIR}/README.md")
        list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/CHANGELOG.md" "${DEPLOY_DIR}/CHANGELOG.md")
        list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/.env.example" "${DEPLOY_DIR}/.env.example")

        add_custom_target(deploy ${DEPLOY_CMDS}
                DEPENDS ${PROJECT_NAME}
                COMMENT "Сборка портативного дистрибутива (windeployqt)")

        # ---- CPack: портативный ZIP + установщик NSIS из папки deploy ----
        include(InstallRequiredSystemLibraries)
        file(READ "${POC_CONFIG_TEMPLATE}" POC_CONFIG_JSON)
        string(JSON POC_VERSION GET "${POC_CONFIG_JSON}" "application" "version")

        # Содержимое deploy (exe + Qt runtime + DLL + конфиг + docs) становится install-контентом.
        # OPTIONAL — каталог появляется после сборки цели deploy.
        install(DIRECTORY "${DEPLOY_DIR}/" DESTINATION "." OPTIONAL)

        set(CPACK_PACKAGE_NAME "POC_Terminal_Tracker")
        set(CPACK_PACKAGE_VERSION "${POC_VERSION}")
        set(CPACK_PACKAGE_VENDOR "POC")
        set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Система учёта POC-терминалов и SIM-карт")
        set(CPACK_PACKAGE_FILE_NAME "POC_Terminal_Tracker-${POC_VERSION}")
        set(CPACK_PACKAGE_INSTALL_DIRECTORY "POC_Terminal_Tracker")
        set(CPACK_GENERATOR "ZIP")

        # Установщик NSIS (собирается только если makensis найден)
        if(MAKENSIS_EXE)
            list(APPEND CPACK_GENERATOR "NSIS")
            set(CPACK_NSIS_EXECUTABLE "${MAKENSIS_EXE}")
        endif()
        set(CPACK_NSIS_DISPLAY_NAME "POC Terminal Tracker")
        set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
        set(CPACK_NSIS_MODIFY_PATH OFF)
        set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
        set(CPACK_NSIS_MENU_LINKS "docs/OPS.md" "Руководство по эксплуатации")
        include(CPack)
    else()
        message(FATAL_ERROR "windeployqt не найден — сборка портативного дистрибутива невозможна.\n"
                "Задайте -DCMAKE_PREFIX_PATH=<каталог Qt, например E:/Qt/6.11.1/mingw_64>\n"
                "или установите windeployqt в PATH.")
    endif()

    # ============================================================
    # Код-подпись exe (опционально, требуется PFX-сертификат)
    # ============================================================
    # Использование: cmake -DPOC_SIGNING=ON -DPOC_SIGNING_PFX=C:/certs/sign.pfx
    #                -DPOC_SIGNING_PASSWORD=...
    # Требуется установленный osslsigncode (https://github.com/mtrojnar/osslsigncode).
    # Подпись выполняется после сборки, до копирования в deploy — в пакет попадает
    # уже подписанный exe. В CI используется PowerShell Set-AuthenticodeSignature
    # (см. .github/workflows/ci.yml), если задан секрет CODE_SIGN_PFX_BASE64.
    option(POC_SIGNING "Подписывать POC_Terminal_Tracker.exe после сборки" OFF)
    set(POC_SIGNING_PFX "" CACHE FILEPATH "Путь к PFX-сертификату для подписи")
    set(POC_SIGNING_PASSWORD "" CACHE STRING "Пароль от PFX-сертификата")

    if(POC_SIGNING AND POC_SIGNING_PFX)
        find_program(OSSLSIGNCODE_EXE osslsigncode)
        if(OSSLSIGNCODE_EXE)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND "${OSSLSIGNCODE_EXE}" sign -pkcs12 "${POC_SIGNING_PFX}"
                            -pass "${POC_SIGNING_PASSWORD}"
                            -in "$<TARGET_FILE:${PROJECT_NAME}>"
                            -out "$<TARGET_FILE:${PROJECT_NAME}>.signed"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "$<TARGET_FILE:${PROJECT_NAME}>.signed" "$<TARGET_FILE:${PROJECT_NAME}>"
                    COMMAND ${CMAKE_COMMAND} -E remove -f "$<TARGET_FILE:${PROJECT_NAME}>.signed"
                    COMMENT "Подпись POC_Terminal_Tracker.exe (osslsigncode)")
        else()
            message(WARNING "POC_SIGNING=ON, но osslsigncode не найден — подпись не выполняется")
        endif()
    endif()
endif()

# ============================================================
# Lint: clang-format (dry-run) — отдельная цель, не влияет на сборку.
# Запуск: cmake --build build --target lint
# ============================================================
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    file(GLOB_RECURSE LINT_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.h"
        "${CMAKE_SOURCE_DIR}/tests/*.cpp"
        "${CMAKE_SOURCE_DIR}/tests/*.h")
    add_custom_target(lint
        COMMAND "${CLANG_FORMAT}" --dry-run -Werror ${LINT_SOURCES}
        COMMENT "Проверка форматирования (clang-format --dry-run -Werror)")
else()
    add_custom_target(lint COMMAND ${CMAKE_COMMAND} -E echo
        "clang-format не найден — проверка форматирования пропущена")
endif()