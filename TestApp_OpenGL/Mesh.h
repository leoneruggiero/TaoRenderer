#ifndef MESH_H
#define MESH_H

#include <glad/glad.h>
#include <assert.h>
#include "Shader.h"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "SceneUtils.h"
#include "Assimp/Importer.hpp"
#include "Assimp/scene.h"
#include "Assimp/postprocess.h"
#include "GeometryHelper.h"
#include "stb_image.h"
#include "FileUtils.h"

class Mesh : public RenderableBasic
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
        if (_indices.size() == 0 || _textureCoordinates.size() == 0)
            throw "Cannot compute tangents without triangles and texture coordinates.";

        
        std::vector<glm::vec3>
            tangents = std::vector<glm::vec3>(_indices.size()),
            bitangents = std::vector<glm::vec3>(_indices.size());

        
        for (int i = 0; i < _indices.size(); i+=3)
        {
            int
                t1 = _indices.at(i+0),
                t2 = _indices.at(i+1),
                t3 = _indices.at(i+2);

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
    Mesh( const std::vector<glm::vec3> &verts, const std::vector<glm::vec3>  &normals, const std::vector<int> &tris)
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
        _normals  = normals;
        _indices  = tris;
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
    std::vector<glm::vec3> GetPositions() override { return std::vector<glm::vec3>(_vertices); };
    std::vector<glm::vec3> GetNormals() override { return std::vector<glm::vec3>(_normals); };
    std::vector<glm::vec3> GetTangents() override { return std::vector<glm::vec3>(_tangents); };
    std::vector<glm::vec3> GetBitangents() override { return std::vector<glm::vec3>(_bitangents); };
    std::vector<glm::vec2> GetTextureCoordinates() override { return std::vector<glm::vec2>(_textureCoordinates); };
    std::vector<int> GetIndices() override { return std::vector<int>(_indices); };
    int NumVertices() { return _vertices.size(); }
    int NumNormals() { return _normals.size(); }
    int NumIndices() { return _indices.size(); }

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
        for (int j = 0, k = 0; j < subdivisions; j++, k+=2)
        {
            circleVertices.at(k) = glm::vec3(glm::cos(j * increment), glm::sin(j * increment), 0.0);
            circleVertices.at(k+1) = glm::vec3(glm::cos((j + 1) * increment), glm::sin((j + 1) * increment), 0.0);

            circleTextureCoordinates.at(k) = glm::vec2(j /(float)subdivisions, 0.0);
            circleTextureCoordinates.at(k+1) = glm::vec2((j + 1) /(float)subdivisions, 0.0);
        }

        increment =  (glm::pi<float>() / stacks);
        for (int i = 0; i <= stacks; i++)
        {
            float height = glm::sin(i * increment - glm::pi<float>() / 2.0f);
            
            float cos = glm::cos(glm::asin(height/radius));
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
        for (int i = 0; i <stacks; i++)
        {
            for (int j = 0; j < cvCount - 1; j+=2)
            {
                triangles.push_back((i * cvCount) + j);
                triangles.push_back((i * cvCount) + j + 1);
                triangles.push_back(((i + 1) * cvCount) + j + 1);

                triangles.push_back((i * cvCount) + j);
                triangles.push_back(((i + 1) * cvCount) + j + 1);
                triangles.push_back(((i + 1) * cvCount) + j );
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

        int boxTris[] =
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
        std::vector<glm::vec3> vertices=std::vector<glm::vec3>();
        std::vector<glm::vec3> normals = std::vector<glm::vec3>();
        std::vector<glm::vec2> textureCoordinates = std::vector<glm::vec2>();
        std::vector<int> triangles = std::vector<int>();

        float dAngle = glm::pi<float>()*2 / subdivisions;

        for (int i= 0;  i< subdivisions; i++)
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
                cosA*radius,
                sinA*radius,
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
        for (int k = 0; k < subdivisions-2; k++)
        {
            int i = offset + k;
            triangles.push_back(offset);
            triangles.push_back(i+1);
            triangles.push_back(i+2);
        }

        // top
        for (int i = 0; i < subdivisions; i++)
        {
            float
                cosA = glm::cos(i * dAngle),
                sinA = glm::sin(i * dAngle);

            vertices.push_back(glm::vec3(
                cosA*tipRadius,
                sinA*tipRadius,
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
        return Mesh(vertices, normals,triangles, textureCoordinates);
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

        return Mesh(arrowVertices, arrowNormals,arrowIndices);
    }
};

class Wire
{
private:
    std::vector<glm::vec3> _points;

public:
    Wire(const std::vector<glm::vec3> points) : _points{ points } {};
    std::vector<glm::vec3> GetPositions() { return std::vector<glm::vec3>(_points); };
    int NumPoints() { return _points.size(); };

    static Wire Grid(glm::vec2 min, glm::vec2 max, int step)
    {
        int numLinesX = ((max.x - min.x) / step) + 1;
        int numLinesY = ((max.y - min.y) / step) + 1;

        int numLines = numLinesX + numLinesY;

        std::vector<glm::vec3> points;

        int k = 0;

        for (int i = 0; i < numLinesX; i++)
        {
            // start
            points.push_back(glm::vec3(min.x + i * step, min.y, 0.0f));

            // end
            points.push_back(glm::vec3(min.x + i * step, max.y, 0.0f));

        }

        for (int j = 0; j < numLinesY; j++)
        {
            // start
            points.push_back(glm::vec3(min.x , min.y + j * step, 0.0f));

            // end
            points.push_back(glm::vec3(max.x , min.y + j * step, 0.0f));
        }

        return Wire(points);
    }
};

class FileReader
{
private:
    const char* _path;
    std::vector<Mesh> _meshes;
    std::vector<glm::mat4> _transformations;

    Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene)
    {
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec3> tangents;
        std::vector<glm::vec2> uvCoordinates;
        std::vector<int> triangles;

        // ___ POSITIONS ___ //
        // ----------------- //
        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            vertices.push_back(glm::vec3(
                mesh->mVertices[i].x,
                mesh->mVertices[i].y,
                mesh->mVertices[i].z
            ));
        }
        // ----------------- //


        // ___ NORMALS ___ //
        // --------------- //
        if (mesh->mNormals != NULL)
        {
            for (unsigned int i = 0; i < mesh->mNumVertices/*same as normals*/; i++)
            {
                normals.push_back(glm::vec3(
                    mesh->mNormals[i].x,
                    mesh->mNormals[i].y,
                    mesh->mNormals[i].z
                ));
            }
        }
        // --------------- //
       

        // ___ TEXTURE COORDINATES ___ //
        // --------------------------- //
        if (mesh->mTextureCoords[0] != NULL)
        {
            for (unsigned int i = 0; i < mesh->mNumVertices/*same as normals*/; i++)
            {
                uvCoordinates.push_back(glm::vec2(
                    mesh->mTextureCoords[0][i].x,
                    mesh->mTextureCoords[0][i].y
                ));
            }
        }
        // --------------------------- //

        // ___ TANGENTS ___ //
        // ---------------- //
        if (mesh->mTangents != NULL)
        {
            for (unsigned int i = 0; i < mesh->mNumVertices/*same as normals*/; i++)
            {
                tangents.push_back(glm::vec3 (
                    mesh->mTangents[i].x,
                    mesh->mTangents[i].y,
                    mesh->mTangents[i].z
                ));
            }
        }
        // ---------------- //

        // ___ TRIANGLES ___ //
        // ----------------- //
        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];

            for (unsigned int j = 0; j < face.mNumIndices; j++)
            {
                triangles.push_back(face.mIndices[j]);
            }

        }
        // ----------------- //
        
        return Mesh(vertices, normals, triangles, uvCoordinates, tangents);
    }

    void ProcessNode(aiNode* node, const aiScene* scene, glm::mat4 transformation)
    {
        glm::vec4 v0 = glm::vec4(node->mTransformation[0][0], node->mTransformation[0][1], node->mTransformation[0][2], node->mTransformation[0][3]);
        glm::vec4 v1 = glm::vec4(node->mTransformation[1][0], node->mTransformation[1][1], node->mTransformation[1][2], node->mTransformation[1][3]);
        glm::vec4 v2 = glm::vec4(node->mTransformation[2][0], node->mTransformation[2][1], node->mTransformation[2][2], node->mTransformation[2][3]);
        glm::vec4 v3 = glm::vec4(node->mTransformation[3][0], node->mTransformation[3][1], node->mTransformation[3][2], node->mTransformation[3][3]);
        glm::mat4 tr = glm::transpose(glm::mat4(v0, v1, v2, v3));
        
        // process all the node's meshes (if any)
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            _meshes.push_back(ProcessMesh(mesh, scene));
            _transformations.push_back(tr * transformation);
        }
        // then do the same for each of its children
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            ProcessNode(node->mChildren[i], scene, tr * transformation);
        }
    }

