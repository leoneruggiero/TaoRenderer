#ifndef MESH_H
#define MESH_H

#include <glad/glad.h>
#include <assert.h>
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
#include "Shader.h"
#include "Scene.h"

using namespace OGLResources;

enum class WireNature
{
    POINTS = 0,
    LINES = 1,
    LINE_STRIP = 2
};

class ShaderBase
{
private:
    OGLResources::ShaderProgram _shaderProgram;
    std::string _vertexCode;
    std::string _geometryCode;
    std::string _fragmentCode;

protected:
    void SetUBOBindings()
    {

        unsigned int blkIndex = -1;

        constexpr static const char* uboNames[] =
        {
            "blk_PerFrameData",
            "blk_PerFrameData_Lights",
            "blk_PerFrameData_Shadows",
            "blk_PerFrameData_Ao"
        };

        for (int i = 0; i <= UBOBinding::AmbientOcclusion; i++)
        {
            if ((blkIndex = glGetUniformBlockIndex(ShaderBase::ShaderProgramId(), uboNames[i])) != GL_INVALID_INDEX)
                glUniformBlockBinding(ShaderBase::ShaderProgramId(), blkIndex, i);
        };

        OGLUtils::CheckOGLErrors();

    }

public:

    ShaderBase(std::string vertexSource, std::string fragmentSource) :
        _vertexCode(vertexSource), _fragmentCode(fragmentSource), _shaderProgram(vertexSource, fragmentSource)
    {
    }

    ShaderBase(std::string vertexSource, std::string geometrySource, std::string fragmentSource) :
        _vertexCode(vertexSource), _fragmentCode(fragmentSource), _geometryCode(geometrySource), _shaderProgram(vertexSource, fragmentSource, geometrySource)
    {
    }

    void SetCurrent() const { _shaderProgram.Enable(); }

    std::vector<std::pair<unsigned int, std::string>> GetVertexInput() { return _shaderProgram.GetInput(); }

    int UniformLocation(std::string name) const { return glGetUniformLocation(_shaderProgram.ID(), name.c_str()); };

    unsigned int ShaderProgramId() { return _shaderProgram.ID(); };

    virtual void SetMatrices(glm::mat4 modelMatrix) const {};
};

class WiresShader : public ShaderBase
{

public:
    WiresShader(std::vector<std::string> vertexExpansions, std::vector<std::string> fragmentExpansions, WireNature wireNature) :
        ShaderBase(

            VertexSource_Geometry::Expand(
                VertexSource_Geometry::EXP_VERTEX,
                vertexExpansions),

            ResolveGeometryShader(wireNature),

            FragmentSource_Geometry::Expand(
                FragmentSource_Geometry::EXP_FRAGMENT,
                fragmentExpansions))
    {
        ShaderBase::SetUBOBindings();
    };

    void SetColor(glm::vec4 color)
    {
        glUniform4fv(UniformLocation("material.Albedo"), 1, glm::value_ptr(color));
    }

