#pragma once

#include "RenderContext.h"
#include "RenderContextUtils.h"

#include <list>
#include <optional>
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat3x3.hpp"
#include "glm/mat3x2.hpp"

namespace tao_pbr
{
    template<typename T>
    struct GenKey
    {
        using type = T;

        unsigned int Generation;
        std::size_t Index;
    };

    template<typename T>
    class GenKeyVector
    {
    public:
        GenKeyVector() :
                _vector{}, _generation{},  _free{}, _freeList{}
        {

        }

        bool keyValid(const GenKey<T>& key) const
        {
            return
                    key.Index<_generation.size()            &&
                    key.Index<_vector.size()                &&
                    key.Generation==_generation[key.Index]  ;
        }

        bool indexValid(int index) const
        {
            return !_free[index];
        }

        T& at(const GenKey<T>& key)
        {
            if(!keyValid(key))
                throw std::runtime_error("The given key is no longer valid.");

            return _vector[key.Index];
        }

        const std::vector<T> vector() const
        {
                    return _vector;
        }

        GenKey<T> insert(const T& element)
        {
            size_t idx;
            if (_freeList.empty()) {
                _vector.push_back(element);
                _free.push_back(false);
                _generation.push_back(0);
                idx = _vector.size() - 1;
            } else {
                idx = _freeList.front();
                _vector[idx] = element;
                _free[idx] = false;
                _freeList.pop_front();
            }

            return GenKey<T>
                    {
                            .Generation = _generation[idx],
                            .Index = _vector.size() - 1
                    };
        }

        GenKey<T> insert(T&& element)
        {
            size_t idx;
            if (_freeList.empty()) {
                _vector.push_back(std::move(element));
                _free.push_back(false);
                _generation.push_back(0);
                idx = _vector.size() - 1;
            } else {
                idx = _freeList.front();
                _vector[idx] = std::move(element);
                _free[idx] = false;
                _freeList.pop_front();
            }

            return GenKey<T>
                    {
                            .Generation = _generation[idx],
                            .Index = _vector.size() - 1
                    };
        }

        void remove_at(unsigned int index)
        {
            ~_vector[index];
            _free[index] = true;
            _freeList.push_back(index);
            _generation[index]++;
        }

    private:
        std::vector<T> _vector;
        std::vector<unsigned int> _generation;
        std::vector<bool> _free;
        std::list<std::size_t> _freeList;
    };


    struct MeshGraphicsData
    {
        tao_ogl_resources::OglVertexAttribArray _glVao;
        tao_ogl_resources::OglVertexBuffer _glVbo;
        tao_ogl_resources::OglIndexBuffer _glEbo;

        int _indicesCount=0;
    };

    class Mesh
    {
        friend class PbrRenderer;

    public:
        Mesh(const std::vector<glm::vec3> &positions,
             const std::vector<glm::vec3> &normals,
             const std::vector<glm::vec2> &textureCoordinates,
             const std::vector<int> &indices) : _positions(positions), _normals(normals),
                                                            _textureCoordinates(textureCoordinates),
                                                            _indices(indices)
        {
            ComputeTangentsAndBitangents(_textureCoordinates, _positions, _indices, _tangents, _bitangents);
        }

    private:
        std::vector<glm::vec3> _positions;
        std::vector<glm::vec3> _normals;
        std::vector<glm::vec3> _tangents;
        std::vector<glm::vec3> _bitangents;
        std::vector<glm::vec2> _textureCoordinates;
        std::vector<int> _indices;

        // Handle to graphics data (ugly)
        std::optional<GenKey<MeshGraphicsData>> _graphicsData;

        static void ComputeTangentsAndBitangents(
                const std::vector<glm::vec2> &textureCoordinates,
                const std::vector<glm::vec3> &positions,
                const std::vector<int> &indices,

                std::vector<glm::vec3>& tangents,
                std::vector<glm::vec3>& bitangents)
        {

            if (textureCoordinates.size() != positions.size())
                throw std::runtime_error("Invalid input: positions and textureCoordinates must match in size");

            tangents    = std::vector<glm::vec3>(textureCoordinates.size());
            bitangents  = std::vector<glm::vec3>(textureCoordinates.size());

            for (int i = 0; i < indices.size(); i += 3) {
                {
                    int
                            t1 = indices.at(i + 0),
                            t2 = indices.at(i + 1),
                            t3 = indices.at(i + 2);

                    glm::vec3
                            e1 = (positions.at(t2) - positions.at(t1)),
                            e2 = (positions.at(t3) - positions.at(t2));

                    glm::vec2
                            delta1 = (textureCoordinates.at(t2) - textureCoordinates.at(t1)),
                            delta2 = (textureCoordinates.at(t3) - textureCoordinates.at(t2));


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

                tangents = tangents;
                bitangents = bitangents;
            }
        }
    };

    struct ImageTextureGraphicsData
    {
        tao_ogl_resources::OglTexture2D _glTexture;
    };

    class ImageTexture
    {
        friend class PbrRenderer;

