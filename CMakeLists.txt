cmake_minimum_required(VERSION 3.20)
project(GearheadsNative VERSION 1.0.0 LANGUAGES CXX RC)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

file(GLOB_RECURSE EMBEDDED_ASSET_FILES CONFIGURE_DEPENDS
    LIST_DIRECTORIES false
    "${CMAKE_SOURCE_DIR}/assets/*"
)
set(EMBEDDED_ASSET_PACK "${CMAKE_BINARY_DIR}/generated/gearheads-assets.pack")
set(EMBEDDED_RESOURCE_FILE "${CMAKE_BINARY_DIR}/generated/gearheads-resources.rc")
add_custom_command(
    OUTPUT "${EMBEDDED_ASSET_PACK}" "${EMBEDDED_RESOURCE_FILE}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build_embedded_assets.py"
            --assets "${CMAKE_SOURCE_DIR}/assets"
            --pack "${EMBEDDED_ASSET_PACK}"
            --resource "${EMBEDDED_RESOURCE_FILE}"
            --manifest "${CMAKE_SOURCE_DIR}/src/gearheads.manifest"
            --version "${PROJECT_VERSION}"
    DEPENDS
        "${CMAKE_SOURCE_DIR}/tools/build_embedded_assets.py"
        "${CMAKE_SOURCE_DIR}/src/gearheads.manifest"
        ${EMBEDDED_ASSET_FILES}
    COMMENT "Embedding the converted Gearheads runtime"
    VERBATIM
)
set_source_files_properties("${EMBEDDED_RESOURCE_FILE}" PROPERTIES GENERATED TRUE)

add_executable(gearheads WIN32
    "${EMBEDDED_RESOURCE_FILE}"
    src/main.cpp
    src/embedded_assets.cpp
    src/render.cpp
    src/screens.cpp
    src/levels.cpp
    src/scripts.cpp
    src/defaults.cpp
    src/physics.cpp
    src/match_rules.cpp
    src/obstacles.cpp
    src/original_dispatch.cpp
    src/toy_effects.cpp
    src/powerups.cpp
    src/dynamic_obstacles.cpp
    src/tournament.cpp
    src/attract.cpp
    src/champions.cpp
    src/launch_timing.cpp
    src/computer_ai.cpp
    src/game.cpp
)

target_compile_features(gearheads PRIVATE cxx_std_20)
target_compile_definitions(gearheads PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)
target_compile_options(gearheads PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall;-Wextra;-Wpedantic;-Wconversion>
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive->
)
target_link_options(gearheads PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-static;-static-libgcc;-static-libstdc++;-s>
)
target_link_libraries(gearheads PRIVATE
    gdi32
    ole32
    shell32
    uuid
    windowscodecs
    winmm
)

set_target_properties(gearheads PROPERTIES
    OUTPUT_NAME "Gearheads"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist"
)

