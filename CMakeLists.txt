# Red Dead Revolver (original Xbox), statically recompiled

cmake_minimum_required(VERSION 3.25)
project(rdr LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# the SDK: sdk/ here, two folders up inside the SDK's templates, a checkout beside this repository, else downloaded
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/sdk/cmake/XbeRecomp.cmake)
    set(SDK ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/XbeRecomp.cmake)
    set(SDK ${CMAKE_CURRENT_SOURCE_DIR}/../..)
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../XboxXBE-Recomp/cmake/XbeRecomp.cmake)
    set(SDK ${CMAKE_CURRENT_SOURCE_DIR}/../XboxXBE-Recomp)
else()
    include(FetchContent)
    FetchContent_Declare(xbe_sdk GIT_REPOSITORY https://github.com/TGP482/XboxXBE-Recomp.git GIT_TAG main)
    FetchContent_MakeAvailable(xbe_sdk)
    set(SDK ${xbe_sdk_SOURCE_DIR})
endif()
include(${SDK}/cmake/XbeRecomp.cmake)

file(GLOB XBE_HOST_SOURCES CONFIGURE_DEPENDS src/*/*.cpp)
list(FILTER XBE_HOST_SOURCES EXCLUDE REGEX "/src/game/")
# the executable's version and product name
list(APPEND XBE_HOST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/app/version.rc)
set_source_files_properties(src/app/version.rc PROPERTIES INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/src)
# the game builds its own Direct3D, renderer, render thread, shader, DirectSound, audio output, input and game code
# window copies
list(FILTER XBE_RUNTIME_SOURCES EXCLUDE REGEX "/(xdk/dsound|xdk/d3d8|gpu/d3d12|gpu/render_thread|gpu/shaders|audio/xaudio2|core/input|app/game_code)\\.cpp$")
# SDL's headers go to the input, gyro and compiler window code alone (it is linked LINK_ONLY)
set_source_files_properties(src/core/input.cpp src/core/gyro.cpp src/app/game_code.cpp
                            PROPERTIES INCLUDE_DIRECTORIES "${sdl3_SOURCE_DIR}/include")

# the release in assets builds with its own config, the one whose entry point is the xbe's (retail key)
set(GAME_CONFIG game.toml)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets/default.xbe)
    file(READ assets/default.xbe XBE_ENTRY OFFSET 296 LIMIT 4 HEX)
    string(REGEX REPLACE "(..)(..)(..)(..)" "0x\\4\\3\\2\\1" XBE_ENTRY "${XBE_ENTRY}")
    math(EXPR XBE_ENTRY "${XBE_ENTRY} ^ 0xA8FC57AB")
    file(GLOB RELEASE_CONFIGS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/config CONFIGURE_DEPENDS config/*.toml)
    foreach(config ${RELEASE_CONFIGS})
        file(STRINGS config/${config} CONFIG_ENTRY REGEX "^entry *=")
        string(REGEX MATCH "0x[0-9A-Fa-f]+" CONFIG_ENTRY "${CONFIG_ENTRY}")
        if(CONFIG_ENTRY)
            math(EXPR CONFIG_ENTRY "${CONFIG_ENTRY}")
            if(CONFIG_ENTRY EQUAL XBE_ENTRY)
                set(GAME_CONFIG ${config})
            endif()
        endif()
    endforeach()
endif()
message(STATUS "Game config: ${GAME_CONFIG}")

# partitions as xbe_symbols wrote them into the config
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS config/${GAME_CONFIG})
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/config/${GAME_CONFIG})
    file(STRINGS config/${GAME_CONFIG} GAME_PARTITIONS REGEX "^partitions *=")
    string(REGEX MATCH "[0-9]+" GAME_PARTITIONS "${GAME_PARTITIONS}")
endif()
# no config yet: build.bat writes it with xbe_symbols, the change reconfigures
if(NOT GAME_PARTITIONS)
    set(GAME_PARTITIONS 8)
endif()

# target name = GAME_EXE in build.bat, xbe = GAME_XBE
add_xbe_image(RDR default.xbe ${GAME_CONFIG} ${GAME_PARTITIONS} game)
