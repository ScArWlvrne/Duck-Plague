# Duck-Plague

Duck-Plague is a **safe, educational ransomware simulation** built with **C++17 and Qt 6 Widgets**.  
It demonstrates how a trojan-style program can disguise malicious behavior, how ransomware workflows affect files, and why good security practices (like backups) are critical.

This project is intended for **learning and demonstration purposes** and should be run in a **controlled environment**, ideally inside a **virtual machine**.

That being said, it has been extensively tested on personal machines and not caused any lasting damage **yet**.

---

# Features

- Trojan-style fake calculator interface
- Simulated ransomware workflow
- File discovery within a limited directory
- Demo file copying and reversible transformation
- Educational lesson explaining ransomware behavior
- Restore process that removes demo artifacts
- Error handling and recovery paths

---

# Safety Model

Duck-Plague is designed to **avoid modifying original files**.

Safety measures include:

- Original files are **never overwritten**
- Only **demo copies** are modified
- File scanning is restricted to the **Downloads folder**
- Processing is capped by a **size limit**
- Demo files receive a **unique suffix**
- Restore logic removes generated files after the lesson

The transformation used is **simple XOR** and is **not secure encryption**.  
It exists purely to simulate the experience safely.

Even with these protections, you should **only run this in a disposable environment**.

---

# Tech Stack

- **C++17**
- **Qt 6 Widgets**
- **CMake 3.21+**

---

# Project Structure

```
.
├── CMakeLists.txt
├── Architecture.md
├── controller.cpp
├── trojan.cpp
├── encrypt.cpp
├── educate.cpp
├── restore.cpp
├── error.cpp
├── mode_messages.h
└── LICENSE
```

### File Overview

| File | Purpose |
|-----|--------|
| controller.cpp | Main Qt application and UI controller |
| trojan.cpp | Fake calculator interface and trigger logic |
| encrypt.cpp | File scanning and demo transformation |
| educate.cpp | Educational walkthrough and quiz |
| restore.cpp | Restores demo files and cleans artifacts |
| error.cpp | Error logging and recovery behavior |
| mode_messages.h | Shared message types and state structures |
| Architecture.md | Architecture documentation |

---

# Architecture

Duck-Plague uses a **mode-based architecture**.

The application transitions between different modes:

1. **Trojan Mode**  
   Displays a fake calculator UI.

2. **Encrypt Mode**  
   Simulates ransomware behavior by creating demo file copies and transforming them.

3. **Educate Mode**  
   Explains how trojans and ransomware work and teaches safer response habits.

4. **Restore Mode**  
   Reverses the demo transformation and deletes generated copies.

5. **Error Mode**  
   Provides recovery instructions if a failure occurs.

### Design Principles

- Only `controller.cpp` directly uses Qt widgets
- Mode modules use **plain C++ structures**
- Modes return UI instructions instead of manipulating the UI themselves
- Shared application state is stored in `Context` and `AppState`

This keeps the logic **modular and easier to maintain**.

---

# Build Requirements

## All Platforms

- CMake **3.21+**
- Qt **6.x** with Widgets module
- C++ compiler compatible with Qt

## Windows

- Windows 10 or 11
- Qt Windows kit
- MSVC or other Qt-supported compiler

## macOS

- Qt macOS kit
- Xcode command line tools

The project primarily targets **Windows-style behavior**, but the UI can compile on macOS for development and the program does support auto detection of OS in order to find the Downloads directory.

---

# Building

## Using Qt Creator

1. Open **Qt Creator**
2. Select **Open Project**
3. Choose `CMakeLists.txt`
4. Select a Qt kit
5. Configure the project
6. Build and run

## Command Line

```
cmake -S . -B build
cmake --build build
```

Run the executable:

```
./build/DuckPlague
```

On Windows:

```
build\DuckPlague.exe
```

---

# Demo Workflow

A typical run looks like this:

1. Application launches
2. Fake calculator UI appears
3. Hidden trigger (typing/calculating **67** or waiting **67 seconds**) activates encryption demo
4. Downloads folder is scanned
5. Demo copies of selected files are created
6. Copies are XOR-transformed
7. Educational explanation appears
8. Restore process reverses the transformation and deletes demo files

At any time, the debug screen can be accessed by holding down the `D`, `E`, and `V` keys, allowing manual selection of any module for testing.

---

# Configuration Behavior

The controller initializes runtime settings such as:

- Downloads directory
- Demo file suffix
- Maximum file size
- Encryption key for demo transform
- Log file location

A log file is written near the executable to assist with recovery or debugging.

---

# Educational Goals

Duck-Plague is designed to teach:

- How trojans disguise malicious behavior
- How ransomware spreads and affects files
- Why **offline backups** are important
- Safer first-response steps after a suspected attack
- How user interfaces can manipulate user trust

The project focuses on **defensive understanding**, not offensive malware development.

---

# Current Limitations

- File operations are intentionally simplified
- XOR transform is not secure encryption
- Worker modes run inline rather than in background threads
- The project is a **learning tool**, not a production simulator

---

# Recommended Usage

For safest testing:

- Run inside a **virtual machine**
- Keep the **Downloads folder non-critical**
- Avoid storing important files in the test environment
- Review the log file if unexpected behavior occurs

---

# Disclaimer

Duck-Plague is an **educational simulation** intended to demonstrate ransomware concepts safely.

It should only be used in **controlled, ethical, and legal environments**.