include(CTest)
if(BUILD_TESTING)
    find_program(OBJDUMP_EXECUTABLE objdump REQUIRED)
    find_program(POWERSHELL_EXECUTABLE powershell REQUIRED)
    add_executable(embedded_assets_test
        tests/embedded_assets_test.cpp
        src/embedded_assets.cpp
        "${EMBEDDED_RESOURCE_FILE}"
    )
    target_compile_features(embedded_assets_test PRIVATE cxx_std_20)
    target_compile_definitions(embedded_assets_test PRIVATE
        UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    )
    add_test(
        NAME embedded_assets
        COMMAND embedded_assets_test "${CMAKE_BINARY_DIR}/embedded-assets-test-cache"
    )
    add_executable(screen_data_test
        tests/screen_data_test.cpp
        src/screens.cpp
    )
    target_compile_features(screen_data_test PRIVATE cxx_std_20)
    target_compile_definitions(screen_data_test PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(screen_data_test PRIVATE src)
    add_test(
        NAME screen_data
        COMMAND screen_data_test "${CMAKE_SOURCE_DIR}/assets/data/screens.ini"
    )
    add_executable(level_data_test
        tests/level_data_test.cpp
        src/levels.cpp
    )
    target_compile_features(level_data_test PRIVATE cxx_std_20)
    target_compile_definitions(level_data_test PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(level_data_test PRIVATE src)
    add_test(
        NAME level_data
        COMMAND level_data_test "${CMAKE_SOURCE_DIR}/assets/data/gearhead.ini"
    )
    add_executable(script_data_test
        tests/script_data_test.cpp
        src/scripts.cpp
    )
    target_compile_features(script_data_test PRIVATE cxx_std_20)
    target_compile_definitions(script_data_test PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(script_data_test PRIVATE src)
    add_test(
        NAME script_data
        COMMAND script_data_test
                "${CMAKE_SOURCE_DIR}/assets/data/script.ini"
                "${CMAKE_SOURCE_DIR}/assets"
    )
    add_test(
        NAME asset_integrity
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/verify_assets.py"
                "${CMAKE_SOURCE_DIR}/assets"
    )
    add_test(
        NAME sprite_decode
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/sprite_decode_test.py"
    )
    add_test(
        NAME defaults_data
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/verify_defaults.py"
                "${CMAKE_SOURCE_DIR}/assets/data/defaults.json"
                "${CMAKE_SOURCE_DIR}/assets/data/runtime-defaults.ini"
    )
    add_executable(defaults_runtime_test
        tests/defaults_runtime_test.cpp
        src/defaults.cpp
    )
    target_compile_features(defaults_runtime_test PRIVATE cxx_std_20)
    target_compile_definitions(defaults_runtime_test PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(defaults_runtime_test PRIVATE src)
    add_test(
        NAME defaults_runtime
        COMMAND defaults_runtime_test "${CMAKE_SOURCE_DIR}/assets/data/runtime-defaults.ini"
    )
    add_executable(physics_test
        tests/physics_test.cpp
        src/physics.cpp
    )
    target_compile_features(physics_test PRIVATE cxx_std_20)
    target_include_directories(physics_test PRIVATE src)
    add_test(NAME physics COMMAND physics_test)
    add_executable(match_rules_test
        tests/match_rules_test.cpp
        src/match_rules.cpp
    )
    target_compile_features(match_rules_test PRIVATE cxx_std_20)
    target_include_directories(match_rules_test PRIVATE src)
    add_test(NAME match_rules COMMAND match_rules_test)
    add_executable(launch_timing_test
        tests/launch_timing_test.cpp
        src/launch_timing.cpp
    )
    target_compile_features(launch_timing_test PRIVATE cxx_std_20)
    target_include_directories(launch_timing_test PRIVATE src)
    add_test(NAME launch_timing COMMAND launch_timing_test)
    add_executable(computer_ai_test
        tests/computer_ai_test.cpp
        src/computer_ai.cpp
    )
    target_compile_features(computer_ai_test PRIVATE cxx_std_20)
    target_include_directories(computer_ai_test PRIVATE src)
    add_test(NAME computer_ai COMMAND computer_ai_test)
    add_executable(obstacles_test
        tests/obstacles_test.cpp
        src/obstacles.cpp
    )
    target_compile_features(obstacles_test PRIVATE cxx_std_20)
    target_include_directories(obstacles_test PRIVATE src)
    add_test(NAME obstacles COMMAND obstacles_test)
    add_executable(original_dispatch_test
        tests/original_dispatch_test.cpp
        src/original_dispatch.cpp
    )
    target_compile_features(original_dispatch_test PRIVATE cxx_std_20)
    target_include_directories(original_dispatch_test PRIVATE src)
    add_test(NAME original_dispatch COMMAND original_dispatch_test)
    add_executable(toy_effects_test
        tests/toy_effects_test.cpp
        src/toy_effects.cpp
    )
    target_compile_features(toy_effects_test PRIVATE cxx_std_20)
    target_include_directories(toy_effects_test PRIVATE src)
    add_test(NAME toy_effects COMMAND toy_effects_test)
    add_executable(powerups_test
        tests/powerups_test.cpp
        src/powerups.cpp
    )
    target_compile_features(powerups_test PRIVATE cxx_std_20)
    target_include_directories(powerups_test PRIVATE src)
    add_test(NAME powerups COMMAND powerups_test)
    add_executable(dynamic_obstacles_test
        tests/dynamic_obstacles_test.cpp
        src/dynamic_obstacles.cpp
    )
    target_compile_features(dynamic_obstacles_test PRIVATE cxx_std_20)
    target_include_directories(dynamic_obstacles_test PRIVATE src)
    add_test(NAME dynamic_obstacles COMMAND dynamic_obstacles_test)
    add_executable(tournament_test
        tests/tournament_test.cpp
        src/tournament.cpp
    )
    target_compile_features(tournament_test PRIVATE cxx_std_20)
    target_include_directories(tournament_test PRIVATE src)
    add_test(NAME tournament COMMAND tournament_test)
    add_executable(attract_data_test
        tests/attract_data_test.cpp
        src/attract.cpp
    )
    target_compile_features(attract_data_test PRIVATE cxx_std_20)
    target_compile_definitions(attract_data_test PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(attract_data_test PRIVATE src)
    add_test(
        NAME attract_data
        COMMAND attract_data_test "${CMAKE_SOURCE_DIR}/assets/data/anim.dat"
    )
    add_executable(champions_test
        tests/champions_test.cpp
        src/champions.cpp
    )
    target_compile_features(champions_test PRIVATE cxx_std_20)
    target_compile_definitions(champions_test PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(champions_test PRIVATE src)
    add_test(
        NAME champions
        COMMAND champions_test "${CMAKE_SOURCE_DIR}/assets/data/gearhead.ini"
    )
    add_test(
        NAME native_imports
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/verify_pe_imports.py"
                "$<TARGET_FILE:gearheads>" "${OBJDUMP_EXECUTABLE}"
    )
    add_test(
        NAME fullscreen_toggle
        COMMAND "${POWERSHELL_EXECUTABLE}"
                -NoProfile
                -ExecutionPolicy Bypass
                -File "${CMAKE_SOURCE_DIR}/tests/fullscreen_toggle_test.ps1"
                -Executable "$<TARGET_FILE:gearheads>"
    )
endif()
