cmake_minimum_required(VERSION 3.22)
# Enable CMake support for ASM and C languages
enable_language(C ASM)
# AT32 WorkBench generated symbols (macros)
set(WK_Defines_Syms
    AT32F425R8T7
    USE_STDPERIPH_DRIVER
    AT_START_F425_V1
    USE_BMI4XY        # BMI323 改成 USE_BMI32X ；BMI4xy 改成 USE_BMI4XY
)

# AT32 WorkBench generated include paths
set(WK_Include_Dirs
    ${CMAKE_SOURCE_DIR}/libraries/drivers/inc
    ${CMAKE_SOURCE_DIR}/libraries/cmsis/cm4/core_support
    ${CMAKE_SOURCE_DIR}/libraries/cmsis/cm4/device_support
    ${CMAKE_SOURCE_DIR}/middlewares/usb_drivers/inc
    ${CMAKE_SOURCE_DIR}/middlewares/usbd_class/cdc
    ${CMAKE_SOURCE_DIR}/project/inc
    ${CMAKE_SOURCE_DIR}/project/at32f425_board
)

# AT32 WorkBench generated application sources
set(WK_Application_Src
    ${CMAKE_SOURCE_DIR}/middlewares/usb_drivers/src/usbd_core.c
    ${CMAKE_SOURCE_DIR}/middlewares/usb_drivers/src/usbd_int.c
    ${CMAKE_SOURCE_DIR}/middlewares/usb_drivers/src/usbd_sdr.c
    ${CMAKE_SOURCE_DIR}/middlewares/usb_drivers/src/usb_core.c      # ← 补加！解决 usbd_init / usb_ept_open 等
    ${CMAKE_SOURCE_DIR}/middlewares/usbd_class/cdc/cdc_class.c
    ${CMAKE_SOURCE_DIR}/middlewares/usbd_class/cdc/cdc_desc.c
    ${CMAKE_SOURCE_DIR}/project/src/at32f425_clock.c
    ${CMAKE_SOURCE_DIR}/project/src/at32f425_int.c
    ${CMAKE_SOURCE_DIR}/project/src/main.c
    ${CMAKE_SOURCE_DIR}/project/at32f425_board/at32f425_board.c
    ${CMAKE_SOURCE_DIR}/startup_at32f425.s
    ${CMAKE_SOURCE_DIR}/sysmem.c
    ${CMAKE_SOURCE_DIR}/syscalls.c
    ${CMAKE_SOURCE_DIR}/project/src/bsp_spi.c
    ${CMAKE_SOURCE_DIR}/project/src/bsp_i2c.c
    ${CMAKE_SOURCE_DIR}/project/src/bsp_config.c
    ${CMAKE_SOURCE_DIR}/project/src/pat9126.c
    ${CMAKE_SOURCE_DIR}/project/src/bsp_usb.c
)

# AT32 Drivers
set(AT32_Drivers_Src
    ${CMAKE_SOURCE_DIR}/libraries/drivers/src/at32f425_gpio.c
    ${CMAKE_SOURCE_DIR}/libraries/drivers/src/at32f425_crm.c
    ${CMAKE_SOURCE_DIR}/libraries/drivers/src/at32f425_usart.c
    ${CMAKE_SOURCE_DIR}/libraries/drivers/src/at32f425_misc.c
    ${CMAKE_SOURCE_DIR}/libraries/cmsis/cm4/device_support/system_at32f425.c
    ${CMAKE_SOURCE_DIR}/libraries/drivers/src/at32f425_spi.c
    ${CMAKE_SOURCE_DIR}/libraries/drivers/src/at32f425_dma.c
    ${CMAKE_SOURCE_DIR}/libraries/drivers/src/at32f425_exint.c
    ${CMAKE_SOURCE_DIR}/libraries/drivers/src/at32f425_scfg.c
    ${CMAKE_SOURCE_DIR}/libraries/drivers/src/at32f425_flash.c
    ${CMAKE_SOURCE_DIR}/libraries/drivers/src/at32f425_usb.c
    ${CMAKE_SOURCE_DIR}/libraries/drivers/src/at32f425_i2c.c         # ← 补加！解决 i2c_init / i2c_flag_get 等
)

# Drivers Middlewares

# Link directories setup
set(WK_LINK_DIRS

)

# Project static libraries
set(WK_LINK_LIBS
    AT32_Drivers
)

# Interface library for includes and symbols
add_library(at32_workbench INTERFACE)
target_include_directories(at32_workbench INTERFACE ${WK_Include_Dirs})
target_compile_definitions(at32_workbench INTERFACE ${WK_Defines_Syms})

# Create AT32_Drivers static library
add_library(AT32_Drivers OBJECT)
target_sources(AT32_Drivers PRIVATE ${AT32_Drivers_Src})
target_link_libraries(AT32_Drivers PUBLIC at32_workbench)

# Add AT32 WorkBench generated application sources to the project
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${WK_Application_Src})

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${WK_LINK_DIRS})

# Add libraries to the project
target_link_libraries(${CMAKE_PROJECT_NAME} ${WK_LINK_LIBS})

# Add the map file to the list of files to be removed with 'clean' target
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)

# Validate that AT32 WorkBench code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()
