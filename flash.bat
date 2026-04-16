@echo off
echo [1/2] Building firmware...
cd firmware
pio run
if errorlevel 1 (
    echo Build failed!
    pause
    exit /b 1
)
cd ..
echo [2/2] Flashing...
"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" -c port=SWD freq=480 reset=HWrst -d firmware\.pio\build\stm32wb5mm_dk\firmware.elf -v -hardRst
