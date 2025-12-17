# u-blox GNSS Emulator

Unified firmware for RP2040 (Pico) and RP2350 (Pico 2) that emulates u-blox GNSS receiver.

## Features

- **UBX Protocol Support:**
  - UBX-NAV-PVT (Position, Velocity, Time) - 10Hz configurable
  - UBX-NAV-SVINFO (Satellite Information) - 1Hz configurable
  - UBX-NAV-POSLLH (Position LLH)
  - UBX-NAV-POSECEF (Position ECEF)
  - UBX-MON-HW (Hardware Status)
  - UBX-MON-VER (Version - emulates M8 or M10)
  - UBX-TIM-TP (Timepulse)

- **Dynamic Configuration:**
  - Baudrate switching (9600, 115200, 460800)
  - Message rate configuration
  - Enable/disable individual messages

- **Two Operation Modes:**
  - **Emulation Mode** (Green LED): Generates fake GNSS data
  - **Passthrough Mode** (Blue LED): Transparent UART bridge via PIO

## Hardware

### Pin Configuration

| Pin | Function |
|-----|----------|
| GP0 | UART TX |
| GP1 | UART RX |
| GP3 | PIO Passthrough Input |
| GP5 | Mode Button Power |
| GP6 | Mode Button Input |
| GP16 | WS2812 LED |

### Architecture Differences

| Feature | RP2040 | RP2350 |
|---------|--------|--------|
| Cores used | Single-core | Dual-core |
| LED control | FreeRTOS task | Core1 alarm pool |
| GPIO IRQ | Core0 | Core1 |
| CRC calculation | Core0 | Core1 |

## Building

### Prerequisites

- Pico SDK 2.x installed
- CMake 3.13+
- ARM GCC toolchain

### Build Commands

```bash
# Create build directory
mkdir build && cd build

# For RP2040 (Raspberry Pi Pico)
cmake -DTARGET_BOARD=pico ..
make -j4

# For RP2350 (Raspberry Pi Pico 2)
cmake -DTARGET_BOARD=pico2 ..
make -j4
```

### Output Files

- `ublox_fake_rp2040.uf2` - For Raspberry Pi Pico
- `ublox_fake_rp2350.uf2` - For Raspberry Pi Pico 2

## Usage

1. Flash the `.uf2` file to your Pico/Pico2
2. Connect UART to your host device
3. Green LED = Emulation mode (default)
4. Press mode button to toggle Passthrough mode (Blue LED)

### Supported UBX Commands

| Command | Class | ID | Description |
|---------|-------|-----|-------------|
| MON-VER | 0x0A | 0x04 | Poll version |
| CFG-PRT | 0x06 | 0x00 | Configure port/baudrate |
| CFG-RATE | 0x06 | 0x08 | Set navigation rate |
| CFG-MSG | 0x06 | 0x01 | Enable/disable messages |
| CFG-NAV5 | 0x06 | 0x24 | Navigation settings |
| CFG-NAVX5 | 0x06 | 0x23 | Extended navigation |
| CFG-GNSS | 0x06 | 0x3E | GNSS configuration |
| CFG-PMS | 0x06 | 0x86 | Power management |
| CFG-CFG | 0x06 | 0x09 | Save/Load/Clear config |

## LED Status

| Color | State | Meaning |
|-------|-------|---------|
| Green | Blinking | Emulation mode active |
| Blue | Blinking | Passthrough mode active |
| Red | Solid | Stack overflow error |

## License

Based on FreeRTOS (MIT License)
