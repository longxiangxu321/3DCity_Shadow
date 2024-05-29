cmake_minimum_required(VERSION 3.20)
project(shadow_calculation)

set(CMAKE_CXX_STANDARD 17)
find_package(OpenMP REQUIRED)

file(GLOB DEFINITIONS "include/*.h" "include/*.hpp")

add_executable(shadow_calculation
        src/shadow_calculation.cpp
        src/IO.h
        src/IO.cpp
        src/sample_pointGrid.h
        src/sample_pointGrid.cpp
        ${DEFINITIONS})

target_include_directories(shadow_calculation PRIVATE .)
target_link_libraries(shadow_calculation PRIVATE OpenMP::OpenMP_CXX)

add_executable(svf_calculation
        src/svf_calculation.cpp
        src/IO.h
        src/IO.cpp
        src/sample_pointGrid.h
        src/sample_pointGrid.cpp
        ${DEFINITIONS})

target_include_directories(svf_calculation PRIVATE .)
target_link_libraries(svf_calculation PRIVATE OpenMP::OpenMP_CXX)