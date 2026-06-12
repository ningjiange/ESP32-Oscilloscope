param(
    [Parameter(Mandatory = $true)]
    [string] $Port,
    [string] $IdfPath = "D:\ESP-IDF\.espressif\v5.4.4\esp-idf",
    [string] $IdfToolsPath = "D:\DevTools\.espressif",
    [string] $IdfPython = "D:\DevTools\.espressif\python_env\idf5.4_py3.13_env\Scripts\python.exe"
)

$ErrorActionPreference = "Stop"

$env:IDF_TOOLS_PATH = $IdfToolsPath
. (Join-Path $IdfPath "export.ps1")

& $IdfPython -m esptool `
    --chip esp32 `
    --port $Port `
    --baud 460800 `
    --before default_reset `
    --after hard_reset `
    write_flash `
    --flash_mode dio `
    --flash_size 2MB `
    --flash_freq 40m `
    0x1000 build\bootloader\bootloader.bin `
    0x8000 build\partition_table\partition-table.bin `
    0x10000 build\esp32_oscilloscope.bin