    public:
        ImageTexture(const std::string &path, bool accountForGamma) : _path(path), _accountForGamma(accountForGamma)
        {

        }
    private:
        std::string _path;
        int _numChannels;
        bool _accountForGamma = false;

        // Handle to graphics data (ugly)
        std::optional<GenKey<ImageTextureGraphicsData>> _graphicsData;
    };
        
    class PbrMaterial
    {
        friend class PbrRenderer;

    public:
        PbrMaterial(float roughness, float metalness, const glm::vec3 &diffuse) : _roughness(roughness),
                                                                                  _metalness(metalness),
                                                                                  _diffuse(diffuse) {}

    private:
        float _roughness;
        float _metalness;
        glm::vec3 _diffuse;
        std::optional<GenKey<ImageTexture>> _diffuseTex;
        std::optional<GenKey<ImageTexture>> _normalMap;
        std::optional<GenKey<ImageTexture>> _roughnessMap;
        std::optional<GenKey<ImageTexture>> _metalnessMap;

        static bool CheckKey(const std::optional<GenKey<ImageTexture>>& key, const GenKeyVector<ImageTexture>& vector)
        {
            if(!key.has_value()) return true;

            return vector.keyValid(key.value());
        }

    };

    class Transformation
    {
    public:
        Transformation(glm::mat4 transform) : _transformation(transform)
        {
        }

        inline const glm::mat4& transformation() const {return _transformation;}

    private:
        glm::mat4 _transformation;
    };

    struct MeshRenderer
    {
        Transformation transformation;
        GenKey<Mesh> mesh;
        GenKey<PbrMaterial> material;
    };


    class PbrRenderer
    {
    public:
        PbrRenderer(tao_render_context::RenderContext& rc, int windowWidth, int windowHeight):
                _windowWidth  (windowWidth),
                _windowHeight (windowHeight),
                _renderContext(&rc),
                _gBuffer
                {
                        .texColor0{_renderContext->CreateTexture2D()},
                        .texColor1{_renderContext->CreateTexture2D()},
                        .texColor2{_renderContext->CreateTexture2D()},
                        .texColor3{_renderContext->CreateTexture2D()},
                        .texDepth{_renderContext->CreateTexture2D()},
                        .gBuff{_renderContext->CreateFramebuffer<tao_ogl_resources::OglTexture2D>()},
                },
                _shaders
                {
                        .gPass{_renderContext->CreateShaderProgram()}
                },
                _shaderBuffers
                {
                        .cameraUbo  {_renderContext->CreateUniformBuffer()},
                        .transformUbo   {*_renderContext, 0, tao_ogl_resources::buf_usg_dynamic_draw, tao_render_context::ResizeBufferPolicy},
                        .materialUbo    {*_renderContext, 0, tao_ogl_resources::buf_usg_dynamic_draw, tao_render_context::ResizeBufferPolicy}
                },
                _meshes(),
                _textures(),
                _materials(),
                _meshRenderers()
        {
            InitGBuffer(_windowWidth, _windowHeight);
            InitShaders();
            InitStaticShaderBuffers();

        }

        GenKey<Mesh> AddMesh(Mesh& mesh)
        {
            auto key = _meshes.insert(mesh);

            _meshes.at(key)._graphicsData = CreateGraphicsData(mesh);

            return key;
        }

        GenKey<ImageTexture> AddImageTexture(ImageTexture& texture)
        {
            auto key = _textures.insert(texture);

            _textures.at(key)._graphicsData = CreateGraphicsData(texture);

            return key;
        }

        GenKey<PbrMaterial> AddMaterial(const PbrMaterial& material)
        {
            if(!PbrMaterial::CheckKey(material._diffuseTex, _textures))     throw std::runtime_error("Invalid texture key for `Diffuse` texture.");
            if(!PbrMaterial::CheckKey(material._metalnessMap, _textures))   throw std::runtime_error("Invalid texture key for `Metalness` texture.");
            if(!PbrMaterial::CheckKey(material._normalMap, _textures))      throw std::runtime_error("Invalid texture key for `Normal` texture.");
            if(!PbrMaterial::CheckKey(material._roughnessMap, _textures))   throw std::runtime_error("Invalid texture key for `Roughness` texture.");

            auto key = _materials.insert(material);

            return key;
        }

