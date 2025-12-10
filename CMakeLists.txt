cmake_minimum_required(VERSION 3.16)

# Fix Boost policy for new CMake versions
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

# -----------------------------------------
# Common include directory
# -----------------------------------------
include_directories(${PROJECT_SOURCE_DIR}/include)

# -----------------------------------------
# Collect all .cpp files in src/
# -----------------------------------------
file(GLOB APP_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.cpp)

# -----------------------------------------
# Create one executable for each .cpp file
# -----------------------------------------
foreach(APP_FILE ${APP_SOURCES})
    get_filename_component(APP_NAME ${APP_FILE} NAME_WE)

    add_executable(${APP_NAME} ${APP_FILE})

    # Link required libraries
    target_link_libraries(${APP_NAME}
            PRIVATE
            Boost::system
            OpenSSL::SSL
            OpenSSL::Crypto
    )

    # IDE include paths
    target_include_directories(${APP_NAME}
            PRIVATE
            ${PROJECT_SOURCE_DIR}/include
    )
endforeach()
