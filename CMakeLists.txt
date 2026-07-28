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
        src/ui/delegates/CheckBoxDelegate.h
        src/ui/dialogs/receiptform.h
        src/ui/delegates/comboboxdelegate.h
        src/ui/dialogs/rentalform.h
        src/ui/delegates/comboboxmodel.h
        src/ui/dialogs/returnform.h
        src/ui/dialogs/archivedocumentsform.h
        src/ui/dialogs/paymentform.h
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
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${UI_FILES})

# Линковка Qt6 и PostgreSQL
target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Sql
        Qt6::PrintSupport
        PostgreSQL::PostgreSQL
        QXlsx::QXlsx
)

# Копируем config.json в папку сборки, чтобы приложение его видело при запуске
configure_file(
        ${CMAKE_SOURCE_DIR}/config/config.json
        ${CMAKE_BINARY_DIR}/config/config.json
        COPYONLY
)

# Добавляем include директории
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/ui
        ${PostgreSQL_INCLUDE_DIRS}
)