cmake_minimum_required(VERSION 3.27.0)

# Initialize Pico-SDK From Submodule
include(external/pico-sdk/pico_sdk_init.cmake)

set("BUILD_EXAMPLES" OFF)
add_subdirectory(external/CRC32)
add_subdirectory(external/FlashKV)

# Create Project
project(CyphalFirmwareServer VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)

# Initialize The Raspberry Pi Pico SDK
set(PICO_COPY_TO_RAM 1)
pico_sdk_init()

add_executable(CyphalBoot2040
    "src/CyphalBoot2040.cpp"
)

# Add pico_stdlib Library Which Aggregates Commonly Used Features
target_link_libraries(CyphalBoot2040
    pico_stdlib
    hardware_resets
    hardware_flash
    cmsis_core
    CRC32
    FlashKV
)

# Custom Linker Script
pico_set_linker_script(CyphalBoot2040 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/memmap_cyphal_boot_2040_copy_to_ram.ld)

# Enable USB Output, Disable UART Output
pico_enable_stdio_usb(CyphalBoot2040 1)
pico_enable_stdio_uart(CyphalBoot2040 0)

# Create Map / Bin / Hex / uf2 Files In Addition To ELF
pico_add_extra_outputs(CyphalBoot2040)