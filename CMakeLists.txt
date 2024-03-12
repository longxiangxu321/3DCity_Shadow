cmake_minimum_required(VERSION 3.20)
project(shadow_calculation)

set(CMAKE_CXX_STANDARD 17)
find_package(OpenMP REQUIRED)

add_executable(shadow_calculation
        point_grid.cpp
        include/aabb.h
        include/bvh.h
        include/hittable.h
        include/hittable_list.h
        include/ray.h
        include/rtweekend.h
        include/sphere.h
        include/Triangle.h
        include/vec3.h
        include/grid_point.h
        include/json.hpp)

target_include_directories(shadow_calculation PRIVATE .)
target_link_libraries(shadow_calculation PRIVATE OpenMP::OpenMP_CXX)