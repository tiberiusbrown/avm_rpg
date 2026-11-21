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
file(GLOB_RECURSE QUEST_HEADERS CONFIGURE_DEPENDS src/quest*.h src/quests/*.h)
set(RULES src/encounter.cpp src/equipment.cpp src/supplies.cpp src/shop.cpp src/game.cpp src/party_progress.cpp src/runtime_workspace.cpp src/world.cpp src/save.cpp src/npc.cpp src/soundscape.cpp src/logic/npc_logic.cpp
    src/battle.cpp src/battle_ai.cpp generated/map_assets.cpp src/chunk_dispatch.cpp generated/npc_routes.cpp
    src/quest_runtime.cpp)
add_library(rpg_rules STATIC ${RULES})
target_compile_features(rpg_rules PUBLIC cxx_std_20)
target_include_directories(rpg_rules PUBLIC src generated)
# Host stand-ins for the AVM battle screen, shared by tests that link chunk dispatch.
add_library(rpg_story_stubs STATIC tests/story_stub.cpp)
target_compile_features(rpg_story_stubs PRIVATE cxx_std_20)
target_include_directories(rpg_story_stubs PRIVATE src generated)
if(MSVC)
    target_compile_options(rpg_rules PRIVATE /W4)
else()
    target_compile_options(rpg_rules PRIVATE -Wall -Wextra)
endif()
add_executable(rpg_tests tests/test_core.cpp)
target_link_libraries(rpg_tests PRIVATE rpg_rules rpg_story_stubs)
target_include_directories(rpg_tests PRIVATE generated)
add_test(NAME core COMMAND rpg_tests)
foreach(fixture IN ITEMS index hook)
    add_library(rpg_${fixture}_dispatch OBJECT src/chunk_dispatch.cpp)
    target_include_directories(rpg_${fixture}_dispatch PRIVATE src generated tests)
    target_compile_definitions(rpg_${fixture}_dispatch PRIVATE RPG_NPC_POLICY_HEADER="${fixture}_policies.h")
    add_dependencies(rpg_${fixture}_dispatch rpg_assets)
endforeach()
set(SCALING_RULES ${RULES})
list(REMOVE_ITEM SCALING_RULES src/logic/npc_logic.cpp)
add_library(rpg_scaling_rules STATIC ${SCALING_RULES})
target_include_directories(rpg_scaling_rules PUBLIC src generated tests)
target_compile_definitions(rpg_scaling_rules PUBLIC RPG_NPC_ID_LIMIT=65 RPG_DISABLE_ENEMY_SPAWNS RPG_NPC_POLICY_HEADER="scaling_policies.h")
add_dependencies(rpg_scaling_rules rpg_assets)
add_executable(rpg_npc_scaling_tests tests/test_npc_scaling.cpp)
target_link_libraries(rpg_npc_scaling_tests PRIVATE rpg_scaling_rules rpg_story_stubs)
add_test(NAME npc_scaling COMMAND rpg_npc_scaling_tests)
add_executable(rpg_npc_policy_tests tests/test_npc_policies.cpp)
target_link_libraries(rpg_npc_policy_tests PRIVATE rpg_rules rpg_story_stubs)
add_test(NAME npc_policies COMMAND rpg_npc_policy_tests)
add_test(NAME npc_policy_validation COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_npc_policy_validation.py
    ${CMAKE_CXX_COMPILER} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
add_executable(rpg_npc_tests tests/test_npc.cpp)
target_link_libraries(rpg_npc_tests PRIVATE rpg_rules rpg_story_stubs)
add_test(NAME essential_npcs COMMAND rpg_npc_tests)
add_executable(rpg_npc_hook_tests tests/test_npc_hooks.cpp $<TARGET_OBJECTS:rpg_hook_dispatch>)
target_link_libraries(rpg_npc_hook_tests PRIVATE rpg_rules rpg_story_stubs)
add_test(NAME npc_lifecycle_hooks COMMAND rpg_npc_hook_tests)
add_executable(rpg_npc_dialogue_tests tests/test_npc_dialogue.cpp)
target_link_libraries(rpg_npc_dialogue_tests PRIVATE rpg_rules rpg_story_stubs)
add_test(NAME npc_dialogue COMMAND rpg_npc_dialogue_tests)
add_executable(rpg_npc_residence_tests tests/test_npc_residence.cpp)
target_link_libraries(rpg_npc_residence_tests PRIVATE rpg_rules rpg_story_stubs)
add_test(NAME npc_residence COMMAND rpg_npc_residence_tests)
add_executable(rpg_npc_index_tests tests/test_npc_index.cpp $<TARGET_OBJECTS:rpg_index_dispatch>)
target_link_libraries(rpg_npc_index_tests PRIVATE rpg_rules rpg_story_stubs)
add_test(NAME npc_index COMMAND rpg_npc_index_tests)
add_executable(rpg_npc_continuity_tests tests/test_npc_continuity.cpp)
target_link_libraries(rpg_npc_continuity_tests PRIVATE rpg_rules rpg_story_stubs)
add_test(NAME npc_continuity COMMAND rpg_npc_continuity_tests)
add_executable(rpg_npc_detour_tests tests/test_npc_detour.cpp)
target_link_libraries(rpg_npc_detour_tests PRIVATE rpg_rules rpg_story_stubs)
add_test(NAME npc_detour COMMAND rpg_npc_detour_tests)
add_executable(rpg_flag_tests tests/test_flags.cpp)
target_include_directories(rpg_flag_tests PRIVATE src)
add_test(NAME flags COMMAND rpg_flag_tests)
add_executable(rpg_world_tests tests/test_world.cpp)
target_link_libraries(rpg_world_tests PRIVATE rpg_rules rpg_story_stubs)
target_include_directories(rpg_world_tests PRIVATE generated)
add_test(NAME chunk_world COMMAND rpg_world_tests)
add_executable(rpg_quest_tests tests/test_quests.cpp)
target_link_libraries(rpg_quest_tests PRIVATE rpg_rules rpg_story_stubs)
target_include_directories(rpg_quest_tests PRIVATE generated)
add_dependencies(rpg_quest_tests rpg_assets)
add_test(NAME quests COMMAND rpg_quest_tests)
# The same runtime compiled against a forty-ID catalog with synthetic probe quests, so table sizes,
# flag bitsets, Journal scans and bounded settling are exercised at scale.
add_library(rpg_quest_catalog_rules STATIC ${RULES})
target_compile_features(rpg_quest_catalog_rules PUBLIC cxx_std_20)
target_include_directories(rpg_quest_catalog_rules PUBLIC src generated tests)
target_compile_definitions(rpg_quest_catalog_rules PUBLIC RPG_QUEST_ID_LIMIT=40 RPG_QUEST_REGISTRY_HEADER="quest_catalog_registry.h")
add_dependencies(rpg_quest_catalog_rules rpg_assets)
add_executable(rpg_quest_catalog_tests tests/test_quest_catalog.cpp)
target_link_libraries(rpg_quest_catalog_tests PRIVATE rpg_quest_catalog_rules rpg_story_stubs)
target_include_directories(rpg_quest_catalog_tests PRIVATE generated tests)
add_test(NAME quest_catalog COMMAND rpg_quest_catalog_tests)
add_test(NAME quest_validation COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_quest_validation.py
    ${CMAKE_CXX_COMPILER} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
add_executable(rpg_world_map_tests tests/test_world_map.cpp)
target_include_directories(rpg_world_map_tests PRIVATE src generated)
add_dependencies(rpg_world_map_tests rpg_assets)
add_test(NAME world_map COMMAND rpg_world_map_tests)
# Test Party navigation and its equipment requests without the AVM runtime.
add_executable(rpg_party_ui_tests tests/test_party_ui.cpp src/party_ui.cpp)
target_link_libraries(rpg_party_ui_tests PRIVATE rpg_rules rpg_story_stubs)
target_compile_features(rpg_party_ui_tests PRIVATE cxx_std_20)
target_include_directories(rpg_party_ui_tests PRIVATE src generated)
add_dependencies(rpg_party_ui_tests rpg_assets)
add_test(NAME party_ui COMMAND rpg_party_ui_tests)

add_executable(rpg_supplies_tests tests/test_supplies.cpp src/supplies_ui.cpp src/shop_ui.cpp)
target_link_libraries(rpg_supplies_tests PRIVATE rpg_rules)
add_test(NAME supplies COMMAND rpg_supplies_tests)
add_executable(rpg_ambient_tests tests/test_ambient.cpp)
target_link_libraries(rpg_ambient_tests PRIVATE rpg_rules rpg_story_stubs)
target_include_directories(rpg_ambient_tests PRIVATE generated)
add_test(NAME ambient COMMAND rpg_ambient_tests)
add_executable(rpg_soundscape_tests tests/test_soundscape.cpp)
target_link_libraries(rpg_soundscape_tests PRIVATE rpg_rules rpg_story_stubs)
target_include_directories(rpg_soundscape_tests PRIVATE generated)
add_test(NAME soundscape COMMAND rpg_soundscape_tests)
add_executable(rpg_battle_tests tests/test_battle.cpp tests/battle_game.cpp)
target_link_libraries(rpg_battle_tests PRIVATE rpg_rules)
target_include_directories(rpg_battle_tests PRIVATE generated)
add_dependencies(rpg_battle_tests rpg_assets)
add_test(NAME battle COMMAND rpg_battle_tests)
add_executable(rpg_battle_bench tests/battle_bench.cpp tests/battle_game.cpp)
target_link_libraries(rpg_battle_bench PRIVATE rpg_rules)
target_include_directories(rpg_battle_bench PRIVATE generated)
add_dependencies(rpg_battle_bench rpg_assets)
add_test(NAME battle_bench COMMAND rpg_battle_bench)
add_executable(rpg_battle_sim tests/battle_simulation.cpp tests/battle_game.cpp)
target_link_libraries(rpg_battle_sim PRIVATE rpg_rules)
target_include_directories(rpg_battle_sim PRIVATE generated)
add_dependencies(rpg_battle_sim rpg_assets)
add_test(NAME battle_simulation COMMAND rpg_battle_sim)
add_test(NAME tiled_pipeline COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pipeline.py)
add_test(NAME generated_assets COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_assets.py --check)
add_test(NAME content COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_content.py)
file(GLOB_RECURSE ASSET_INPUTS CONFIGURE_DEPENDS assets/*.tmx assets/*.tsx assets/*.png)
set(ASSET_OUTPUTS)
foreach(asset area_ids.h world_assets.h map_positions.h tile_ids.h handler_defaults.h map_assets.cpp map_handler_catalog.h npc_routes.h npc_routes.cpp sprites.cpp)
    list(APPEND ASSET_OUTPUTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/${asset}")
endforeach()
add_custom_command(OUTPUT ${ASSET_OUTPUTS}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_assets.py
    DEPENDS tools/generate_assets.py ${ASSET_INPUTS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} VERBATIM)
add_custom_target(rpg_assets DEPENDS ${ASSET_OUTPUTS})
add_dependencies(rpg_rules rpg_assets)
add_dependencies(rpg_world_tests rpg_assets)

file(GLOB MUSIC_SCORES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/music/*.avms")
# A changed file list must regenerate the catalog even when a score was removed.
set(MUSIC_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/music_sources.txt")
file(GENERATE OUTPUT "${MUSIC_MANIFEST}" CONTENT "${MUSIC_SCORES}\n")
set(MUSIC_CATALOG_OUTPUTS
    "${CMAKE_CURRENT_SOURCE_DIR}/generated/music_ids.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/generated/music_tracks.h")
add_custom_command(OUTPUT ${MUSIC_CATALOG_OUTPUTS}
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_music_catalog.py"
        --source-dir "${CMAKE_CURRENT_SOURCE_DIR}/assets/music"
        --output-dir "${CMAKE_CURRENT_SOURCE_DIR}/generated"
    DEPENDS tools/generate_music_catalog.py "${MUSIC_MANIFEST}" ${MUSIC_SCORES}
    VERBATIM)
add_custom_target(rpg_music_catalog ALL DEPENDS ${MUSIC_CATALOG_OUTPUTS})
add_dependencies(rpg_world_tests rpg_music_catalog)

# Compile exported dispatch with new, unimplemented named/unnamed handlers.
# Keep the fixture unlinked so its generated Handler definition is independent.
set(HANDLER_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/handler-fixture")
set(HANDLER_FIXTURE_OUTPUTS)
foreach(asset area_ids.h world_assets.h map_positions.h tile_ids.h handler_defaults.h map_assets.cpp map_handler_catalog.h)
    list(APPEND HANDLER_FIXTURE_OUTPUTS "${HANDLER_FIXTURE_DIR}/${asset}")
endforeach()
add_custom_command(OUTPUT ${HANDLER_FIXTURE_OUTPUTS}
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pipeline.py"
        --write-handler-fixture "${HANDLER_FIXTURE_DIR}"
    DEPENDS tests/test_pipeline.py tools/generate_assets.py ${ASSET_INPUTS}
    VERBATIM)
add_custom_target(rpg_handler_fixture_assets DEPENDS ${HANDLER_FIXTURE_OUTPUTS})
add_library(rpg_handler_fixture OBJECT src/chunk_dispatch.cpp)
add_dependencies(rpg_handler_fixture rpg_handler_fixture_assets)
# The fixture's own generated headers come first. The sound catalog is shared, so it comes last.
target_include_directories(rpg_handler_fixture PRIVATE src "${HANDLER_FIXTURE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/generated")
add_dependencies(rpg_world_tests rpg_handler_fixture)

if(TARGET rpg_device_tests)
    add_dependencies(rpg_device_tests rpg_assets)
    add_dependencies(rpg_device_tests rpg_music_catalog)
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
    set(MUSIC_OUTPUTS ${MUSIC_CATALOG_OUTPUTS})
    foreach(score IN LISTS MUSIC_SCORES)
        get_filename_component(song "${score}" NAME_WLE)
        string(REGEX REPLACE "[^A-Za-z0-9_]" "_" symbol "${song}")
        string(TOUPPER "MUSIC_${symbol}" symbol)
        set(output "${CMAKE_CURRENT_SOURCE_DIR}/generated/music_${song}.h")
        add_custom_command(OUTPUT "${output}"
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/compile_music.py"
                --compiler "${RPG_MUSIC_COMPILER}" "${score}" "${output}" --name "${symbol}_DATA"
            DEPENDS "${score}" "${RPG_MUSIC_COMPILER}" tools/compile_music.py
            VERBATIM)
        list(APPEND MUSIC_OUTPUTS "${output}")
    endforeach()
    # Every assets/sfx/*.avms effect compiles into one header, with its ID in another.
    file(GLOB SFX_SCORES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/sfx/*.avms")
    # A changed file list must regenerate the effects even when a score was removed.
    set(SFX_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/sfx_sources.txt")
    file(GENERATE OUTPUT "${SFX_MANIFEST}" CONTENT "${SFX_SCORES}\n")
    set(SFX_OUTPUTS
        "${CMAKE_CURRENT_SOURCE_DIR}/generated/sfx_ids.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/generated/sfx_data.h")
    add_custom_command(OUTPUT ${SFX_OUTPUTS}
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/compile_sfx.py"
            --compiler "${RPG_MUSIC_COMPILER}"
            --source-dir "${CMAKE_CURRENT_SOURCE_DIR}/assets/sfx"
            --output-dir "${CMAKE_CURRENT_SOURCE_DIR}/generated"
        DEPENDS tools/compile_sfx.py "${RPG_MUSIC_COMPILER}" "${SFX_MANIFEST}" ${SFX_SCORES}
        VERBATIM)
    list(APPEND MUSIC_OUTPUTS ${SFX_OUTPUTS})
    add_custom_target(rpg_music DEPENDS ${MUSIC_OUTPUTS})
    add_dependencies(rpg_music rpg_music_catalog)
    if(TARGET rpg_device_tests)
        add_dependencies(rpg_device_tests rpg_music)
        add_dependencies(rpg_audio_timing_tests rpg_music)
    endif()
    function(guest name)
        set(objects)
        foreach(source IN LISTS ARGN)
            get_filename_component(stem "${source}" NAME_WE)
            set(object "${CMAKE_CURRENT_BINARY_DIR}/${name}/${stem}.o")
            set(defines -DRPG_AVM=1)
            if(name STREQUAL "rpg_qa")
                list(APPEND defines -DRPG_DEVICE_QA=1)
            endif()
            # The battle probe reads the search's work counters; other guests compile them out.
            if(name STREQUAL "m17_battle_probe")
                list(APPEND defines -DRPG_BATTLE_STATS=1)
            endif()
            set(source_dependencies)
            if(source STREQUAL "src/platform_avm.cpp")
                list(APPEND source_dependencies src/music.h src/music_catalog.h src/sfx.h ${MUSIC_OUTPUTS})
            endif()
            if(source STREQUAL "src/battle_screen.cpp" OR source STREQUAL "src/dialogue_overlay.cpp")
                list(APPEND source_dependencies src/sfx.h ${SFX_OUTPUTS}
                    src/music_catalog.h ${MUSIC_CATALOG_OUTPUTS})
            endif()
            add_custom_command(OUTPUT "${object}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/${name}"
                COMMAND "${AVM_CLANG}" -std=c++20 -O2 -g -flto=full -fno-discard-value-names -Wall -Wextra -Werror
                    ${defines} -I "${CMAKE_CURRENT_SOURCE_DIR}/src" -I "${CMAKE_CURRENT_SOURCE_DIR}/generated"
                    -c "${CMAKE_CURRENT_SOURCE_DIR}/${source}" -o "${object}"
                DEPENDS "${source}" src/rpg.h src/perks.def src/world_map.h src/ambient.h src/sfx.h src/soundscape.h ${SFX_OUTPUTS} src/title.h src/flags.h src/world_types.h src/static_chunk_dispatch.h src/npc_policy.h src/encounter.h src/chunk.h src/npc.h src/npc_types.h src/actor_draw_order.h src/battle.h src/battle_ai.h src/battle_motion.h src/battle_cursor.h src/journal.h src/text_wrap.h src/party_ui.h src/equipment.h src/equipment_ids.h src/supplies.h src/supplies_ui.h src/shop.h src/shop_ui.h src/pause_menu.h ${LOGIC_HEADERS} ${QUEST_HEADERS} tests/npc_scenarios.h tests/world_scenarios.h tests/battle_fixtures.h ${ASSET_OUTPUTS} ${source_dependencies} ${SDK_DEPS} "${AVM_CLANG}"
                VERBATIM)
            list(APPEND objects "${object}")
        endforeach()
        set(elf "${CMAKE_CURRENT_BINARY_DIR}/artifacts/${name}.elf")
        set(bin "${CMAKE_CURRENT_BINARY_DIR}/artifacts/${name}.bin")
        set(package "${CMAKE_CURRENT_BINARY_DIR}/artifacts/${name}.arduboy")
        set(fxc_options)
        set(fxc_outputs)
        set(fxc_dependencies)
        if(name STREQUAL "rpg" OR name STREQUAL "rpg_qa")
            set(fxc_outputs "${CMAKE_CURRENT_BINARY_DIR}/artifacts/${name}.fxc.arduboy")
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
            add_custom_target(${name} ALL DEPENDS "${package}" ${fxc_outputs} "${elf}.resources.json")
        endif()
        add_dependencies(${name} rpg_assets)
    endfunction()
    # Verify that identity growth needs neither portrait rows nor a Party name array.
    set(SCALING_UI_OBJECTS)
    foreach(source IN ITEMS src/render.cpp src/dialogue_overlay.cpp generated/sprites.cpp)
        get_filename_component(stem "${source}" NAME_WE)
        set(object "${CMAKE_CURRENT_BINARY_DIR}/scaling-ui/${stem}.o")
        add_custom_command(OUTPUT "${object}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/scaling-ui"
            COMMAND "${AVM_CLANG}" -std=c++20 -O2 -g -flto=full -Wall -Wextra -Werror
                -DRPG_AVM=1 -DRPG_NPC_ID_LIMIT=65 -DRPG_DISABLE_ENEMY_SPAWNS
                -I "${CMAKE_CURRENT_SOURCE_DIR}/src" -I "${CMAKE_CURRENT_SOURCE_DIR}/generated"
                -c "${CMAKE_CURRENT_SOURCE_DIR}/${source}" -o "${object}"
            DEPENDS "${source}" src/rpg.h src/perks.def src/npc.h src/npc_types.h src/npc_ids.h src/journal.h src/text_wrap.h src/party_ui.h src/equipment.h src/equipment_ids.h src/supplies.h src/supplies_ui.h src/shop.h src/shop_ui.h src/pause_menu.h ${QUEST_HEADERS}
                ${ASSET_OUTPUTS} ${MUSIC_OUTPUTS} ${SDK_DEPS} "${AVM_CLANG}"
            VERBATIM)
        list(APPEND SCALING_UI_OBJECTS "${object}")
    endforeach()
    add_custom_target(rpg_scaling_ui_fixture ALL DEPENDS ${SCALING_UI_OBJECTS})
    add_dependencies(rpg_scaling_ui_fixture rpg_assets rpg_music)

    guest(m0_proof src/proof.cpp)
    guest(m15_probe ${RULES} tests/world_probe.cpp)
    guest(m16_probe ${RULES} tests/npc_probe.cpp)
    guest(m17_battle_probe ${RULES} src/battle_thinking.cpp tests/battle_probe.cpp)
    set(PLATFORM src/supplies_ui.cpp src/shop_ui.cpp src/input.cpp src/screens.cpp src/dialogue_overlay.cpp src/battle_screen.cpp src/battle_thinking.cpp src/render.cpp src/party_ui.cpp src/platform_avm.cpp generated/sprites.cpp)
    guest(rpg ${RULES} ${PLATFORM})
    add_dependencies(rpg rpg_music)
    # The shipping guest has no markers. Device QA reads this same logic with markers enabled.
    guest(rpg_qa ${RULES} ${PLATFORM})
    add_dependencies(rpg_qa rpg_music)
    if(TARGET rpg_audio_timing_tests)
        add_custom_target(rpg_audio_timing
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/measure_audio_timing.py"
                --device "$<TARGET_FILE:rpg_audio_timing_tests>"
                --build "${CMAKE_CURRENT_BINARY_DIR}"
                --interpreter-build "${AVM_BUILD_ROOT}/interp"
                --fxc-interpreter-build "${AVM_BUILD_ROOT}/interp/fxc"
                --seconds 40 --fxc-seconds 8
            DEPENDS rpg_qa rpg_audio_timing_tests VERBATIM)
    endif()
    if(EXISTS "${AVM_LLDB}")
        foreach(suite IN ITEMS rpg_integration rpg_integration_full)
            set(suite_options)
            if(suite STREQUAL "rpg_integration_full")
                list(APPEND suite_options --full)
            endif()
            add_custom_target(${suite}
                COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/guest_integration.py"
                    --sdk "${AVM_SDK_ROOT}" --build "${CMAKE_CURRENT_BINARY_DIR}" ${suite_options}
                DEPENDS rpg rpg_qa m0_proof m15_probe m16_probe USES_TERMINAL VERBATIM)
            if(TARGET rpg_device_tests)
                add_dependencies(${suite} rpg_device_tests)
            endif()
        endforeach()
    endif()
endif()
