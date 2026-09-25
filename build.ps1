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
if ($LASTEXITCODE -ne 0) { throw 'stage 0 compilation failed' }
& $objcopy -O binary void_recovery.elf void_recovery.bin
if ($LASTEXITCODE -ne 0) { throw 'stage 0 objcopy failed' }
& $size void_recovery.elf
if ($LASTEXITCODE -ne 0) { throw 'stage 0 size failed' }

& $gcc @common '-DVOID_STAGE=1' '-Wl,-T,stage1.ld' '-Wl,--gc-sections' `
    '-Wl,-Map,void_bootloader.map' -o void_bootloader.elf startup.S void_bootloader_usb.c
if ($LASTEXITCODE -ne 0) { throw 'stage 1 compilation failed' }
& $objcopy -O binary void_bootloader.elf void_bootloader.body.bin
if ($LASTEXITCODE -ne 0) { throw 'stage 1 objcopy failed' }
& $size void_bootloader.elf
if ($LASTEXITCODE -ne 0) { throw 'stage 1 size failed' }

python .\voidtool.py pack --target boot --input .\void_bootloader.body.bin `
    --output .\void_bootloader.vbi --entry 0x00101080 --version 1
if ($LASTEXITCODE -ne 0) { throw 'stage 1 packaging failed' }

& $gcc @common '-Wl,-T,examples/menu/app.ld' '-Wl,--gc-sections' `
    '-Wl,-Map,examples/menu/menu.map' -o examples/menu/menu.elf `
    examples/menu/startup.S examples/menu/main.c
if ($LASTEXITCODE -ne 0) { throw 'menu compilation failed' }
& $objcopy -O binary examples/menu/menu.elf examples/menu/menu.bin
if ($LASTEXITCODE -ne 0) { throw 'menu objcopy failed' }
& $size examples/menu/menu.elf
if ($LASTEXITCODE -ne 0) { throw 'menu size failed' }
python .\voidtool.py pack --target app --input .\examples\menu\menu.bin `
    --output .\examples\menu\menu.vbi --entry 0x00103080 --version 1
if ($LASTEXITCODE -ne 0) { throw 'menu packaging failed' }

& $gcc @common '-Wl,-T,examples/windows-login/app.ld' '-Wl,--gc-sections' `
    '-Wl,-Map,examples/windows-login/windows-login.map' `
    -o examples/windows-login/windows-login.elf `
    examples/windows-login/startup.S examples/windows-login/main.c `
    examples/windows-login/lcd.c examples/windows-login/sha256.c `
    examples/windows-login/smartcard.c examples/windows-login/usb_hid.c `
    examples/windows-login/runtime.S
if ($LASTEXITCODE -ne 0) { throw 'Windows Login firmware compilation failed' }
& $objcopy -O binary examples/windows-login/windows-login.elf `
    examples/windows-login/windows-login.bin
if ($LASTEXITCODE -ne 0) { throw 'Windows Login firmware objcopy failed' }
& $size examples/windows-login/windows-login.elf
if ($LASTEXITCODE -ne 0) { throw 'Windows Login firmware size failed' }
python .\voidtool.py pack --target app --input .\examples\windows-login\windows-login.bin `
    --output .\examples\windows-login\windows-login.vbi --entry 0x00103080 --version 1
if ($LASTEXITCODE -ne 0) { throw 'Windows Login firmware packaging failed' }
Pop-Location
