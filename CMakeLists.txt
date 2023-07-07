cmake_minimum_required(VERSION 3.20)
project(refer)

set(CMAKE_CXX_STANDARD 17)


add_executable(refer
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
        include/grid_point.h)

target_include_directories(refer PRIVATE .)