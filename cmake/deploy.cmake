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
