cmake_minimum_required(VERSION 3.20)
project(POC_Terminal_Tracker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

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

# Подавляем предупреждение Qt о подключении GuiPrivate (нужен QXlsx для Qt >= 6.10)
set(QT_NO_PRIVATE_MODULE_WARNING ON CACHE BOOL "" FORCE)

# Подключаем QXlsx
add_subdirectory(libs/QXlsx/QXlsx)

# Исходные файлы
set(SOURCES
        src/main.cpp
        src/database/databasemanager.cpp
        src/database/submiterrortablemodel.cpp
        src/ui/mainwindow.cpp
        src/ui/dialogs/manufacturersform.cpp
        src/ui/dialogs/modelsform.cpp
        src/ui/dialogs/clientsform.cpp
        src/ui/dialogs/simcardsform.cpp
        src/ui/dialogs/terminalsform.cpp
        resources.qrc
        src/ui/dialogs/receiptform.cpp
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
        src/utils/reportexporter.cpp
        src/utils/validator.cpp
        src/utils/logging.cpp
        src/ops/backupmanager.cpp
        src/ops/backupworker.cpp
        src/ops/opslog.cpp
        src/ops/opsscheduler.cpp
        src/update/updatemanager.cpp
)

set(HEADERS
        src/database/databasemanager.h
        src/database/idatabasemanager.h
        src/database/submiterrortablemodel.h
        src/ui/mainwindow.h
        src/ui/dialogs/manufacturersform.h
        src/ui/dialogs/modelsform.h
        src/ui/dialogs/clientsform.h
        src/ui/dialogs/simcardsform.h
        src/ui/dialogs/terminalsform.h
        src/ui/delegates/readonlydelegate.h
        src/ui/dialogs/receiptform.h
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
        src/utils/reportexporter.h
        src/utils/validator.h
        src/utils/logging.h
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
        src/ui/dialogs/receiptform.ui
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

# Копируем config.json в папку сборки
configure_file(
        ${CMAKE_SOURCE_DIR}/config/config.json
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

    add_executable(test_update_utils tests/test_update_utils.cpp)
    target_include_directories(test_update_utils PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_update_utils PRIVATE Qt6::Test Qt6::Core)
    add_test(NAME test_update_utils COMMAND test_update_utils)

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

    if(DEFINED CMAKE_PREFIX_PATH AND EXISTS "${CMAKE_PREFIX_PATH}/plugins/sqldrivers/qsqlpsql.dll")
        foreach(_test test_db_integration test_concurrency)
            add_custom_command(TARGET ${_test} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${_test}>/sqldrivers"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_PREFIX_PATH}/plugins/sqldrivers/qsqlpsql.dll"
                    "$<TARGET_FILE_DIR:${_test}>/sqldrivers/qsqlpsql.dll"
                    COMMENT "Copying qsqlpsql.dll for ${_test}"
            )
        endforeach()
    endif()

    # Копируем DLL Qt и PostgreSQL рядом с тестами, чтобы ctest работал без настройки PATH
    foreach(_test test_password_utils test_validator test_update_utils test_db_integration test_concurrency)
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
                "${CMAKE_SOURCE_DIR}/config/config.json" "${DEPLOY_DIR}/config/config.json")
        list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/sql/migrations" "${DEPLOY_DIR}/sql/migrations")
        list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/docs/OPS.md" "${DEPLOY_DIR}/docs/OPS.md")
        list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/README.md" "${DEPLOY_DIR}/README.md")
        list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/CHANGELOG.md" "${DEPLOY_DIR}/CHANGELOG.md")
        list(APPEND DEPLOY_CMDS COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/.env.example" "${DEPLOY_DIR}/.env.example")

        add_custom_target(deploy ${DEPLOY_CMDS}
                DEPENDS ${PROJECT_NAME}
                COMMENT "Сборка портативного дистрибутива (windeployqt)")

        # ---- CPack: ZIP-архив из папки deploy ----
        include(InstallRequiredSystemLibraries)
        file(READ "${CMAKE_SOURCE_DIR}/config/config.json" POC_CONFIG_JSON)
        string(JSON POC_VERSION GET "${POC_CONFIG_JSON}" "application" "version")

        set(CPACK_PACKAGE_NAME "POC_Terminal_Tracker")
        set(CPACK_PACKAGE_VERSION "${POC_VERSION}")
        set(CPACK_PACKAGE_VENDOR "POC")
        set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Система учёта POC-терминалов и SIM-карт")
        set(CPACK_PACKAGE_FILE_NAME "POC_Terminal_Tracker-${POC_VERSION}-portable")
        set(CPACK_PACKAGE_INSTALL_DIRECTORY "POC_Terminal_Tracker")
        set(CPACK_GENERATOR "ZIP")
        set(CPACK_INSTALLED_DIRECTORIES "${DEPLOY_DIR}" "/")
        include(CPack)
    else()
        message(WARNING "windeployqt не найден — цель deploy и CPack-пакетирование отключены")
    endif()
endif()