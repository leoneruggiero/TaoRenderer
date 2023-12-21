#include "TaoMath.h"

using namespace glm;

namespace tao_math
{

    std::optional<glm::vec3> RayPlaneIntersection(const Ray& r, const Plane& pl, float tol)
    {
        float RoZ = glm::dot(r.Direction() ,pl.AxisZ());
        float num = glm::dot(pl.Origin() - r.Origin(), pl.AxisZ());

        if( glm::abs(RoZ)<tol ||
            glm::abs(num)<tol)
                return std::nullopt;

        float t = num / RoZ;

        return std::make_optional(r.Origin() + t*r.Direction());
    }

    void RaySphereIntersection(const Ray& r, const glm::vec3& center,float radius, std::optional<glm::vec3>& intersection0, std::optional<glm::vec3>& intersection1, float tol)
    {
        // translate all by -rayOrigin
        glm::vec3 sc = center-r.Origin();
        glm::vec3 ro = glm::vec3{0.0f}; // rayOrigin-rayOrigin;

        float RoC = dot(r.Direction(), sc);
        float scLen = length(sc);

        float D = 4.0f*((RoC*RoC) - (scLen*scLen - radius*radius));

        if(D<tol) return; // no intersections

        D = sqrt(D);

        float t0 = RoC + D*0.5f;
        float t1 = RoC - D*0.5f;

        // may be the same point
        intersection0 = t0>0.0f ? std::make_optional(r.PointAt(t0)) : std::nullopt;
        intersection1 = t1>0.0f ? std::make_optional(r.PointAt(t1)) : std::nullopt;
    }

    std::optional<glm::vec3> RayTriangleIntersection(const Ray& r, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, float tol)
    {
        // origin at t0
        glm::vec3 r0=r.Origin()-v0;

        glm::vec3 e1 = (v1-v0);
        glm::vec3 e2 = (v2-v0);

        glm::vec3 e2Xr=glm::cross(e2, -r.Direction());
        float det = glm::dot(e1, e2Xr);

        if(glm::abs(det)<=tol)
            return std::nullopt;

        float invDet = 1.0f/ det;

        float detU = glm::dot(r0, e2Xr);
        float u = detU*invDet;

        if(u<tol || u>1.0f-tol)
            return std::nullopt;

        glm::vec3 r0Xr = glm::cross(r0, -r.Direction());
        float detV = glm::dot(e1, r0Xr);
        float v = detV*invDet;

        if(v<tol || v>(1.0f-u-tol))
            return std::nullopt;

        glm::vec3 e2Xr0 = glm::cross(e2, r0);
        float detT = glm::dot(e1, e2Xr0);
        float t = detT*invDet;

        return std::make_optional<glm::vec3>(r.PointAt(t));
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
