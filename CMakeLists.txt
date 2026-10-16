cmake_minimum_required(VERSION 3.20)
project(avm_rpg LANGUAGES CXX)
include(CTest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CMAKE_CXX_STANDARD 17)
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
    add_executable(rpg_device_tests tests/device_tests.cpp)
    target_compile_features(rpg_device_tests PRIVATE cxx_std_17)
    target_include_directories(rpg_device_tests PRIVATE src
        "${AVM_SOURCE_ROOT}/deps/Ardens/src"
        "${AVM_SOURCE_ROOT}/deps/Ardens/deps"
        "${AVM_SOURCE_ROOT}/deps/Ardens/deps/bitsery/include")
    if(MSVC)
        target_compile_options(rpg_device_tests PRIVATE /Zc:__cplusplus)
        target_link_libraries(rpg_device_tests PRIVATE
            "${AVM_BUILD_ROOT}/ardens-build/ardensdebuggerlib.lib"
            "${AVM_BUILD_ROOT}/ardens-build/deps/fmt/fmt.lib"
            "${AVM_BUILD_ROOT}/ardens-build/deps/miniz/miniz.lib")
    else()
        target_link_libraries(rpg_device_tests PRIVATE
            "${AVM_BUILD_ROOT}/ardens-build/libardensdebuggerlib.a"
            "${AVM_BUILD_ROOT}/ardens-build/deps/fmt/libfmt.a"
            "${AVM_BUILD_ROOT}/ardens-build/deps/miniz/libminiz.a")
    endif()
endif()
set(AVM_SDK_ROOT "" CACHE PATH "Installed AVM SDK")
option(RPG_BUILD_GUEST "Build guest programs using installed SDK" ON)
set(RULES src/game.cpp src/world.cpp src/dialogue.cpp src/save.cpp)
add_library(rpg_rules STATIC ${RULES})
target_include_directories(rpg_rules PUBLIC src PRIVATE generated)
if(MSVC)
    target_compile_options(rpg_rules PRIVATE /W4)
else()
    target_compile_options(rpg_rules PRIVATE -Wall -Wextra)
endif()
add_executable(rpg_tests tests/test_core.cpp)
target_link_libraries(rpg_tests PRIVATE rpg_rules)
target_include_directories(rpg_tests PRIVATE generated)
add_executable(rpg_stress_tests tests/test_core.cpp ${RULES})
target_include_directories(rpg_stress_tests PRIVATE src generated)
target_compile_definitions(rpg_stress_tests PRIVATE RPG_STRESS=1)
add_test(NAME core COMMAND rpg_tests)
add_test(NAME twelve_npcs COMMAND rpg_stress_tests)
add_test(NAME generated_assets COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_assets.py --check)
add_test(NAME content COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_content.py)
add_custom_target(rpg_assets
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_assets.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} VERBATIM)

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
    foreach(required IN ITEMS "${AVM_CLANG}" "${AVM_IMAGE}" "${AVM_SDK_ROOT}/sysroot/include/avm.h" "${AVM_SDK_ROOT}/bin/avm/interp.hex")
        if(NOT EXISTS "${required}")
            message(FATAL_ERROR "Missing SDK prerequisite: ${required}")
        endif()
    endforeach()
    file(GLOB_RECURSE SDK_DEPS CONFIGURE_DEPENDS "${AVM_SDK_ROOT}/sysroot/include/*" "${AVM_SDK_ROOT}/sysroot/lib/*")
    function(guest name)
        set(objects)
        foreach(source IN LISTS ARGN)
            get_filename_component(stem "${source}" NAME_WE)
            set(object "${CMAKE_CURRENT_BINARY_DIR}/${name}/${stem}.o")
            set(defines -DRPG_AVM=1)
            if(name STREQUAL "rpg_stress")
                list(APPEND defines -DRPG_STRESS=1)
            endif()
            add_custom_command(OUTPUT "${object}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/${name}"
                COMMAND "${AVM_CLANG}" -std=c++17 -O2 -g -flto=full -fno-discard-value-names -Wall -Wextra -Werror
                    ${defines} -I "${CMAKE_CURRENT_SOURCE_DIR}/src" -I "${CMAKE_CURRENT_SOURCE_DIR}/generated"
                    -c "${CMAKE_CURRENT_SOURCE_DIR}/${source}" -o "${object}"
                DEPENDS "${source}" src/rpg.h generated/world_assets.h ${SDK_DEPS} "${AVM_CLANG}"
                VERBATIM)
            list(APPEND objects "${object}")
        endforeach()
        set(elf "${CMAKE_CURRENT_BINARY_DIR}/artifacts/${name}.elf")
        set(bin "${CMAKE_CURRENT_BINARY_DIR}/artifacts/${name}.bin")
        set(package "${CMAKE_CURRENT_BINARY_DIR}/artifacts/${name}.arduboy")
        add_custom_command(OUTPUT "${elf}" "${bin}" "${package}" "${elf}.resources.json" "${elf}.lto.json"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/artifacts"
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/link_guest.py"
                --compiler "${AVM_CLANG}" --image "${AVM_IMAGE}" --output "${elf}" ${objects}
            DEPENDS ${objects} tools/link_guest.py tools/resources.py tools/check_lto.py "${AVM_IMAGE}" "${AVM_SDK_ROOT}/bin/avm/interp.hex"
            VERBATIM)
        if(name STREQUAL "rpg")
            # Copy rather than declare an output: CMake clean preserves the playable package.
            add_custom_target(${name} ALL
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${package}" "${CMAKE_CURRENT_SOURCE_DIR}/rpg.arduboy"
                DEPENDS "${package}" "${elf}.resources.json" VERBATIM)
        else()
            add_custom_target(${name} ALL DEPENDS "${package}" "${elf}.resources.json")
        endif()
    endfunction()
    guest(m0_proof src/proof.cpp)
    set(PLATFORM src/input.cpp src/screens.cpp src/dialogue_overlay.cpp src/render.cpp src/platform_avm.cpp generated/sprites.cpp)
    guest(rpg ${RULES} ${PLATFORM})
    guest(rpg_stress ${RULES} ${PLATFORM})
    if(EXISTS "${AVM_LLDB}")
        add_custom_target(rpg_integration
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/guest_integration.py"
                --sdk "${AVM_SDK_ROOT}" --build "${CMAKE_CURRENT_BINARY_DIR}"
            DEPENDS rpg m0_proof rpg_stress VERBATIM)
        if(TARGET rpg_device_tests)
            add_dependencies(rpg_integration rpg_device_tests)
        endif()
    endif()
endif()
