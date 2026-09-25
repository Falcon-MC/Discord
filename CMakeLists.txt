cmake_minimum_required(VERSION 3.21)
project(FalconDiscord LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(BUILD_VOICE_SUPPORT OFF CACHE BOOL "" FORCE)
set(DPP_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(DPP_INSTALL OFF CACHE BOOL "" FORCE)
set(RUN_LDCONFIG OFF CACHE BOOL "" FORCE)
set(DPP_NO_VCPKG ON CACHE BOOL "" FORCE)
set(DPP_NO_CONAN ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    dpp
    GIT_REPOSITORY https://github.com/brainboxdotcc/DPP.git
    GIT_TAG v10.1.6
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(dpp)

file(GLOB_RECURSE FALCON_DISCORD_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE FALCON_DISCORD_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

add_executable(FalconDiscord ${FALCON_DISCORD_SOURCES} ${FALCON_DISCORD_HEADERS})
target_include_directories(FalconDiscord PRIVATE include)
find_package(Threads REQUIRED)
target_link_libraries(FalconDiscord PRIVATE dpp Threads::Threads ${CMAKE_DL_LIBS})

if(WIN32)
    file(GLOB DPP_RUNTIME_DLLS ${dpp_SOURCE_DIR}/win32/bin/*.dll)
    add_custom_command(TARGET FalconDiscord POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:FalconDiscord> ${DPP_RUNTIME_DLLS} $<TARGET_FILE_DIR:FalconDiscord>
        COMMAND_EXPAND_LISTS
    )
endif()
