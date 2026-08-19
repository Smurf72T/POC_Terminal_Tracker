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
