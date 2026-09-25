$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
if ($env:ARM_GCC_PREFIX) {
    $gcc = "$($env:ARM_GCC_PREFIX)gcc.exe"
    $objcopy = "$($env:ARM_GCC_PREFIX)objcopy.exe"
    $size = "$($env:ARM_GCC_PREFIX)size.exe"
} elseif (Test-Path '..\arm-toolchain\bin\arm-none-eabi-gcc.exe') {
    $gcc = '..\arm-toolchain\bin\arm-none-eabi-gcc.exe'
    $objcopy = '..\arm-toolchain\bin\arm-none-eabi-objcopy.exe'
    $size = '..\arm-toolchain\bin\arm-none-eabi-size.exe'
} else {
    $gcc = 'arm-none-eabi-gcc.exe'
    $objcopy = 'arm-none-eabi-objcopy.exe'
    $size = 'arm-none-eabi-size.exe'
}
$common = @('-mcpu=arm7tdmi', '-marm', '-Os', '-std=c99', '-Wall', '-Wextra',
            '-ffreestanding', '-fno-builtin', '-fdata-sections', '-ffunction-sections', '-flto',
            '-nostdlib', '-nostartfiles')

& $gcc @common '-DVOID_STAGE=0' '-Wl,-T,stage0.ld' '-Wl,--gc-sections' `
    '-Wl,-Map,void_recovery.map' -o void_recovery.elf startup.S void_bootloader_usb.c
& $objcopy -O binary void_recovery.elf void_recovery.bin
& $size void_recovery.elf

& $gcc @common '-DVOID_STAGE=1' '-Wl,-T,stage1.ld' '-Wl,--gc-sections' `
    '-Wl,-Map,void_bootloader.map' -o void_bootloader.elf startup.S void_bootloader_usb.c
& $objcopy -O binary void_bootloader.elf void_bootloader.body.bin
& $size void_bootloader.elf

python .\voidtool.py pack --target boot --input .\void_bootloader.body.bin `
    --output .\void_bootloader.vbi --entry 0x00101080 --version 1

& $gcc @common '-Wl,-T,examples/menu/app.ld' '-Wl,--gc-sections' `
    '-Wl,-Map,examples/menu/menu.map' -o examples/menu/menu.elf `
    examples/menu/startup.S examples/menu/main.c
& $objcopy -O binary examples/menu/menu.elf examples/menu/menu.bin
& $size examples/menu/menu.elf
python .\voidtool.py pack --target app --input .\examples\menu\menu.bin `
    --output .\examples\menu\menu.vbi --entry 0x00103080 --version 1
Pop-Location
