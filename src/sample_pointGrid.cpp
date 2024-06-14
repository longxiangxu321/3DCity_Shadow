#include "sample_pointGrid.h"
#include "include/rtweekend.h"

std::vector<vec3> sampling::hemisphere_sampling(const vec3 &direction, const float seperation_in_degrees) {
    // Precompute the number of samples
    float seperation = degrees_to_radians(seperation_in_degrees);
    int num_theta = static_cast<int>(2 * pi / seperation);
    int num_phi = static_cast<int>(pi / 2 / seperation);
    std::vector<vec3> directions(num_theta * num_phi);

    vec3 normal = unit_vector(direction);
    vec3 tangent = unit_vector(cross(normal, vec3(0, 0, 1)));
    vec3 bitangent = unit_vector(cross(normal, tangent));

    // Use OpenMP to parallelize the loop
    // #pragma omp parallel for collapse(2)
    for (int i = 0; i < num_theta; ++i) {
        for (int j = 0; j < num_phi; ++j) {
            float theta = i * seperation;
            float phi = j * seperation;

            float x = sin(phi) * cos(theta);
            float y = sin(phi) * sin(theta);
            float z = cos(phi);
            vec3 dir = x * tangent + y * bitangent + z * normal;

            directions[i * num_phi + j] = unit_vector(dir); // Ensure the vector is a unit vector
        }
    }

    return directions;
}

void sampling::calculate_mass_center(const vec3 &A, const vec3 &B, const vec3 &C, const vec3 &direction, const int splits,
                           std::stringstream &output_stream, std::vector<GridPoint> &grid_n, int current_depth,
                           const int &max_depth, const std::shared_ptr<std::string> &surf_gmlid_ptr)
{
//    vec3 sun_d(1,1,0.5);
    vec3 D,E,F;
    D = (A+B)/2;
    E = (B+C)/2;
    F = (C+A)/2;

    if (current_depth > max_depth) {
        return;  // prevent excessive recursion
    }

    if (splits == 0) {
        vec3 vo = (A + B + C) /3;
        if (!vo.x() || !vo.y() || !vo.z()) return;
        output_stream << std::setprecision(10) << vo.x() + direction.x() * 0.1 << " " << vo.y() + direction.y() * 0.1 << " " << vo.z() + direction.z() * 0.1<< " "<< direction.x() << " "<<
        direction.y() << " "<< direction.z() <<"\n";
//        output_stream << std::setprecision(10) << vo.x() + sun_d.x() * 2 << " " << vo.y() + sun_d.y() * 2 << " " << vo.z() + sun_d.z() * 2<<"\n";
        vec3 position = vec3(vo.x() + direction.x() * 0.1, vo.y() + direction.y() * 0.1, vo.z() + direction.z() * 0.1);
        vec3 normal = vec3(direction.x(), direction.y(), direction.z());
        GridPoint gp(position + direction*0.001, normal);

        // std::vector<vec3> hemisphere_samples = hemisphere_sampling(gp.normal, 5);
        // gp.hemisphere_samples = hemisphere_samples;
        gp.surf_gmlid = surf_gmlid_ptr;
        grid_n.emplace_back(gp);
//        return;
    }

    if (splits == -1) {

        if ( (vec_distance(A,B)/ vec_distance(B,C))>10 || (vec_distance(A,C)/ vec_distance(B,C))>10) {
            calculate_mass_center(A, D, F, direction, 0, output_stream, grid_n, current_depth + 1, max_depth,
                                  surf_gmlid_ptr);
        }
        if ( (vec_distance(A,B)/ vec_distance(A,C)) >10 || (vec_distance(B,C)/ vec_distance(A,C))>10) {
            calculate_mass_center(D, B, E, direction, 0, output_stream, grid_n, current_depth + 1, max_depth,
                                  surf_gmlid_ptr);
        }
        if ( (vec_distance(A,C)/ vec_distance(A,B)) >10 || (vec_distance(B,C)/ vec_distance(A,B))>10) {
            calculate_mass_center(F, E, C, direction, 0, output_stream, grid_n, current_depth + 1, max_depth,
                                  surf_gmlid_ptr);
        }
        return;
    }

    calculate_mass_center(A, D, F, direction, splits - 1, output_stream, grid_n, current_depth + 1, max_depth,
                          surf_gmlid_ptr);
    calculate_mass_center(D, B, E, direction, splits - 1, output_stream, grid_n, current_depth + 1, max_depth,
                          surf_gmlid_ptr);
    calculate_mass_center(F, E, C, direction, splits - 1, output_stream, grid_n, current_depth + 1, max_depth,
                          surf_gmlid_ptr);
    calculate_mass_center(D, E, F, direction, splits - 1, output_stream, grid_n, current_depth + 1, max_depth,
                          surf_gmlid_ptr);
}


std::vector<GridPoint> sampling::create_point_grid(const std::vector<std::vector<Triangle>>& mesh, const json& CFG) {
//    std::string output_file = "../data/grid.xyz";
    std::string output_file = CFG["shadow_calc"]["pointgrid_path"];
    std::ofstream out(output_file);



    std::vector<GridPoint> grid_current;

    if (!out.is_open()) {
        std::cout << "Error opening file " << "'" << output_file <<"'.";
    }

    else
    {
        std::stringstream ss;
        std::stringstream specification_stream;
        specification_stream<<"grid_start"<<" "<<"grid_end"<<"\n";
        for (auto it = mesh.begin(); it!=mesh.end(); ++it) {
            for (auto jt = it->begin(); jt!=it->end(); ++jt) {
                specification_stream<<grid_current.size()<<" ";
                int num_s;
                if (jt->area() <=2) num_s = 0;
                else if (jt->area() <4 && jt-> area() >2) num_s = 1;
                else
                {
                    double num_half_square = jt->area();
                    num_s = std::floor(std::log(num_half_square/8+1) / std::log(3));
                }

                if (jt->surf_gmlid == nullptr) {
                    // Pointer is nullptr, raise an error
                    std::cerr << "Error: gmlidPtr is nullptr!" << std::endl;
                } else {
                    // Pointer is not nullptr, you can safely use it
                    //                calculate_mass_center(jt->v0, jt->v1, jt->v2, unit_vector(jt->normal), num_s, ss, grid_current, 0, num_s+2);
                    calculate_mass_center(jt->v0, jt->v1, jt->v2, unit_vector(jt->normal), num_s, ss, grid_current, 0, 3,
                                          jt->surf_gmlid);
                    specification_stream<<grid_current.size() - 1<<"\n";
                }
            }
        }

        out << ss.str();
    }
    return grid_current;
}