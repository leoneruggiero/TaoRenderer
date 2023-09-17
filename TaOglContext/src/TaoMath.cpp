#include "TaoMath.h"

namespace tao_math
{

    std::optional<glm::vec3> RayPlaneIntersection(const Ray& r, const Plane& pl)
    {
        float t =
                glm::dot(pl.Origin() - r.Origin(), pl.AxisZ()) /
                glm::dot(r.Direction() ,pl.AxisZ());

        return t > 0.0f ? std::make_optional(r.Origin() + t*r.Direction()) : std::nullopt;
    }

    float DamperMotion(float current, float goal, float halfTime, float timeStep)
    {
        return std::lerp(current, goal, 1.0f - powf(2, -timeStep / halfTime));
    }
}
