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

# Сборка разбита на модули (см. каталог cmake/). Порядок include() важен:
# tests.cmake использует QT_DLL_DIR/PG_DLL_DIR из main_target.cmake,
# deploy.cmake — POC_CONFIG_TEMPLATE из main_target.cmake.
include(cmake/dependencies.cmake)   # find_package: Qt6, PostgreSQL, QXlsx, утилиты
include(cmake/sources.cmake)        # списки SOURCES / HEADERS / UI_FILES
include(cmake/main_target.cmake)    # цель ${PROJECT_NAME}: линковка, копирование DLL/конфига
include(cmake/tools.cmake)          # вспомогательные цели (scanner_probe)
include(cmake/tests.cmake)          # опциональные unit-тесты (BUILD_TESTS)
include(cmake/deploy.cmake)         # windeployqt + CPack (ZIP/NSIS) + код-подпись
include(cmake/lint.cmake)           # цель lint (clang-format --dry-run)