        GenKey<MeshRenderer> AddMeshRenderer(const MeshRenderer& meshRenderer)
        {
            if(!_meshes.keyValid(meshRenderer.mesh))        throw std::runtime_error("Invalid `mesh` key.");
            if(!_materials.keyValid(meshRenderer.material)) throw std::runtime_error("Invalid `material` key.");

            auto key = _meshRenderers.insert(meshRenderer);

            // Write transform and material to their buffers.
            // Resize and copy back old data if necessary.
            int rdrCount = _meshRenderers.vector().size();
            int trDataBlkSize = sizeof(transform_gl_data_block);
            if( _shaderBuffers.transformUbo.Resize(rdrCount*trDataBlkSize))
            {
                std::vector<transform_gl_data_block> transformData(_meshRenderers.vector().size());
                std::vector<transform_gl_data_block> materialData(_meshRenderers.vector().size());

                for(int i=0;i<rdrCount;i++)
                {
                    auto model = _meshRenderers.vector()[i].transformation.transformation();
                    auto normal = glm::inverse(glm::transpose(model)); // TODO: are you sure???

                    transformData[i]=(transform_gl_data_block
                       {
                           .modelMatrix = model,
                           .normalMatrix = normal
                       });

                    // TODO: materials

                }
                _shaderBuffers.transformUbo.OglBuffer().SetSubData(0, rdrCount*trDataBlkSize, transformData.data());
            }
            else
            {
                auto model = _meshRenderers.at(key).transformation.transformation();
                auto normal = glm::inverse(glm::transpose(model)); // TODO: are you sure???
                transform_gl_data_block data
                {
                    .modelMatrix = model,
                    .normalMatrix = normal
                };
                _shaderBuffers.transformUbo.OglBuffer().SetSubData(key.Index*trDataBlkSize, trDataBlkSize, &data);
            }

            return key;
        }

        void Render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, float near, float far);

    private:

        static constexpr const char* GPASS_VERT_SOURCE = "GPass.vert";
        static constexpr const char* GPASS_FRAG_SOURCE = "GPass.frag";

        static constexpr const int GPASS_UBO_BINDING_TRANSFORM  = 2;
        static constexpr const int GPASS_UBO_BINDING_MATERIAL   = 3;
        static constexpr const int GPASS_UBO_BINDING_CAMERA     = 1;

        static constexpr tao_ogl_resources::ogl_depth_state DEFAULT_DEPTH_STATE  =
                tao_ogl_resources::ogl_depth_state
                {
                    .depth_test_enable = true,
                    .depth_write_enable  = true,
                    .depth_func = tao_ogl_resources::depth_func_less,
                    .depth_range_near = 0.0f,
                    .depth_range_far = 1.0f
                };

        static constexpr tao_ogl_resources::ogl_blend_state DEFAULT_BLEND_STATE =
                tao_ogl_resources::ogl_blend_state
                {
                    .blend_enable = false,
                    .color_mask = tao_ogl_resources::mask_all
                };

        static constexpr tao_ogl_resources::ogl_rasterizer_state DEFAULT_RASTERIZER_STATE =
                tao_ogl_resources::ogl_rasterizer_state
                {
                        .culling_enable = true,
                        .front_face = tao_ogl_resources::front_face_ccw,
                        .cull_mode = tao_ogl_resources::cull_mode_back,
                        .polygon_mode = tao_ogl_resources::polygon_mode_fill,
                        .multisample_enable = false,
                        .alpha_to_coverage_enable = false
                };

        struct GBuffer
        {
            tao_ogl_resources::OglTexture2D texColor0; // position (3) - roughness (1)
            tao_ogl_resources::OglTexture2D texColor1; // normal   (3) - metalness (1)
            tao_ogl_resources::OglTexture2D texColor2; // diffuse  (3) - occlusion (1)
            tao_ogl_resources::OglTexture2D texColor3; // emission (3) - unused    (1)

            tao_ogl_resources::OglTexture2D texDepth;

            tao_ogl_resources::OglFramebuffer<tao_ogl_resources::OglTexture2D> gBuff;
        };

        struct Shaders
        {
            tao_ogl_resources::OglShaderProgram gPass;
        };

        struct camera_gl_data_block
        {
            glm::mat4 viewMatrix;
            glm::mat4 projectionMatrix;
            float near;
            float far;
        };

        struct transform_gl_data_block
        {
            glm::mat4 modelMatrix;
            glm::mat4 normalMatrix;
        };

        struct material_gl_data_block
        {
            glm::vec4 diffuse;
            glm::mat4 emission;
            float roughness;
            float metalness;

            int has_diffuse_tex;
            int has_emission_tex;
            int has_normal_tex;
            int has_roughness_tex;
            int has_metalness_tex;
            int has_occlusion_tex;
        };

        struct ShaderBuffers
        {
            tao_ogl_resources::OglUniformBuffer cameraUbo;
            tao_render_context::ResizableUbo transformUbo;
            tao_render_context::ResizableUbo materialUbo;
        };

        int                                _windowWidth, _windowHeight;
        tao_render_context::RenderContext* _renderContext;

        GBuffer _gBuffer;
        Shaders _shaders;
        ShaderBuffers _shaderBuffers;

        GenKeyVector<Mesh>          _meshes;
        GenKeyVector<MeshGraphicsData> _meshesGraphicsData;

        GenKeyVector<ImageTexture>  _textures;
        GenKeyVector<ImageTextureGraphicsData>  _texturesGraphicsData;

        GenKeyVector<PbrMaterial>   _materials;
        GenKeyVector<MeshRenderer>  _meshRenderers;


        void InitGBuffer(int width, int height);
        void InitShaders();
        void InitStaticShaderBuffers();

        GenKey<MeshGraphicsData> CreateGraphicsData(Mesh& mesh);
        GenKey<ImageTextureGraphicsData> CreateGraphicsData(ImageTexture& image);

    };

}