param(
    [string] $IdfPath = "D:\ESP-IDF\.espressif\v5.4.4\esp-idf",
    [string] $IdfToolsPath = "D:\DevTools\.espressif",
    [string] $IdfPython = "D:\DevTools\.espressif\python_env\idf5.4_py3.13_env\Scripts\python.exe",
    [string] $CMake = "D:\DevTools\.espressif\tools\cmake\3.30.2\bin\cmake.exe",
    [string] $Ninja = "D:\DevTools\.espressif\tools\ninja\1.12.1\ninja.exe"
)

$ErrorActionPreference = "Stop"

$env:IDF_TOOLS_PATH = $IdfToolsPath
. (Join-Path $IdfPath "export.ps1")

& $CMake `
    -G Ninja `
    -S . `
    -B build `
    -DPYTHON_DEPS_CHECKED=1 `
    -DPYTHON="$IdfPython" `
    -DESP_PLATFORM=1 `
    -DCCACHE_ENABLE=0

& $Ninja -C build
