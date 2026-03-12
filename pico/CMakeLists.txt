# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Include build functions from Pico SDK
include(pico_sdk_import.cmake)

# Set name of project (as PROJECT_NAME) and C/C++ standards
project(magnetometer C CXX ASM)


# Creates a pico-sdk subdirectory in our project for the libraries
pico_sdk_init()

# Tell CMake where to find the executable source file
add_executable(${PROJECT_NAME} 
  #test.cpp
  magnetometer.cpp
  #examples/ST7735_TFT_TESTS/main.cpp
  #examples/ST7735_TFT_BMP_DATA/main.cpp
  #examples/ST7735_TFT_BMP_SDCARD/main.cpp
)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})

add_library(pico_st7735 INTERFACE)

target_sources(pico_st7735 INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/ST7735/src/st7735/ST7735_TFT.cpp
  ${CMAKE_CURRENT_LIST_DIR}/ST7735/src/st7735/ST7735_TFT_graphics.cpp
  ${CMAKE_CURRENT_LIST_DIR}/ST7735/src/st7735/ST7735_TFT_Print.cpp
)

target_include_directories(pico_st7735 INTERFACE ${CMAKE_CURRENT_LIST_DIR}/ST7735/include)

# Pull in pico libraries that we need
target_link_libraries(${PROJECT_NAME} pico_stdlib hardware_adc hardware_spi pico_st7735 )


# Enable usb output, disable uart output
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

