cmake_minimum_required(VERSION 3.15)
project(resize_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(OpenCV 4.0 REQUIRED 
    COMPONENTS 
        core 
        imgcodecs 
        imgproc
)

if(NOT OpenCV_FOUND)
    message(FATAL_ERROR "OpenCV not found. Please install OpenCV 4.0 or later.")
endif()

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(libasyik REQUIRED)
find_package(Boost COMPONENTS context fiber url date_time REQUIRED)
find_package(SOCI REQUIRED)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(resize_server src/main.cpp)

target_link_libraries(resize_server
    PRIVATE
        libasyik
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
        ${OpenCV_LIBS}
        Boost::fiber 
        Boost::context
        Boost::url
        Boost::date_time
        nlohmann_json::nlohmann_json
)

target_include_directories(resize_server
    PRIVATE
        ${OpenCV_INCLUDE_DIRS}
        ${libasyik_INCLUDE_DIR}
        ${Boost_INCLUDE_DIR}

)

install(TARGETS resize_server
    RUNTIME DESTINATION bin
)

# # Option to build tests
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()
    
    find_package(Boost REQUIRED COMPONENTS)
    add_subdirectory(external/catch2)
    
    add_executable(test_resize_server tests/test_resizer.cpp)
    
    target_link_libraries(test_resize_server
        PRIVATE
            Catch2::Catch2WithMain
            ${OpenCV_LIBS}
    )
    
    target_include_directories(test_resize_server
        PRIVATE
            ${OpenCV_INCLUDE_DIRS}
    )
    
    include(CTest)
    include(Catch)
    add_test(NAME ResizeServerTest COMMAND test_resize_server)
    
    message(STATUS "Unit tests enabled")
endif()

# Print configuration summary
message(STATUS "========================================")
message(STATUS "Build Configuration Summary")
message(STATUS "========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "Boost version: ${Boost_VERSION}")
# message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "========================================")