public:
    FileReader(const char* path) : _path(path)
    {}

    void Load()
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(_path, aiProcess_Triangulate  | aiProcess_CalcTangentSpace );

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
            return;
        }
        _meshes = std::vector<Mesh>();
        _transformations = std::vector<glm::mat4>();

        glm::mat4 transf = glm::mat4(1.0);
        
        ProcessNode(scene->mRootNode, scene, transf);
    }

    std::vector<Mesh> Meshes()
    {
        return _meshes;
    }

    std::vector<glm::mat4> Transformations()
    {
        return _transformations;
    }
};



class MeshRenderer
{

private:
    MeshShader* _shader;
    MeshShader* _shader_noShadows;
    Material _material;
    Mesh _mesh;
    int _numIndices;
    VertexAttribArray _vao;
    std::vector<VertexBufferObject> _vbos;
    IndexBufferObject _ebo;
    std::optional<OGLTexture2D> _albedo;
    std::optional<OGLTexture2D> _normals;
    std::optional<OGLTexture2D> _roughness;
    std::optional<OGLTexture2D> _metallic;

    glm::mat4 _modelMatrix;

    int ResolveVertexInput(std::string inputName)
    {
        if (inputName._Equal("position")) return VertexInputType::Position;
        if (inputName._Equal("normal")) return VertexInputType::Normal;
        else return -1;

    }

