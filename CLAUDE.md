# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# For RP2040 (Raspberry Pi Pico)
mkdir build_rp2040 && cd build_rp2040
cmake -DTARGET_BOARD=pico ..
make -j4
# Output: ublox_fake_rp2040.uf2

# For RP2350 (Raspberry Pi Pico 2)
mkdir build_rp2350 && cd build_rp2350
cmake -DTARGET_BOARD=pico2 ..
make -j4
# Output: ublox_fake_rp2350.uf2
```

Requirements: Pico SDK 2.x, CMake 3.13+, ARM GCC toolchain.

## Architecture Overview

This is a u-blox GNSS M8/M10 receiver emulator for RP2040/RP2350 with dual-core architecture:

**Core0 (FreeRTOS):**
- UART IRQ handler for UBX command processing
- FreeRTOS timers: 5Hz (NAV messages), 1Hz (MON messages), 3s/4s (SEC-SIGN)
- Message transmission via `uart_write_blocking`

**Core1 (Bare-metal loop):**
- WS2812 LED control via PIO
- GPIO button handler for mode switching
- SEC-SIGN computation (SHA256 + ECDSA) - heavy crypto offloaded here

**Inter-core communication for SEC-SIGN:**
```
Core0 timer → sec_sign_request=true → Core1 computes → sec_sign_ready=true → Core0 sends
```

## Key Source Files

- `src/main_rp2040.c` / `src/main_rp2350.c` - Platform-specific entry points (nearly identical)
- `src/massivs.h` - All UBX message byte arrays with iTOW at bytes [6-9]
- `src/FreeRTOSConfig.h` - FreeRTOS config (128KB heap RP2040, 350KB RP2350)

## UBX Protocol Implementation

Messages enabled/disabled via `volatile bool UBX_*_fl` flags. CRC recalculated with `CRC_gen()` after iTOW updates.

CFG commands handled in UART IRQ (`on_uart_rx0`):
- `CFG-PRT` (0x06,0x00): baudrate change
- `CFG-MSG` (0x06,0x01): enable/disable message types
- `CFG-VALSET` (0x06,0x8A): M10 configuration

## Hardware Pins

| Pin | Function |
|-----|----------|
| GP0 | UART TX |
| GP1 | UART RX |
| GP3 | PIO passthrough input |
| GP5/GP6 | Mode button (power/input) |
| GP16 | WS2812 LED |

## Two Operating Modes

1. **Emulation** (green LED): Generates UBX stream with SEC-SIGN signatures
2. **Passthrough** (blue LED): PIO-based transparent UART bridge

Mode persists in flash (last sector). Button toggles mode.

## SEC-SIGN Cryptography

- ECDSA SECP192R1 via micro-ecc library
- Private key in `sec_sign_private_key[24]`
- All transmitted UBX messages accumulated in SHA256 context
- Signature computed on Core1 every 4 seconds (first at 3s)
