#include "cube.h"
#include <algorithm>
#include <limits>

Cube::Cube(const glm::vec3 &center, float size, const Material &mat)
    : center(center), size(size), Object(mat) {}

Intersect Cube::rayIntersect(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection) const
{
    glm::vec3 invDir = 1.0f / rayDirection; // Precompute the inverse of the ray direction
    glm::vec3 minBounds = center - glm::vec3(size / 2.0f);
    glm::vec3 maxBounds = center + glm::vec3(size / 2.0f);

    glm::vec3 t1 = (minBounds - rayOrigin) * invDir;
    glm::vec3 t2 = (maxBounds - rayOrigin) * invDir;

    glm::vec3 tMin = glm::min(t1, t2);
    glm::vec3 tMax = glm::max(t1, t2);

    float tEnter = glm::max(glm::max(tMin.x, tMin.y), tMin.z);
    float tExit = glm::min(glm::min(tMax.x, tMax.y), tMax.z);

    if (tEnter > tExit || tExit < 0.0f) // If there is no intersection or the cube is behind the ray start
        return Intersect{false};

    float t = tEnter;
    if (t < 0.0f)
    {
        t = tExit;    // If tEnter is negative, the ray starts inside the box so we use tExit
        if (t < 0.0f) // If tExit is also negative, the intersection is behind the ray start
            return Intersect{false};
    }

    glm::vec3 hitPoint = rayOrigin + t * rayDirection;
    glm::vec3 normal = glm::vec3(0.0f);

    // Set normal based on the dominant axis of intersection
    float epsilon = 0.001f;
    if (t == tMin.x)
        normal = -glm::sign(rayDirection.x) * glm::vec3(1, 0, 0);
    else if (t == tMin.y)
        normal = -glm::sign(rayDirection.y) * glm::vec3(0, 1, 0);
    else if (t == tMin.z)
        normal = -glm::sign(rayDirection.z) * glm::vec3(0, 0, 1);

    return Intersect{true, t, hitPoint, normal};
}
