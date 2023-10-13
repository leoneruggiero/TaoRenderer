#include "TaoMath.h"

using namespace glm;

namespace tao_math
{

    std::optional<glm::vec3> RayPlaneIntersection(const Ray& r, const Plane& pl)
    {
        float t =
                glm::dot(pl.Origin() - r.Origin(), pl.AxisZ()) /
                glm::dot(r.Direction() ,pl.AxisZ());

        return t > 0.0f ? std::make_optional(r.Origin() + t*r.Direction()) : std::nullopt;
    }

    glm::vec2 PointLineClosestPoint(const glm::vec2& pt, const glm::vec2& l0, const glm::vec2& l1)
    {
        vec2 l01Nrm = glm::normalize(l1-l0);
        return  l0+dot(pt-l0, l01Nrm)*l01Nrm;
    }

    // from: https://paulbourke.net/geometry/pointlineplane/
    glm::vec3 RayLineClosestPoint(const glm::vec3& ro, const glm::vec3& rd, const glm::vec3& lp, const glm::vec3& ld)
    {
        // Algorithm is ported from the C algorithm of
        // Paul Bourke at http://local.wasp.uwa.edu.au/~pbourke/geometry/lineline3d/

        const float kE = 1e-5;

        vec3 p1 = ro;
        vec3 p2 = ro+rd;
        vec3 p3 = lp;
        vec3 p4 = lp+ld;
        vec3 p13 = p1 - p3;
        vec3 p43 = p4 - p3;

        if (dot(p43, p43) < kE) {
            return ro;
        }
        vec3 p21 = p2 - p1;
        if (dot(p21, p21) < kE) {
            return ro;
        }

        float d1343 = p13.x * p43.x + p13.y * p43.y + p13.z * p43.z;
        float d4321 = p43.x * p21.x + p43.y * p21.y + p43.z * p21.z;
        float d1321 = p13.x * p21.x + p13.y * p21.y + p13.z * p21.z;
        float d4343 = p43.x * p43.x + p43.y * p43.y + p43.z * p43.z;
        float d2121 = p21.x * p21.x + p21.y * p21.y + p21.z * p21.z;

        float denom = d2121 * d4343 - d4321 * d4321;
        if (abs(denom) < kE) {
            return ro;
        }
        float numer = d1343 * d4321 - d1321 * d4343;

        float mua = numer / denom;
        float mub = (d1343 + d4321 * (mua)) / d4343;

        return lp+mub*ld;
    }

    float DamperMotion(float current, float goal, float halfTime, float timeStep)
    {
        return std::lerp(current, goal, 1.0f - powf(2, -timeStep / halfTime));
    }
}
