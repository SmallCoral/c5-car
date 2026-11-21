cmake_minimum_required(VERSION 3.22)
project(coral_c5 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HOST_TESTS "Build portable logic tests instead of firmware" OFF)
if(HOST_TESTS)
  enable_testing()
  add_executable(test_logic tests/test_logic.c src/logic.c src/emm.c src/stepper.c src/jog.c src/contest.c)
  target_include_directories(test_logic PRIVATE include)
  target_compile_options(test_logic PRIVATE -Wall -Wextra -Werror)
  add_test(NAME logic COMMAND test_logic)
  add_executable(test_grayscale tests/test_grayscale.c src/grayscale.c src/logic.c)
  target_include_directories(test_grayscale PRIVATE include)
  target_compile_options(test_grayscale PRIVATE -Wall -Wextra -Werror)
  add_test(NAME grayscale COMMAND test_grayscale)
  add_executable(test_lap tests/test_lap.c src/lap.c src/logic.c src/grayscale.c)
  target_include_directories(test_lap PRIVATE include)
  target_compile_options(test_lap PRIVATE -Wall -Wextra -Werror)
  add_test(NAME lap_control COMMAND test_lap)
  add_executable(test_lap_dynamics tests/test_lap_dynamics.c src/lap.c src/logic.c src/grayscale.c)
  target_include_directories(test_lap_dynamics PRIVATE include)
  target_compile_options(test_lap_dynamics PRIVATE -Wall -Wextra -Werror)
  target_link_libraries(test_lap_dynamics PRIVATE m)
  add_test(NAME lap_dynamics COMMAND test_lap_dynamics)
  add_executable(test_ball tests/test_ball.c src/ball.c)
  target_include_directories(test_ball PRIVATE include)
  target_compile_options(test_ball PRIVATE -Wall -Wextra -Werror)
  add_test(NAME camera_ball COMMAND test_ball)
  add_executable(test_balance tests/test_balance.c src/balance.c src/ball.c ports/classmate_26h/balance.c)
  target_include_directories(test_balance PRIVATE include ports)
  target_compile_options(test_balance PRIVATE -Wall -Wextra -Werror)
  add_test(NAME stationary_balance COMMAND test_balance)
  add_executable(render_ui tests/render_ui.c src/ui.c src/contest.c src/logic.c src/grayscale.c src/lap.c)
  target_include_directories(render_ui PRIVATE include)
  target_compile_options(render_ui PRIVATE -Wall -Wextra -Werror)
  add_test(NAME ui_bounds COMMAND render_ui ${CMAKE_CURRENT_BINARY_DIR}/ui.ppm)
  add_executable(test_pipe_display tests/test_pipe_display.c src/display.c src/ui.c
    src/logic.c src/contest.c src/grayscale.c src/lap.c src/ball.c)
  target_include_directories(test_pipe_display PRIVATE include)
  target_compile_options(test_pipe_display PRIVATE -Wall -Wextra -Werror)
  add_test(NAME live_pipe_display COMMAND test_pipe_display)
  add_executable(test_ota tests/test_ota.c src/ota.c boot/ota_store.c)
  target_include_directories(test_ota PRIVATE include)
  target_compile_options(test_ota PRIVATE -Wall -Wextra -Werror)
  add_test(NAME ota_recovery COMMAND test_ota)
  return()
endif()

set(DFP ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stm32c5xx_dfp)
add_executable(coral_c5
  src/main.c src/contest.c src/board.c src/serial.c src/logic.c src/emm.c src/ball.c src/balance.c ports/classmate_26h/balance.c
  src/stepper.c src/jog.c src/display.c src/ui.c src/runtime.c src/ota.c src/grayscale.c src/lap.c
  ${DFP}/Source/Templates/system_stm32c5xx.c
  ${DFP}/Source/startup_stm32c5a3xx.c)
add_executable(coral_boot
  boot/main.c boot/flash_port.c boot/ota_store.c src/ota.c src/contest.c
  src/board.c src/serial.c src/logic.c src/emm.c src/display.c src/ui.c src/grayscale.c src/lap.c src/runtime.c
  ${DFP}/Source/Templates/system_stm32c5xx.c
  ${DFP}/Source/startup_stm32c5a3xx.c)
set(CPU_FLAGS -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard)
foreach(target coral_c5 coral_boot)
  set_target_properties(${target} PROPERTIES SUFFIX .elf)
  target_include_directories(${target} PRIVATE include)
  target_include_directories(${target} SYSTEM PRIVATE
    vendor/CMSIS/Core/Include ${DFP}/Include vendor/stm32c5xx_drivers/ll)
  target_compile_definitions(${target} PRIVATE STM32C5A3xx)
  target_compile_options(${target} PRIVATE ${CPU_FLAGS}
    -Wall -Wextra -Werror -Wshadow -Wformat=2 -Wundef
    -ffunction-sections -fdata-sections -fno-common -fstack-usage
    $<$<CONFIG:Debug>:-Og> $<$<CONFIG:Release>:-Os>)
  if(target STREQUAL "coral_boot")
    set(linker_script platform/bootloader.ld)
  else()
    set(linker_script platform/stm32c5a3.ld)
  endif()
  target_link_options(${target} PRIVATE ${CPU_FLAGS} -nostartfiles
    --specs=nano.specs --specs=nosys.specs
    -T${CMAKE_CURRENT_SOURCE_DIR}/${linker_script}
    -Wl,--gc-sections,-Map=${target}.map,--print-memory-usage)
  set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/${linker_script})
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${target}> ${target}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${target}> ${target}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${target}>)
endforeach()
target_compile_definitions(coral_c5 PRIVATE $<$<CONFIG:Debug>:CORAL_DEBUG_KEYS>)
target_include_directories(coral_c5 PRIVATE ports)

if(NOT Python3_EXECUTABLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../.scratch/firmware-tools/Scripts/python.exe")
  set(Python3_EXECUTABLE "${CMAKE_CURRENT_SOURCE_DIR}/../.scratch/firmware-tools/Scripts/python.exe")
endif()
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
add_custom_target(firmware_package ALL
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/package_firmware.py
    --app ${CMAKE_CURRENT_BINARY_DIR}/coral_c5.bin
    --boot ${CMAKE_CURRENT_BINARY_DIR}/coral_boot.bin
    --out ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS coral_c5 coral_boot tools/package_firmware.py tools/ota_protocol.py
  COMMENT "Packaging wireless image and first-install HEX")
function(add_wireless_flash_target target)
  add_custom_target(${target}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/wireless_flash.py
      ${CMAKE_CURRENT_BINARY_DIR}/coral_c5.ota ${ARGN}
    DEPENDS firmware_package USES_TERMINAL
    COMMENT "Updating Coral-C5 over HC-08 BLE")
endfunction()
add_wireless_flash_target(wireless_flash)
add_wireless_flash_target(wireless_flash_supported --pipe-supported)
