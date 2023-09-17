#pragma once

//#include <glm/gtc/matrix_transform.hpp>
//#include <glm/gtc/type_ptr.hpp>
#include "glm/glm.hpp"
#include <limits>
#include <vector>
#include <algorithm>
#include <optional>

namespace tao_math
{

    class Ray
    {
    public:
        Ray(const glm::vec3& origin, const glm::vec3& direction):
                _origin{origin}, _direction{normalize(direction)}
        {

        }

        const glm::vec3& Origin() const {return _origin;}
        const glm::vec3& Direction() const {return _direction;}

    private:
        glm::vec3 _origin;
        glm::vec3 _direction;
    };

    class Plane
    {
    public:
        Plane(const glm::vec3& origin, const glm::vec3& normal):
        _origin{origin}
        {
            _axisZ = normalize(normal);
            _axisX =
                    abs(glm::dot(_axisZ, glm::vec3{0.0f,0.0f,1.0f})) > (1.0f-1e-3f)
                    ? glm::cross(_axisZ, glm::vec3{1.0f, 0.0f, 0.0f})
                    : glm::cross(_axisZ, glm::vec3{0.0f, 0.0f, 1.0f});

            _axisX  = normalize(_axisX);

            _axisY = cross(_axisZ, _axisX);
        }

        glm::vec2 ProjectPoint(const glm::vec3& p) const
        {
            glm::vec3 diff = p - _origin;

            return glm::vec2
            {
                glm::dot(diff, _axisX),
                glm::dot(diff, _axisY)
            };
        }

        glm::mat4 Transformation() const
        {
            return glm::mat4
                    {
                        glm::vec4{_axisX, 0.0f},
                        glm::vec4{_axisY, 0.0f},
                        glm::vec4{_axisZ, 0.0f},
                        glm::vec4{_origin, 1.0f}
                    };
        }

        const glm::vec3 &Origin() const{return _origin;}
        const glm::vec3 &AxisX() const{return _axisX;}
        const glm::vec3 &AxisY() const{return _axisY;}
        const glm::vec3 &AxisZ() const{return _axisZ;}

    private:
        glm::vec3 _origin;
        glm::vec3 _axisX;
        glm::vec3 _axisY;
        glm::vec3 _axisZ;
    };

    std::optional<glm::vec3> RayPlaneIntersection(const Ray& r, const Plane& pl);

    template<typename T, int N>
    class BoundingBox
    {
    public:
        typedef glm::vec<N, T>       vec_type;
        typedef vec_type::value_type value_type;
        class AaBb
        {
        public:
            vec_type Min;
            vec_type Max;

            AaBb():
                Min{std::numeric_limits<value_type>::max()},
                Max{std::numeric_limits<value_type>::min()}
            {
            }

            AaBb(const vec_type& min, const vec_type& max):
                Min{min},
                Max{max}
            {
            }

            value_type Diagonal() const
            {
                return glm::length(Max-Min);
            }

            vec_type Center() const
            {
                return Min+static_cast<value_type>(0.5)*(Max-Min);
            }
        };

        BoundingBox(const std::vector<vec_type> &points)
        {

            value_type maxVal = std::numeric_limits<value_type>::max();
            value_type minVal = -maxVal;
            _min = T{maxVal};
            _max = T{minVal};

            Update(points);
        }

        static void ComputeBbox(const std::vector<vec_type>& points, vec_type & min, vec_type & max)
        {
            MinMaxAA f{ vec_type {min}, vec_type {max}};

            f = std::for_each(points.begin(), points.end(), f);

            /* Error conditions ?*/

            min = f.Min();
            max = f.Max();
        }

        static AaBb ComputeBbox(const std::vector<vec_type>& points)
        {
            value_type maxVal = std::numeric_limits<value_type>::max();
            value_type minVal = -maxVal;

            vec_type min{ maxVal }, max{ minVal };

            ComputeBbox(points, min, max);

            return AaBb(min, max);
        }

        static AaBb ComputeBbox(const std::vector<vec_type>& points, glm::mat<N+1,N+1,T> transformation)
        {
            value_type maxVal = std::numeric_limits<value_type>::max();
            value_type minVal = -maxVal;

            vec_type min{ maxVal }, max{ minVal };

            std::vector<vec_type> transfPoints = std::vector<vec_type>();

            for (int i = 0; i < points.size(); i++)
            {
                vec_type transfPoint = vec_type{ transformation * glm::vec<N+1, T>{ points[i], static_cast<value_type>(1)} };
                transfPoints.push_back(transfPoint);
            }

            ComputeBbox(transfPoints, min, max);

            return AaBb(min, max);
        }

        void Update(const std::vector<vec_type> &points)
        {
            vec_type nMin{ _min }, nMax{ _max };

            ComputeBbox(points, nMin, nMax);

            _min = nMin;
            _max = nMax;

            _diagonal = _max - _min;
            _center = _min + _diagonal * 0.5f;
            _size = glm::length(_diagonal);
        }

        vec_type Min()              const { return _min;        };
        vec_type Max()              const { return _max;        };
        vec_type Center()           const { return _center;     };
        vec_type Diagonal()         const { return _diagonal;   };
        typename value_type Size()  const { return _size;       };

    private:
        class MinMaxAA
        {
        private:
            vec_type _min, _max;

        public:
            MinMaxAA(const vec_type& min, const vec_type& max) : _min { min }, _max{ max } {};

            void operator()(const vec_type& p)
            {
                // Find Minimum
                // ---------------------------
                _min.x = glm::min(_min.x, p.x);
                _min.y = glm::min(_min.y, p.y);
                _min.z = glm::min(_min.z, p.z);

                // Find Maximum
                // ---------------------------
                _max.x = glm::max(_max.x, p.x);
                _max.y = glm::max(_max.y, p.y);
                _max.z = glm::max(_max.z, p.z);
            }

            vec_type Min() { return _min; }
            vec_type Max() { return _max; }
        };
        vec_type _min, _max, _center, _diagonal;
        typename value_type _size;
    };

    // from https://theorangeduck.com/page/spring-roll-call
    float DamperMotion(float current, float goal, float halfTime, float timeStep);

}