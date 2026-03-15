# Duck-Plague
A harmless program which simulates the behavior of ransomware to educate users on computer safety.

---

## Build Instructions

Duck-Plague uses **CMake** and **Qt 6 (Widgets)**. The project is intended to run as a **Windows application** (typically inside a VM), but the controller UI can also be built on macOS for development.

### Prerequisites

#### All Platforms
- **CMake** 3.21 or newer
- **Qt 6** with the **Widgets** component

#### Windows
- Windows 10/11
- Qt 6 (Windows kit)
- A C++ compiler supported by Qt (e.g., MSVC)

#### macOS
- macOS (Apple Silicon or Intel)
- Qt 6 (macOS kit)
- Xcode Command Line Tools

> ⚠️ **Safety note:** File-manipulation logic (e.g., Encrypt/Restore) shouldn't cause issues with user data, but a Virutal Machine can be used to ensure 100% safety.

---

## Building with Qt Creator (Recommended)

1. Open **Qt Creator**
2. Click **Open Project**
3. Select `CMakeLists.txt` in the project root
4. Choose an appropriate Qt kit for your platform
5. Click **Configure Project**
6. Press **Run** (green ▶ button)

Qt Creator will generate build files automatically in a local `build/` directory (ignored by git).

---

## Building from the Command Line

From the project root:

```bash
cmake -S . -B build
cmake --build build
```

To run:

```bash
./build/DuckPlague
```

(On Windows, the executable will be `DuckPlague.exe`.)

---

## Notes

- The controller/UI owns all Qt logic.
- Individual modes communicate via plain C++ interfaces and must not depend on Qt.
- Interactive modes (e.g., Trojan, Educate) are step-driven; worker modes run to completion.
