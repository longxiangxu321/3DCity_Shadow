#ifndef PG_H
#define PG_H

#include <sstream>
#include <vector>
#include <fstream>
#include "include/Triangle.h"
#include "include/vec3.h"
#include "include/grid_point.h"
#include "include/json.hpp"
using json = nlohmann::json;

namespace sampling {
    void calculate_mass_center(const vec3 &A, const vec3 &B, const vec3 &C, const vec3 &direction, const int splits,
                            std::stringstream &output_stream, std::vector<GridPoint> &grid_n, int current_depth = 0,
                            const int &max_depth = 10, const std::shared_ptr<std::string> &surf_gmlid_ptr = nullptr);

    std::vector<GridPoint> create_point_grid(const std::vector<std::vector<Triangle>>& mesh, const json& CFG);
    }


#endif