    int ResolveNumberOfComponents(VertexInputType vInType)
    {
        switch (vInType)
        {
        case(VertexInputType::Position): return 3; break;
        case(VertexInputType::Normal): return 3; break;
        case(VertexInputType::Tangent): return 3; break;
        case(VertexInputType::Bitangent): return 3; break;
        case(VertexInputType::TextureCoordinates): return 2; break;

        default:
            throw " \n\n\t Unhandled vertex input type.\n\n";
        }
    }

public:
    MeshRenderer(glm::vec3 position, float rotation, glm::vec3 rotationAxis, glm::vec3 scale, Mesh mesh, MeshShader* shader, MeshShader* shaderNoShadows)
        :
        _shader(shader), _shader_noShadows(shaderNoShadows),_mesh(mesh),
        _ebo(IndexBufferObject::EBOType::StaticDraw),
        _vao()
    {
        _mesh = mesh;

        // Compute Model Transform Matrix ====================================================================================================== //
        SetTransformation(position, rotation, rotationAxis, scale);

        // Generate Graphics Data ============================================================================================================= //
        
        std::vector<std::pair<unsigned int, std::string>> vIns{ std::move(shader->GetVertexInput()) };

        _vbos = std::vector<VertexBufferObject>();
        
        // TODO: good enough? prabably ok for this small application
        // 
        //  layout(location = 0) in vec3 position;
        //  layout(location = 1) in vec3 normal;
        //  layout(location = 2) in vec2 textureCoordinates;
        //  layout(location = 3) in vec2 tangent;
        //  layout(location = 4) in vec2 bitangent;

        for ( int i = 0; i < VertexInputType::Last ; i++)
        {
            std::vector<float> dataVec;

            switch (i)
            {
                case(VertexInputType::Position):
                    dataVec = Utils::flatten3(mesh.GetPositions());
                    break;

                case(VertexInputType::Normal):
                    dataVec = Utils::flatten3(mesh.GetNormals());
                    break;

                case(VertexInputType::TextureCoordinates):
                    dataVec = Utils::flatten2(mesh.GetTextureCoordinates());
                    break;

                case(VertexInputType::Tangent):
                    dataVec = Utils::flatten3(mesh.GetTangents());
                    break;

                case(VertexInputType::Bitangent):
                    dataVec = Utils::flatten3(mesh.GetBitangents());
                    break;

                default:
                    throw " \n\n\t Mesh data initialization: unhandled case.\n\n";
            }

            if (dataVec.size() == 0)
                continue;

            // Filling a VBO for each attribute
            VertexBufferObject vbo{VertexBufferObject::VBOType::StaticDraw};
            vbo.SetData(dataVec.size(), dataVec.data());

            _vao.SetAndEnableAttrib(vbo.ID(), i, ResolveNumberOfComponents((VertexInputType)i), false, 0, 0);

            _vbos.push_back(std::move(vbo));
        }

        auto indicesVec(mesh.GetIndices());
        _numIndices = indicesVec.size();
        _ebo.SetData(indicesVec.size(), indicesVec.data());

        // "Saving" the index buffer
        _vao.SetIndexBuffer(_ebo.ID());
      
        SetMaterial(MaterialsCollection::NullMaterial);
    }

