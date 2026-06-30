# TextFX

TextFX is the C++/Qt successor MVP for TypeX. The app opens image folders, shows the active page image, navigates pages, autosaves `.textfx` project data, and uses the same Qt-rendered PNG path for preview and export.

## Dependencies

- CMake 3.25+
- Ninja
- C++23 compiler
- Qt 6.8+ (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `QuickDialogs2`; `Test` for smoke tests)
- Optional: Catch2 3 for core/text/render test sources

## Build

For daily development from this folder:

```bash
./run.sh        # configure, build, and launch
./run.sh test   # configure, build verification targets, and run ctest
```

```bash
cmake --preset debug
cmake --build --preset debug
```

For release packaging:

```bash
cmake --preset release
cmake --build --preset release
cmake --build build/release --target package
```

CPack currently produces a minimal `.tar.gz`. Qt deployment uses `qt_generate_deploy_qml_app_script()` when the installed Qt CMake package provides it. Platform-specific installers are intentionally not configured until target platforms are confirmed.

Qt runtime deployment is intentionally off by default because target platforms are not confirmed:

```bash
cmake --preset release -DTEXTFX_ENABLE_QT_DEPLOY=ON
```

## Verification

```bash
cmake --preset test
cmake --build build/test --target check
```

Expected local limits:

- If Catch2 3 is missing, core tests are not registered.
- If Qt Test is missing, Qt smoke tests are not registered.
- Render goldens verify the deterministic render seam plus a runtime all-effects PNG export.

## MVP Parity Notes

Implemented seams cover real folder opening, project/page discovery, page image display, previous/next/direct page navigation with dirty JSON autosave, Save/Save All for the active project document, TextFX JSON, document commands, copy/paste, canvas actions, QML shell wiring, text layout fallback, deterministic preview/export render comparison, and a shared preview/export PNG path for the current page image plus text boxes with fill/outline/shadow/shadow blur/blur/gradient handling.

Not complete yet: full text shaping/bidi behavior, production perspective warp, exact TypeX smooth path geometry, and target-platform installer policy. Perspective is currently an affine approximation and path text follows the configured handles as a polyline approximation; both are reported as warnings.
