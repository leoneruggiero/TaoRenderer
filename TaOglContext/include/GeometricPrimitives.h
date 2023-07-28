#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


namespace tao_geometry
{

    class Mesh
    {

    private:
        std::vector<glm::vec3> _vertices;

        std::vector<glm::vec3> _normals;

        std::vector<glm::vec3> _tangents;

        std::vector<glm::vec3> _bitangents;

        std::vector<glm::vec2> _textureCoordinates;

        std::vector<int> _indices;

        void ComputeTangentsAndBitangents()
        {
            if (_indices.empty() || _textureCoordinates.empty()) throw std::runtime_error("Cannot compute tangents without triangles and texture coordinates.");


            std::vector<glm::vec3>
                tangents = std::vector<glm::vec3>(_indices.size()),
                bitangents = std::vector<glm::vec3>(_indices.size());


            for (int i = 0; i < _indices.size(); i += 3)
            {
                int
                    t1 = _indices.at(i + 0),
                    t2 = _indices.at(i + 1),
                    t3 = _indices.at(i + 2);

                glm::vec3
                    e1 = (_vertices.at(t2) - _vertices.at(t1)),
                    e2 = (_vertices.at(t3) - _vertices.at(t2));

                glm::vec2
                    delta1 = (_textureCoordinates.at(t2) - _textureCoordinates.at(t1)),
                    delta2 = (_textureCoordinates.at(t3) - _textureCoordinates.at(t2));


                glm::mat3x2 mE = glm::transpose(glm::mat2x3(e1, e2));
                glm::mat2 mDelta = glm::transpose(glm::mat2(delta1, delta2));
                glm::mat2 mDeltaInverse = glm::inverse(mDelta);

                glm::mat3x2 mTB = mDeltaInverse * mE;

                glm::vec3 tangent = glm::transpose(mTB)[0];
                glm::vec3 bitangent = glm::transpose(mTB)[1];

                // Average vertex tangents and bitangents
                tangents.at(t1) += tangent;
                tangents.at(t2) += tangent;
                tangents.at(t3) += tangent;

                bitangents.at(t1) += bitangent;
                bitangents.at(t2) += bitangent;
                bitangents.at(t3) += bitangent;
            }

            for (auto& v : tangents)
                v = glm::normalize(v);

            for (auto& v : bitangents)
                v = glm::normalize(v);

            _tangents = tangents;
            _bitangents = bitangents;
        }

    public:
        Mesh(const std::vector<glm::vec3>& verts, const std::vector<glm::vec3>& normals, const std::vector<int>& tris)
        {
            int numVerts = verts.size();
            int numNormals = normals.size();
            int numTris = tris.size();

            // Error conditions
            if (numVerts != numNormals)
                throw "number of vertices not compatible with number of normals";
            if (numTris < 3)
                throw "a mesh must contain at least one triangle";

            _vertices = verts;
            _normals = normals;
            _indices = tris;
        }

        Mesh(const std::vector<glm::vec3>& verts, const std::vector<glm::vec3>& normals, const std::vector<int>& tris, const std::vector<glm::vec2>& TextureCoordinates)
            : Mesh(verts, normals, tris)
        {
            int numVerts = verts.size();
            int numTextureCoordinates = TextureCoordinates.size();
            // Error conditions
            if (numVerts != numTextureCoordinates)
                throw "number of vertices not compatible with number of texture coordinates";

            _textureCoordinates = TextureCoordinates;

            ComputeTangentsAndBitangents();
        }

        Mesh(const std::vector<glm::vec3>& verts, const std::vector<glm::vec3>& normals, const std::vector<int>& tris,
            const std::vector<glm::vec2>& textureCoordinates, const std::vector<glm::vec3>& tangents)
            : Mesh(verts, normals, tris, textureCoordinates)
        {

            _tangents = tangents;
        }
    public:
        std::vector<glm::vec3> GetPositions()           const { return std::vector<glm::vec3>(_vertices); };
        std::vector<glm::vec3> GetNormals()             const { return std::vector<glm::vec3>(_normals); };
        std::vector<glm::vec3> GetTangents()            const { return std::vector<glm::vec3>(_tangents); };
        std::vector<glm::vec3> GetBitangents()          const { return std::vector<glm::vec3>(_bitangents); };
        std::vector<glm::vec2> GetTextureCoordinates()  const { return std::vector<glm::vec2>(_textureCoordinates); };
        std::vector<int> GetIndices()          const { return std::vector<int>(_indices); };
        int NumVertices() { return _vertices.size(); }
        int NumNormals()  { return _normals .size(); }
        int NumIndices()  { return _indices .size(); }

