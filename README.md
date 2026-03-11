# UltraMarkdown

UltraMarkdown is a Windows-only Markdown editor/viewer prototype built with Win32, Scintilla, and cmark-gfm. It starts in raw editable Markdown mode, toggles to a native preview with `F6`, and keeps the implementation small and reviewable.

## Goals

- Native Win32 desktop app
- Fast cold start and low steady-state overhead
- Raw Markdown first, preview on demand
- No browser UI, no WebView, no Electron/Tauri/.NET/Qt
- Minimal local dependencies

## Stack

- UI shell: Win32 API
- Raw editor: Scintilla 5.5.2 (vendored in [`third_party/scintilla`](/C:/dev/App/third_party/scintilla))
- Markdown parser: cmark-gfm 0.29.0.gfm.13 (vendored in [`third_party/cmark-gfm`](/C:/dev/App/third_party/cmark-gfm))
- Build: CMake + Visual Studio 2022

## Architecture

- [`src/AppWindow.cpp`](/C:/dev/App/src/AppWindow.cpp): main window, menus, accelerators, command routing, dirty prompts, title updates
- [`src/Document.cpp`](/C:/dev/App/src/Document.cpp): shared document state and UTF-8 file load/save
- [`src/EditorHost.cpp`](/C:/dev/App/src/EditorHost.cpp): Scintilla setup and raw editing host
- [`src/MarkdownRenderer.cpp`](/C:/dev/App/src/MarkdownRenderer.cpp): cmark-gfm AST to lightweight preview block/span model
- [`src/PreviewView.cpp`](/C:/dev/App/src/PreviewView.cpp): native preview layout, scrolling, and GDI drawing
- [`src/Utf8.cpp`](/C:/dev/App/src/Utf8.cpp): UTF-8 / UTF-16 helpers

The preview path does not reread files from disk. When switching from Raw to Preview, the app renders from the current in-memory editor contents.

## Build

### Requirements

- Windows 10 or 11
- Visual Studio 2022 with C++ build tools
- CMake 3.24+

### Configure

From a Developer PowerShell / Developer Command Prompt for VS 2022:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

### Build

```powershell
cmake --build build --config Release --target ultra_markdown
```

### Run

Without a file:

```powershell
.\build\Release\UltraMarkdown.exe
```

With a Markdown file:

```powershell
.\build\Release\UltraMarkdown.exe C:\path\to\notes.md
```

## v1 Behavior

- Startup mode is always Raw mode
- `View > Toggle Preview` or `F6` switches modes
- Raw mode is editable and UTF-8 based
- Preview is generated only when switching into Preview mode
- File operations: New, Open, Save, Save As
- `File > Set as Default for Markdown Files...` registers UltraMarkdown as a Markdown handler for the current user and opens Windows Default Apps settings
- Dirty state is tracked through Scintilla save points
- Closing, opening, or creating a new document prompts to save changes first
- Window title shows app name, file name, and `*` when dirty

## Smoke Checklist

Validated locally on Windows in this workspace after building `Release`:

- [x] Launch app without a file
- [x] Launch app with a `.md` command line argument
- [x] Type Markdown in Raw mode
- [x] Toggle to Preview with `F6`
- [x] Toggle back to Raw with `F6`
- [x] Open a file
- [x] Save a file
- [x] Dirty prompt blocks closing until the user chooses

## Tradeoffs

- Preview is native GDI rendering, not HTML/CSS. That keeps startup and dependency weight low, but rendering is intentionally modest.
- The preview currently focuses on a practical subset: headings, paragraphs, emphasis, strong text, lists, block quotes, code blocks, horizontal rules, and link-styled text.
- No live preview in v1. Rendering happens only on mode switch.
- No syntax highlighting lexer is enabled in Scintilla to keep the dependency surface and startup path simpler.

## Current Limitations

- Preview does not implement every Markdown feature or every edge case in CommonMark/GFM.
- Links are styled like links but are not clickable in v1.
- Save/Open dialogs use the classic Win32 common dialogs and do not support long-path edge cases.
- On Windows 10/11, the app can register itself for `.md` / `.markdown`, but Windows still requires the user to confirm the default app choice in Settings.
- No tabs, split view, settings UI, plugins, themes, or background services.
