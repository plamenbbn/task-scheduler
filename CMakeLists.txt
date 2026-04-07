cmake_minimum_required(VERSION 3.21)
project(moo_task_scheduler VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MOO_BUILD_TESTS "Build unit tests" OFF)

add_library(moo_core
    src/MooScheduler.cpp
    src/TaskRegistry.cpp
    src/ThreadPool.cpp
    src/optimizer/NSGA2.cpp
)

 target_include_directories(moo_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
 )

add_executable(moo_demo src/main.cpp)
target_link_libraries(moo_demo PRIVATE moo_core)

if(MOO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