        static Mesh Sphere(float radius, int subdivisions)
        {
            std::vector<glm::vec3> vertices = std::vector<glm::vec3>();
            std::vector<glm::vec3> normals = std::vector<glm::vec3>();
            std::vector<glm::vec2> textureCoordinates = std::vector<glm::vec2>();
            std::vector<int> triangles = std::vector<int>();

            int stacks = subdivisions / 2;

            std::vector<glm::vec3> circleVertices = std::vector<glm::vec3>(subdivisions * 2);
            std::vector<glm::vec2> circleTextureCoordinates = std::vector<glm::vec2>(subdivisions * 2);

            float increment = 2 * glm::pi<float>() / subdivisions;

            // Creating a single stack that will be replicated for the number of total stacks
            // with the proper translation and scale
            for (int j = 0, k = 0; j < subdivisions; j++, k += 2)
            {
                circleVertices.at(k) = glm::vec3(glm::cos(j * increment), glm::sin(j * increment), 0.0);
                circleVertices.at(k + 1) = glm::vec3(glm::cos((j + 1) * increment), glm::sin((j + 1) * increment), 0.0);

                circleTextureCoordinates.at(k) = glm::vec2(j / (float)subdivisions, 0.0);
                circleTextureCoordinates.at(k + 1) = glm::vec2((j + 1) / (float)subdivisions, 0.0);
            }

            increment = (glm::pi<float>() / stacks);
            for (int i = 0; i <= stacks; i++)
            {
                float height = glm::sin(i * increment - glm::pi<float>() / 2.0f);

                float cos = glm::cos(glm::asin(height / radius));
                float scale = cos * radius;

                for (int j = 0; j < circleVertices.size(); j++)
                {
                    glm::vec3 vertex = circleVertices.at(j) * glm::vec3(scale, scale, 1) + glm::vec3(0.0, 0.0, height);
                    vertices.push_back(vertex);
                    normals.push_back(vertex / radius);
                    textureCoordinates.push_back(circleTextureCoordinates.at(j) + glm::vec2(0.0, (float)i / stacks));
                }

            }

            int cvCount = circleVertices.size();
            for (int i = 0; i < stacks; i++)
            {
                for (int j = 0; j < cvCount - 1; j += 2)
                {
                    triangles.push_back((i * cvCount) + j);
                    triangles.push_back((i * cvCount) + j + 1);
                    triangles.push_back(((i + 1) * cvCount) + j + 1);

                    triangles.push_back((i * cvCount) + j);
                    triangles.push_back(((i + 1) * cvCount) + j + 1);
                    triangles.push_back(((i + 1) * cvCount) + j);
                }
            }

            return Mesh(vertices, normals, triangles, textureCoordinates);
        }

