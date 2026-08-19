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

# Версия приложения генерируется из CHANGELOG.md — первая запись "## [x.y.z]"
# считается актуальной. Единый источник правды: изменение CHANGELOG.md
# автоматически переконфигурирует сборку (CMAKE_CONFIGURE_DEPENDS) и новая
# версия попадает в бинарник без ручного редактирования config.json.
file(READ ${CMAKE_SOURCE_DIR}/CHANGELOG.md POC_CHANGELOG_TEXT)
string(REGEX MATCH "## \\[([0-9]+\\.[0-9]+\\.[0-9]+)" POC_CHANGELOG_VERSION "${POC_CHANGELOG_TEXT}")
set(POC_APP_VERSION "${CMAKE_MATCH_1}")
configure_file(
        ${CMAKE_SOURCE_DIR}/src/update/version.h.in
        ${CMAKE_BINARY_DIR}/generated/app_version.h
)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/CHANGELOG.md)

# Копируем SQL-миграции в папку сборки
file(COPY ${CMAKE_SOURCE_DIR}/sql/migrations/ DESTINATION ${CMAKE_BINARY_DIR}/sql/migrations/)

# Добавляем include директории
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/ui
        ${CMAKE_BINARY_DIR}/generated
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
