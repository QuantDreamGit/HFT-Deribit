cmake_minimum_required(VERSION 3.16)
project(HFT_Deribit LANGUAGES CXX)

# Use modern C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------------------
# Find spdlog (use vcpkg, system package, or Conan)
# ------------------------------------------------------------------------------
find_package(spdlog REQUIRED)

# ------------------------------------------------------------------------------
# Header-only "deribit" library
# ------------------------------------------------------------------------------
add_library(deribit INTERFACE)

# Include directory for header-only library
target_include_directories(deribit INTERFACE
        ${PROJECT_SOURCE_DIR}/include
)

# Link spdlog into interface target
target_link_libraries(deribit INTERFACE spdlog::spdlog)

# ------------------------------------------------------------------------------
# Add executables from src/
# ------------------------------------------------------------------------------
file(GLOB APP_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.cpp)

foreach(APP_FILE ${APP_SOURCES})
    get_filename_component(APP_NAME ${APP_FILE} NAME_WE)

    add_executable(${APP_NAME} ${APP_FILE})

    # Link header-only lib + spdlog
    target_link_libraries(${APP_NAME} PRIVATE deribit)

    # Nice for CLion / IDE autocomplete
    target_include_directories(${APP_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)
endforeach()