    MeshRenderer(glm::vec3 position, float rotation, glm::vec3 rotationAxis, glm::vec3 scale, Mesh mesh, MeshShader* shader, MeshShader* shaderNoShadows, Material mat)
        : MeshRenderer(position, rotation, rotationAxis, scale, mesh, shader, shaderNoShadows)
    {
        SetMaterial(mat);
    }

    MeshRenderer(Mesh mesh, MeshShader* shader, MeshShader* shaderNoShadows)
        : MeshRenderer(glm::vec3(0.0, 0.0, 0.0), 0, glm::vec3(1.0, 0.0, 0.0), glm::vec3(1.0, 1.0, 1.0), mesh, shader, shaderNoShadows)
    {

    }


public:
    void Draw(glm::vec3 eye, const SceneParams& sceneParams) const
    {

        MeshShader* shader = sceneParams.drawParams.doShadows ? _shader : _shader_noShadows;

        shader->SetCurrent();

        // Model + NormalMatrix ==============================================================================================//
        shader->SetMatrices(_modelMatrix);
       
        // Material Properties ===============================================================================================//
        shader->SetMaterial(_material, _albedo, _normals, _roughness, _metallic, sceneParams);
        
        shader->SetSamplers(sceneParams);

        // Draw Call =========================================================================================================//
        _vao.Bind();
        glDrawElements(GL_TRIANGLES, _numIndices, GL_UNSIGNED_INT, 0); 
        _vao.UnBind();

        OGLUtils::CheckOGLErrors();
    }

    void DrawCustom(const MeshShader& shader) const
    {

        shader.SetCurrent();

        // Model + NormalMatrix ==============================================================================================//
        shader.SetMatrices(_modelMatrix);
        
        // Draw Call =========================================================================================================//
        _vao.Bind();
        glDrawElements(GL_TRIANGLES, _numIndices, GL_UNSIGNED_INT, 0);
        _vao.UnBind();

        OGLUtils::CheckOGLErrors();
    }

    void SetMaterial(Material mat)
    {
        _material = mat;

        // Load Texture data if any
        if (_material.Textures != nullptr)
        {
            std::string path = std::string(MaterialsFolder) + "/" + _material.Textures;

            if(exists((path + "/Albedo.png").c_str()))
                _albedo = OGLTexture2D((path + "/Albedo.png").c_str(), TextureFiltering::Linear_Mip_Linear, TextureFiltering::Linear);

            if (exists((path + "/Roughness.png").c_str()))
                _roughness = OGLTexture2D((path + "/Roughness.png").c_str(), TextureFiltering::Linear_Mip_Linear, TextureFiltering::Linear);

            if (exists((path + "/Metallic.png").c_str()))
                _metallic = OGLTexture2D((path + "/Metallic.png").c_str(), TextureFiltering::Linear_Mip_Linear, TextureFiltering::Linear);

            if (exists((path + "/Normals.png").c_str()))
                _normals = OGLTexture2D((path + "/Normals.png").c_str(), TextureFiltering::Linear_Mip_Linear, TextureFiltering::Linear);
        }
    }

    void SetTransformation(glm::vec3 position, float rotation, glm::vec3 rotationAxis, glm::vec3 scale)
    {
        glm::mat4 model = glm::mat4(1.0f);

        model = glm::translate(model, position);
        model = glm::rotate(model, rotation, rotationAxis);
        model = glm::scale(model, scale);

        _modelMatrix = model;
    }

