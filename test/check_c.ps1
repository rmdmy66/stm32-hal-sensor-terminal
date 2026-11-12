$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$gcc = $null

$candidates = @(
    "D:\stm32cubeide\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344\tools\bin\arm-none-eabi-gcc.exe",
    "D:\STM32CubeIDE\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344\tools\bin\arm-none-eabi-gcc.exe"
)

foreach ($path in $candidates) {
    if (Test-Path $path) {
        $gcc = $path
        break
    }
}

if (-not $gcc) {
    $found = Get-ChildItem "D:\stm32cubeide", "D:\STM32CubeIDE" -Recurse -Filter "arm-none-eabi-gcc.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $gcc = $found.FullName
    }
}

if (-not $gcc) {
    Write-Host "arm-none-eabi-gcc not found. Open STM32CubeIDE once, or edit this script with your toolchain path."
    exit 1
}

Write-Host "Using compiler: $gcc"
& $gcc -std=c99 -Wall -Wextra -Werror -fsyntax-only `
    -I "$root\firmware\protocol" `
    "$root\test\test_frame.c" `
    "$root\firmware\protocol\frame.c"

if ($LASTEXITCODE -ne 0) {
    Write-Host "C syntax check failed."
    exit $LASTEXITCODE
}

Write-Host "C syntax check passed."
Write-Host "Running Python self-test..."
python "$root\host\frame_tool.py" --self-test
