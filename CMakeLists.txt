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
option(TEXTFX_REQUIRE_TEST_DEPS "Fail configuration when test-only dependencies are unavailable" OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick Qml QuickControls2 QuickDialogs2)
if(TEXTFX_REQUIRE_TEST_DEPS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
else()
    find_package(Qt6 6.8 QUIET COMPONENTS Test)
endif()

qt_standard_project_setup(REQUIRES 6.8)
qt_policy(SET QTP0004 NEW)

qt_add_executable(textfx
    src/app/main.cpp
    src/app/qt/OutlinedTextItem.cpp
    src/app/qt/OutlinedTextItem.h
)

set(TEXTFX_DOMAIN_SOURCES
    src/domain/AuthoringLimits.h
    src/domain/document/DocumentModel.cpp
    src/domain/document/DocumentModel.h
)

set(TEXTFX_APPLICATION_SOURCES
    src/application/queries/CommandAvailability.cpp
    src/application/queries/CommandAvailability.h
    src/application/queries/SelectionQueryService.cpp
    src/application/queries/SelectionQueryService.h
    src/application/queries/BoxRoles.h
    src/application/queries/EffectMetadata.cpp
    src/application/queries/EffectMetadata.h
    src/application/ports/IPageExportRenderer.h
    src/application/ports/IProjectExportStore.h
    src/application/ports/IProjectPageSource.h
    src/application/services/PageTextService.cpp
    src/application/services/PageTextService.h
    src/application/services/ProjectExportService.cpp
    src/application/services/ProjectExportService.h
    src/application/services/ProjectSessionService.cpp
    src/application/services/ProjectSessionService.h
    src/application/services/TextBoxEditingService.cpp
    src/application/services/TextBoxEditingService.h
    src/application/services/TextBoxSelectionService.cpp
    src/application/services/TextBoxSelectionService.h
    src/application/services/TextPresetService.cpp
    src/application/services/TextPresetService.h
    src/application/services/TextWorkflowService.cpp
    src/application/services/TextWorkflowService.h
)

set(TEXTFX_INFRASTRUCTURE_SOURCES
    src/infrastructure/fonts/FontResolver.cpp
    src/infrastructure/fonts/FontResolver.h
    src/infrastructure/fonts/SfntNames.h
    src/infrastructure/persistence/ProjectStore.cpp
    src/infrastructure/persistence/ProjectStore.h
    src/infrastructure/persistence/JsonSerializer.cpp
    src/infrastructure/persistence/JsonSerializer.h
)

set(TEXTFX_RENDER_SOURCES
    src/render/GaussianBlur.h
    src/render/RenderGraph.cpp
    src/render/RenderGraph.h
    src/render/RenderTextLayout.cpp
    src/render/RenderTextLayout.h
)

set(TEXTFX_APP_SOURCES
    src/app/viewmodels/BoxRenderState.cpp
    src/app/viewmodels/BoxRenderState.h
    src/app/models/BoxesModel.cpp
    src/app/models/BoxesModel.h
    src/app/controllers/EditorController.cpp
    src/app/controllers/EditorControllerModelWorkflows.cpp
    src/app/controllers/EditorControllerSupport.cpp
    src/app/controllers/EditorController.h
    src/app/controllers/EditorControllerStringUtils.h
    src/app/project/ProjectCreationWorkflow.cpp
    src/app/project/ProjectCreationWorkflow.h
    src/app/project/ProjectSaveExportWorkflow.cpp
    src/app/project/ProjectSaveExportWorkflow.h
    src/app/qt/EditorLimits.cpp
    src/app/qt/EditorLimits.h
    src/app/viewmodels/EditorViewModels.cpp
    src/app/viewmodels/EditorViewModels.h
    src/app/services/TextBoxClipboardService.cpp
    src/app/services/TextBoxClipboardService.h
)

function(textfx_configure_includes target)
    target_include_directories(${target} PUBLIC src)
endfunction()

function(textfx_add_infrastructure_library target)
    add_library(${target} STATIC ${TEXTFX_INFRASTRUCTURE_SOURCES})
    textfx_configure_includes(${target})
    target_link_libraries(${target}
        PUBLIC
            textfx_application
            textfx_domain
            Qt6::Core
            Qt6::Gui
    )
endfunction()

function(textfx_add_render_library target infrastructure_target)
    add_library(${target} STATIC ${TEXTFX_RENDER_SOURCES})
    textfx_configure_includes(${target})
    target_link_libraries(${target}
        PUBLIC
            textfx_application
            textfx_domain
            ${infrastructure_target}
            Qt6::Core
            Qt6::Gui
    )
endfunction()

function(textfx_add_app_core_library target infrastructure_target render_target)
    add_library(${target} STATIC ${TEXTFX_APP_SOURCES})
    textfx_configure_includes(${target})
    target_link_libraries(${target}
        PUBLIC
            textfx_application
            textfx_domain
            ${infrastructure_target}
            ${render_target}
            Qt6::Core
            Qt6::Gui
            Qt6::Quick
    )
endfunction()

add_library(textfx_domain STATIC ${TEXTFX_DOMAIN_SOURCES})
textfx_configure_includes(textfx_domain)

add_library(textfx_application STATIC ${TEXTFX_APPLICATION_SOURCES})
textfx_configure_includes(textfx_application)
target_link_libraries(textfx_application
    PUBLIC
        textfx_domain
        Qt6::Core
)

textfx_add_infrastructure_library(textfx_infrastructure)
textfx_add_render_library(textfx_render textfx_infrastructure)
textfx_add_app_core_library(textfx_app_core textfx_infrastructure textfx_render)

set(TEXTFX_ICON_RESOURCES
    assets/icons/flat/align-center.svg
    assets/icons/flat/align-left.svg
    assets/icons/flat/align-right.svg
    assets/icons/flat/arrow-down.svg
    assets/icons/flat/arrow-left.svg
    assets/icons/flat/arrow-right.svg
    assets/icons/flat/arrow-up.svg
    assets/icons/flat/bold.svg
    assets/icons/flat/italic.svg
    assets/icons/flat/lowercase.svg
    assets/icons/flat/uppercase.svg
)

qt_add_qml_module(textfx
    URI TextFX
    VERSION 1.0
    QML_FILES
        qml/app/Main.qml
        qml/constants/UiConstants.js
        qml/utils/ColorUtils.js
        qml/features/rightPanel/BoxEffectsSection.qml
        qml/features/canvas/interactions/BoxMoveInteractionState.qml
        qml/features/canvas/interactions/BoxRotateInteractionState.qml
        qml/features/canvas/interactions/BoxResizeInteractionState.qml
        qml/features/canvas/CanvasInteractionState.qml
        qml/shared/ColorButton.qml
        qml/features/canvas/CentralCanvasShell.qml
        qml/features/canvas/CanvasView.qml
        qml/app/EditorChrome.qml
        qml/commands/EditorCommands.qml
        qml/features/rightPanel/LayersSection.qml
        qml/features/leftPanel/LeftInspectorPanel.qml
        qml/features/rightPanel/NavigationSection.qml
        qml/features/rightPanel/PageEffectsSection.qml
        qml/features/leftPanel/PageTextsSection.qml
        qml/features/canvas/PaintLayer.qml
        qml/features/rightPanel/PaintSection.qml
        qml/features/canvas/interactions/PathHandleInteractionState.qml
        qml/features/canvas/geometry/PerspectiveGeometry.qml
        qml/features/canvas/interactions/PerspectiveInteractionState.qml
        qml/features/rightPanel/RightInspectorPanel.qml
        qml/features/menus/FileMenu.qml
        qml/features/menus/EditMenu.qml
        qml/features/menus/ViewMenu.qml
        qml/shared/SelectedBoxState.qml
        qml/features/rightPanel/TextEffectsSection.qml
        qml/features/rightPanel/textEffects/OutlineEffectEditor.qml
        qml/features/rightPanel/textEffects/BlurEffectEditor.qml
        qml/features/rightPanel/textEffects/ShadowEffectEditor.qml
        qml/features/rightPanel/textEffects/GradientEffectEditor.qml
        qml/features/rightPanel/textEffects/PathEffectEditor.qml
        qml/features/leftPanel/TextPropertiesSection.qml
        qml/features/leftPanel/TextPresetsSection.qml
        qml/features/canvas/text/TextBoxDelegate.qml
        qml/features/canvas/text/TextBoxContent.qml
        qml/features/canvas/text/TextBoxEditControls.qml
        qml/features/canvas/text/TextBoxPathControls.qml
        qml/features/canvas/text/TextBoxSelectionControls.qml
        qml/features/canvas/text/TextBoxMoveArea.qml
        qml/features/canvas/text/TextEditOverlay.qml
        qml/features/canvas/text/TextPathGuide.qml
        qml/features/canvas/text/TextPathHandles.qml
        qml/features/canvas/text/TextResizeHandles.qml
        qml/features/canvas/text/TextRotateHandle.qml
        qml/shared/TextStyleButton.qml
        qml/features/canvas/ViewportMetrics.qml
    RESOURCES
        ${TEXTFX_ICON_RESOURCES}
)

target_link_libraries(textfx
    PRIVATE
        textfx_app_core
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
    if(TEXTFX_REQUIRE_TEST_DEPS)
        find_package(Catch2 3 REQUIRED)
    else()
        find_package(Catch2 3 QUIET)
    endif()

    set(TEXTFX_QT_TEST_ENVIRONMENT
        QT_QPA_PLATFORM=offscreen
        QT_QUICK_BACKEND=software
        QSG_RHI_BACKEND=software
    )

    textfx_add_infrastructure_library(textfx_infrastructure_test)
    target_compile_definitions(textfx_infrastructure_test PUBLIC TEXTFX_ENABLE_TEST_HOOKS=1)

    textfx_add_render_library(textfx_render_test textfx_infrastructure_test)
    target_compile_definitions(textfx_render_test PUBLIC TEXTFX_ENABLE_TEST_HOOKS=1)

    textfx_add_app_core_library(textfx_app_core_test textfx_infrastructure_test textfx_render_test)
    target_compile_definitions(textfx_app_core_test PUBLIC TEXTFX_ENABLE_TEST_HOOKS=1)

    function(textfx_assert_no_test_hooks target)
        get_target_property(compile_definitions ${target} COMPILE_DEFINITIONS)
        get_target_property(interface_definitions ${target} INTERFACE_COMPILE_DEFINITIONS)
        get_target_property(link_libraries ${target} LINK_LIBRARIES)
        foreach(values IN ITEMS compile_definitions interface_definitions link_libraries)
            if(${values} MATCHES "TEXTFX_ENABLE_TEST_HOOKS|textfx_(infrastructure|render)_test|textfx_app_core_test")
                message(FATAL_ERROR "${target} must not depend on test hooks")
            endif()
        endforeach()
    endfunction()

    textfx_assert_no_test_hooks(textfx_app_core)
    textfx_assert_no_test_hooks(textfx)

    function(textfx_add_test_icon_resources target)
        qt_add_resources(${target} "${target}_icon_resources"
            PREFIX "/qt/qml/TextFX"
            FILES ${TEXTFX_ICON_RESOURCES}
        )
    endfunction()

    add_executable(textfx_serialization_check
        tests/serialization_check.cpp
    )

    target_link_libraries(textfx_serialization_check PRIVATE textfx_app_core)
    target_compile_definitions(textfx_serialization_check PRIVATE TEXTFX_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    list(APPEND TEXTFX_TEST_TARGETS textfx_serialization_check)

    add_executable(textfx_render_golden_check
        tests/render_golden_check.cpp
    )

    target_link_libraries(textfx_render_golden_check PRIVATE textfx_app_core_test)
    target_compile_definitions(textfx_render_golden_check PRIVATE
        TEXTFX_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
    )
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
                textfx_app_core
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
            src/app/qt/OutlinedTextItem.cpp
            src/app/qt/OutlinedTextItem.h
        )

        target_link_libraries(textfx_qt_smoke_tests
            PRIVATE
                textfx_app_core
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
        textfx_add_test_icon_resources(textfx_qt_smoke_tests)

        add_test(NAME textfx_qt_smoke_tests COMMAND textfx_qt_smoke_tests)
        set_tests_properties(textfx_qt_smoke_tests PROPERTIES
            LABELS "qml;smoke"
            TIMEOUT 90
            ENVIRONMENT "${TEXTFX_QT_TEST_ENVIRONMENT}"
        )
        list(APPEND TEXTFX_TEST_TARGETS textfx_qt_smoke_tests)

        qt_add_executable(textfx_editor_controller_workflow_tests
            tests/editor_controller_workflow_tests.cpp
            tests/qt_test_helpers.cpp
            tests/qt_test_helpers.h
            src/app/qt/OutlinedTextItem.cpp
            src/app/qt/OutlinedTextItem.h
        )

        target_link_libraries(textfx_editor_controller_workflow_tests
            PRIVATE
                textfx_app_core_test
                Qt6::Test
        )
        target_compile_definitions(textfx_editor_controller_workflow_tests PRIVATE
            TEXTFX_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
            TEXTFX_TESTING=1
        )

        add_test(NAME textfx_editor_controller_workflow_tests COMMAND textfx_editor_controller_workflow_tests)
        set_tests_properties(textfx_editor_controller_workflow_tests PROPERTIES
            LABELS "controller;workflow"
            TIMEOUT 90
            ENVIRONMENT "${TEXTFX_QT_TEST_ENVIRONMENT}"
        )
        list(APPEND TEXTFX_TEST_TARGETS textfx_editor_controller_workflow_tests)

        qt_add_executable(textfx_qml_shell_smoke_tests
            tests/qml_shell_smoke_tests.cpp
            tests/qt_test_helpers.cpp
            tests/qt_test_helpers.h
            src/app/qt/OutlinedTextItem.cpp
            src/app/qt/OutlinedTextItem.h
        )

        target_link_libraries(textfx_qml_shell_smoke_tests
            PRIVATE
                textfx_app_core
                Qt6::Qml
                Qt6::Quick
                Qt6::QuickControls2
                Qt6::QuickDialogs2
                Qt6::Test
        )
        target_compile_definitions(textfx_qml_shell_smoke_tests PRIVATE
            TEXTFX_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
            TEXTFX_TESTING=1
        )
        textfx_add_test_icon_resources(textfx_qml_shell_smoke_tests)

        add_test(NAME textfx_qml_shell_smoke_tests COMMAND textfx_qml_shell_smoke_tests)
        set_tests_properties(textfx_qml_shell_smoke_tests PROPERTIES
            LABELS "qml;smoke"
            TIMEOUT 90
            ENVIRONMENT "${TEXTFX_QT_TEST_ENVIRONMENT}"
        )
        list(APPEND TEXTFX_TEST_TARGETS textfx_qml_shell_smoke_tests)

        qt_add_executable(textfx_qml_interaction_smoke_tests
            tests/qml_interaction_smoke_tests.cpp
            tests/qt_test_helpers.cpp
            tests/qt_test_helpers.h
            src/app/qt/OutlinedTextItem.cpp
            src/app/qt/OutlinedTextItem.h
        )

        target_link_libraries(textfx_qml_interaction_smoke_tests
            PRIVATE
                textfx_app_core
                Qt6::Qml
                Qt6::Quick
                Qt6::QuickControls2
                Qt6::QuickDialogs2
                Qt6::Test
        )
        target_compile_definitions(textfx_qml_interaction_smoke_tests PRIVATE
            TEXTFX_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
            TEXTFX_TESTING=1
        )
        textfx_add_test_icon_resources(textfx_qml_interaction_smoke_tests)

        add_test(NAME textfx_qml_interaction_smoke_tests COMMAND textfx_qml_interaction_smoke_tests)
        set_tests_properties(textfx_qml_interaction_smoke_tests PROPERTIES
            LABELS "qml;smoke"
            TIMEOUT 90
            ENVIRONMENT "${TEXTFX_QT_TEST_ENVIRONMENT}"
        )
        list(APPEND TEXTFX_TEST_TARGETS textfx_qml_interaction_smoke_tests)

        qt_add_executable(textfx_qml_text_box_delegate_smoke_tests
            tests/qml_text_box_delegate_smoke_tests.cpp
            tests/qt_test_helpers.cpp
            tests/qt_test_helpers.h
            src/app/qt/OutlinedTextItem.cpp
            src/app/qt/OutlinedTextItem.h
        )

        target_link_libraries(textfx_qml_text_box_delegate_smoke_tests
            PRIVATE
                textfx_app_core
                Qt6::Qml
                Qt6::Quick
                Qt6::QuickControls2
                Qt6::QuickDialogs2
                Qt6::Test
        )
        target_compile_definitions(textfx_qml_text_box_delegate_smoke_tests PRIVATE
            TEXTFX_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
            TEXTFX_TESTING=1
        )
        textfx_add_test_icon_resources(textfx_qml_text_box_delegate_smoke_tests)

        add_test(NAME textfx_qml_text_box_delegate_smoke_tests COMMAND textfx_qml_text_box_delegate_smoke_tests)
        set_tests_properties(textfx_qml_text_box_delegate_smoke_tests PROPERTIES
            LABELS "qml;smoke"
            TIMEOUT 90
            ENVIRONMENT "${TEXTFX_QT_TEST_ENVIRONMENT}"
        )
        list(APPEND TEXTFX_TEST_TARGETS textfx_qml_text_box_delegate_smoke_tests)

        qt_add_executable(textfx_outlined_text_item_tests
            tests/outlined_text_item_tests.cpp
            tests/qt_test_helpers.cpp
            tests/qt_test_helpers.h
            src/app/qt/OutlinedTextItem.cpp
            src/app/qt/OutlinedTextItem.h
        )

        target_link_libraries(textfx_outlined_text_item_tests
            PRIVATE
                textfx_app_core
                Qt6::Qml
                Qt6::Quick
                Qt6::Test
        )
        target_compile_definitions(textfx_outlined_text_item_tests PRIVATE
            TEXTFX_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
            TEXTFX_TESTING=1
        )

        add_test(NAME textfx_outlined_text_item_tests COMMAND textfx_outlined_text_item_tests)
        set_tests_properties(textfx_outlined_text_item_tests PROPERTIES
            LABELS "ui;render"
            TIMEOUT 60
            ENVIRONMENT "${TEXTFX_QT_TEST_ENVIRONMENT}"
        )
        list(APPEND TEXTFX_TEST_TARGETS textfx_outlined_text_item_tests)
    else()
        message(STATUS "Qt6 Test not found; Qt/QML smoke tests are not built. Install the Qt6 Test component locally to include them in the check target.")
    endif()

    add_executable(textfx_font_diagnostic
        tools/font_diagnostic.cpp
    )

    target_link_libraries(textfx_font_diagnostic
        PRIVATE
            textfx_app_core
            Qt6::Gui
    )

    add_test(
        NAME textfx_architecture_layer_check
        COMMAND ${CMAKE_COMMAND}
                -DTEXTFX_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/architecture_layer_check.cmake
    )
    set_tests_properties(textfx_architecture_layer_check PROPERTIES
        LABELS architecture
        TIMEOUT 10
    )

    add_custom_target(verify_all
        DEPENDS ${TEXTFX_TEST_TARGETS}
    )

    add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --no-tests=error
        DEPENDS verify_all
        USES_TERMINAL
    )
endif()
