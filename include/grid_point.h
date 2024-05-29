//
// Created by 55241 on 2023/5/7.
//

#ifndef POINT_GRID_CPP_GRID_POINT_H
#define POINT_GRID_CPP_GRID_POINT_H
#include "vec3.h"

struct GridPoint {
    vec3 position;
    vec3 normal;
    std::shared_ptr<std::string> surf_gmlid;
    std::vector<vec3> hemisphere_samples;

    GridPoint() = default;
    GridPoint(const vec3& ori, const vec3 dir): position(ori), normal(dir) {}


};



#endif //POINT_GRID_CPP_GRID_POINT_H
