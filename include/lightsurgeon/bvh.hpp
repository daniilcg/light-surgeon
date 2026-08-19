#pragma once

#include "lightsurgeon/types.hpp"

#include <vector>

namespace lightsurgeon {

struct Triangle {
    Vec3 v0;
    Vec3 v1;
    Vec3 v2;
    int meshIndex = -1;
    int triIndex = -1;

    Vec3 centroid() const { return (v0 + v1 + v2) * (1.0 / 3.0); }
    Vec3 geometricNormal() const { return normalize(cross(v1 - v0, v2 - v0)); }
    double area() const { return 0.5 * length(cross(v1 - v0, v2 - v0)); }
};

struct Aabb {
    Vec3 bmin{1e30, 1e30, 1e30};
    Vec3 bmax{-1e30, -1e30, -1e30};

    void expand(const Vec3& p) {
        bmin = min3(bmin, p);
        bmax = max3(bmax, p);
    }
    void expand(const Aabb& o) {
        expand(o.bmin);
        expand(o.bmax);
    }
    Vec3 center() const { return (bmin + bmax) * 0.5; }
    int longestAxis() const {
        const Vec3 e = bmax - bmin;
        if (e.x >= e.y && e.x >= e.z) return 0;
        if (e.y >= e.z) return 1;
        return 2;
    }
};

struct Hit {
    bool hit = false;
    double t = 0.0;
    Vec3 point;
    Vec3 normal;
    int meshIndex = -1;
    int triIndex = -1;
};

struct Ray {
    Vec3 origin;
    Vec3 direction;
    double tMin = 1e-5;
    double tMax = 1e30;
};

class Bvh {
public:
    void build(std::vector<Triangle> triangles);
    Hit closestHit(const Ray& ray) const;
    bool occluded(const Ray& ray) const;
    double closestApproach(const Ray& ray, int* meshIndex) const;
    std::size_t triangleCount() const { return triangles_.size(); }
    const Triangle& triangle(std::size_t i) const { return triangles_[i]; }

private:
    struct Node {
        Aabb bounds;
        int left = -1;
        int right = -1;
        int start = 0;
        int count = 0;
        bool leaf = false;
    };

    int buildRecursive(int start, int end);
    Hit traverse(const Ray& ray, bool anyHit) const;
    static bool intersectTriangle(const Ray& ray, const Triangle& tri, double* t, Vec3* n);
    static bool intersectAabb(const Ray& ray, const Aabb& b, double* tEnter, double* tExit);
    static double rayPointDistance(const Ray& ray, const Vec3& p, double* t);

    std::vector<Triangle> triangles_;
    std::vector<Node> nodes_;
    int root_ = -1;
};

}  // namespace lightsurgeon
