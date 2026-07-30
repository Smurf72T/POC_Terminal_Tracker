cmake_minimum_required(VERSION 3.20)
project(POC_Terminal_Tracker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Ищем строго Qt6
find_package(Qt6 6.11.1 REQUIRED COMPONENTS
        Core
        Gui
        Widgets
        Sql
        PrintSupport
        Charts
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
)

set(HEADERS
        src/database/databasemanager.h
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
        src/ui/dialogs/rentalform.h
        src/ui/dialogs/returnform.h
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
        PostgreSQL::PostgreSQL
        QXlsx::QXlsx
)

# Копируем config.json в папку сборки
configure_file(
        ${CMAKE_SOURCE_DIR}/config/config.json
        ${CMAKE_BINARY_DIR}/config/config.json
        COPYONLY
)

# Копируем .env в папку сборки
configure_file(
        ${CMAKE_SOURCE_DIR}/.env
        ${CMAKE_BINARY_DIR}/.env
        COPYONLY
)

# Добавляем include директории
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/ui
        ${PostgreSQL_INCLUDE_DIRS}
)

# Копируем DLL PostgreSQL в папку сборки
set(PG_DLL_DIR "${PostgreSQL_ROOT}/bin")
foreach(_pg_dll libpq.dll libintl-9.dll libssl-3-x64.dll libcrypto-3-x64.dll)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PG_DLL_DIR}/${_pg_dll}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${_pg_dll}"
            COMMENT "Copying ${_pg_dll}"
    )
endforeach()

# Копируем Qt Charts DLL
set(QT_DLL_DIR "${CMAKE_PREFIX_PATH}/bin")
foreach(_qt_dll Qt6Charts.dll Qt6OpenGL.dll Qt6OpenGLWidgets.dll)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_DLL_DIR}/${_qt_dll}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${_qt_dll}"
            COMMENT "Copying ${_qt_dll}"
    )
endforeach()

# Опциональная сборка тестов
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(test_password_utils tests/test_password_utils.cpp)
    target_include_directories(test_password_utils PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_password_utils PRIVATE Qt6::Test Qt6::Core)
    add_test(NAME test_password_utils COMMAND test_password_utils)

    # test_validator требует DatabaseManager — временно отключён
endif()