# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
set(PICO_BOARD pico CACHE STRING "Board type")

include(pico_sdk_import.cmake)
include(pico_add_freertos.cmake)

project(fuente_pid_freertosV1 C CXX ASM)

pico_sdk_init()
pico_add_freertos(fuente_pid_freertosV1)

add_executable(fuente_pid_freertosV1
    src/fuente_pid_freertosV1.c
    src/lcd.c
)

pico_set_program_name(fuente_pid_freertosV1 "fuente_pid_freertosV1")
pico_set_program_version(fuente_pid_freertosV1 "0.1")

pico_enable_stdio_uart(fuente_pid_freertosV1 1)
pico_enable_stdio_usb(fuente_pid_freertosV1 1)

target_link_libraries(fuente_pid_freertosV1
    pico_stdlib
    hardware_i2c
    hardware_adc
    hardware_pwm
)

target_include_directories(fuente_pid_freertosV1 PUBLIC
    ${PICO_SDK_PATH}/src/common/pico_base_headers/include
    ${PICO_SDK_PATH}/src/common/pico_stdlib_headers/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_lib/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

pico_add_extra_outputs(fuente_pid_freertosV1)