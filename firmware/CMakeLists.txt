# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# StackChan firmware fork by GOB: split display version (shown in About / log)
# from the embedded APP_VERSION (firmware image header) used by xiaozhi's auto
# OTA check. PROJECT_VER is set high so xiaozhi's
# Ota::IsNewVersionAvailable(current=99.0.0, server=1.x.x) always returns false
# → auto OTA on boot is skipped without modifying xiaozhi sources.
# Manual OTA (AppSetup → Check for Updates) is unaffected because it uses
# server-returned firmware_url directly, independent of version comparison.
# Partition rollback validity is confirmed independently in hal.cpp 20s after
# boot (esp_ota_mark_app_valid_cancel_rollback), so skipping the xiaozhi-side
# MarkCurrentVersionValid is safe.
set(GOB_FORK_DISPLAY_VERSION "1.2.4-gob")
set(PROJECT_VER "99.0.0")
add_definitions(-DFIRMWARE_VERSION=\"${GOB_FORK_DISPLAY_VERSION}\")

# StackChan firmware fork by GOB: inject git short hash for the GOB FORK About
# page. CMAKE_SOURCE_DIR is firmware/, but git looks up the tree to find .git.
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GOB_FORK_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE _git_hash_result
)
if(NOT _git_hash_result EQUAL 0 OR NOT GOB_FORK_GIT_HASH)
    set(GOB_FORK_GIT_HASH "unknown")
endif()
add_definitions(-DGOB_FORK_GIT_HASH=\"${GOB_FORK_GIT_HASH}\")

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)

# Register M5Stack libraries linked under components/m5unit_local/ as
# ESP-IDF components. Each subdir provides its own CMakeLists.txt; the
# library sources are designed to compile without Arduino — Arduino-only
# code paths are gated with #if defined(ARDUINO) so only ESP-IDF native
# branches are built here.
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/components/m5unit_local/M5Utility
    ${CMAKE_CURRENT_LIST_DIR}/components/m5unit_local/M5HAL
    ${CMAKE_CURRENT_LIST_DIR}/components/m5unit_local/M5UnitUnified
    ${CMAKE_CURRENT_LIST_DIR}/components/m5unit_local/M5Unit-NFC
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(stack-chan)
