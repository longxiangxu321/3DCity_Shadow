//
// Created by 55241 on 2023/5/6.
//

#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <filesystem>

#include "include/Triangle.h"
#include "include/vec3.h"
#include "include/bvh.h"
#include "include/grid_point.h"


void calculate_mass_center(const vec3& A, const vec3& B, const vec3& C, const vec3&direction, const int splits, std::stringstream& output_stream,
    std::vector<GridPoint>& grid_n)
{
//    vec3 sun_d(1,1,0.5);
    if (splits == 0) {
        vec3 vo = (A + B + C) /3;
        if (!vo.x() || !vo.y() || !vo.z()) return;
        output_stream << std::setprecision(10) << vo.x() + direction.x() * 0.1 << " " << vo.y() + direction.y() * 0.1 << " " << vo.z() + direction.z() * 0.1<< " "<< direction.x() << " "<<
        direction.y() << " "<< direction.z() <<"\n";
//        output_stream << std::setprecision(10) << vo.x() + sun_d.x() * 2 << " " << vo.y() + sun_d.y() * 2 << " " << vo.z() + sun_d.z() * 2<<"\n";
        vec3 position = vec3(vo.x(), vo.y(), vo.z());
        vec3 normal = vec3(direction.x(), direction.y(), direction.z());
        GridPoint gp(position + direction*0.001, normal);
        grid_n.emplace_back(gp);
        return;
    }
    if (splits <0) {
        return;
    }
    vec3 D,E,F;
    D = (A+B)/2;
    E = (B+C)/2;
    F = (C+A)/2;
    calculate_mass_center(A, D, F, direction, splits - 1,  output_stream, grid_n);
    calculate_mass_center(D, B, E, direction, splits - 1, output_stream, grid_n);
    calculate_mass_center(F, E, C, direction, splits - 1, output_stream, grid_n);
    calculate_mass_center(D, E, F, direction, splits - 1, output_stream,grid_n);
}

std::vector<GridPoint> create_point_grid(const std::vector<std::vector<Triangle>>& mesh) {
    std::string output_file = "../data/grid.xyz";
    std::ofstream out(output_file);
    std::vector<GridPoint> grid_current;

    if (!out.is_open()) {
        std::cout << "Error opening file " << "'" << output_file << "'.";
    }
    else
    {
        std::stringstream ss;
        for (auto it = mesh.begin(); it!=mesh.end(); ++it) {
            for (auto jt = it->begin(); jt!=it->end(); ++jt) {
                int num_s;
                if (jt->area() <=2) num_s = 0;
                else if (jt->area() <4 && jt-> area() >2) num_s = 1;
                else
                {
                    double num_half_square = jt->area();
                    num_s = std::floor(std::log(num_half_square) / std::log(4));
                }
                calculate_mass_center(jt->v0, jt->v1, jt->v2, unit_vector(jt->normal), num_s, ss, grid_current);
            }
        }
        out << ss.str();
    }
    return grid_current;
}

std::vector<std::vector<Triangle>> read_mesh(const std::string &input_file){
    std::ifstream input_stream;
    input_stream.open(input_file);
    std::vector<std::vector<Triangle>> objects;
    std::vector<point3> points;
    std::vector<Triangle> current_object;

    bool first_object_read = false;
    int index = 1;

    if (input_stream.is_open()) {
        std::string line;
        while (std::getline(input_stream, line)) {
            if (line[0] == 'v') {
                double x,y,z;
                std::stringstream ss(line);
                std::string temp;
                ss >> temp >> x >> y >> z;
                points.emplace_back(x, y, z);
            }
            if (line[0] == 'o') {//every o means a new object
                if (!first_object_read) {
                    first_object_read = true;
                } else {
                    objects.push_back(current_object);
                    current_object.clear();
                    index++;
                }
            }
            if (line[0] == 'f') {//shell has different Faces
                unsigned long v0, v1, v2;
                std::stringstream ss(line);
                std::string temp;
                ss >> temp >> v0 >> v1 >> v2;
                point3 vx = points[v0-1];
                point3 vy = points[v1-1];
                point3 vz = points[v2-1];
                current_object.emplace_back(Triangle(vx, vy, vz,index));
            }
            else {
                continue;
            }
        }
        if (!current_object.empty()) {
            objects.push_back(current_object);
        }
    }

    return objects;
};

