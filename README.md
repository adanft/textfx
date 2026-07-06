# TextFX

TextFX is a desktop editor for placing styled text on top of image pages. It is designed for workflows like manga/comic typesetting, image translation cleanup, captions, posters, thumbnails, and any case where text needs to be positioned, styled, previewed, saved, and exported page by page.

The goal is simple: open a folder of images, add text boxes, style them visually, move between pages, and export the final result without losing the live preview/export match.

## What you can do with TextFX

- Open an image folder as a project.
- Navigate pages with previous/next controls.
- Add, select, move, resize, rotate, duplicate, delete, copy, and paste text boxes.
- Edit text directly on the canvas.
- Autosave project data in a `.textfx` folder.
- Export rendered pages to PNG.
- Keep live preview and export rendering aligned.

## Text styling features

TextFX supports common visual text effects used in image editing and typesetting:

- Font family and font size.
- Text color.
- Bold and italic.
- Uppercase and lowercase transforms.
- Left, center, and right alignment.
- Line spacing and letter spacing.
- Multiple outline layers, similar to stacked strokes in image editors.
- Blur.
- Shadow with color, offset, and blur size.
- Gradient fill.
- Path text with editable path handles.
- Perspective/box distortion handles.
- Text presets for reusing styling choices.

## Why TextFX exists

TextFX is built as a modern Qt/C++ desktop editor focused on fast page navigation, direct canvas editing, reusable text styles, and deterministic preview/export output.

It is built for people who need to work visually, not just edit metadata. The canvas is the center of the app: text boxes are manipulated where they appear, and effects update in real time.

## Project data

When a folder is opened, TextFX stores project metadata under a `.textfx` directory inside that project folder. This keeps page text, boxes, presets, and saved state next to the images they belong to.

The original images are not overwritten during normal editing. Exported output is written separately.

## Current status

TextFX is an active MVP. The core workflow is already present: folder opening, page navigation, canvas text editing, effects, presets, autosave, and PNG export.

Some advanced rendering details are still evolving, especially around perfect parity for complex path/perspective cases and final packaging/installers for each target platform.

## Build from source

### Requirements

- CMake 3.25+
- Ninja
- A C++23 compiler
- Qt 6.8+ with:
  - Core
  - Gui
  - Qml
  - Quick
  - QuickControls2
  - QuickDialogs2
- Required for `./run.sh test`: Qt Test and Catch2 3

### Daily development

From the repository root:

```bash
./run.sh
```

Run the full local test gate:

```bash
./run.sh test
```

This configures CMake with `TEXTFX_REQUIRE_TEST_DEPS=ON`, so missing Qt Test or Catch2 3 fails configuration instead of silently skipping test suites.

Or use CMake presets directly:

```bash
cmake --preset debug
cmake --build --preset debug
```

### Verification

```bash
cmake --preset test
cmake --build build/test --target check
```

Expected notes:

- Use `-DTEXTFX_REQUIRE_TEST_DEPS=ON` for verification builds that must fail when Qt Test or Catch2 3 is unavailable.
- Without `TEXTFX_REQUIRE_TEST_DEPS`, CMake keeps the optional local-development behavior and reports missing test-only dependencies at configure time.
- The `check` target uses `ctest --no-tests=error` so an empty test registry is a failure.
- Render checks cover deterministic render seams and all-effects PNG export behavior.

### Release package

```bash
cmake --preset release
cmake --build --preset release
cmake --build build/release --target package
```

CPack currently produces a minimal `.tar.gz` package. Qt runtime deployment is opt-in while target platforms are finalized:

```bash
cmake --preset release -DTEXTFX_ENABLE_QT_DEPLOY=ON
```

## License

TextFX is released under the MIT License. See [LICENSE](LICENSE).