    void SetMatrices(glm::mat4 modelMatrix) const override
    {
        glUniformMatrix4fv(UniformLocation("model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
    }

private:
    std::string ResolveGeometryShader(WireNature nature)
    {
        switch (nature)
        {
        case WireNature::POINTS:
            return GeometrySource_Geometry::THICK_POINTS;
        case WireNature::LINES:
            return GeometrySource_Geometry::THICK_LINES;
            break;
        case WireNature::LINE_STRIP:
            return GeometrySource_Geometry::THICK_LINE_STRIP;
            break;
        default:
            break;
        }
    }
};

class MeshShader : public ShaderBase
{

public:
    MeshShader(std::vector<std::string> vertexExpansions, std::vector<std::string> fragmentExpansions) :
        ShaderBase(

            VertexSource_Geometry::Expand(
                VertexSource_Geometry::EXP_VERTEX,
                vertexExpansions),

            FragmentSource_Geometry::Expand(
                FragmentSource_Geometry::EXP_FRAGMENT,
                fragmentExpansions))
    {
        ShaderBase::SetUBOBindings();
    };

    void SetMatrices(glm::mat4 modelMatrix) const
    {
        glUniformMatrix4fv(UniformLocation("model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
        glUniformMatrix4fv(UniformLocation("normalMatrix"), 1, GL_FALSE, glm::value_ptr(glm::transpose(glm::inverse(modelMatrix))));

    }

    void SetMaterial(const Material& mat,
        const std::optional<OGLTexture2D>& albedoTexture,
        const std::optional<OGLTexture2D>& normalsTexture,
        const std::optional<OGLTexture2D>& roughnessTexture,
        const std::optional<OGLTexture2D>& metallicTexture,
        const SceneParams& sceneParams) const
    {
        // TODO: uniform block?
        glUniform4fv(UniformLocation("material.Albedo"), 1, glm::value_ptr(mat.Albedo));
        glUniform1f(UniformLocation("material.Roughness"), mat.Roughness);
        glUniform1f(UniformLocation("material.Metallic"), mat.Metallic);

        // ***  ALBEDO color texture *** //
        // ----------------------------- //
        if (albedoTexture.has_value())
        {
            glUniform1ui(UniformLocation("material.hasAlbedo"), true);

            glUniform1ui(UniformLocation("u_doGammaCorrection"), sceneParams.postProcessing.GammaCorrection);
            glUniform1f(UniformLocation("u_gamma"), 2.2f);

            glActiveTexture(GL_TEXTURE0 + TextureBinding::Albedo);
            albedoTexture.value().Bind();
            glUniform1i(UniformLocation("Albedo"), TextureBinding::Albedo);
        }
        else
            glUniform1ui(UniformLocation("material.hasAlbedo"), false);
        // ----------------------------- //


        // ***  NORMAL map texture *** //
        // --------------------------- //
        if (normalsTexture.has_value())
        {
            glUniform1ui(UniformLocation("material.hasNormals"), true);

            glActiveTexture(GL_TEXTURE0 + TextureBinding::Normals);
            normalsTexture.value().Bind();
            glUniform1i(UniformLocation("Normals"), TextureBinding::Normals);
        }
        else
            glUniform1ui(UniformLocation("material.hasNormals"), false);
        // --------------------------- //

        // ***  ROUGHNESS map texture *** //
        // ------------------------------ //
        if (roughnessTexture.has_value())
        {
            glUniform1ui(UniformLocation("material.hasRoughness"), true);

            glActiveTexture(GL_TEXTURE0 + TextureBinding::Roughness);
            roughnessTexture.value().Bind();
            glUniform1i(UniformLocation("Roughness"), TextureBinding::Roughness);
        }
        else
            glUniform1ui(UniformLocation("material.hasRoughness"), false);
        // ------------------------------ //

        // ***  METALLIC map texture *** //
        // ----------------------------- //
        if (metallicTexture.has_value())
        {
            glUniform1ui(UniformLocation("material.hasMetallic"), true);

            glActiveTexture(GL_TEXTURE0 + TextureBinding::Metallic);
            metallicTexture.value().Bind();
            glUniform1i(UniformLocation("Metallic"), TextureBinding::Metallic);
        }
        else
            glUniform1ui(UniformLocation("material.hasMetallic"), false);
        // ----------------------------- //
    }

    void SetMaterial(const glm::vec4& diffuse, const glm::vec4& specular, float shininess) const
    {
        // TODO: uniform block?
        glUniform4fv(UniformLocation("material.Diffuse"), 1, glm::value_ptr(diffuse));
        glUniform4fv(UniformLocation("material.Specular"), 1, glm::value_ptr(specular));
        glUniform1f(UniformLocation("material.Shininess"), shininess);

    }

    void SetSamplers(const SceneParams& sceneParams)
    {
        // TODO bind texture once for all
        if (sceneParams.sceneLights.Directional.ShadowMapId)
        {
            glActiveTexture(GL_TEXTURE0 + TextureBinding::ShadowMap);
            glBindTexture(GL_TEXTURE_2D, sceneParams.sceneLights.Directional.ShadowMapId);
            glUniform1i(UniformLocation("shadowMap"), TextureBinding::ShadowMap);

            // Noise textures for PCSS calculations
            // ------------------------------------
            glActiveTexture(GL_TEXTURE0 + TextureBinding::NoiseMap0);
            sceneParams.noiseTextures.at(0).Bind();
            glUniform1i(UniformLocation("noiseTex_0"), TextureBinding::NoiseMap0);

            glActiveTexture(GL_TEXTURE0 + TextureBinding::NoiseMap1);
            sceneParams.noiseTextures.at(1).Bind();
            glUniform1i(UniformLocation("noiseTex_1"), TextureBinding::NoiseMap1);

            glActiveTexture(GL_TEXTURE0 + TextureBinding::NoiseMap2);
            sceneParams.noiseTextures.at(2).Bind();
            glUniform1i(UniformLocation("noiseTex_2"), TextureBinding::NoiseMap2);

            glActiveTexture(GL_TEXTURE0 + TextureBinding::PoissonSamples);
            if (sceneParams.poissonSamples.has_value())
                sceneParams.poissonSamples.value().Bind();
            glUniform1i(UniformLocation("poissonSamples"), TextureBinding::PoissonSamples);

        }

        if (sceneParams.sceneLights.Ambient.AoMapId)
        {
            glActiveTexture(GL_TEXTURE0 + TextureBinding::AoMap);
            glBindTexture(GL_TEXTURE_2D, sceneParams.sceneLights.Ambient.AoMapId);
            glUniform1i(UniformLocation("aoMap"), TextureBinding::AoMap);
        }

        if (sceneParams.environment.IrradianceMap.has_value())
        {
            glUniform1ui(UniformLocation("u_hasIrradianceMap"), true);
            glUniform1f(UniformLocation("u_environmentIntensity"), sceneParams.environment.intensity);
            glActiveTexture(GL_TEXTURE0 + TextureBinding::IrradianceMap);
            sceneParams.environment.IrradianceMap.value().Bind();
            glUniform1i(UniformLocation("IrradianceMap"), TextureBinding::IrradianceMap);
        }
        else
            glUniform1ui(UniformLocation("u_hasIrradianceMap"), false);
    }

};

class RenderableBasic
{
public:
    virtual std::vector<glm::vec3> GetPositions() { return std::vector<glm::vec3>(1); };
    virtual std::vector<glm::vec3> GetNormals() { return std::vector<glm::vec3>(0); };
    virtual std::vector<glm::vec2> GetTextureCoordinates() { return std::vector<glm::vec2>(0); };
    virtual std::vector<glm::vec3> GetTangents() { return std::vector<glm::vec3>(0); };
    virtual std::vector<glm::vec3> GetBitangents() { return std::vector<glm::vec3>(0); };
    virtual std::vector<int> GetIndices() { return std::vector<int>(0); };
};

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

class Wire : public RenderableBasic
{
private:
    std::vector<glm::vec3> _points;
    WireNature _nature;
public:
    Wire(const std::vector<glm::vec3> points, WireNature nature) : _points{ points }, _nature(nature) {};
    std::vector<glm::vec3> GetPositions() override { return std::vector<glm::vec3>(_points); };
    WireNature GetNature() const { return _nature; };
    int NumPoints() const { return _points.size(); };

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

        return Wire(points, WireNature::LINES);
    }
    static Wire Circle(int subdivisions, float radius)
    {
        std::vector<glm::vec3> points{};
        points.reserve(subdivisions+1);
        
        float da = glm::pi<float>() * 2.0f / subdivisions;
        for (int i = 0; i < subdivisions; i++)

            points.push_back(glm::vec3(
                glm::cos(da * i),
                glm::sin(da * i),
                0.0f));
        
        points.push_back(glm::vec3(
            glm::cos(da * subdivisions),
            glm::sin(da * subdivisions),
            0.0f));

        return Wire(points, WireNature::LINE_STRIP);
    }
    static Wire Rect(glm::vec2 min, glm::vec2 max)
    {
        std::vector<glm::vec3> points
        {
            glm::vec3(min, 0.0f),
            glm::vec3(max.x, min.y, 0.0f),
            glm::vec3(max, 0.0f),
            glm::vec3(min.x, max.y, 0.0f),
            glm::vec3(min, 0.0f)
        };
        return Wire(points, WireNature::LINE_STRIP);
    }
    static Wire Square(glm::vec2 center, float size)
    {
        return Rect(center, center + glm::vec2(size));
    }
    static Wire Square(float size)
    {
        return Square(glm::vec2(0.0f, 0.0f), size);
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
        
        if (uvCoordinates.size() > 0)
            return Mesh(vertices, normals, triangles, uvCoordinates, tangents);
        else
            return Mesh(vertices, normals, triangles);
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

class Renderer
{
public:

    // Returns the list of points according to the object's transform
    // --------------------------------------------------------------
    virtual std::vector<glm::vec3> GetTransformedPoints() = 0;

    // Draw with the associated shader and material
    // --------------------------------------------------------------
    virtual void Draw(glm::vec3 eye, const SceneParams& sceneParams) const = 0;

    // Draw with a custom shader
    // --------------------------------------------------------------
    virtual void DrawCustom(ShaderBase* shader) const = 0;

    virtual void SetMaterial(Material mat) = 0;

    // Overrides the old transformation with the one provided
    // --------------------------------------------------------------
    virtual void SetTransformation(glm::mat4 transformation) = 0; // => implemented by derived classes and called by the base.

    virtual void SetTransformation(glm::vec3 position, float rotation, glm::vec3 rotationAxis, glm::vec3 scale) 
    {
        glm::mat4 model = glm::mat4(1.0f);

        model = glm::translate(model, position);
        model = glm::rotate(model, rotation, rotationAxis);
        model = glm::scale(model, scale);

        SetTransformation(model);
    }

    virtual void SetTransformation(glm::vec3 position)
    {
        glm::mat4 model = glm::mat4(1.0f);

        model = glm::translate(model, position);
       
        SetTransformation(model);
    }

    // Transforms the object 
    // --------------------------------------------------------------
    virtual void Transform(glm::mat4 transformation) = 0; // => implemented by derived classes and called by the base.

    virtual void Transform(glm::vec3 position, float rotation, glm::vec3 rotationAxis, glm::vec3 scale)
    {
        glm::mat4 model = glm::mat4(1.0f);

        model = glm::translate(model, position);
        model = glm::rotate(model, rotation, rotationAxis);
        model = glm::scale(model, scale);
        Transform(model);
    }

    void Translate(glm::vec3 translation)
    {
        glm::mat4 tr = glm::mat4(1.0f);
        tr = glm::translate(tr, translation);
        Transform(tr);
    }

    void Translate(float x, float y, float z)
    {
        Translate(glm::vec3(x, y, z));
    }
    
};

class MeshRenderer : public Renderer
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
        Renderer::SetTransformation(position, rotation, rotationAxis, scale);

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

    void Draw(glm::vec3 eye, const SceneParams& sceneParams) const override
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

    void DrawCustom(ShaderBase* shader) const override
    {

        shader->SetCurrent();

        // Model + NormalMatrix ==============================================================================================//
        shader->SetMatrices(_modelMatrix);
        
        // Draw Call =========================================================================================================//
        _vao.Bind();
        glDrawElements(GL_TRIANGLES, _numIndices, GL_UNSIGNED_INT, 0);
        _vao.UnBind();

        OGLUtils::CheckOGLErrors();
    }

    void SetMaterial(Material mat) override
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

    void SetTransformation(glm::mat4 transformation) override { _modelMatrix = transformation; };

    void Transform(glm::mat4 transformation) override { _modelMatrix = transformation * _modelMatrix; };

    std::vector<glm::vec3> GetTransformedPoints() override
    {
        std::vector<glm::vec3> transformed = _mesh.GetPositions();

        for (int i = 0; i < transformed.size(); i++)
        {
            transformed.at(i) = (glm::vec3)(_modelMatrix * glm::vec4(transformed.at(i), 1.0f));
        }

        return transformed;
    }
};

class WiresRenderer : public Renderer
{

private:
    WiresShader* _shader;
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
    WiresRenderer(glm::vec3 position, float rotation, glm::vec3 rotationAxis, glm::vec3 scale, Wire wire, WiresShader* shader, glm::vec4 color)
        : WiresRenderer(wire, shader)
    {

        glm::mat4 model = glm::mat4(1.0f);

        model = glm::translate(model, position);
        model = glm::rotate(model, rotation, rotationAxis);
        model = glm::scale(model, scale);

        SetTransformation(model);

        SetColor(color);
    }

    WiresRenderer(Wire wire, WiresShader* shader)
        :
        _shader(shader), _wire(wire), _vao(), _vbo(VertexBufferObject::VBOType::StaticDraw)
    {

        // Model => Identity matrix
        _modelMatrix = glm::mat4(1.0f);

        // Color => Magenta
        _color = glm::vec4(1.0, 0.0, 1.0, 1.0);
        // ---------------------------------------

        // Generationg graphics data 
        // ---------------------------------------
        _numVertices = wire.GetPositions().size();
        std::vector<glm::vec3> positions = wire.GetPositions();

        if (positions.size() < 2)
            throw ("2 vertices needed for a line.");

        glm::vec3
            start = positions[0],
            end = positions[positions.size() - 1];

        // Happily naive....tolerances are not fun
        bool isClosed = glm::all(glm::equal(start, end, 0.1f));


        // lines with adjacency data
        if (_wire.GetNature() == WireNature::LINE_STRIP)
        {
            // If closed fill adj data rotating the array
            if (isClosed)
            {
                start = positions[positions.size() - 2],
                    end = positions[1];
            }
            else
            {
                start = positions[0] + positions[0] - positions[1],
                    end = positions[positions.size() - 1] + positions[positions.size() - 1] - positions[positions.size() - 2];
            }

            positions.insert(positions.end(), end);
            positions.insert(positions.begin(), start);
            _numVertices += 2;
        }

        std::vector<float> positionsFloatArray = Utils::flatten3(positions);

        _vbo.SetData(positionsFloatArray.size(), positionsFloatArray.data());

        _vao.SetAndEnableAttrib(_vbo.ID(), VertexInputType::Position, 3, false, 0, 0);

        OGLUtils::CheckOGLErrors();

        // ---------------------------------------

        OGLUtils::CheckOGLErrors();
    }

    
    void Draw(glm::vec3 eye, const SceneParams& sceneParams) const override
    {
       
        _shader->SetCurrent();
        _shader->SetMatrices(_modelMatrix);
        _shader->SetColor(_color);

        glm::vec2 screenToWorldFactor = glm::vec2(1.0 / sceneParams.viewportWidth, 1.0 / sceneParams.viewportHeight);

        glUniform2fv(_shader->UniformLocation("u_screenToWorld"), 1, glm::value_ptr(screenToWorldFactor));
        glUniform1ui(_shader->UniformLocation("u_thickness"),
            _wire.GetNature() == WireNature::POINTS
            ? sceneParams.pointWidth
            : sceneParams.lineWidth);
        glUniform1f(_shader->UniformLocation("u_near"), sceneParams.cameraNear);

        _vao.Bind();

        glDrawArrays(ResolveDrawingMode(_wire.GetNature()), 0, _numVertices);

        _vao.UnBind();

        OGLUtils::CheckOGLErrors();
    }

    void DrawCustom(ShaderBase* shader) const override
    {
        shader->SetCurrent();

        // ModelViewProjection + NormalMatrix =========================================================================================================//
        shader->SetMatrices(_modelMatrix);

        // Draw Call =========================================================================================================//
        _vao.Bind();
        glDrawArrays(
            _wire.GetNature() == WireNature::LINES
            ? GL_LINES
            : GL_POINTS
            , 0, _numVertices);

        _vao.UnBind();

        OGLUtils::CheckOGLErrors();
    }

    void SetMaterial(Material mat) override { _color = mat.Albedo; };

    void SetColor(glm::vec4 color)  { _color = color; };

    void SetTransformation(glm::mat4 transformation) override { _modelMatrix = transformation; };

    void Transform(glm::mat4 transformation) override { _modelMatrix = transformation * _modelMatrix; };

    std::vector<glm::vec3> GetTransformedPoints() override
    {
        std::vector<glm::vec3> transformed = _wire.GetPositions();

        for (int i = 0; i < transformed.size(); i++)
        {
            transformed.at(i) = (glm::vec3)(_modelMatrix * glm::vec4(transformed.at(i), 1.0f));
        }

        return transformed;
    }

    private:
        GLenum ResolveDrawingMode(const WireNature &nature) const
        {
            switch (nature)
            {
            case WireNature::POINTS: return     GL_POINTS;                  break;
            case WireNature::LINES: return      GL_LINES;                   break;
            case WireNature::LINE_STRIP: return GL_LINE_STRIP_ADJACENCY;    break;

            default:
                break; throw "Unsupported WireNature. Allowed values are: POINTS, LINES, LINE_STRIP";
            }
        }
};

class SceneMeshCollection /* TODO : public std::vector<std::shared_ptr<MeshRenderer>>::iterator*/
{
private:
    std::vector<std::shared_ptr<Renderer>> _meshRendererCollection;
    Utils::BoundingBox<glm::vec3> _sceneAABBox;

public:
    SceneMeshCollection() : _sceneAABBox(std::vector<glm::vec3>()) // TODO: Why bbox dowsn't have a default constructor???
    {

    }


    void AddToCollection(std::shared_ptr<Renderer> meshRenderer)
    {
        _meshRendererCollection.push_back(meshRenderer);
        _sceneAABBox.Update(meshRenderer.get()->GetTransformedPoints());
    }

    void UpdateBBox()
    {
        _sceneAABBox = Utils::BoundingBox<glm::vec3>(std::vector<glm::vec3>());
        for (auto& m : _meshRendererCollection)
            _sceneAABBox.Update(m.get()->GetTransformedPoints());
    }

    const std::shared_ptr<Renderer>& at(const size_t Pos) const { return _meshRendererCollection.at(Pos); }

    size_t size() const { return _meshRendererCollection.size(); }

    // TODO return reference instead of vec3?
    float GetSceneBBSize() const { return _sceneAABBox.Size(); }
    glm::vec3 GetSceneBBExtents() const { return _sceneAABBox.Diagonal(); }
    glm::vec3 GetSceneBBCenter() const { return _sceneAABBox.Center(); }
    std::vector<glm::vec3> GetSceneBBPoints() const { return _sceneAABBox.GetPoints(); }
    std::vector<glm::vec3> GetSceneBBLines() const { return _sceneAABBox.GetLines(); }


};

class PostProcessingShader : public ShaderBase
{
public:
    PostProcessingShader(std::vector<std::string> vertexExpansions, std::vector<std::string> fragmentExpansions) :
        ShaderBase(

            VertexSource_PostProcessing::Expand(
                VertexSource_PostProcessing::EXP_VERTEX,
                vertexExpansions),

            FragmentSource_PostProcessing::Expand(
                FragmentSource_PostProcessing::EXP_FRAGMENT,
                fragmentExpansions)) {};

    virtual int PositionLayout() { return 0; };
};

class PostProcessingUnit
{

private:
    PostProcessingShader
        _blurShader, _viewFromDepthShader, _ssaoShader, _hbaoShader, _toneMappingAndGammaCorrection;

    std::optional<OGLTexture2D>
        _gaussianBlurValuesTexture, _aoNoiseTexture;

    std::vector<glm::vec3> _ssaoDirectionsToSample;

    /*??? static ???*/ VertexBufferObject _vbo;
    /*??? static ???*/ VertexAttribArray _vao;


    void DrawQuad() const
    {
        _vao.Bind();
        glDrawArrays(GL_TRIANGLES, 0, 6);
        _vao.UnBind();
    }

    void InitBlur(int maxRadius)
    {
        // Create a 2D texture to store gaussian blur kernel values
        // https://www.rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
        std::vector<std::vector<int>> pascalValues = ComputePascalOddRows(maxRadius);
        std::vector<int> flattenedPascalValues = Utils::flatten(pascalValues);

        _gaussianBlurValuesTexture = OGLTexture2D(
            maxRadius, maxRadius,
            TextureInternalFormat::R_16ui,
            flattenedPascalValues.data(), GL_RED_INTEGER, GL_INT);
    }


    void InitAo(int maxDirectionsToSample)
    {
        // ssao random rotation texture
        std::default_random_engine generator;
        std::uniform_real_distribution<float> distribution(-1.0, 1.0);
        std::vector<glm::vec3> ssaoNoise;
        for (unsigned int i = 0; i < 16; i++)
        {
            glm::vec3 noise(
                distribution(generator),
                distribution(generator),
                0.0f);
            ssaoNoise.push_back(noise);
        }
        std::vector<float> ssaoNoiseFlat(Utils::flatten3(ssaoNoise));

        _aoNoiseTexture = OGLTexture2D(
            4, 4,
            TextureInternalFormat::Rgb_16f,
            ssaoNoiseFlat.data(), GL_RGB, GL_FLOAT,
            TextureFiltering::Nearest, TextureFiltering::Nearest,
            TextureWrap::Repeat, TextureWrap::Repeat);


        // ssao directions (set as uniform when executing)
        _ssaoDirectionsToSample.reserve(maxDirectionsToSample);

        for (unsigned int i = 0; i < maxDirectionsToSample; i++)
        {
            glm::vec3 noise(
                distribution(generator),
                distribution(generator),
                distribution(generator) * 0.35f + 0.65f);
            noise = glm::normalize(noise);

            float x = distribution(generator) * 0.5f + 0.5f;
            noise *= Utils::Lerp(0.1, 1.0, x * x);
            _ssaoDirectionsToSample.push_back(noise);
        }

    }

public:
    PostProcessingUnit(int maxBlurRadius, int maxAODirectionsToSample) :
        _blurShader(std::vector<std::string>{}, std::vector<std::string>{"DEFS_GAUSSIAN_BLUR", "CALC_GAUSSIAN_BLUR"}),
        _viewFromDepthShader(std::vector<std::string>{}, std::vector<std::string>{"DEFS_SSAO", "CALC_POSITIONS"}),
        _ssaoShader(std::vector<std::string>{}, std::vector<std::string>{"DEFS_SSAO", "CALC_SSAO"}),
        _hbaoShader(std::vector<std::string>{}, std::vector<std::string>{"DEFS_SSAO", "CALC_HBAO"}),
        _toneMappingAndGammaCorrection(
            std::vector<std::string>{},
            std::vector<std::string>{"DEFS_TONE_MAPPING_AND_GAMMA_CORRECTION", "CALC_TONE_MAPPING_AND_GAMMA_CORRECTION"}),
        _gaussianBlurValuesTexture(),
        _vbo(VertexBufferObject::VBOType::StaticDraw),
        _vao()
    {
        // FIl VBO and VAO for the full screen quad used to trigger the fragment shader execution
        _vbo.SetData(18, Utils::fullScreenQuad_verts);
        _vao.SetAndEnableAttrib(_vbo.ID(), 0, 3, false, 0, 0);

        InitBlur(maxBlurRadius);
        InitAo(maxAODirectionsToSample);
    }

    void IntegrateBRDFintoLUT( SceneParams& sceneParams)
    {
        OGLResources::FrameBuffer fbo = OGLResources::FrameBuffer(512, 512, true, 1, true, TextureInternalFormat::Rg_16f);

        PostProcessingShader shader = PostProcessingShader(
            std::vector<std::string>(
                {/* NO VERTEX_SHADER EXPANSIONS */ }),
            std::vector<std::string>(
                {
                "DEFS_BRDF_LUT",
                "CALC_BRDF_LUT"
                }
        ));

        fbo.Bind();
        shader.SetCurrent();

        glViewport(0, 0, 512, 512);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUniform2i(shader.UniformLocation("u_screenSize"), 512, 512);

        glDisable(GL_DEPTH_TEST);

        DrawQuad();

        glEnable(GL_DEPTH_TEST);

        glViewport(0, 0, sceneParams.viewportWidth, sceneParams.viewportHeight);
        fbo.UnBind();

        sceneParams.environment.LUT =
            OGLTexture2D(512, 512, TextureInternalFormat::Rg_16f, TextureFiltering::Linear, TextureFiltering::Linear);

        
        glCopyImageSubData(
            // Source
            fbo.ColorTextureId(), GL_TEXTURE_2D, 0, 0, 0, 0, 

            // Dest
            sceneParams.environment.LUT.value().ID(), GL_TEXTURE_2D, 0, 0, 0, 0, 

            // Size
            512, 512, 1);

       
        // Release graphics resources
        shader.~PostProcessingShader();
        fbo.~FrameBuffer();

    }

    void BlurTexture(const FrameBuffer& fbo, int attachment, int radius, int width, int height) const
    {
        // Blur pass
        glDisable(GL_DEPTH_TEST);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fbo.ColorTextureId(attachment));

        glActiveTexture(GL_TEXTURE1);
        _gaussianBlurValuesTexture.value().Bind();

        _blurShader.SetCurrent();

        //TODO: block?
        glUniform1i(_blurShader.UniformLocation("u_texture"), 0);
        glUniform1i(_blurShader.UniformLocation("u_weights_texture"), 1);
        glUniform1i(_blurShader.UniformLocation("u_radius"), radius);

        glUniform1i(_blurShader.UniformLocation("u_hor"), 1); // => HORIZONTAL PASS

        DrawQuad();

        fbo.CopyFromOtherFbo(nullptr, true, attachment, false, glm::vec2(0.0, 0.0), glm::vec2(width, height));

        glUniform1i(_blurShader.UniformLocation("u_hor"), 0); // => VERTICAL PASS

        DrawQuad();

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glEnable(GL_DEPTH_TEST);

        fbo.CopyFromOtherFbo(nullptr, true, attachment, false, glm::vec2(0.0, 0.0), glm::vec2(width, height));

    }

    void ApplyToneMappingAndGammaCorrection(const FrameBuffer& fbo, int attachment, bool toneMapping, float exposure, bool gammaCorrection, float gamma) const
    {

        glDisable(GL_DEPTH_TEST);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fbo.ColorTextureId(attachment));

        _toneMappingAndGammaCorrection.SetCurrent();

        glUniform1i(_toneMappingAndGammaCorrection.UniformLocation(" u_hdrBuffer"), 0);
        glUniform1i(_toneMappingAndGammaCorrection.UniformLocation("u_doToneMapping"), toneMapping);
        glUniform1i(_toneMappingAndGammaCorrection.UniformLocation("u_doGammaCorrection"), gammaCorrection);
        glUniform1f(_toneMappingAndGammaCorrection.UniformLocation("u_exposure"), exposure);
        glUniform1f(_toneMappingAndGammaCorrection.UniformLocation("u_gamma"), gamma);

        DrawQuad();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glEnable(GL_DEPTH_TEST);

    }

    void ComputeAO(AOType aoType, const FrameBuffer& fbo, const SceneParams& sceneParams, int width, int height) const
    {
        if (aoType == AOType::NONE)
            return;

        // Extract view positions from depth
        // the fbo should contain relevant depth information
        // at the end the fbo contains the same depth information + not blurred ao map in COLOR_ATTACHMENT0
        glDisable(GL_DEPTH_TEST);

        fbo.Bind(false, true);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fbo.DepthTextureId());

        _viewFromDepthShader.SetCurrent();
        glUniform1i(_viewFromDepthShader.UniformLocation("u_depthTexture"), 0);
        glUniform1f(_viewFromDepthShader.UniformLocation("u_near"), sceneParams.cameraNear);
        glUniform1f(_viewFromDepthShader.UniformLocation("u_far"), sceneParams.cameraFar);
        glUniformMatrix4fv(_viewFromDepthShader.UniformLocation("u_proj"), 1, GL_FALSE, glm::value_ptr(sceneParams.projectionMatrix));

        DrawQuad();

        glBindTexture(GL_TEXTURE_2D, 0);
        fbo.UnBind();

        // Compute SSAO
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fbo.ColorTextureId());     // eye fragment positions => TEXTURE0

        glActiveTexture(GL_TEXTURE1);
        _aoNoiseTexture.value().Bind();                         // random rotation        => TEXTURE1

        if (sceneParams.noiseTextures.size() == 3)
        {
            glActiveTexture(GL_TEXTURE2);
            sceneParams.noiseTextures.at(0).Bind();  // noise 0                => TEXTURE2
            glActiveTexture(GL_TEXTURE3);
            sceneParams.noiseTextures.at(1).Bind();  // noise 1                => TEXTURE3
            glActiveTexture(GL_TEXTURE4);
            sceneParams.noiseTextures.at(2).Bind();  // noise 2                => TEXTURE4
        }

        const PostProcessingShader& aoShader = (aoType == AOType::SSAO ? _ssaoShader : _hbaoShader);

        aoShader.SetCurrent();

        glUniform1i(aoShader.UniformLocation("u_viewPosTexture"), 0);
        glUniform1i(aoShader.UniformLocation("u_rotVecs"), 1);
        glUniform1i(aoShader.UniformLocation("noiseTex_0"), 2);
        glUniform1i(aoShader.UniformLocation("noiseTex_1"), 3);
        glUniform1i(aoShader.UniformLocation("noiseTex_2"), 4);

        glUniform1f(aoShader.UniformLocation("u_ssao_radius"), sceneParams.sceneLights.Ambient.aoRadius);
        glUniform3fv(aoShader.UniformLocation("u_rays"), sceneParams.sceneLights.Ambient.aoSamples, &_ssaoDirectionsToSample[0].x);
        glUniform1i(aoShader.UniformLocation("u_numSamples"), sceneParams.sceneLights.Ambient.aoSamples);
        glUniform1i(aoShader.UniformLocation("u_numSteps"), sceneParams.sceneLights.Ambient.aoSteps);
        glUniform1f(aoShader.UniformLocation("u_radius"), sceneParams.sceneLights.Ambient.aoRadius);
        glUniform1f(aoShader.UniformLocation("u_near"), sceneParams.cameraNear);
        glUniform1f(aoShader.UniformLocation("u_far"), sceneParams.cameraFar);
        glUniformMatrix4fv(aoShader.UniformLocation("u_proj"), 1, GL_FALSE, glm::value_ptr(sceneParams.projectionMatrix));

        DrawQuad();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, 0);

        fbo.CopyFromOtherFbo(nullptr, true, 0, false, glm::vec2(0.0, 0.0), glm::vec2(width, height));

        glEnable(GL_DEPTH_TEST);
    }
};

#endif
