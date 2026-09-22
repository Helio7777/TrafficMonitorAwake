# Repository Guidelines

## Project Structure & Module Organization

This repository builds a Windows-only C++17 DLL plugin for TrafficMonitor. Production code lives in `src/`: `AwakeManager.*` owns state, INI persistence, and Windows power requests; `AwakeOptionsDialog.*` implements the Win32 settings UI; `AwakePlugin.*` connects those components to the host; and `PluginInterface.h` defines the TrafficMonitor API 8 boundary. `CMakeLists.txt` defines the shared-library target. Treat `build-x64/`, other `build-*` directories, and `dist/` as generated output. User documentation belongs in `README.md`; implementation comparisons belong in `REFERENCE.md`.

## Build, Test, and Development Commands

Run commands from a Visual Studio Developer PowerShell with CMake 3.20+ and the Desktop development with C++ workload installed.

- `./build.ps1 -Arch x64 -Config Release` configures, builds, and copies the DLL to `dist/x64/`.
- `./build.ps1 -Arch Win32 -Config Debug` builds a 32-bit debug variant.
- `./build-all.ps1` produces release DLLs for x64 and Win32.
- `./verify-build.ps1 -Arch x64` confirms the required `TMPluginGetInstance` export using `dumpbin`.
- `powercfg /requests` checks the plugin's power request after installation in TrafficMonitor.

CI repeats release builds for both architectures through `.github/workflows/build.yml`.

## Coding Style & Naming Conventions

Match the existing MSVC-oriented style: four-space indentation, braces on their own lines, and no tabs. Use `PascalCase` for types and functions, `camelCase_` for private members, and `kPascalCase` for namespace constants. Prefer standard-library RAII and scoped locking around shared state. Keep UI strings wide (`L"..."`) and call Unicode Win32 APIs explicitly (`CreateWindowExW`). Builds enforce `/W4`, `/permissive-`, `/utf-8`, and `/EHsc`; new code should compile warning-free.

## Testing Guidelines

There is no automated unit-test target yet. Every change must build for both x64 and Win32 and pass `verify-build.ps1`. For behavior changes, install the matching DLL, exercise off, indefinite, timed, and keep-display-on modes, restart TrafficMonitor to verify INI restoration, and inspect `powercfg /requests`. Add focused tests if introducing platform-independent logic.

## Commit & Pull Request Guidelines

Git history is not included in this checkout, so use short imperative subjects such as `Fix timed mode expiration`. Keep commits focused and avoid committing generated build artifacts unless a release process requires them. Pull requests should explain user-visible behavior, list architectures and manual checks performed, link relevant issues, and include screenshots for dialog or menu changes. Note any TrafficMonitor API or minimum-Windows-version impact.
