#include "lightsurgeon/bvh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace lightsurgeon {
namespace {

constexpr int kLeafSize = 8;

double component(const Vec3& v, int axis) {
    if (axis == 0) return v.x;
    if (axis == 1) return v.y;
    return v.z;
}

}  // namespace

void Bvh::build(std::vector<Triangle> triangles) {
    triangles_ = std::move(triangles);
    nodes_.clear();
    nodes_.reserve(triangles_.empty() ? 1 : triangles_.size() * 2);
    if (triangles_.empty()) {
        root_ = -1;
        return;
    }
    root_ = buildRecursive(0, static_cast<int>(triangles_.size()));
}

int Bvh::buildRecursive(int start, int end) {
    Node node;
    for (int i = start; i < end; ++i) {
        node.bounds.expand(triangles_[static_cast<std::size_t>(i)].v0);
        node.bounds.expand(triangles_[static_cast<std::size_t>(i)].v1);
        node.bounds.expand(triangles_[static_cast<std::size_t>(i)].v2);
    }
    const int count = end - start;
    if (count <= kLeafSize) {
        node.leaf = true;
        node.start = start;
        node.count = count;
        nodes_.push_back(node);
        return static_cast<int>(nodes_.size()) - 1;
    }
    const int axis = node.bounds.longestAxis();
    const int mid = start + count / 2;
    std::nth_element(triangles_.begin() + start, triangles_.begin() + mid, triangles_.begin() + end,
                     [axis](const Triangle& a, const Triangle& b) {
                         return component(a.centroid(), axis) < component(b.centroid(), axis);
                     });
    const int index = static_cast<int>(nodes_.size());
    nodes_.push_back(node);
    const int left = buildRecursive(start, mid);
    const int right = buildRecursive(mid, end);
    nodes_[static_cast<std::size_t>(index)].left = left;
    nodes_[static_cast<std::size_t>(index)].right = right;
    return index;
}

bool Bvh::intersectAabb(const Ray& ray, const Aabb& b, double* tEnter, double* tExit) {
    double t0 = ray.tMin;
    double t1 = ray.tMax;
    const double ox[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const double d[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const double mn[3] = {b.bmin.x, b.bmin.y, b.bmin.z};
    const double mx[3] = {b.bmax.x, b.bmax.y, b.bmax.z};
    for (int i = 0; i < 3; ++i) {
        if (std::abs(d[i]) < 1e-15) {
            if (ox[i] < mn[i] || ox[i] > mx[i]) return false;
            continue;
        }
        const double inv = 1.0 / d[i];
        double tNear = (mn[i] - ox[i]) * inv;
        double tFar = (mx[i] - ox[i]) * inv;
        if (tNear > tFar) std::swap(tNear, tFar);
        t0 = std::max(t0, tNear);
        t1 = std::min(t1, tFar);
        if (t0 > t1) return false;
    }
    *tEnter = t0;
    *tExit = t1;
    return true;
}

bool Bvh::intersectTriangle(const Ray& ray, const Triangle& tri, double* tOut, Vec3* nOut) {
    const Vec3 e1 = tri.v1 - tri.v0;
    const Vec3 e2 = tri.v2 - tri.v0;
    const Vec3 p = cross(ray.direction, e2);
    const double det = dot(e1, p);
    if (std::abs(det) < 1e-12) return false;
    const double inv = 1.0 / det;
    const Vec3 s = ray.origin - tri.v0;
    const double u = inv * dot(s, p);
    if (u < 0.0 || u > 1.0) return false;
    const Vec3 q = cross(s, e1);
    const double v = inv * dot(ray.direction, q);
    if (v < 0.0 || u + v > 1.0) return false;
    const double t = inv * dot(e2, q);
    if (t < ray.tMin || t > ray.tMax) return false;
    *tOut = t;
    Vec3 n = cross(e1, e2);
    if (dot(n, ray.direction) > 0.0) n = -n;
    *nOut = normalize(n);
    return true;
}

Hit Bvh::traverse(const Ray& ray, bool anyHit) const {
    Hit best;
    if (root_ < 0) return best;
    int stack[64];
    int sp = 0;
    stack[sp++] = root_;
    while (sp > 0) {
        const Node& node = nodes_[static_cast<std::size_t>(stack[--sp])];
        double tEnter = 0.0;
        double tExit = 0.0;
        Ray r = ray;
        if (best.hit) r.tMax = best.t;
        if (!intersectAabb(r, node.bounds, &tEnter, &tExit)) continue;
        if (node.leaf) {
            for (int i = 0; i < node.count; ++i) {
                const Triangle& tri = triangles_[static_cast<std::size_t>(node.start + i)];
                double t = 0.0;
                Vec3 n;
                if (intersectTriangle(r, tri, &t, &n)) {
                    best.hit = true;
                    best.t = t;
                    best.point = ray.origin + ray.direction * t;
                    best.normal = n;
                    best.meshIndex = tri.meshIndex;
                    best.triIndex = tri.triIndex;
                    if (anyHit) return best;
                    r.tMax = t;
                }
            }
        } else {
            if (sp + 2 < 64) {
                stack[sp++] = node.left;
                stack[sp++] = node.right;
            }
        }
    }
    return best;
}

Hit Bvh::closestHit(const Ray& ray) const { return traverse(ray, false); }

bool Bvh::occluded(const Ray& ray) const { return traverse(ray, true).hit; }

double Bvh::rayPointDistance(const Ray& ray, const Vec3& p, double* t) {
    const double len2 = lengthSquared(ray.direction);
    if (len2 < 1e-20) {
        *t = 0.0;
        return length(p - ray.origin);
    }
    *t = dot(p - ray.origin, ray.direction) / len2;
    const double clamped = std::max(ray.tMin, std::min(ray.tMax, *t));
    const Vec3 closest = ray.origin + ray.direction * clamped;
    return length(p - closest);
}

double Bvh::closestApproach(const Ray& ray, int* meshIndex) const {
    double best = std::numeric_limits<double>::max();
    int mesh = -1;
    for (const Triangle& tri : triangles_) {
        double t0 = 0.0;
        double t1 = 0.0;
        double t2 = 0.0;
        const double d0 = rayPointDistance(ray, tri.v0, &t0);
        const double d1 = rayPointDistance(ray, tri.v1, &t1);
        const double d2 = rayPointDistance(ray, tri.v2, &t2);
        const double d = std::min(d0, std::min(d1, d2));
        if (d < best) {
            best = d;
            mesh = tri.meshIndex;
        }
    }
    if (meshIndex) *meshIndex = mesh;
    return best;
}

}  // namespace lightsurgeon
