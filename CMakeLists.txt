cmake_minimum_required(VERSION 3.22)

#
# STM32CubeMX generates this file only once and does not overwrite it on later
# code generations. Project specific build logic (user sources, post-build
# artifacts, ...) lives here. The STM32CubeMX managed part stays in
# cmake/stm32cubemx/CMakeLists.txt and is regenerated, run
# `python3 .scripts/patch_cubemx` after every CubeMX generation.
#

# Setup compiler settings
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# C++ standard for the user modules. Kept here (not in the CubeMX-generated
# toolchain file) so a CubeMX regeneration cannot reset it.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME Concentration_V2)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# User sources (not managed by STM32CubeMX)
# 用户模块已头文件化（定义与声明一体，头文件仅保留常量/类定义）；
# App.cpp 是唯一的 C++ 编译单元。
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src/app/App.cpp
)

# User include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Inc
    Core/Inc/app
)

# User symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
)

# User modules are C++; link the image with the C++ driver so C++ runtime
# helpers resolve (exceptions/RTTI stay disabled, see the toolchain file).
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

# Generate the binary artifacts consumed by flashing/debugging tooling
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    COMMENT "Generating ${CMAKE_PROJECT_NAME}.hex and ${CMAKE_PROJECT_NAME}.bin"
)