    void SetTransformation(glm::mat4 transformation)
    {
        _modelMatrix = transformation;
    }

    void Transform(glm::vec3 position, float rotation, glm::vec3 rotationAxis, glm::vec3 scale)
    {
        glm::mat4 model = glm::mat4(1.0f);

        model = glm::translate(model, position);  
        model = glm::rotate(model, rotation, rotationAxis);
        model = glm::scale(model, scale);

        _modelMatrix = model* _modelMatrix;
    }

    void Transform(glm::mat4 transformation)
    {
       _modelMatrix = transformation * _modelMatrix;
    }

    std::vector<glm::vec3> GetTransformedPoints()
    {
        std::vector<glm::vec3> transformed = _mesh.GetPositions();

        for (int i = 0; i < transformed.size(); i++)
        {
            transformed.at(i) = (glm::vec3)(_modelMatrix * glm::vec4(transformed.at(i), 1.0f));
        }

        return transformed;
    }
};

class LinesRenderer
{

private:
    MeshShader* _shader;
    Wire _wire;
    glm::vec4 _color;
    VertexAttribArray _vao;
    VertexBufferObject _vbo;
    int _numVertices;
    glm::mat4 _modelMatrix;

    // Looks like a duplicate of MeshRenderer::ResolveVertexInput 
    // but I still don't know if they'll stay the same in the future
    int ResolveVertexInput(std::string inputName)
    {
        if (inputName._Equal("position")) return VertexInputType::Position;
        if (inputName._Equal("normal")) return VertexInputType::Normal;
        else return -1;

    }

public:
    LinesRenderer(glm::vec3 position, float rotation, glm::vec3 rotationAxis, glm::vec3 scale, Wire wire, MeshShader* shader, glm::vec4 color)
        :
        _shader(shader), _wire(wire), _color(color), _vao(), _vbo(VertexBufferObject::VBOType::StaticDraw)
    {

        // Compute Model Transform Matrix ====================================================================================================== //
        glm::mat4 model = glm::mat4(1.0f);

        model = glm::translate(model, position);

        model = glm::rotate(model, rotation, rotationAxis);

        model = glm::scale(model, scale);

        _modelMatrix = model;

        // Generate Graphics Data ============================================================================================================= //
        _numVertices = wire.GetPositions().size();
        std::vector<float> positionsFloatArray = Utils::flatten3(wire.GetPositions());
        _vbo.SetData(positionsFloatArray.size(), positionsFloatArray.data());

        std::vector<std::pair<unsigned int, std::string>> vIns{ std::move(shader->GetVertexInput()) };

        bool hasPositions = false;
        for (auto& vIn : vIns)
        {
            std::vector<float> dataVec;

            if (ResolveVertexInput(vIn.second) == VertexInputType::Position)
            {
                hasPositions = true;
                _vao.SetAndEnableAttrib(_vbo.ID(), vIn.first, 3, false, 0, 0);
            }
        }
        if (!hasPositions)
            throw "No vertex position data provided to the LinesRenderer.";

        OGLUtils::CheckOGLErrors();
    }



public:
    void Draw(glm::mat4 view, glm::mat4 proj, glm::vec3 eye, SceneLights lights)
    {

        _shader->SetCurrent();

        // ModelViewProjection + NormalMatrix =========================================================================================================//
        _shader->SetMatrices(_modelMatrix);
        
        // Material Properties =========================================================================================================//
        _shader->SetMaterial(_color, _color, 0.0f);
        
        // Draw Call =========================================================================================================//
        _vao.Bind();
        glDrawArrays(GL_LINES, 0, _numVertices);
        _vao.UnBind();

        CheckOGLErrors();
    }

    void Transform(glm::vec3 position, float rotation, glm::vec3 rotationAxis, glm::vec3 scale)
    {
        glm::mat4 model = glm::mat4(1.0f);

        model = glm::translate(model, position);
        model = glm::rotate(model, rotation, rotationAxis);
        model = glm::scale(model, scale);

        _modelMatrix = model;

    }
private:
    void CheckOGLErrors()
    {
        GLenum error = 0;
        std::string log = "";
        while ((error = glGetError()) != GL_NO_ERROR)
        {
            log += error;
            log += ' ,';
        }
        if (!log.empty())
            throw log;
    }
};





#endif
