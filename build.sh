#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

CC="${ARM_GCC_PREFIX:-arm-none-eabi-}gcc"
OBJCOPY="${ARM_GCC_PREFIX:-arm-none-eabi-}objcopy"
SIZE="${ARM_GCC_PREFIX:-arm-none-eabi-}size"
COMMON=(-mcpu=arm7tdmi -marm -Os -std=c99 -Wall -Wextra -ffreestanding
        -fno-builtin -fdata-sections -ffunction-sections -flto -nostdlib
        -nostartfiles)

"$CC" "${COMMON[@]}" -DVOID_STAGE=0 -Wl,-T,stage0.ld -Wl,--gc-sections \
  -Wl,-Map,void_recovery.map -o void_recovery.elf startup.S void_bootloader_usb.c
"$OBJCOPY" -O binary void_recovery.elf void_recovery.bin
"$SIZE" void_recovery.elf

"$CC" "${COMMON[@]}" -DVOID_STAGE=1 -Wl,-T,stage1.ld -Wl,--gc-sections \
  -Wl,-Map,void_bootloader.map -o void_bootloader.elf startup.S void_bootloader_usb.c
"$OBJCOPY" -O binary void_bootloader.elf void_bootloader.body.bin
"$SIZE" void_bootloader.elf
python3 voidtool.py pack --target boot --input void_bootloader.body.bin \
  --output void_bootloader.vbi --entry 0x00101080 --version 1

"$CC" "${COMMON[@]}" -Wl,-T,examples/menu/app.ld -Wl,--gc-sections \
  -Wl,-Map,examples/menu/menu.map -o examples/menu/menu.elf \
  examples/menu/startup.S examples/menu/main.c
"$OBJCOPY" -O binary examples/menu/menu.elf examples/menu/menu.bin
"$SIZE" examples/menu/menu.elf
python3 voidtool.py pack --target app --input examples/menu/menu.bin \
  --output examples/menu/menu.vbi --entry 0x00103080 --version 1

"$CC" "${COMMON[@]}" -Wl,-T,examples/windows-login/app.ld -Wl,--gc-sections \
  -Wl,-Map,examples/windows-login/windows-login.map \
  -o examples/windows-login/windows-login.elf \
  examples/windows-login/startup.S examples/windows-login/main.c \
  examples/windows-login/lcd.c examples/windows-login/sha256.c \
  examples/windows-login/smartcard.c examples/windows-login/usb_hid.c \
  examples/windows-login/runtime.S
"$OBJCOPY" -O binary examples/windows-login/windows-login.elf \
  examples/windows-login/windows-login.bin
"$SIZE" examples/windows-login/windows-login.elf
python3 voidtool.py pack --target app \
  --input examples/windows-login/windows-login.bin \
  --output examples/windows-login/windows-login.vbi \
  --entry 0x00103080 --version 1

