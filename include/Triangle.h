//
// Created by 55241 on 2023/3/27.
//

#ifndef CPP_TRIANGLE_H
#define CPP_TRIANGLE_H

#include "vec3.h"
#include "hittable.h"


class Triangle : public hittable {
public:
    point3 v0;
    point3 v1;
    point3 v2;
    unsigned long id;
    std::shared_ptr<std::string> surf_gmlid;
    vec3 normal;

    Triangle () = default;
    Triangle(const point3& p0, const point3& p1, const point3& p2, const unsigned long &idx): v0(p0), v1(p1), v2(p2), id(idx) {
        vec3 edge1, edge2;
        edge1 = v1 - v0;
        edge2 = v2 - v0;
        normal = cross(edge1, edge2);
    };

    bool hit(const ray& r, double t_min, double t_max, hit_record& rec) const override;
    bool bounding_box(double time0, double time1, aabb& output_box) const;
    double area() const;

    bool hit1(const ray &r) const;
};

inline bool Triangle::hit(const ray &r, double t_min, double t_max, hit_record &rec) const {
//    std::cout<<"reach a triangle"<<std::endl;
//    std::cout<<"v0 "<<v0.x()<<" "<<v0.y()<<" "<<v0.z()<<std::endl;
//    std::cout<<"v1 "<<v1.x()<<" "<<v1.y()<<" "<<v1.z()<<std::endl;
//    std::cout<<"v2 "<<v2.x()<<" "<<v2.y()<<" "<<v2.z()<<std::endl;

    const float EPSILON = 0.00000001;
    vec3 edge1, edge2, h, s, q;
    double a,f,u,v;
    edge1 = v1 - v0;
    edge2 = v2 - v0;
    h = cross(r.direction(), edge2);
    a = dot(edge1, h);
    if (a > - EPSILON && a < EPSILON)
        return false;
    f = 1.0/a;
    s = r.origin() - v0;
    u = f * dot(s,h);
    if (u<0.0 || u>1.0)
        return false;
    q = cross(s, edge1);
    v = f * dot(r.dir, q);
    if (v<0.0 || u + v > 1.0)
        return false;
    float t = f * dot(edge2, q);
    if (t > EPSILON)
        return true;
    else
        return false;

}


inline bool Triangle::bounding_box(double time0, double time1, aabb &output_box) const {
    double min_x, min_y, min_z, max_x, max_y, max_z;
    min_x = std::min(v0.x(), std::min(v1.x(), v2.x()));
    min_y = std::min(v0.y(), std::min(v1.y(), v2.y()));
    min_z = std::min(v0.z(), std::min(v1.z(), v2.z()));
    max_x = std::max(v0.x(), std::max(v1.x(), v2.x()));
    max_y = std::max(v0.y(), std::max(v1.y(), v2.y()));
    max_z = std::max(v0.z(), std::max(v1.z(), v2.z()));
    output_box = aabb(vec3(min_x, min_y, min_z), vec3(max_x, max_y, max_z));
    return true;
}

inline double Triangle::area() const {
    double area = normal.length()/2;
    return area;
}





#endif //CPP_TRIANGLE_H
