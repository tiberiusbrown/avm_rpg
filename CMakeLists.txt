cmake_minimum_required(VERSION 3.20)
project(avm_rpg LANGUAGES CXX)
include(CTest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build configuration" FORCE)
endif()

# Optional full-device QA reuses a matching AVM checkout's existing Ardens library.
# Installed SDK users can run guest_integration.py with LLDB alone.
set(AVM_SOURCE_ROOT "" CACHE PATH "Optional AVM source checkout for full-device QA")
set(AVM_BUILD_ROOT "" CACHE PATH "Matching RelWithDebInfo AVM build")
if(AVM_SOURCE_ROOT AND AVM_BUILD_ROOT)
    add_executable(rpg_device_tests tests/device_tests.cpp generated/sprites.cpp)
    add_executable(rpg_audio_timing_tests tests/audio_timing.cpp generated/sprites.cpp)
    foreach(qa_target IN ITEMS rpg_device_tests rpg_audio_timing_tests)
        target_compile_features(${qa_target} PRIVATE cxx_std_20)
        target_include_directories(${qa_target} PRIVATE src generated
            "${AVM_SOURCE_ROOT}/tests/interp"
            "${AVM_SOURCE_ROOT}/deps/Ardens/src"
            "${AVM_SOURCE_ROOT}/deps/Ardens/deps"
            "${AVM_SOURCE_ROOT}/deps/Ardens/deps/bitsery/include")
        if(MSVC)
            target_compile_options(${qa_target} PRIVATE /Zc:__cplusplus)
            target_link_libraries(${qa_target} PRIVATE
                "${AVM_BUILD_ROOT}/ardens-build/ardensdebuggerlib.lib"
                "${AVM_BUILD_ROOT}/ardens-build/deps/fmt/fmt.lib"
                "${AVM_BUILD_ROOT}/ardens-build/deps/miniz/miniz.lib")
        else()
            target_link_libraries(${qa_target} PRIVATE
                "${AVM_BUILD_ROOT}/ardens-build/libardensdebuggerlib.a"
                "${AVM_BUILD_ROOT}/ardens-build/deps/fmt/libfmt.a"
                "${AVM_BUILD_ROOT}/ardens-build/deps/miniz/libminiz.a")
        endif()
    endforeach()
endif()
set(AVM_SDK_ROOT "" CACHE PATH "Installed AVM SDK")
option(RPG_BUILD_GUEST "Build guest programs using installed SDK" ON)
file(GLOB_RECURSE LOGIC_HEADERS CONFIGURE_DEPENDS src/logic/*.h src/logic/*.inc)
set(RULES src/game.cpp src/world.cpp src/save.cpp src/npc.cpp src/logic/npc_logic.cpp
    generated/map_assets.cpp generated/chunk_dispatch.cpp generated/npc_routes.cpp)
add_library(rpg_rules STATIC ${RULES})
target_compile_features(rpg_rules PUBLIC cxx_std_20)
target_include_directories(rpg_rules PUBLIC src generated)
if(MSVC)
    target_compile_options(rpg_rules PRIVATE /W4)
else()
    target_compile_options(rpg_rules PRIVATE -Wall -Wextra)
endif()
add_executable(rpg_tests tests/test_core.cpp)
target_link_libraries(rpg_tests PRIVATE rpg_rules)
target_include_directories(rpg_tests PRIVATE generated)
add_test(NAME core COMMAND rpg_tests)
add_executable(rpg_npc_tests tests/test_npc.cpp)
target_link_libraries(rpg_npc_tests PRIVATE rpg_rules)
add_test(NAME essential_npcs COMMAND rpg_npc_tests)
add_executable(rpg_npc_hook_tests tests/test_npc_hooks.cpp)
target_link_libraries(rpg_npc_hook_tests PRIVATE rpg_rules)
add_test(NAME npc_lifecycle_hooks COMMAND rpg_npc_hook_tests)
add_executable(rpg_flag_tests tests/test_flags.cpp)
target_include_directories(rpg_flag_tests PRIVATE src)
add_test(NAME flags COMMAND rpg_flag_tests)
add_executable(rpg_world_tests tests/test_world.cpp)
target_link_libraries(rpg_world_tests PRIVATE rpg_rules)
target_include_directories(rpg_world_tests PRIVATE generated)
add_test(NAME chunk_world COMMAND rpg_world_tests)
add_test(NAME tiled_pipeline COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pipeline.py)
add_test(NAME generated_assets COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_assets.py --check)
add_test(NAME content COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_content.py)
file(GLOB_RECURSE ASSET_INPUTS CONFIGURE_DEPENDS assets/*.tmx assets/*.tsx assets_src/*.png)
set(ASSET_OUTPUTS)
foreach(asset area_ids.h world_assets.h map_positions.h tile_ids.h handler_defaults.h map_assets.cpp chunk_dispatch.cpp npc_routes.h npc_routes.cpp sprites.cpp)
    list(APPEND ASSET_OUTPUTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/${asset}")
endforeach()
add_custom_command(OUTPUT ${ASSET_OUTPUTS}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_assets.py
    DEPENDS tools/generate_assets.py ${ASSET_INPUTS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} VERBATIM)
add_custom_target(rpg_assets DEPENDS ${ASSET_OUTPUTS})
add_dependencies(rpg_rules rpg_assets)
add_dependencies(rpg_world_tests rpg_assets)

# Compile exported dispatch with new, unimplemented named/unnamed handlers.
# Keep the fixture unlinked so its generated Handler definition is independent.
set(HANDLER_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/handler-fixture")
set(HANDLER_FIXTURE_OUTPUTS)
foreach(asset area_ids.h world_assets.h map_positions.h tile_ids.h handler_defaults.h map_assets.cpp chunk_dispatch.cpp)
    list(APPEND HANDLER_FIXTURE_OUTPUTS "${HANDLER_FIXTURE_DIR}/${asset}")
endforeach()
add_custom_command(OUTPUT ${HANDLER_FIXTURE_OUTPUTS}
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pipeline.py"
        --write-handler-fixture "${HANDLER_FIXTURE_DIR}"
    DEPENDS tests/test_pipeline.py tools/generate_assets.py ${ASSET_INPUTS}
    VERBATIM)
add_library(rpg_handler_fixture OBJECT "${HANDLER_FIXTURE_DIR}/chunk_dispatch.cpp")
target_include_directories(rpg_handler_fixture PRIVATE src "${HANDLER_FIXTURE_DIR}")
add_dependencies(rpg_world_tests rpg_handler_fixture)

if(TARGET rpg_device_tests)
    add_dependencies(rpg_device_tests rpg_assets)
    add_dependencies(rpg_audio_timing_tests rpg_assets)
endif()

if(RPG_BUILD_GUEST)
    if(NOT AVM_SDK_ROOT)
        find_program(RPG_AVM_CLANG NAMES avm-clang REQUIRED)
        get_filename_component(AVM_SDK_ROOT "${RPG_AVM_CLANG}/../.." ABSOLUTE)
    endif()
    if(CMAKE_HOST_WIN32)
        set(EXE .exe)
    endif()
    set(AVM_CLANG "${AVM_SDK_ROOT}/bin/avm-clang${EXE}")
    set(AVM_IMAGE "${AVM_SDK_ROOT}/bin/avm-image${EXE}")
    set(AVM_LLDB "${AVM_SDK_ROOT}/bin/avm-lldb${EXE}")
    set(AVM_FXC_INTERPRETER "${AVM_SDK_ROOT}/bin/avm/interp.fxc.hex")
    foreach(required IN ITEMS "${AVM_CLANG}" "${AVM_IMAGE}" "${AVM_SDK_ROOT}/sysroot/include/avm.h" "${AVM_SDK_ROOT}/bin/avm/interp.hex" "${AVM_FXC_INTERPRETER}")
        if(NOT EXISTS "${required}")
            message(FATAL_ERROR "Missing SDK prerequisite: ${required}")
        endif()
    endforeach()
    file(GLOB_RECURSE SDK_DEPS CONFIGURE_DEPENDS "${AVM_SDK_ROOT}/sysroot/include/*" "${AVM_SDK_ROOT}/sysroot/lib/*")
    set(RPG_MUSIC_COMPILER "${AVM_SDK_ROOT}/bin/avm_music.py")
    if(NOT EXISTS "${RPG_MUSIC_COMPILER}")
        set(RPG_MUSIC_COMPILER "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/avm_music.py")
    endif()
    set(MUSIC_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/generated/peaceful_music.h")
    add_custom_command(OUTPUT "${MUSIC_OUTPUT}"
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_music.py"
            --compiler "${RPG_MUSIC_COMPILER}"
        DEPENDS tools/generate_music.py assets/music/peaceful.avms "${RPG_MUSIC_COMPILER}"
        VERBATIM)
    add_custom_target(rpg_music DEPENDS "${MUSIC_OUTPUT}")
    add_test(NAME peaceful_music COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_music.py" --compiler "${RPG_MUSIC_COMPILER}")
    function(guest name)
        set(objects)
        foreach(source IN LISTS ARGN)
            get_filename_component(stem "${source}" NAME_WE)
            set(object "${CMAKE_CURRENT_BINARY_DIR}/${name}/${stem}.o")
            set(defines -DRPG_AVM=1)
            set(source_dependencies)
            if(source STREQUAL "src/platform_avm.cpp")
                list(APPEND source_dependencies "${MUSIC_OUTPUT}")
            endif()
            add_custom_command(OUTPUT "${object}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/${name}"
                COMMAND "${AVM_CLANG}" -std=c++20 -O2 -g -flto=full -fno-discard-value-names -Wall -Wextra -Werror
                    ${defines} -I "${CMAKE_CURRENT_SOURCE_DIR}/src" -I "${CMAKE_CURRENT_SOURCE_DIR}/generated"
                    -c "${CMAKE_CURRENT_SOURCE_DIR}/${source}" -o "${object}"
                DEPENDS "${source}" src/rpg.h src/flags.h src/world_types.h src/chunk.h src/npc.h src/npc_types.h src/actor_draw_order.h ${LOGIC_HEADERS} tests/npc_scenarios.h tests/world_scenarios.h ${ASSET_OUTPUTS} ${source_dependencies} ${SDK_DEPS} "${AVM_CLANG}"
                VERBATIM)
            list(APPEND objects "${object}")
        endforeach()
        set(elf "${CMAKE_CURRENT_BINARY_DIR}/artifacts/${name}.elf")
        set(bin "${CMAKE_CURRENT_BINARY_DIR}/artifacts/${name}.bin")
        set(package "${CMAKE_CURRENT_BINARY_DIR}/artifacts/${name}.arduboy")
        set(fxc_options)
        set(fxc_outputs)
        set(fxc_dependencies)
        if(name STREQUAL "rpg")
            set(fxc_outputs "${CMAKE_CURRENT_BINARY_DIR}/artifacts/rpg.fxc.arduboy")
            set(fxc_options --fxc-interpreter "${AVM_FXC_INTERPRETER}")
            set(fxc_dependencies "${AVM_FXC_INTERPRETER}")
        endif()
        add_custom_command(OUTPUT "${elf}" "${bin}" "${package}" ${fxc_outputs} "${elf}.resources.json" "${elf}.lto.json"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/artifacts"
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/link_guest.py"
                --compiler "${AVM_CLANG}" --image "${AVM_IMAGE}" --output "${elf}" ${fxc_options} ${objects}
            DEPENDS ${objects} tools/link_guest.py tools/resources.py tools/check_lto.py "${AVM_IMAGE}" "${AVM_SDK_ROOT}/bin/avm/interp.hex" ${fxc_dependencies}
            VERBATIM)
        if(name STREQUAL "rpg")
            # Copy rather than declare an output: CMake clean preserves the playable package.
            add_custom_target(${name} ALL
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${package}" "${CMAKE_CURRENT_SOURCE_DIR}/rpg.arduboy"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${fxc_outputs}" "${CMAKE_CURRENT_SOURCE_DIR}/rpg.fxc.arduboy"
                DEPENDS "${package}" ${fxc_outputs} "${elf}.resources.json" VERBATIM)
        else()
            add_custom_target(${name} ALL DEPENDS "${package}" "${elf}.resources.json")
        endif()
        add_dependencies(${name} rpg_assets)
    endfunction()
    guest(m0_proof src/proof.cpp)
    guest(m15_probe ${RULES} tests/world_probe.cpp)
    guest(m16_probe ${RULES} tests/npc_probe.cpp)
    set(PLATFORM src/input.cpp src/screens.cpp src/dialogue_overlay.cpp src/render.cpp src/platform_avm.cpp generated/sprites.cpp)
    guest(rpg ${RULES} ${PLATFORM})
    add_dependencies(rpg rpg_music)
    if(TARGET rpg_audio_timing_tests)
        add_custom_target(rpg_audio_timing
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_audio_timing.py"
                --device "$<TARGET_FILE:rpg_audio_timing_tests>"
                --build "${CMAKE_CURRENT_BINARY_DIR}"
                --interpreter-build "${AVM_SOURCE_ROOT}/interp/build"
                --fxc-interpreter-build "${AVM_BUILD_ROOT}/interp/fxc"
                --seconds 40 --fxc-seconds 8
            DEPENDS rpg rpg_audio_timing_tests VERBATIM)
        add_test(NAME audio_timing COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_audio_timing.py"
            --device "$<TARGET_FILE:rpg_audio_timing_tests>"
            --build "${CMAKE_CURRENT_BINARY_DIR}"
            --interpreter-build "${AVM_SOURCE_ROOT}/interp/build"
            --fxc-interpreter-build "${AVM_BUILD_ROOT}/interp/fxc"
            --seconds 2 --fxc-seconds 2 --no-plot
            --output "${CMAKE_CURRENT_BINARY_DIR}/audio-timing-smoke")
        set_tests_properties(audio_timing PROPERTIES TIMEOUT 180)
    endif()
    if(EXISTS "${AVM_LLDB}")
        add_custom_target(rpg_integration
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/guest_integration.py"
                --sdk "${AVM_SDK_ROOT}" --build "${CMAKE_CURRENT_BINARY_DIR}"
            DEPENDS rpg m0_proof m15_probe m16_probe VERBATIM)
        if(TARGET rpg_device_tests)
            add_dependencies(rpg_integration rpg_device_tests)
        endif()
    endif()
endif()
