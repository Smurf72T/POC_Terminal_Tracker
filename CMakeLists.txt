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

# Исходные файлы
set(SOURCES
        src/main.cpp
        src/database/databasemanager.cpp
        src/ui/mainwindow.cpp
        src/ui/manufacturersform.cpp
        src/ui/modelsform.cpp
        src/ui/clientsform.cpp
        src/ui/simcardsform.cpp
        src/ui/terminalsform.cpp
)

set(HEADERS
        src/database/databasemanager.h
        src/ui/mainwindow.h
        src/ui/manufacturersform.h
        src/ui/modelsform.h
        src/ui/clientsform.h
        src/ui/simcardsform.h
        src/ui/terminalsform.h
        src/ui/readonlydelegate.h
)

set(UI_FILES
        src/ui/mainwindow.ui
        src/ui/manufacturersform.ui
        src/ui/modelsform.ui
        src/ui/clientsform.ui
        src/ui/simcardsform.ui
        src/ui/terminalsform.ui
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
        ${PostgreSQL_INCLUDE_DIRS}
)