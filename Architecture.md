# Duck Plague Architecture

## Big idea
Duck Plague is a **mode-driven** educational ransomware simulation.
- **Controller** owns the UI and state transitions.
- **Modes** contain logic and produce **plain C++ outputs** describing what the controller should display/do.

## File map
- `controller.cpp` — Qt Widgets UI + mode dispatcher (ONLY Qt file)
- `trojan.cpp` — interactive fake app (calculator), step-driven
- `educate.cpp` — interactive safety course, step-driven (pages + quizzes)
- `encrypt.cpp` — worker mode: select targets, copy, demo-transform copies, hide originals
- `restore.cpp` — worker mode: undo demo effects, unhide originals, delete copies
- `error.cpp` — error reporting content + failsafe logging

## Core rules
1. **Only controller uses Qt.** No Qt headers in mode modules.
2. Modes never transition directly; they **request** transitions via return values.
3. `restore` must be **idempotent**: safe to run multiple times and after partial failure.
4. Safety invariants:
   - never delete/overwrite originals
   - only operate in allowlisted directory (Downloads)
   - size-bounded (e.g., 256–512MB)
   - demo copies identifiable by suffix

## Shared data structures
All modules share a single header (e.g., `mode_messages.h`) containing:

### `Mode`
Enum of modes: Controller/Home, Trojan, Encrypt, Educate, Restore, Error, Exit.

### `Context`
Shared configuration + state (no UI):
- downloads path
- demo suffix
- max bytes limit
- log path
- (optional) manifest path

### Worker mode return: `ModeResult`
Used by run-to-completion modules:
- `bool success`
- `Mode nextMode`
- `std::string userMessage`
- (optional) debug/details fields

### Interactive mode return: `UiRequest`
Used by step-driven modules:
- Message pages (title/body/button)
- Quiz pages (question/choices/correct/feedback)
- Navigate request (next mode)

### User input: `UserInput`
Sent by controller into interactive modes:
- Next/primary button click
- choice selection index
- (optional) text entry later

## Mode categories
### Worker modes (run-to-completion)
`encrypt_run(ctx)` and `restore_run(ctx)` do work and return `ModeResult`.
Later: move them to a background thread and report progress.

### Interactive modes (step-driven)
`trojan_start/handle_input` and `educate_start/handle_input` produce `UiRequest` and consume `UserInput`.

## Startup behavior
If demo artifacts exist on startup (e.g., demo suffix files), controller should enter `Restore` automatically to protect file integrity.

## Extending the project
- Add a new UI screen type: extend `UiRequest` + add a render function in controller.
- Add lesson content: add steps in `educate.cpp`.
- Add trojan features: expand calculator input handling in `trojan.cpp`.
- Add robustness: implement a manifest file in `encrypt.cpp` and use it in `restore.cpp`.
