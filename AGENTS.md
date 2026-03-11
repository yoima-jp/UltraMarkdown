# AGENTS.md

## Project Overview

This repository contains a **Windows-only ultra-lightweight Markdown editor/viewer**.

The project is intentionally narrow in scope:

- It must start fast from a cold launch
- It must not rely on a resident background process
- It must open in **Raw Markdown mode** by default
- It must allow switching to a **rendered Preview mode** with a simple UI action
- It must remain small, native, and easy to reason about

This is **not** a general-purpose IDE, note-taking platform, or browser-based Markdown app.

---

## Core Product Goal

Build a **minimal native Markdown desktop app** for Windows that is faster and lighter than heavyweight editor stacks.

The intended UX is:

- Launch the app
- See and edit the raw `.md` text immediately
- Click a menu item or press a shortcut to switch to rendered preview
- Switch back to raw editing instantly
- Open/save files without friction

Speed, simplicity, and correctness matter more than feature count.

---

## Target Stack

Use the following stack unless explicitly changed by the repository owner:

- **Language:** C++
- **Platform:** Win32 API
- **Raw text editor:** Scintilla
- **Markdown parser:** cmark-gfm
- **Build system:** CMake
- **Primary toolchain:** Visual Studio 2022 on Windows

---

## Hard Constraints

Agents must respect these constraints:

- **Windows only**
- **No Electron**
- **No Tauri**
- **No WebView2**
- **No Chromium**
- **No .NET / WPF / WinUI / MAUI**
- **No Qt**
- **No background resident process**
- **No tray icon**
- **No auto-launch**
- **No telemetry**
- **No auto-updater**
- **No plugin system**
- **No hidden runtime-heavy dependency**

Do not replace the native stack with an easier web-based approach.

---

## v1 Scope

The v1 application should support:

- Raw Markdown editing
- Preview mode rendering
- Toggle between Raw and Preview
- New / Open / Save / Save As
- Dirty state tracking
- Prompt before losing unsaved changes
- Command-line opening of a file
- Empty untitled document when no file is provided
- UTF-8 text handling
- Basic handling of reasonably large Markdown files

### Startup behavior

- Always start in **Raw mode**
- Raw mode must be editable
- Preview is generated from the current in-memory document
- No live preview while typing in v1
- Preview should update when switching from Raw to Preview

### Toggle behavior

- Top menu item: `View > Toggle Preview`
- Keyboard shortcut: `F6`

---

## Non-Goals for v1

These are explicitly out of scope unless the repository owner asks for them:

- Split view
- Live preview while typing
- Tabs
- Workspace/session systems
- Plugin architecture
- Settings UI
- Theme system
- Browser rendering
- HTML editing
- Mermaid
- LaTeX/math rendering
- Embedded web content
- Background indexing
- Heavy recent-files database
- Advanced IDE-like features

If a proposed feature risks startup speed or codebase simplicity, cut it.

---

## Architecture Intent

Keep the codebase small and understandable.

Prefer a small number of focused components:

- **App / Window bootstrap**
- **Document model and file state**
- **Raw editor host**
- **Markdown parse/render pipeline**
- **Preview view**
- **Basic command handling**

### Important design rules

- Use one shared in-memory document model
- Do not reread the file from disk when toggling modes
- Do not overabstract
- Prefer simple, explicit code over framework-like layering
- Favor low startup overhead over “clean architecture” theater
- Avoid unnecessary heap allocations during startup when practical

---

## Preview Rendering Guidance

Preview must remain native and lightweight.

For v1, it is acceptable to support a practical subset first, such as:

- Headings
- Paragraphs
- Bold / italic
- Unordered and ordered lists
- Block quotes
- Code blocks
- Horizontal rules
- Links as visible text, even if full navigation is deferred

Do **not** introduce a browser engine just to get richer rendering.

If rendering full Markdown becomes too broad, implement a smaller but robust native subset.

---

## Coding Principles

When editing this repository, follow these principles:

1. **Do not expand scope**
2. **Do not introduce heavy dependencies**
3. **Do not optimize for flashy UI**
4. **Do optimize for startup speed and low complexity**
5. **Prefer boring, direct solutions**
6. **Keep files and modules reviewable**
7. **Avoid speculative abstraction**
8. **If a simpler version works, ship that first**

This project values completion over architectural vanity.

---

## Build and Validation Expectations

Before considering a task done:

- Build the project
- Fix compile errors
- Run a basic smoke test
- Confirm the main user flow works

Minimum smoke test checklist:

- App launches
- Raw mode is editable
- Toggle to preview works
- Toggle back to raw works
- Open file works
- Save file works
- Dirty-state prompt works

Do not stop at scaffolding if the task clearly expects a working implementation.

---

## Repository Change Policy for Agents

Agents should:

- Inspect the current repository before making major assumptions
- Make incremental changes
- Keep commits/changes logically grouped when possible
- Avoid sweeping rewrites unless necessary
- Preserve the project’s lightweight direction
- Explain tradeoffs briefly and concretely

Agents should not:

- Replace the chosen native stack for convenience
- Add broad future-proofing that is not needed yet
- Add optional systems “for later”
- Turn the app into a general-purpose editor platform

---

## Definition of Success

A successful contribution moves the project toward:

- Faster cold startup
- Smaller conceptual surface area
- Simpler native implementation
- Reliable raw editing
- Reliable preview toggle
- Clean file open/save flow

If a change makes the app more complex without clearly improving the core workflow, it is probably the wrong change.