cmake_minimum_required(VERSION 3.16)

if (POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

project(HFTDeribit CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------
# Dependencies
# -----------------------------------------
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(simdjson REQUIRED)

# -----------------------------------------
# Include directory
# -----------------------------------------
include_directories(${PROJECT_SOURCE_DIR}/include)

# =========================================
# CORE LIBRARY (src/)
# =========================================

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
        ${PROJECT_SOURCE_DIR}/src/*.cpp
)

add_library(deribit_core ${CORE_SOURCES})

target_link_libraries(deribit_core
        PUBLIC
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        simdjson
)

target_include_directories(deribit_core
        PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# =========================================
# TEST EXECUTABLES (tests/)
# =========================================

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
        ${PROJECT_SOURCE_DIR}/tests/*.cpp
)

foreach(TEST_FILE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)

    add_executable(${TEST_NAME} ${TEST_FILE})

    target_link_libraries(${TEST_NAME}
            PRIVATE
            deribit_core
    )
endforeach()
