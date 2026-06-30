cmake_minimum_required(VERSION 3.25)

project(textfx
    VERSION 0.1.0
    DESCRIPTION "TextFX desktop editor"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(GNUInstallDirs)

option(TEXTFX_ENABLE_QT_DEPLOY "Run Qt QML deployment during install/package" OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick Qml QuickControls2 QuickDialogs2)
find_package(Qt6 6.8 QUIET COMPONENTS Test)

qt_standard_project_setup(REQUIRES 6.8)
qt_policy(SET QTP0004 NEW)

qt_add_executable(textfx
    src/app/main.cpp
    src/ui/OutlinedTextItem.cpp
    src/ui/OutlinedTextItem.h
)

add_library(textfx_core
    src/app/EditorController.cpp
    src/app/EditorController.h
    src/app/EditorViewModels.cpp
    src/app/EditorViewModels.h
    src/app/PageTextService.cpp
    src/app/PageTextService.h
    src/app/ProjectExportService.cpp
    src/app/ProjectExportService.h
    src/app/ProjectSessionService.cpp
    src/app/ProjectSessionService.h
    src/app/TextBoxClipboardService.cpp
    src/app/TextBoxClipboardService.h
    src/app/TextBoxEditingService.cpp
    src/app/TextBoxEditingService.h
    src/app/TextBoxSelectionService.cpp
    src/app/TextBoxSelectionService.h
    src/app/TextPresetService.cpp
    src/app/TextPresetService.h
    src/core/DocumentModel.cpp
    src/core/DocumentModel.h
    src/fonts/FontResolver.cpp
    src/fonts/FontResolver.h
    src/fonts/SfntNames.h
    src/core/ProjectStore.cpp
    src/core/ProjectStore.h
    src/io/JsonSerializer.cpp
    src/io/JsonSerializer.h
    src/render/RenderGraph.cpp
    src/render/RenderGraph.h
    src/render/RenderTextLayout.cpp
    src/render/RenderTextLayout.h
)

target_include_directories(textfx_core PUBLIC src)

target_link_libraries(textfx_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
)

qt_add_qml_module(textfx
    URI TextFX
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/ColorButton.qml
        qml/CentralCanvasShell.qml
        qml/EditorChrome.qml
        qml/LeftInspectorPanel.qml
        qml/RightInspectorPanel.qml
        qml/ShortcutMenuItem.qml
        qml/TextBoxDelegate.qml
        qml/TextStyleButton.qml
)

target_link_libraries(textfx
    PRIVATE
        textfx_core
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickDialogs2
)

install(TARGETS textfx
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(TEXTFX_ENABLE_QT_DEPLOY AND COMMAND qt_generate_deploy_qml_app_script)
    qt_generate_deploy_qml_app_script(
        TARGET textfx
        OUTPUT_SCRIPT textfx_qt_deploy_script
    )
    install(SCRIPT ${textfx_qt_deploy_script})
elseif(TEXTFX_ENABLE_QT_DEPLOY)
    message(STATUS "Qt QML deployment helper unavailable; package will contain only installed TextFX targets")
else()
    message(STATUS "Qt deployment disabled; set TEXTFX_ENABLE_QT_DEPLOY=ON after target platform validation")
endif()

set(CPACK_PACKAGE_VENDOR "TextFX")
set(CPACK_PACKAGE_CONTACT "TextFX maintainers")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "TextFX desktop editor MVP")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "TextFX")
set(CPACK_GENERATOR "TGZ")
include(CPack)

if(BUILD_TESTING)
    find_package(Catch2 3 QUIET)

    set(TEXTFX_QT_TEST_ENVIRONMENT
        QT_QPA_PLATFORM=offscreen
        QT_QUICK_BACKEND=software
        QSG_RHI_BACKEND=software
    )

    add_executable(textfx_serialization_check
        tests/serialization_check.cpp
    )

    target_link_libraries(textfx_serialization_check PRIVATE textfx_core)
    target_compile_definitions(textfx_serialization_check PRIVATE TEXTFX_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    list(APPEND TEXTFX_TEST_TARGETS textfx_serialization_check)

    add_executable(textfx_render_golden_check
        tests/render_golden_check.cpp
    )

    target_link_libraries(textfx_render_golden_check PRIVATE textfx_core)
    target_compile_definitions(textfx_render_golden_check PRIVATE TEXTFX_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    list(APPEND TEXTFX_TEST_TARGETS textfx_render_golden_check)

    add_test(NAME textfx_serialization_check COMMAND textfx_serialization_check)
    add_test(NAME textfx_render_golden_check COMMAND textfx_render_golden_check)
    set_tests_properties(textfx_serialization_check PROPERTIES
        LABELS core
        TIMEOUT 30
    )
    set_tests_properties(textfx_render_golden_check PROPERTIES
        LABELS render
        TIMEOUT 60
        ENVIRONMENT "${TEXTFX_QT_TEST_ENVIRONMENT}"
    )

    if(Catch2_FOUND)
        include(Catch)

        add_executable(textfx_core_tests
            tests/core_io_tests.cpp
        )

        target_link_libraries(textfx_core_tests
            PRIVATE
                textfx_core
                Catch2::Catch2WithMain
        )

        catch_discover_tests(textfx_core_tests
            PROPERTIES
                LABELS core
                TIMEOUT 30
        )
        list(APPEND TEXTFX_TEST_TARGETS textfx_core_tests)
    else()
        message(STATUS "Catch2 3 not found; Catch2 core tests are not built. Install Catch2 3 locally to include them in the check target.")
    endif()

    if(Qt6Test_FOUND)
        qt_add_executable(textfx_qt_smoke_tests
            tests/qt_smoke_tests.cpp
            tests/qt_test_helpers.cpp
            tests/qt_test_helpers.h
            src/ui/OutlinedTextItem.cpp
            src/ui/OutlinedTextItem.h
        )

        target_link_libraries(textfx_qt_smoke_tests
            PRIVATE
                textfx_core
                Qt6::Qml
                Qt6::Quick
                Qt6::QuickControls2
                Qt6::QuickDialogs2
                Qt6::Test
        )
        target_compile_definitions(textfx_qt_smoke_tests PRIVATE
            TEXTFX_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
            TEXTFX_TESTING=1
        )

        add_test(NAME textfx_qt_smoke_tests COMMAND textfx_qt_smoke_tests)
        set_tests_properties(textfx_qt_smoke_tests PROPERTIES
            LABELS "qml;smoke"
            TIMEOUT 90
            ENVIRONMENT "${TEXTFX_QT_TEST_ENVIRONMENT}"
        )
        list(APPEND TEXTFX_TEST_TARGETS textfx_qt_smoke_tests)
    else()
        message(STATUS "Qt6 Test not found; Qt/QML smoke tests are not built. Install the Qt6 Test component locally to include them in the check target.")
    endif()

    add_executable(textfx_font_diagnostic
        tools/font_diagnostic.cpp
    )

    target_link_libraries(textfx_font_diagnostic
        PRIVATE
            textfx_core
            Qt6::Gui
    )

    add_custom_target(verify_all
        DEPENDS ${TEXTFX_TEST_TARGETS}
    )

    add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS verify_all
        USES_TERMINAL
    )
endif()