double calculate_shadow(const std::vector<vec3> &directions, const bvh_node &bvh, const std::vector<GridPoint> &point_grid) {
    std::vector<ray> rays;
    std::string out_file = "../data/result.xyz";
    std::ofstream out(out_file);
    std::stringstream ss;
    int i = 0;

    for (auto it = point_grid.begin(); it!=point_grid.end(); it++) {
        for (auto jt = directions.begin(); jt!=directions.end(); jt++) {
//            vec3 origin = it->position;
//            if (origin.x()>85134 && origin.x()<85135 && origin.y()>446347 &&
//            origin.y()< 446347.8 && origin.z() < -2 && origin.z() > -3)
//            {
//                ray r(origin, *jt);
//                rays.emplace_back(r);
//            }
            vec3 origin = it->position;
            ray r(origin, *jt);
            rays.emplace_back(r);
        }
    }
    std::cout<<"ray number "<<rays.size()<<std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    for (auto it = rays.begin(); it!= rays.end(); it++) {
        hit_record rec;
        if (bvh.hit(*it, 0, infinity, rec)) {
            ss <<std::setprecision(10) << it->origin().x() << " " << it->origin().y() << " " << it->origin().z() <<"\n";
            //            std::cout<<"hit"<<std::endl;
            i++;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
//    std::cout <<"time for one moment: " << duration << " milliseconds." << std::endl;
    std::cout<<"number of hit "<<i<<std::endl;
    out << ss.str();
    return duration;
}

int main() {

//    std::string in_dir = "../data/rotterdam/lod12";  // Path to the folder containing OBJ files
    std::string in_dir = "../data/rotterdam/lod";  // Path to the folder containing OBJ files
    vec3 sun_dir_n(0,0,1);
    vec3 sun_dir = unit_vector(sun_dir_n);
//    vec3 sun_dir2(1,1,3);
    std::vector<vec3> directions;
    directions.push_back(sun_dir);
    std::cout<<directions.size()<<std::endl;
    bool read_individual=true;
    if (read_individual) {
        for(const auto & entry : std::filesystem::directory_iterator(in_dir)) {
            if(entry.path().extension() == ".obj") {
                std::cout<<entry.path().filename()<<std::endl;
                std::cout<<" "<<std::endl;
                std::cout<<"file name "<<entry.path().string()<<std::endl;
                std::vector<std::vector<Triangle>> objects = read_mesh(entry.path().string());
                std::cout<<"building num "<<objects.size()<<std::endl;
                int totalTriangles = 0;
                for (const auto& obj : objects) {
                    totalTriangles += obj.size();
                }
                std::cout<<"total triangles "<< totalTriangles<<std::endl;
                std::vector<GridPoint> grid = create_point_grid(objects);
                std::cout<<"grid point number "<<grid.size()<<std::endl;
                std::cout<<"point grid created"<<std::endl;
                std::vector<std::shared_ptr<hittable>> triangles;
                hittable_list scene;
                int face_num =0;
                for (auto it = objects.begin(); it!=objects.end(); ++it) {
                    for (auto jt = it->begin(); jt != it->end(); ++jt) {
                        scene.add(std::make_shared<Triangle>(*jt));
                        face_num++;
                    }
                }

                auto start = std::chrono::high_resolution_clock::now();

                auto bvh = bvh_node(scene, 0, 1);
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                std::cout << "Time taken for bvh construction: " << duration << " milliseconds." << std::endl;
                double t = calculate_shadow(directions, bvh, grid);
                double result = t/directions.size();
                std::cout<<"average time for intersection test "<<result<<std::endl;
                std::cout<<" "<<std::endl;
            }
            else
                continue;
        }
        return 0;
    }
    else {
        std::vector<std::vector<Triangle>> objects;

        for(const auto & entry : std::filesystem::directory_iterator(in_dir)) {
            if(entry.path().extension() == ".obj") {
                std::vector<std::vector<Triangle>> obj1 = read_mesh(entry.path().string());
                objects.insert(objects.end(), obj1.begin(), obj1.end());
            }
        }
        std::vector<GridPoint> grid = create_point_grid(objects);
        std::cout<<"point grid created"<<std::endl;
        std::vector<ray> rays;
        std::vector<std::shared_ptr<hittable>> triangles;
        hittable_list scene;
        int face_num =0;
        for (auto it = objects.begin(); it!=objects.end(); ++it) {
            for (auto jt = it->begin(); jt != it->end(); ++jt) {
                scene.add(std::make_shared<Triangle>(*jt));
                face_num++;
            }
        }

        auto start = std::chrono::high_resolution_clock::now();

        auto bvh = bvh_node(scene, 0, 1);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "Time taken for bvh construction: " << duration << " milliseconds." << std::endl;
        double t = calculate_shadow(directions,bvh, grid);
        double result = t/directions.size();
        std::cout<<"average time for intersection test "<<result<<std::endl;
        return 0;
    }

}