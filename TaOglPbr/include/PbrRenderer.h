#pragma once

#include "RenderContext.h"
#include "RenderContextUtils.h"
#include "TaoMath.h"
#include "Instrumentation.h"

#include <list>
#include <optional>
#include "glm/glm.hpp"
#include <glm/ext/matrix_transform.hpp>

#define ENABLE_GPU_PROFILING

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
            return index<_vector.size() && !_free[index];
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

            tangents = std::vector<glm::vec3>(textureCoordinates.size());
            bitangents = std::vector<glm::vec3>(textureCoordinates.size());

            for (int i = 0; i < indices.size(); i += 3)
            {

                int
                        t1 = indices.at(i + 0),
                        t2 = indices.at(i + 1),
                        t3 = indices.at(i + 2);

                if (t1 >= positions.size() || t2 >= positions.size() || t3 >= positions.size())
                    throw std::runtime_error("Invalid input: indices reference out of bounds");

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

            for (auto &v: tangents)
                v = glm::normalize(v);

            for (auto &v: bitangents)
                v = glm::normalize(v);

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
        bool _accountForGamma = false;

        // Handle to graphics data (ugly)
        std::optional<GenKey<ImageTextureGraphicsData>> _graphicsData;
    };

    struct EnvironmentTextureGraphicsData
    {
        tao_ogl_resources::OglTextureCube _envCube;              // environment map as cube map
        tao_ogl_resources::OglTextureCube _irradianceCube;       // irradiance cube map
        tao_ogl_resources::OglTextureCube _prefilteredEnvCube;   // incoming env irradiance map (split-sum approx)
    };

    class EnvironmentLight
    {
        friend class PbrRenderer;

    public:
        explicit EnvironmentLight(const std::string &path) : _path(path)
        {

        }
    private:
        std::string _path;

        // Handle to graphics data (ugly)
        std::optional<GenKey<EnvironmentTextureGraphicsData>> _graphicsData;
    };


    struct pbr_material_descriptor
    {
        // Diffuse color
        glm::vec3 diffuse;
        std::optional<GenKey<ImageTexture>> diffuseTex;

        // Normals
        std::optional<GenKey<ImageTexture>> normalTex;

        // Roughness (or metalness/roughness)
        float roughness;
        std::optional<GenKey<ImageTexture>> roughnessTex;
        bool mergedMetalnessRoughness = false;

        // Metalness
        float metalness;
        std::optional<GenKey<ImageTexture>> metalnessTex;

        // Emissive color
        glm::vec3 emission;
        std::optional<GenKey<ImageTexture>> emissionTex;

        // Occlusion
        std::optional<GenKey<ImageTexture>> occlusionTex;
    };

    class PbrMaterial
    {
        friend class PbrRenderer;

    public:
        PbrMaterial(float roughness, float metalness, const glm::vec3 &diffuse) : _roughness(roughness),
                                                                                  _metalness(metalness),
                                                                                  _diffuse(diffuse)
        {

        }

        PbrMaterial(const pbr_material_descriptor& desc):
        _roughness(desc.roughness), _metalness(desc.metalness), _diffuse(desc.diffuse), _emission(desc.emission),
        _diffuseTex(desc.diffuseTex), _emissionTex(desc.emissionTex), _normalMap(desc.normalTex), _roughnessMap(desc.roughnessTex),
        _metalnessMap(desc.metalnessTex), _occlusionMap(desc.occlusionTex), _mergedMetalRough(desc.mergedMetalnessRoughness)
        {
        }

    private:
        float _roughness;
        float _metalness;
        glm::vec3 _diffuse;
        glm::vec3 _emission;
        std::optional<GenKey<ImageTexture>> _diffuseTex;
        std::optional<GenKey<ImageTexture>> _emissionTex;
        std::optional<GenKey<ImageTexture>> _normalMap;
        std::optional<GenKey<ImageTexture>> _roughnessMap;
        std::optional<GenKey<ImageTexture>> _metalnessMap;
        std::optional<GenKey<ImageTexture>> _occlusionMap;
        bool _mergedMetalRough;


        static bool CheckKey(const std::optional<GenKey<ImageTexture>>& key, const GenKeyVector<ImageTexture>& vector)
        {
            if(!key.has_value()) return true;

            return vector.keyValid(key.value());
        }

    };

    class Transformation
    {
    public:
        Transformation(glm::mat4 transform) : _matrix(transform)
        {
        }

        inline const glm::mat4& matrix() const {return _matrix;}
        void Transform(const glm::mat4& t)
        {
            _matrix  =  _matrix * t;
        }

    private:
        glm::mat4 _matrix;
    };

    struct DirectionalLight
    {
        Transformation transformation;
        glm::vec3      intensity;
    };

    struct SphereLight
    {
        Transformation transformation;
        glm::vec3      intensity;
        float          radius;
    };

    struct RectLight
    {
        Transformation transformation;
        glm::vec3      intensity;
        glm::vec2      size;
    };

    class MeshRenderer
    {
        friend class PbrRenderer;

    public:
        void SetTransformation(const Transformation& transformation);

    private:
        const PbrRenderer*                      _renderer; //TODO: weak_ptr?
        Transformation                          _transformation;
        tao_math::BoundingBox<float, 3>::AaBb   _aabb;
        GenKey<Mesh>                            _mesh;
        GenKey<PbrMaterial>                     _material;

        MeshRenderer(const PbrRenderer* renderer,
                     const GenKey<Mesh> &mesh,
                     const GenKey<PbrMaterial> &material,
                     const Transformation& transformation):
                            _renderer{renderer},
                            _transformation{transformation},
                            _mesh{mesh},_material{material}
        {
            // TODO: dangling ptr
            if(!renderer) throw std::runtime_error("renderer param can't be null.");
        }
    };

    class PbrRenderer
    {
    public:
        PbrRenderer(tao_render_context::RenderContext& rc, int windowWidth, int windowHeight):
                _windowWidth  (windowWidth),
                _windowHeight (windowHeight),
                _renderContext(&rc),
                _envBRDFLut {rc.CreateTexture2D()},
                _ltcLut1    {rc.CreateTexture2D()},
                _ltcLut2    {rc.CreateTexture2D()},
                _frameDataUbo(rc.CreateUniformBuffer()),
                _lightsDataUbo(rc.CreateUniformBuffer()),
                _gBuffer
                {
                        .texColor0  {_renderContext->CreateTexture2D()},
                        .texColor1  {_renderContext->CreateTexture2D()},
                        .texColor2  {_renderContext->CreateTexture2D()},
                        .texColor3  {_renderContext->CreateTexture2D()},
                        .texDepth   {_renderContext->CreateTexture2D()},
                        .gBuff      {_renderContext->CreateFramebuffer<tao_ogl_resources::OglTexture2D>()},
                },
                _outBuffer
                {
                        .texColor   {_renderContext->CreateTexture2D()},
                        .buff       {_renderContext->CreateFramebuffer<tao_ogl_resources::OglTexture2D>()},
                },
                _shaders
                {
                        .gPass          {_renderContext->CreateShaderProgram()},
                        .lightPass      {_renderContext->CreateShaderProgram()},
                        .pointShadowMap {_renderContext->CreateShaderProgram()}
                },
                _shaderBuffers
                {
                        .cameraUbo  {_renderContext->CreateUniformBuffer()},
                        .lightsUbo  {_renderContext->CreateUniformBuffer()},
                        .transformUbo               {*_renderContext, 0, tao_ogl_resources::buf_usg_dynamic_draw, tao_render_context::ResizeBufferPolicy},
                        .materialUbo                {*_renderContext, 0, tao_ogl_resources::buf_usg_dynamic_draw, tao_render_context::ResizeBufferPolicy},
                        .directionalLightsSsbo      {*_renderContext, 0, tao_ogl_resources::buf_usg_dynamic_draw, tao_render_context::ResizeBufferPolicy},
                        .sphereLightsSsbo           {*_renderContext, 0, tao_ogl_resources::buf_usg_dynamic_draw, tao_render_context::ResizeBufferPolicy},
                        .rectLightsSsbo             {*_renderContext, 0, tao_ogl_resources::buf_usg_dynamic_draw, tao_render_context::ResizeBufferPolicy}
                },
                _computeShaders
                {
                        .generateEnvironmentCube    {_renderContext->CreateShaderProgram()},
                        .generateIrradianceCube     {_renderContext->CreateShaderProgram()},
                        .generatePrefilteredEnvCube {_renderContext->CreateShaderProgram()},
                        .generateEnvBRDFLut         {_renderContext->CreateShaderProgram()},
                },
                _fsQuad
                {
                        .vbo{_renderContext->CreateVertexBuffer()},
                        .ebo{_renderContext->CreateIndexBuffer()},
                        .vao{_renderContext->CreateVertexAttribArray()}
                },
                _meshes(),
                _meshesGraphicsData(),
                _textures(),
                _texturesGraphicsData(),
                _environmentTextures(),
                _environmentTexturesGraphicsData(),
                _currentEnvironment(),
                _pointSampler           {_renderContext->CreateSampler()},
                _linearSampler          {_renderContext->CreateSampler()},
                _linearSamplerRepeat    {_renderContext->CreateSampler()},
                _linearMipLinearSampler {_renderContext->CreateSampler()},
                _shadowSampler          {_renderContext->CreateSampler()},
                _materials(),
                _meshRenderers()
#ifdef ENABLE_GPU_PROFILING
                ,
                _gpuStopwatch(*_renderContext)
#endif
        {
            InitGBuffer(_windowWidth, _windowHeight);
            InitOutputBuffer(_windowWidth, _windowHeight);
            InitFsQuad();
            InitShaders();
            InitSamplers();
            InitShadowMaps();
            InitStaticShaderBuffers();
            InitEnvBRDFLut();
            InitLtcLut();

            _frameDataUbo .SetData(sizeof(frame_gl_data_block) , nullptr, tao_ogl_resources::buf_usg_dynamic_draw);
            _lightsDataUbo.SetData(sizeof(lights_gl_data_block), nullptr, tao_ogl_resources::buf_usg_dynamic_draw);

            int glOffAlignment = _renderContext->UniformBufferOffsetAlignment();
            int trBlkSize = sizeof(transform_gl_data_block);
            int mtBlkSize = sizeof(material_gl_data_block);
            _transformDataBlockAlignment = trBlkSize/glOffAlignment + (trBlkSize%glOffAlignment) ? glOffAlignment : 0;
            _materialDataBlockAlignment = mtBlkSize/glOffAlignment + (mtBlkSize%glOffAlignment) ? glOffAlignment : 0;
        }

        [[nodiscard]] GenKey<Mesh>                AddMesh(Mesh& mesh);
        [[nodiscard]] GenKey<ImageTexture>        AddImageTexture(ImageTexture& texture);
        [[nodiscard]] GenKey<EnvironmentLight>    AddEnvironmentTexture(const char* path);
        [[nodiscard]] GenKey<PbrMaterial>         AddMaterial(const PbrMaterial& material);
        [[nodiscard]] GenKey<MeshRenderer>        AddMeshRenderer(const Transformation& transform, const GenKey<Mesh>& mesh, const GenKey<PbrMaterial> &material);
        [[nodiscard]] GenKey<DirectionalLight>    AddLight(const DirectionalLight& directionalLight);
        [[nodiscard]] GenKey<SphereLight>         AddLight(const SphereLight& sphereLight);
        [[nodiscard]] GenKey<RectLight>           AddLight(const RectLight& rectLigth);

        void UpdateDirectionalLight (GenKey<DirectionalLight>   key, const DirectionalLight& value);
        void UpdateSphereLight      (GenKey<SphereLight>        key, const SphereLight& value);
        void UpdateRectLight        (GenKey<RectLight>          key, const RectLight& value);

        void SetCurrentEnvironment(const GenKey<EnvironmentLight>& environment);

        void ReloadShaders();

        struct pbrRendererOut
        {
            tao_ogl_resources::OglTexture2D                                    *_colorTexture;
            tao_ogl_resources::OglTexture2D                                    *_depthTexture;
        };

        pbrRendererOut Render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, float near, float far);

        void Resize(int newWidth, int newHeight);

        struct GpuPerfCounters
        {
            unsigned long long GPassTime = 0;
            unsigned long long LightPassTime = 0;
        };
        GpuPerfCounters PerfCounters;

    private:

        static constexpr const char* GPASS_VERT_SOURCE                      = "GPass.vert";
        static constexpr const char* GPASS_FRAG_SOURCE                      = "GPass.frag";
        static constexpr const char* LIGHTPASS_VERT_SOURCE                  = "LightPass.vert";
        static constexpr const char* LIGHTPASS_FRAG_SOURCE                  = "LightPass.frag";
        static constexpr const char* POINT_SHADOWS_VERT_SOURCE              = "PointShadowMap.vert";
        static constexpr const char* POINT_SHADOWS_GEOM_SOURCE              = "PointShadowMap.geom";
        static constexpr const char* POINT_SHADOWS_FRAG_SOURCE              = "PointShadowMap.frag";

        static constexpr const char* LIGHTPASS_NAME_GBUFF0                  = "gBuff0";
        static constexpr const char* LIGHTPASS_NAME_GBUFF1                  = "gBuff1";
        static constexpr const char* LIGHTPASS_NAME_GBUFF2                  = "gBuff2";
        static constexpr const char* LIGHTPASS_NAME_GBUFF3                  = "gBuff3";
        static constexpr const char* LIGHTPASS_NAME_ENV_BRDF_LUT            = "envBrdfLut";
        static constexpr const char* LIGHTPASS_NAME_ENV_IRRADIANCE          = "envIrradiance";
        static constexpr const char* LIGHTPASS_NAME_LTC_LUT_1               = "ltcLut1";
        static constexpr const char* LIGHTPASS_NAME_LTC_LUT_2               = "ltcLut2";
        static constexpr const char* LIGHTPASS_NAME_ENV_PREFILTERED         = "envPrefiltered";
        static constexpr const char* LIGHTPASS_NAME_ENV_PREFILTERED_MIN_LOD = "u_envPrefilteredMinLod";
        static constexpr const char* LIGHTPASS_NAME_ENV_PREFILTERED_MAX_LOD = "u_envPrefilteredMaxLod";
        static constexpr const char* LIGHTPASS_NAME_DIR_SHADOW_MATRIX       = "u_dirShadowMatrix";
        static constexpr const char* LIGHTPASS_NAME_DIR_SHADOW_POS          = "u_dirShadowPosition";
        static constexpr const char* LIGHTPASS_NAME_DIR_SHADOW_SIZE         = "u_dirShadowSize";
        static constexpr const char* LIGHTPASS_NAME_DO_DIR_SHADOW           = "u_doDirShadow";
        static constexpr const char* LIGHTPASS_NAME_DO_SPHERE_SHADOW        = "u_doSphereShadow";
        static constexpr const char* LIGHTPASS_NAME_SPHERE_SHADOW_SIZE      = "u_sphereShadowMapSize";
        static constexpr const char* LIGHTPASS_NAME_SPHERE_SHADOW_RES       = "u_sphereShadowMapResolution";
        static constexpr const char* LIGHTPASS_NAME_DO_RECT_SHADOW          = "u_doRectShadow";
        static constexpr const char* LIGHTPASS_NAME_RECT_SHADOW_SIZE        = "u_rectShadowMapSize";
        static constexpr const char* LIGHTPASS_NAME_RECT_SHADOW_RES         = "u_rectShadowMapResolution";
        static constexpr const char* LIGHTPASS_NAME_RECT_SHADOW_RADIUS      = "u_rectShadowRadius";
        static constexpr const char* POINT_SHADOWS_NAME_LIGHT_POS           = "u_lightWorldPos";
        static constexpr const char* POINT_SHADOWS_NAME_VIEWPROJ            = "u_viewProjMat";

        static constexpr const char* LIGHTPASS_ENV_LIGHTS_SYMBOL            = "LIGHT_PASS_ENVIRONMENT";
        static constexpr const char* LIGHTPASS_DIR_LIGHTS_SYMBOL            = "LIGHT_PASS_DIRECTIONAL";
        static constexpr const char* LIGHTPASS_SPHERE_LIGHTS_SYMBOL         = "LIGHT_PASS_SPHERE";
        static constexpr const char* LIGHTPASS_RECT_LIGHTS_SYMBOL           = "LIGHT_PASS_RECT";
        static constexpr const char* LIGHTPASS_MAX_DIR_SHADOW_CNT_SYMBOL    = "MAX_DIR_LIGHT_SHADOW_COUNT";
        static constexpr const char* LIGHTPASS_MAX_SPHERE_SHADOW_CNT_SYMBOL = "MAX_SPHERE_LIGHT_SHADOW_COUNT";
        static constexpr const char* LIGHTPASS_MAX_RECT_SHADOW_CNT_SYMBOL   = "MAX_RECT_LIGHT_SHADOW_COUNT";

        static constexpr int DIR_SHADOW_RES = 2048;
        static constexpr int POINT_SHADOW_RES = 512;
        static constexpr int ENV_CUBE_RES = 512;
        static constexpr int IRR_CUBE_RES = 64;
        static constexpr int PRE_CUBE_RES = 128;
        static constexpr int PRE_CUBE_MIN_LOD = 0;
        static constexpr int PRE_CUBE_MAX_LOD = 4;
        static constexpr int MAX_DIR_SHADOW_COUNT = 4;
        static constexpr int MAX_SPHERE_SHADOW_COUNT = 4;
        static constexpr int MAX_RECT_SHADOW_COUNT = 4;

        static constexpr const int LIGHTPASS_TEX_BINDING_GBUFF0              = 0;
        static constexpr const int LIGHTPASS_TEX_BINDING_GBUFF1              = 1;
        static constexpr const int LIGHTPASS_TEX_BINDING_GBUFF2              = 2;
        static constexpr const int LIGHTPASS_TEX_BINDING_GBUFF3              = 3;
        static constexpr const int LIGHTPASS_TEX_BINDING_ENV_BRDF_LUT        = 4;
        static constexpr const int LIGHTPASS_TEX_BINDING_ENV_IRRADIANCE      = 5;
        static constexpr const int LIGHTPASS_TEX_BINDING_ENV_PREFILTERED     = 6;
        static constexpr const int LIGHTPASS_TEX_BINDING_LTC_LUT_1           = 7;
        static constexpr const int LIGHTPASS_TEX_BINDING_LTC_LUT_2           = 8;
        static constexpr const int LIGHTPASS_TEX_BINDING_DIR_SHADOW_MAP      = 9;
        static constexpr const int LIGHTPASS_TEX_BINDING_SPHERE_SHADOW_MAP   = LIGHTPASS_TEX_BINDING_DIR_SHADOW_MAP + MAX_DIR_SHADOW_COUNT;
        static constexpr const int LIGHTPASS_TEX_BINDING_RECT_SHADOW_MAP     = LIGHTPASS_TEX_BINDING_SPHERE_SHADOW_MAP + MAX_SPHERE_SHADOW_COUNT;

        static constexpr const int LIGHTPASS_BUFFER_BINDING_DIR_LIGHTS      = 5;
        static constexpr const int LIGHTPASS_BUFFER_BINDING_SPHERE_LIGHTS   = 6;
        static constexpr const int LIGHTPASS_BUFFER_BINDING_RECT_LIGHTS     = 7;

        static constexpr const int GPASS_TEX_BINDING_DIFFUSE    = 0;
        static constexpr const int GPASS_TEX_BINDING_EMISSION   = 1;
        static constexpr const int GPASS_TEX_BINDING_NORMALS    = 2;
        static constexpr const int GPASS_TEX_BINDING_METALNESS  = 3;
        static constexpr const int GPASS_TEX_BINDING_ROUGHNESS  = 4;
        static constexpr const int GPASS_TEX_BINDING_OCCLUSION  = 5;

        static constexpr const int GPASS_UBO_BINDING_TRANSFORM  = 2;
        static constexpr const int GPASS_UBO_BINDING_MATERIAL   = 3;
        static constexpr const int GPASS_UBO_BINDING_CAMERA     = 1;
        static constexpr const int UBO_BINDING_FRAME_DATA       = 0;
        static constexpr const int LIGHTPASS_UBO_BINDING_LIGHTS_DATA = 4;

        static constexpr const char* PROCESS_ENV_COMPUTE_SOURCE      = "ProcessEnvironment.comp";
        static constexpr const char* GEN_ENV_SYMBOL                  = "GEN_ENVIRONMENT_CUBE";
        static constexpr const char* GEN_IRR_SYMBOL                  = "GEN_IRRADIANCE_CUBE";
        static constexpr const char* GEN_PRE_SYMBOL                  = "GEN_PREFILTERED_ENV_CUBE";
        static constexpr const char* GEN_LUT_SYMBOL                  = "GEN_ENVIRONMENT_BRDF_LUT";
        static constexpr const char* PROCESS_ENV_ENV2D_TEX_NAME      = "envTex";
        static constexpr const char* PROCESS_ENV_ENVCUBE_TEX_NAME    = "envCube";
        static constexpr const char* PROCESS_ENV_IRRCUBE_TEX_NAME    = "irradianceCube";
        static constexpr const char* PROCESS_ENV_PRECUBE_TEX_NAME    = "prefilteredEnvCube";
        static constexpr const char* PROCESS_ENV_ENVLUT_TEX_NAME     = "envBRDFLut";
        static constexpr const char* PROCESS_ENV_ROUGHNESS_NAME      = "u_roughness";
        static constexpr int         PROCESS_ENV_GROUP_SIZE_X        = 8;
        static constexpr int         PROCESS_ENV_GROUP_SIZE_Y        = 8;
        static constexpr int         PROCESS_ENV_GROUP_SIZE_Z        = 1;

        static constexpr tao_ogl_resources::ogl_depth_state DEFAULT_DEPTH_STATE  =
                tao_ogl_resources::ogl_depth_state
                {
                    .depth_test_enable = true,
                    .depth_write_enable  = true,
                    .depth_func = tao_ogl_resources::depth_func_less,
                    .depth_range_near = 0.0f,
                    .depth_range_far = 1.0f
                };


        static constexpr tao_ogl_resources::ogl_depth_state DEPTH_STATE_OFF  =
                tao_ogl_resources::ogl_depth_state
                        {
                                .depth_test_enable = false,
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

        static constexpr tao_ogl_resources::ogl_rasterizer_state RASTERIZER_STATE_SHADOW_MAP =
                tao_ogl_resources::ogl_rasterizer_state
                        {
                                .culling_enable = true,
                                .front_face = tao_ogl_resources::front_face_ccw,
                                .cull_mode = tao_ogl_resources::cull_mode_back,
                                .polygon_mode = tao_ogl_resources::polygon_mode_fill,
                                .polygon_offset_factor = 2.0f,
                                .polygon_offset_units  = 4.0f,
                                .multisample_enable = false,
                                .alpha_to_coverage_enable = false
                        };

        struct DirectionalShadowMap
        {
            tao_ogl_resources::OglTexture2D                                     shadowMap;
            tao_ogl_resources::OglFramebuffer<tao_ogl_resources::OglTexture2D>  shadowFbo;
            glm::mat4                                                           shadowMatrix;
            glm::vec3                                                           lightPos;
            glm::vec4                                                           shadowSize;
        };

        struct SphereShadowMap
        {
            tao_ogl_resources::OglTextureCube                                     shadowMapColor;
            tao_ogl_resources::OglTextureCube                                     shadowMapDepth;
            tao_ogl_resources::OglFramebuffer<tao_ogl_resources::OglTextureCube>  shadowFbo;
            glm::vec3                                                             shadowCenter;
            glm::vec4                                                             shadowSize;
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

        struct OutputFramebuffer
        {
            tao_ogl_resources::OglTexture2D texColor;
            tao_ogl_resources::OglFramebuffer<tao_ogl_resources::OglTexture2D> buff;
        };

        struct Shaders
        {
            tao_ogl_resources::OglShaderProgram gPass;
            tao_ogl_resources::OglShaderProgram lightPass;
            tao_ogl_resources::OglShaderProgram pointShadowMap;
        };

        struct ComputeShaders
        {
            tao_ogl_resources::OglShaderProgram generateEnvironmentCube;
            tao_ogl_resources::OglShaderProgram generateIrradianceCube;
            tao_ogl_resources::OglShaderProgram generatePrefilteredEnvCube;
            tao_ogl_resources::OglShaderProgram generateEnvBRDFLut;
        };

        struct NdcQuad
        {
            tao_ogl_resources::OglVertexBuffer vbo;
            tao_ogl_resources::OglIndexBuffer ebo;
            tao_ogl_resources::OglVertexAttribArray vao;
        };

        tao_ogl_resources::OglTexture2D   _envBRDFLut;  // env BRDF lut (split-sum approx)
        tao_ogl_resources::OglTexture2D   _ltcLut1;     // see: https://github.com/selfshadow/ltc_code
        tao_ogl_resources::OglTexture2D   _ltcLut2;     // "" ""

        tao_ogl_resources::OglUniformBuffer _frameDataUbo;
        struct frame_gl_data_block
        {
            glm::vec4 eyePosition;
            glm::vec2 viewportSize;
            glm::vec2 taaJitter;
            int doGamma;
            float gamma;
            int radianceMinLod;
            int radianceMaxLod;
            int doTaa;
        };

        tao_ogl_resources::OglUniformBuffer _lightsDataUbo;
        struct lights_gl_data_block
        {
            int doEnvironment;
            float environmentIntensity;
            int directionalLightsCnt;
            int sphereLightsCnt;
            int rectLightsCnt;
        };

        struct directional_light_gl_data_block
        {
            glm::vec4 direction;
            glm::vec4 intensity;
        };

        static directional_light_gl_data_block ToGraphicsData(const tao_pbr::DirectionalLight &dirLight)
        {
            return directional_light_gl_data_block
            {
                    .direction =  dirLight.transformation.matrix()[2],
                    .intensity = glm::vec4(dirLight.intensity, 0.0),
            };
        }

        struct sphere_light_gl_data_block
        {
            glm::vec4 position;
            glm::vec3 intensity;
            float     radius;
        };

        static sphere_light_gl_data_block ToGraphicsData(const tao_pbr::SphereLight &sphereLight)
        {
            return sphere_light_gl_data_block
            {
                .position =  sphereLight.transformation.matrix()[3],
                .intensity = glm::vec4(sphereLight.intensity, 0.0),
                .radius    = sphereLight.radius
            };
        }

        struct rect_light_gl_data_block
        {
            glm::vec4 position;
            glm::vec4 intensity;
            glm::vec4 axisX;
            glm::vec4 axisY;
            glm::vec4 axisZ;
            glm::vec2 size;
            glm::vec2 pad;
        };

        static rect_light_gl_data_block ToGraphicsData(const tao_pbr::RectLight &rectLight)
        {
            return rect_light_gl_data_block
            {
                    .position =  rectLight.transformation.matrix()[3],
                    .intensity = glm::vec4(rectLight.intensity, 0.0),
                    .axisX = rectLight.transformation.matrix()[0],
                    .axisY = rectLight.transformation.matrix()[1],
                    .axisZ = rectLight.transformation.matrix()[2],
                    .size  = rectLight.size
            };
        }

        struct camera_gl_data_block
        {
            glm::mat4 viewMatrix;
            glm::mat4 projectionMatrix;
            float near;
            float far;
        };

        int _transformDataBlockAlignment;
        struct transform_gl_data_block
        {
            glm::mat4 modelMatrix;
            glm::mat4 normalMatrix;
        };

        int _materialDataBlockAlignment;
        struct material_gl_data_block
        {
            glm::vec4 diffuse;
            glm::vec4 emission;
            float roughness;
            float metalness;

            int has_diffuse_tex;
            int has_emission_tex;
            int has_normal_tex;
            int has_roughness_tex;
            int has_merged_rough_metal;
            int has_metalness_tex;
            int has_occlusion_tex;
        };

        struct ShaderBuffers
        {
            tao_ogl_resources::OglUniformBuffer cameraUbo;
            tao_ogl_resources::OglUniformBuffer lightsUbo;
            tao_render_context::ResizableUbo    transformUbo;
            tao_render_context::ResizableUbo    materialUbo;
            tao_render_context::ResizableSsbo   directionalLightsSsbo;
            tao_render_context::ResizableSsbo   sphereLightsSsbo;
            tao_render_context::ResizableSsbo   rectLightsSsbo;
        };


        int                                _windowWidth, _windowHeight;
        tao_render_context::RenderContext* _renderContext;

        GBuffer _gBuffer;
        OutputFramebuffer _outBuffer;

        std::vector<DirectionalShadowMap> _directionalShadowMaps;
        std::vector<SphereShadowMap>      _sphereShadowMaps;
        std::vector<SphereShadowMap>      _rectShadowMaps;

        Shaders _shaders;
        ShaderBuffers _shaderBuffers;

        ComputeShaders _computeShaders;

        NdcQuad _fsQuad;

        GenKeyVector<Mesh>                      _meshes;
        GenKeyVector<MeshGraphicsData>          _meshesGraphicsData;

        GenKeyVector<ImageTexture>              _textures;
        GenKeyVector<ImageTextureGraphicsData>  _texturesGraphicsData;

        GenKeyVector<EnvironmentLight>              _environmentTextures;
        GenKeyVector<EnvironmentTextureGraphicsData>  _environmentTexturesGraphicsData;
        std::optional<GenKey<EnvironmentLight>>     _currentEnvironment;

        tao_ogl_resources::OglSampler _pointSampler;
        tao_ogl_resources::OglSampler _linearSampler;
        tao_ogl_resources::OglSampler _linearSamplerRepeat;
        tao_ogl_resources::OglSampler _linearMipLinearSampler;
        tao_ogl_resources::OglSampler _shadowSampler;

        GenKeyVector<PbrMaterial>       _materials;
        GenKeyVector<MeshRenderer>      _meshRenderers;
        GenKeyVector<DirectionalLight>  _directionalLights;
        GenKeyVector<SphereLight>       _sphereLights;
        GenKeyVector<RectLight>         _rectLights;

#ifdef ENABLE_GPU_PROFILING
        tao_instrument::GpuStopwatch _gpuStopwatch;
#endif
        void InitGBuffer        (int width, int height);
        void ResizeGBuffer      (int width, int height);
        void InitOutputBuffer   (int width, int height);
        void ResizeOutputBuffer (int width, int height);
        void InitFsQuad();
        void InitShaders();
        void InitSamplers();
        void InitEnvBRDFLut();
        void InitLtcLut();
        void InitShadowMaps();
        void InitStaticShaderBuffers();
        void WriteTransfromToShaderBuffer(const MeshRenderer& mesh, int offset);
        void WriteTransfromToShaderBuffer(const std::vector<MeshRenderer>& meshes, int offset);
        void WriteMaterialToShaderBuffer(const MeshRenderer& mesh, int offset);
        void WriteMaterialToShaderBuffer(const std::vector<MeshRenderer>& meshes, int offset);
        [[nodiscard]] GenKey<MeshGraphicsData>                CreateGraphicsData(Mesh& mesh);
        [[nodiscard]] GenKey<ImageTextureGraphicsData>        CreateGraphicsData(ImageTexture& image);
        [[nodiscard]] GenKey<EnvironmentTextureGraphicsData>  CreateGraphicsData(EnvironmentLight& image);
        [[nodiscard]] EnvironmentTextureGraphicsData          CreateEnvironmentTextures(tao_ogl_resources::OglTexture2D &env);

        void CreateShadowMap(DirectionalShadowMap &shadowMapData, const tao_pbr::DirectionalLight &l, int shadowMapWidth, int shadowMapHeight);
        void CreateShadowMap(SphereShadowMap      &shadowMapData, const tao_pbr::SphereLight      &l, int shadowMapResolution);
        void CreateShadowMap(SphereShadowMap      &shadowMapData, const tao_pbr::RectLight        &l, int shadowMapResolution);
        void CreateShadowMap(SphereShadowMap      &shadowMapData, const glm::vec3& position, const glm::vec3& direction, const glm::vec3& intensity, float radius, int shadowMapResolution);

        tao_ogl_resources::OglTexture2D& GetGlTexture(const GenKey<ImageTexture>& tex);
        void BindMaterialTextures(const GenKey<PbrMaterial>& mat);
    };

}