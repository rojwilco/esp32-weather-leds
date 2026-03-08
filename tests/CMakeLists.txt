cmake_minimum_required(VERSION 3.14)
project(esp32_weather_leds_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# ── GoogleTest (auto-downloaded) ──────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)   # Windows compat
FetchContent_MakeAvailable(googletest)

# ── ArduinoJson v7 (auto-downloaded, header-only) ────────────────────────────
FetchContent_Declare(
    arduinojson
    URL https://github.com/bblanchon/ArduinoJson/archive/refs/tags/v7.2.1.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(arduinojson)

# ── Common include paths ───────────────────────────────────────────────────────
# mocks/ must come before the ArduinoJson source dir so that our stub headers
# intercept angle-bracket includes like <WiFi.h> before they hit the system.
set(MOCK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mocks)
set(SKETCH_DIR       ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(AJ_INCLUDE_DIR   ${arduinojson_SOURCE_DIR}/src)

# ── Compile definitions ───────────────────────────────────────────────────────
# ARDUINOJSON_ENABLE_ARDUINO_STRING=1 tells ArduinoJson to accept our ::String
# class via its existing IsString<::String> specialisation.
set(COMMON_DEFS
    UNIT_TESTING
    ARDUINOJSON_ENABLE_ARDUINO_STRING=1
)

# ── Shared sketch library ─────────────────────────────────────────────────────
# Compiles the .ino (via sketch_wrapper.cpp) and the mock globals once;
# each test executable links against it.
add_library(sketch_lib STATIC
    sketch_wrapper.cpp
    mocks/mock_support.cpp
)
target_include_directories(sketch_lib PUBLIC
    ${MOCK_INCLUDE_DIR}
    ${SKETCH_DIR}
    ${AJ_INCLUDE_DIR}
)
target_compile_definitions(sketch_lib PUBLIC ${COMMON_DEFS})

# ── Macro: one test executable per source file ────────────────────────────────
function(add_sketch_test TEST_SRC)
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SRC})
    target_include_directories(${TEST_NAME} PRIVATE
        ${MOCK_INCLUDE_DIR}
        ${SKETCH_DIR}
        ${AJ_INCLUDE_DIR}
    )
    target_compile_definitions(${TEST_NAME} PRIVATE ${COMMON_DEFS})
    target_link_libraries(${TEST_NAME}
        sketch_lib
        GTest::gtest_main
    )
    # Run via ctest; GTest writes per-test-case JUnit XML for dorny/test-reporter.
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME} --gtest_output=xml:${TEST_NAME}.xml)
endfunction()

add_sketch_test(test_temp_to_color.cpp)
add_sketch_test(test_fetch_forecast.cpp)
add_sketch_test(test_poll_weather.cpp)
add_sketch_test(test_animations.cpp)
add_sketch_test(test_handle_root.cpp)
add_sketch_test(test_handle_save.cpp)
add_sketch_test(test_handle_scan.cpp)
add_sketch_test(test_handle_ota.cpp)