        static Mesh Box(float width, float height, float depth)
        {
            glm::vec3 boxVerts[] =
            {
                //bottom
                glm::vec3(0 ,0, 0),
                glm::vec3(width ,0, 0),
                glm::vec3(width ,height, 0),

                glm::vec3(0 ,0, 0),
                glm::vec3(width ,height, 0),
                glm::vec3(0 ,height, 0),

                //front
                glm::vec3(0 ,0, 0),
                glm::vec3(width ,0, 0),
                glm::vec3(width ,0, depth),

                glm::vec3(0 ,0, 0),
                glm::vec3(width ,0, depth),
                glm::vec3(0 ,0, depth),

                //right
                glm::vec3(width ,0, 0),
                glm::vec3(width ,height, 0),
                glm::vec3(width ,height, depth),

                glm::vec3(width ,0, 0),
                glm::vec3(width ,height, depth),
                glm::vec3(width ,0, depth),

                //back
                glm::vec3(0 ,height, 0),
                glm::vec3(width ,height, 0),
                glm::vec3(width ,height, depth),

                glm::vec3(0 ,height, 0),
                glm::vec3(width ,height, depth),
                glm::vec3(0 ,height, depth),

                //left
                glm::vec3(0 ,0, 0),
                glm::vec3(0 ,height, 0),
                glm::vec3(0 ,height, depth),

                glm::vec3(0 ,0, 0),
                glm::vec3(0 ,height, depth),
                glm::vec3(0 ,0, depth),

                //top
                glm::vec3(0 ,0, depth),
                glm::vec3(width ,0, depth),
                glm::vec3(width ,height, depth),

                glm::vec3(0 ,0, depth),
                glm::vec3(width ,height, depth),
                glm::vec3(0 ,height, depth),
            };
            glm::vec3 boxNormals[] =
            {
                //bottom
                glm::vec3(0 ,0, -1),
                glm::vec3(0 ,0, -1),
                glm::vec3(0 ,0, -1),
                glm::vec3(0 ,0, -1),
                glm::vec3(0 ,0, -1),
                glm::vec3(0 ,0, -1),

                //front
                glm::vec3(0 ,-1, 0),
                glm::vec3(0 ,-1, 0),
                glm::vec3(0 ,-1, 0),
                glm::vec3(0 ,-1, 0),
                glm::vec3(0 ,-1, 0),
                glm::vec3(0 ,-1, 0),

                //right
                glm::vec3(1 ,0, 0),
                glm::vec3(1 ,0, 0),
                glm::vec3(1 ,0, 0),
                glm::vec3(1 ,0, 0),
                glm::vec3(1 ,0, 0),
                glm::vec3(1 ,0, 0),

                //back
                glm::vec3(0 ,1, 0),
                glm::vec3(0 ,1, 0),
                glm::vec3(0 ,1, 0),
                glm::vec3(0 ,1, 0),
                glm::vec3(0 ,1, 0),
                glm::vec3(0 ,1, 0),

                //left
                glm::vec3(-1 ,0, 0),
                glm::vec3(-1 ,0, 0),
                glm::vec3(-1 ,0, 0),
                glm::vec3(-1 ,0, 0),
                glm::vec3(-1 ,0, 0),
                glm::vec3(-1 ,0, 0),


                //top
                glm::vec3(0 ,0, 1),
                glm::vec3(0 ,0, 1),
                glm::vec3(0 ,0, 1),
                glm::vec3(0 ,0, 1),
                glm::vec3(0 ,0, 1),
                glm::vec3(0 ,0, 1),
            };

            glm::vec2 boxTextureCoordinates[] =
            {
                glm::vec2(0, 0),
                glm::vec2(1, 0),
                glm::vec2(1, 1),
                glm::vec2(0, 0),
                glm::vec2(1, 1),
                glm::vec2(0, 1),

                glm::vec2(0, 0),
                glm::vec2(1, 0),
                glm::vec2(1, 1),
                glm::vec2(0, 0),
                glm::vec2(1, 1),
                glm::vec2(0, 1),

                glm::vec2(0, 0),
                glm::vec2(1, 0),
                glm::vec2(1, 1),
                glm::vec2(0, 0),
                glm::vec2(1, 1),
                glm::vec2(0, 1),

                glm::vec2(0, 0),
                glm::vec2(1, 0),
                glm::vec2(1, 1),
                glm::vec2(0, 0),
                glm::vec2(1, 1),
                glm::vec2(0, 1),

                glm::vec2(0, 0),
                glm::vec2(1, 0),
                glm::vec2(1, 1),
                glm::vec2(0, 0),
                glm::vec2(1, 1),
                glm::vec2(0, 1),

                glm::vec2(0, 0),
                glm::vec2(1, 0),
                glm::vec2(1, 1),
                glm::vec2(0, 0),
                glm::vec2(1, 1),
                glm::vec2(0, 1),
            };

            unsigned int boxTris[] =
            {
                0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
                12, 13, 14, 15, 16 ,17 ,18 ,19, 20, 21, 22, 23,
                24, 25, 26, 27, 28, 29, 30 ,31, 32, 33, 34, 35,
            };

            std::vector<glm::vec3>
                vertices{ std::begin(boxVerts), std::end(boxVerts) },
                normals{ std::begin(boxNormals), std::end(boxNormals) };
            std::vector<glm::vec2>
                TextureCoordinates{ std::begin(boxTextureCoordinates), std::end(boxTextureCoordinates) };
            std::vector<int>
                triangles{ std::begin(boxTris), std::end(boxTris) };


            return Mesh(vertices, normals, triangles, TextureCoordinates);
        }
        static Mesh TruncCone(float radius, float tipRadius, float height, int subdivisions)
        {
            // Side
            std::vector<glm::vec3> vertices = std::vector<glm::vec3>();
            std::vector<glm::vec3> normals = std::vector<glm::vec3>();
            std::vector<glm::vec2> textureCoordinates = std::vector<glm::vec2>();
            std::vector<int> triangles = std::vector<int>();

            float dAngle = glm::pi<float>() * 2 / subdivisions;

            for (int i = 0; i < subdivisions; i++)
            {
                float
                    cosA = glm::cos(i * dAngle),
                    sinA = glm::sin(i * dAngle);

                vertices.push_back(glm::vec3(cosA * radius, sinA * radius, 0));
                textureCoordinates.push_back(glm::vec2(i / (float)subdivisions, 0));
                normals.push_back(glm::vec3(cosA, sinA, (radius - tipRadius) / height));

                vertices.push_back(glm::vec3(cosA * tipRadius, sinA * tipRadius, height));
                textureCoordinates.push_back(glm::vec2(i / (float)subdivisions, 1));
                normals.push_back(glm::vec3(cosA, sinA, (radius - tipRadius) / height));

                // Building an "open" cylinder to define texture coordinates per-triangle instead of per-vertex
                cosA = glm::cos((i + 1) * dAngle);
                sinA = glm::sin((i + 1) * dAngle);

                vertices.push_back(glm::vec3(cosA * radius, sinA * radius, 0));
                textureCoordinates.push_back(glm::vec2((i + 1.0) / subdivisions, 0));
                normals.push_back(glm::vec3(cosA, sinA, (radius - tipRadius) / height));

                vertices.push_back(glm::vec3(cosA * tipRadius, sinA * tipRadius, height));
                textureCoordinates.push_back(glm::vec2((i + 1.0) / subdivisions, 1));
                normals.push_back(glm::vec3(cosA, sinA, (radius - tipRadius) / height));
            }

            for (int k = 0; k < subdivisions; k++)
            {
                int i = k * 4;

                triangles.push_back(i);
                triangles.push_back((i + 2) % (subdivisions * 4));
                triangles.push_back((i + 3) % (subdivisions * 4));

                triangles.push_back(i);
                triangles.push_back((i + 3) % (subdivisions * 4));
                triangles.push_back((i + 1) % (subdivisions * 4));
            }

            // bottom
            for (int i = 0; i < subdivisions; i++)
            {
                float
                    cosA = glm::cos(i * dAngle),
                    sinA = glm::sin(i * dAngle);

                vertices.push_back(glm::vec3(
                    cosA * radius,
                    sinA * radius,
                    0
                ));
                textureCoordinates.push_back(glm::vec2(
                    cosA * 0.5 + 0.5,
                    sinA * 0.5 + 0.5
                ));
                normals.push_back(glm::vec3(
                    0,
                    0,
                    -1
                ));

            }

            int offset = subdivisions * 4;
            for (int k = 0; k < subdivisions - 2; k++)
            {
                int i = offset + k;
                triangles.push_back(offset);
                triangles.push_back(i + 1);
                triangles.push_back(i + 2);
            }

            // top
            for (int i = 0; i < subdivisions; i++)
            {
                float
                    cosA = glm::cos(i * dAngle),
                    sinA = glm::sin(i * dAngle);

                vertices.push_back(glm::vec3(
                    cosA * tipRadius,
                    sinA * tipRadius,
                    height
                ));
                textureCoordinates.push_back(glm::vec2(
                    cosA * 0.5 + 0.5,
                    sinA * 0.5 + 0.5
                ));
                normals.push_back(glm::vec3(
                    0,
                    0,
                    1
                ));

            }

            offset = subdivisions * 5;
            for (int k = 0; k < subdivisions - 2; k++)
            {
                int i = offset + k;
                triangles.push_back(offset);
                triangles.push_back(i + 1);
                triangles.push_back(i + 2);
            }
            return Mesh(vertices, normals, triangles, textureCoordinates);
        }
        static Mesh Cone(float radius, float height, int subdivisions) { return TruncCone(radius, 0.001, height, subdivisions); }
        static Mesh Cylinder(float radius, float height, int subdivisions) { return TruncCone(radius, radius, height, subdivisions); }
        static Mesh Arrow(float cylRadius, float cylLenght, float coneRadius, float coneLength, int subdivisions)
        {
            Mesh cyl = Cylinder(cylRadius, cylLenght, subdivisions);
            Mesh cone = Cone(coneRadius, coneLength, subdivisions);

            std::vector<glm::vec3> cylVertices = cyl.GetPositions();
            std::vector<glm::vec3> coneVertices = cone.GetPositions();

            glm::vec3 offset = glm::vec3(0.0, 0.0, cylLenght);
            for (int i = 0; i < coneVertices.size(); i++)
            {
                coneVertices.at(i) += offset;
            }

            std::vector<glm::vec3> cylNormals = cyl.GetNormals();
            std::vector<glm::vec3> coneNormals = cone.GetNormals();

            std::vector<int> cylIndices = cyl.GetIndices();
            std::vector<int> coneIndices = cone.GetIndices();

            std::vector<glm::vec3> arrowVertices;
            arrowVertices.insert(arrowVertices.end(), cylVertices.begin(), cylVertices.end());
            arrowVertices.insert(arrowVertices.end(), coneVertices.begin(), coneVertices.end());

            std::vector<glm::vec3> arrowNormals;
            arrowNormals.insert(arrowNormals.end(), cylNormals.begin(), cylNormals.end());
            arrowNormals.insert(arrowNormals.end(), coneNormals.begin(), coneNormals.end());

            std::vector<int> arrowIndices;
            arrowIndices.insert(arrowIndices.end(), cylIndices.begin(), cylIndices.end());
            arrowIndices.insert(arrowIndices.end(), coneIndices.begin(), coneIndices.end());
            int k = cylVertices.size();
            for (int j = cylIndices.size(); j < cylIndices.size() + coneIndices.size(); j++)
            {
                arrowIndices.at(j) += k;
            }

            return Mesh(arrowVertices, arrowNormals, arrowIndices);
        }
    };
}