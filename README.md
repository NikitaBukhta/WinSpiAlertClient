# WinSpiAlertClient

Windows client built with MSVC and CMake.

## Prerequisites

| Tool | Notes |
|------|-------|
| [Visual Studio 2022 or newer](https://visualstudio.microsoft.com/downloads/) | Workload **Desktop development with C++** (MSVC compiler + Windows SDK) |
| [Python 3.10+](https://www.python.org/downloads/windows/) | Used by `scripts/project.py` |
| [Git](https://git-scm.com/downloads/win) | CMake downloads dependencies (rapidjson) via Git |

CMake, clang-format, cmake-format and ruff do **not** need to be installed: the script puts them into a local `.venv` on first run.

## Build

```bat
py scripts\project.py build            :: Debug build
py scripts\project.py build Release    :: Release build
```

Output: `build\Debug\client.exe` / `build\Release\client.exe`.

## Code formatting

```bat
py scripts\project.py format    :: format C++, CMake and Python files
py scripts\project.py check     :: check only, exit code 1 if something is not formatted
```

Style configs: `.clang-format` (Microsoft style), `.cmake-format.yaml`, `ruff.toml`, `.editorconfig`.

All commands: `py scripts\project.py help`.
