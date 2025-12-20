# ============================================================================
# u-blox GNSS Emulator - Unified Project for RP2040/RP2350
# ============================================================================
#
# Build instructions:
#   mkdir build && cd build
#
#   For RP2040 (Pico):
#     cmake -DTARGET_BOARD=pico ..
#     make
#
#   For RP2350 (Pico 2):
#     cmake -DTARGET_BOARD=pico2 ..
#     make
#
# ============================================================================

cmake_minimum_required(VERSION 3.13)

# ============================================================================
# Platform Selection
# ============================================================================

# Default to pico2 if not specified
if(NOT DEFINED TARGET_BOARD)
    set(TARGET_BOARD "pico2" CACHE STRING "Target board: pico or pico2")
endif()

message(STATUS "Building for: ${TARGET_BOARD}")

if(TARGET_BOARD STREQUAL "pico")
    set(PICO_BOARD pico CACHE STRING "Board type")
    set(MAIN_SOURCE "src/main_rp2040.c")
    set(FREERTOS_PORT "RP2040")
    set(PROJECT_SUFFIX "rp2040")
elseif(TARGET_BOARD STREQUAL "pico2")
    set(PICO_BOARD pico2 CACHE STRING "Board type")
    set(PICO_PLATFORM rp2350-arm-s)
    set(MAIN_SOURCE "src/main_rp2350.c")
    set(FREERTOS_PORT "RP2350_ARM_NTZ")
    set(PROJECT_SUFFIX "rp2350")
else()
    message(FATAL_ERROR "Unknown TARGET_BOARD: ${TARGET_BOARD}. Use 'pico' or 'pico2'")
endif()

# ============================================================================
# SDK Import
# ============================================================================

# Check for VS Code Pico extension
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()

set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if(EXISTS ${picoVscode})
    include(${picoVscode})
endif()

include(pico_sdk_import.cmake)

# ============================================================================
# Project Definition
# ============================================================================

project(ublox_fake_${PROJECT_SUFFIX} C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# ============================================================================
# FreeRTOS Configuration
# ============================================================================

set(FREERTOS_CONFIG_FILE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/src)
set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS)

if(FREERTOS_PORT STREQUAL "RP2040")
    include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)
else()
    include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake)
endif()

set(FREERTOS_HEAP "4" CACHE STRING "" FORCE)

# ============================================================================
# SDK Initialization
# ============================================================================

pico_sdk_init()

# ============================================================================
# Compiler Options
# ============================================================================

add_compile_options(
    -Wall
    -Wno-format
    -Wno-unused-function
)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-maybe-uninitialized)
endif()

# ============================================================================
# Executable
# ============================================================================

add_executable(ublox_fake_${PROJECT_SUFFIX}
    ${MAIN_SOURCE}
    src/sha256.c
    lib/micro-ecc/uECC.c
)

# micro-ecc configuration - use secp192r1
target_compile_definitions(ublox_fake_${PROJECT_SUFFIX} PRIVATE
    uECC_SUPPORTS_secp160r1=0
    uECC_SUPPORTS_secp192r1=1
    uECC_SUPPORTS_secp224r1=0
    uECC_SUPPORTS_secp256r1=0
    uECC_SUPPORTS_secp256k1=0
)

# Generate PIO headers
pico_generate_pio_header(ublox_fake_${PROJECT_SUFFIX}
    ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio
    OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/src/
)

pico_generate_pio_header(ublox_fake_${PROJECT_SUFFIX}
    ${CMAKE_CURRENT_LIST_DIR}/src/uart_rx.pio
    OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/src/
)

# Include directories
target_include_directories(ublox_fake_${PROJECT_SUFFIX} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/lib/micro-ecc
)

# Link libraries
target_link_libraries(ublox_fake_${PROJECT_SUFFIX} PRIVATE
    pico_stdlib
    pico_multicore
    pico_time
    hardware_clocks
    hardware_pio
    hardware_timer
    hardware_gpio
    hardware_uart
    hardware_irq
    hardware_flash
    pico_flash
    pico_async_context_freertos
    FreeRTOS-Kernel-Heap4
)

# Generate UF2 and other outputs
pico_add_extra_outputs(ublox_fake_${PROJECT_SUFFIX})

# ============================================================================
# Build Info
# ============================================================================

message(STATUS "")
message(STATUS "=== Build Configuration ===")
message(STATUS "  Target Board:    ${TARGET_BOARD}")
message(STATUS "  Main Source:     ${MAIN_SOURCE}")
message(STATUS "  FreeRTOS Port:   ${FREERTOS_PORT}")
message(STATUS "  Output Name:     ublox_fake_${PROJECT_SUFFIX}")
message(STATUS "===========================")
message(STATUS "")
