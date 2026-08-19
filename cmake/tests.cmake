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
target_include_directories(test_update_utils PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
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
    target_include_directories(test_updatemanager PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
    target_link_libraries(test_updatemanager PRIVATE Qt6::Test Qt6::Core Qt6::Network)
    add_test(NAME test_updatemanager COMMAND test_updatemanager)

    add_executable(test_opsscheduler tests/test_opsscheduler.cpp
            src/ops/opsscheduler.cpp
            src/ops/backupmanager.cpp
            src/ops/backupmanager_helpers.cpp
            src/ops/backupmanager_fallback.cpp
            src/ops/backupmanager_restore.cpp
            src/ops/backupworker.cpp
            src/ops/opslog.cpp
            src/database/databasemanager.cpp
            src/database/databasemanager_connection.cpp
            src/database/databasemanager_migrations.cpp
            src/database/databasemanager_queries.cpp
            src/database/databasemanager_session.cpp
            src/database/connectionpool.cpp
            src/utils/circuitbreaker.cpp
            src/utils/logging.cpp)
    target_include_directories(test_opsscheduler PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_opsscheduler PRIVATE Qt6::Test Qt6::Core Qt6::Sql Qt6::Widgets Threads::Threads)
    add_test(NAME test_opsscheduler COMMAND test_opsscheduler)

    add_executable(test_db_integration tests/test_db_integration.cpp
            src/ops/backupmanager.cpp
            src/ops/backupmanager_helpers.cpp
            src/ops/backupmanager_fallback.cpp
            src/ops/backupmanager_restore.cpp
            src/ops/opslog.cpp
            src/utils/logging.cpp)
    target_include_directories(test_db_integration PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_db_integration PRIVATE Qt6::Test Qt6::Core Qt6::Sql)
    add_test(NAME test_db_integration COMMAND test_db_integration)

    add_executable(test_concurrency
            tests/test_concurrency.cpp
            tests/test_concurrency_support.cpp
            tests/test_concurrency_rental.cpp
            tests/test_concurrency_numbers.cpp)
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
