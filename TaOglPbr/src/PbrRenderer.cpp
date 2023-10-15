#include "PbrRenderer.h"
#include "TaOglPbrConfig.h"
#include "stb_image/stb_image.h"
#include "gli/gli.hpp"
#include "glm/gtc/type_ptr.hpp"

namespace tao_pbr
{

    using namespace  tao_ogl_resources;
    using namespace  tao_render_context;
    using namespace  tao_math;

    using namespace glm;

    void PbrRenderer::InitGBuffer(int width, int height)
    {
        _gBuffer.texColor0.TexImage(0, tex_int_for_rgba16f, width, height,tex_for_rgba, tex_typ_float, nullptr);
        _gBuffer.texColor1.TexImage(0, tex_int_for_rgba16f, width, height,tex_for_rgba, tex_typ_float, nullptr);
        _gBuffer.texColor2.TexImage(0, tex_int_for_rgba16f, width, height,tex_for_rgba, tex_typ_float, nullptr);
        _gBuffer.texColor3.TexImage(0, tex_int_for_rgba16f, width, height,tex_for_rgba, tex_typ_float, nullptr);
        _gBuffer.texDepth.TexImage(0, tex_int_for_depth_stencil, width, height, tex_for_depth, tex_typ_float, nullptr);

        ogl_tex_filter_params pointFilter
        {
            .min_filter = tex_min_filter_nearest,
            .mag_filter = tex_mag_filter_nearest
        };

        _gBuffer.texColor0.SetFilterParams(pointFilter);
        _gBuffer.texColor1.SetFilterParams(pointFilter);
        _gBuffer.texColor2.SetFilterParams(pointFilter);
        _gBuffer.texColor3.SetFilterParams(pointFilter);
        _gBuffer.texDepth .SetFilterParams(pointFilter);

        _gBuffer.gBuff.AttachTexture(fbo_attachment_depth_stencil, _gBuffer.texDepth, 0);
        _gBuffer.gBuff.AttachTexture(static_cast<ogl_framebuffer_attachment>(fbo_attachment_color0 + 0), _gBuffer.texColor0, 0);
        _gBuffer.gBuff.AttachTexture(static_cast<ogl_framebuffer_attachment>(fbo_attachment_color0 + 1), _gBuffer.texColor1, 0);
        _gBuffer.gBuff.AttachTexture(static_cast<ogl_framebuffer_attachment>(fbo_attachment_color0 + 2), _gBuffer.texColor2, 0);
        _gBuffer.gBuff.AttachTexture(static_cast<ogl_framebuffer_attachment>(fbo_attachment_color0 + 3), _gBuffer.texColor3, 0);


        const ogl_framebuffer_read_draw_buffs drawBuffs[]
                {
                static_cast<ogl_framebuffer_read_draw_buffs>(fbo_read_draw_buff_color0+0),
                static_cast<ogl_framebuffer_read_draw_buffs>(fbo_read_draw_buff_color0+1),
                static_cast<ogl_framebuffer_read_draw_buffs>(fbo_read_draw_buff_color0+2),
                static_cast<ogl_framebuffer_read_draw_buffs>(fbo_read_draw_buff_color0+3)
                };

        _gBuffer.gBuff.SetDrawBuffers(4, drawBuffs);
    }

    void PbrRenderer::ResizeGBuffer(int width, int height)
    {
        _gBuffer.texColor0.TexImage(0, tex_int_for_rgba16f, width, height,tex_for_rgba, tex_typ_float, nullptr);
        _gBuffer.texColor1.TexImage(0, tex_int_for_rgba16f, width, height,tex_for_rgba, tex_typ_float, nullptr);
        _gBuffer.texColor2.TexImage(0, tex_int_for_rgba16f, width, height,tex_for_rgba, tex_typ_float, nullptr);
        _gBuffer.texColor3.TexImage(0, tex_int_for_rgba16f, width, height,tex_for_rgba, tex_typ_float, nullptr);
        _gBuffer.texDepth .TexImage(0, tex_int_for_depth_stencil, width, height, tex_for_depth, tex_typ_float, nullptr);
    }

    void PbrRenderer::InitOutputBuffer(int width, int height)
    {
        _outBuffer.texColor.TexImage(0, tex_int_for_rgba16f, width, height,tex_for_rgba, tex_typ_float, nullptr);
        _outBuffer.buff.AttachTexture(fbo_attachment_color0, _outBuffer.texColor, 0);

        const ogl_framebuffer_read_draw_buffs drawBuffs = fbo_read_draw_buff_color0;
        _outBuffer.buff.SetDrawBuffers(1, &drawBuffs);
    }

    void PbrRenderer::ResizeOutputBuffer(int width, int height)
    {
        _outBuffer.texColor.TexImage(0, tex_int_for_rgba16f, width, height,tex_for_rgba, tex_typ_float, nullptr);
    }

    void PbrRenderer::InitShadowMaps()
    {
        ogl_tex_filter_params pointFilter
        {
                .min_filter = tex_min_filter_nearest,
                .mag_filter = tex_mag_filter_nearest
        };

        // Directional Shadow Map
        // ---------------------------------------------------
        for(int i=0; i<MAX_DIR_SHADOW_COUNT; i++)
        {
            _directionalShadowMaps.push_back(DirectionalShadowMap
             {
                     .shadowMap = _renderContext->CreateTexture2D(),
                     .shadowFbo = _renderContext->CreateFramebuffer<OglTexture2D>()
             });

            _directionalShadowMaps[i].shadowMap.TexImage(0, tex_int_for_depth, DIR_SHADOW_RES, DIR_SHADOW_RES,
                                                     tex_for_depth, tex_typ_float, nullptr);

            _directionalShadowMaps[i].shadowFbo.AttachTexture(fbo_attachment_depth, _directionalShadowMaps[i].shadowMap, 0);

            // don't write to any color buffer
            ogl_framebuffer_read_draw_buffs buffs[] = {fbo_read_draw_buff_none};
            _directionalShadowMaps[i].shadowFbo.SetDrawBuffers(1, buffs);
            _directionalShadowMaps[i].shadowFbo.SetReadBuffer(buffs[0]);
        }


        // Point and Rect Shadow Map
        // ---------------------------------------------------
        for(int i=0; i<MAX_SPHERE_SHADOW_COUNT + MAX_RECT_SHADOW_COUNT; i++)
        {
            bool sphere = i<MAX_SPHERE_SHADOW_COUNT;
            int idx = sphere ? i : i-MAX_SPHERE_SHADOW_COUNT;
            vector<SphereShadowMap>& shadows = sphere ? _sphereShadowMaps : _rectShadowMaps;

            shadows.push_back(SphereShadowMap
            {
                .shadowMapColor = _renderContext->CreateTextureCube(),
                .shadowMapDepth = _renderContext->CreateTextureCube(),
                .shadowFbo = _renderContext->CreateFramebuffer<OglTextureCube>(),
                .shadowCenter = vec3{0.0f}
            });

            for (int f = 0; f < 6; f++)
            {
                shadows[idx].shadowMapDepth.TexImage(
                        static_cast<ogl_texture_cube_target>(tex_tar_cube_map_positive_x + f), 0, tex_int_for_depth,
                        POINT_SHADOW_RES, POINT_SHADOW_RES, 0, tex_for_depth, tex_typ_float, nullptr);
                shadows[idx].shadowMapColor.TexImage(
                        static_cast<ogl_texture_cube_target>(tex_tar_cube_map_positive_x + f), 0, tex_int_for_r16f,
                        POINT_SHADOW_RES, POINT_SHADOW_RES, 0, tex_for_red, tex_typ_float, nullptr);
                shadows[idx].shadowMapColor.SetFilterParams(pointFilter);
            }
            shadows[idx].shadowFbo.AttachTexture(fbo_attachment_color0, shadows[idx].shadowMapColor, 0);
            shadows[idx].shadowFbo.AttachTexture(fbo_attachment_depth, shadows[idx].shadowMapDepth, 0);

            // don't write to any color buffer
            ogl_framebuffer_read_draw_buffs buffs1[] = {fbo_read_draw_buff_color0};
            shadows[idx].shadowFbo.SetDrawBuffers(1, buffs1);
            shadows[idx].shadowFbo.SetReadBuffer(buffs1[0]);
        }
    }

    void PbrRenderer::InitSamplers()
    {
        ogl_sampler_compare_params noCompare
        {
            .compare_mode = sampler_cmp_mode_none,
            .compare_func = sampler_cmp_func_never
        };
        ogl_sampler_compare_params compareLess
        {
                .compare_mode = sampler_cmp_mode_compare_ref_to_texture,
                .compare_func = sampler_cmp_func_less,
        };
        ogl_sampler_wrap_params clamp
        {
            .wrap_s = sampler_wrap_clamp_to_edge,
            .wrap_t = sampler_wrap_clamp_to_edge,
            .wrap_r = sampler_wrap_clamp_to_edge,
        };
        ogl_sampler_wrap_params mirror_clamp
        {
                .wrap_s = sampler_wrap_mirror_clamp_to_edge,
                .wrap_t = sampler_wrap_mirror_clamp_to_edge,
                .wrap_r = sampler_wrap_mirror_clamp_to_edge,
        };
        ogl_sampler_wrap_params repeat
        {
                .wrap_s = sampler_wrap_repeat,
                .wrap_t = sampler_wrap_repeat,
                .wrap_r = sampler_wrap_repeat,
        };
        ogl_sampler_filter_params pointFilter
        {
            .min_filter = sampler_min_filter_nearest,
            .mag_filter = sampler_mag_filter_nearest
        };
        ogl_sampler_filter_params linearFilter
        {
                .min_filter = sampler_min_filter_linear,
                .mag_filter = sampler_mag_filter_linear
        };
        ogl_sampler_filter_params linearMipLinearFilter
        {
                .min_filter = sampler_min_filter_linear_mip_linear,
                .mag_filter = sampler_mag_filter_linear
        };

        _pointSampler           .SetParams(ogl_sampler_params{.filter_params = pointFilter,             .wrap_params = clamp, .lod_params{}, .compare_params = noCompare,});
        _linearSampler          .SetParams(ogl_sampler_params{.filter_params = linearFilter,            .wrap_params = clamp, .lod_params{}, .compare_params = noCompare,});
        _linearSamplerRepeat    .SetParams(ogl_sampler_params{.filter_params = linearFilter,            .wrap_params = repeat,.lod_params{}, .compare_params = noCompare,});
        _linearMipLinearSampler .SetParams(ogl_sampler_params{.filter_params = linearMipLinearFilter,   .wrap_params = clamp, .lod_params{}, .compare_params = noCompare,});
        _shadowSampler          .SetParams(ogl_sampler_params{.filter_params = linearFilter,            .wrap_params = clamp, .lod_params{}, .compare_params = compareLess,});
    }

    void PbrRenderer::InitShaders()
    {
        // Geometry and Light - Pass shaders
        // ------------------------------------
        auto gPassShader = _renderContext->CreateShaderProgram(
                ShaderLoader::LoadShader(GPASS_VERT_SOURCE, SHADER_SRC_DIR, SHADER_SRC_DIR).c_str(),
                ShaderLoader::LoadShader(GPASS_FRAG_SOURCE, SHADER_SRC_DIR, SHADER_SRC_DIR).c_str()
        );

        auto lightPassShader = _renderContext->CreateShaderProgram(
                ShaderLoader::LoadShader(LIGHTPASS_VERT_SOURCE, SHADER_SRC_DIR, SHADER_SRC_DIR).c_str(),
                ShaderLoader::DefineConditional(
            ShaderLoader::LoadShader(LIGHTPASS_FRAG_SOURCE, SHADER_SRC_DIR, SHADER_SRC_DIR),
                // Uber-shader approach: contains the code (and branching) for
                // all the supported light types.
                {
                            LIGHTPASS_DIR_LIGHTS_SYMBOL,
                            LIGHTPASS_ENV_LIGHTS_SYMBOL,
                            LIGHTPASS_SPHERE_LIGHTS_SYMBOL,
                            LIGHTPASS_RECT_LIGHTS_SYMBOL,
                            string{LIGHTPASS_MAX_DIR_SHADOW_CNT_SYMBOL   }.append(" ").append(to_string(MAX_DIR_SHADOW_COUNT)),
                            string{LIGHTPASS_MAX_SPHERE_SHADOW_CNT_SYMBOL}.append(" ").append(to_string(MAX_SPHERE_SHADOW_COUNT)),
                            string{LIGHTPASS_MAX_RECT_SHADOW_CNT_SYMBOL  }.append(" ").append(to_string(MAX_RECT_SHADOW_COUNT)),
                }).c_str()
        );

        // Shadow mapping shaders
        // ------------------------------------
        auto pointLightShadowShader = _renderContext->CreateShaderProgram(
                ShaderLoader::LoadShader(POINT_SHADOWS_VERT_SOURCE, SHADER_SRC_DIR, SHADER_SRC_DIR).c_str(),
                ShaderLoader::LoadShader(POINT_SHADOWS_GEOM_SOURCE, SHADER_SRC_DIR, SHADER_SRC_DIR).c_str(),
                ShaderLoader::LoadShader(POINT_SHADOWS_FRAG_SOURCE, SHADER_SRC_DIR, SHADER_SRC_DIR).c_str()
                );

        // Set Uniforms
        // --------------------------------------
        lightPassShader.UseProgram();
        lightPassShader.SetUniform(LIGHTPASS_NAME_ENV_PREFILTERED_MIN_LOD, PRE_CUBE_MIN_LOD);
        lightPassShader.SetUniform(LIGHTPASS_NAME_ENV_PREFILTERED_MAX_LOD, PRE_CUBE_MAX_LOD);

        // Compute Shaders
        // --------------------------------------
        auto source = ShaderLoader::LoadShader(PROCESS_ENV_COMPUTE_SOURCE, SHADER_SRC_DIR, SHADER_SRC_DIR);

        auto genEnv = _renderContext->CreateShaderProgram(ShaderLoader::DefineConditional(source, {GEN_ENV_SYMBOL}).c_str());
        auto genIrr = _renderContext->CreateShaderProgram(ShaderLoader::DefineConditional(source, {GEN_IRR_SYMBOL}).c_str());
        auto genPre = _renderContext->CreateShaderProgram(ShaderLoader::DefineConditional(source, {GEN_PRE_SYMBOL}).c_str());
        auto genLut = _renderContext->CreateShaderProgram(ShaderLoader::DefineConditional(source, {GEN_LUT_SYMBOL}).c_str());

        // if CreateShaderProgram() throws the
        // current shader is not affected.
        _shaders.gPass                              = std::move(gPassShader);
        _shaders.lightPass                          = std::move(lightPassShader);
        _shaders.pointShadowMap                     = std::move(pointLightShadowShader);
        _computeShaders.generateEnvironmentCube     = std::move(genEnv);
        _computeShaders.generateIrradianceCube      = std::move(genIrr);
        _computeShaders.generatePrefilteredEnvCube  = std::move(genPre);
        _computeShaders.generateEnvBRDFLut          = std::move(genLut);
    }

    void PbrRenderer::InitFsQuad()
    {
        constexpr float positions[]
        {
            -1.0f, -1.0f,
             1.0f, -1.0f,
             1.0f,  1.0f,
            -1.0f,  1.0f
        };
        constexpr int indices[]
        {
        0, 1, 2,
        0, 2, 3
        };

        _fsQuad.vbo.SetData(sizeof(positions), positions, buf_usg_static_draw);
        _fsQuad.ebo.SetData(sizeof(indices), indices, buf_usg_static_draw);
        _fsQuad.vao.SetVertexAttribPointer(_fsQuad.vbo, 0, 2, vao_typ_float, false, 2*sizeof(float), nullptr);
        _fsQuad.vao.EnableVertexAttrib(0);
        _fsQuad.vao.SetIndexBuffer(_fsQuad.ebo);
    }

    void PbrRenderer::InitStaticShaderBuffers()
    {
        _shaderBuffers.cameraUbo.SetData(sizeof(camera_gl_data_block), nullptr, buf_usg_dynamic_draw);
    }

    GenKey<MeshGraphicsData>  PbrRenderer::CreateGraphicsData(Mesh& mesh)
    {
        constexpr int kVertSize =
                3* sizeof(float) +    // position
                3* sizeof(float) +    // normal
                2* sizeof(float) +    // textureCoord
                3* sizeof(float) +    // tangent
                3* sizeof(float);     // bitangent

        MeshGraphicsData graphicsData
        {
                ._glVao{_renderContext->CreateVertexAttribArray()},
                ._glVbo{_renderContext->CreateVertexBuffer()},
                ._glEbo{_renderContext->CreateIndexBuffer()}
        };

        // Vertex buffer (interleaved)
        tao_render_context::BufferDataPacker pack{};
        auto data = pack
                .AddDataArray(mesh._positions)
                .AddDataArray(mesh._normals)
                .AddDataArray(mesh._textureCoordinates)
                .AddDataArray(mesh._tangents)
                .AddDataArray(mesh._bitangents)
                .InterleavedBuffer();

        graphicsData._glVbo.SetData(data.size(),reinterpret_cast<void*>(data.data()), tao_ogl_resources::buf_usg_static_draw);

        // Index buffer
        graphicsData._indicesCount = mesh._indices.size();
        graphicsData._glEbo.SetData(mesh._indices.size()*sizeof(int), mesh._indices.data(), tao_ogl_resources::buf_usg_static_draw);
        graphicsData._glVao = _renderContext->CreateVertexAttribArray();

        // Vertex attribs
        graphicsData._glVao.SetVertexAttribPointer(graphicsData._glVbo, 0, 3, tao_ogl_resources::vao_typ_float, false, kVertSize, 0);
        graphicsData._glVao.SetVertexAttribPointer(graphicsData._glVbo, 1, 3, tao_ogl_resources::vao_typ_float, false, kVertSize, reinterpret_cast<void*>(3 * sizeof (float)));
        graphicsData._glVao.SetVertexAttribPointer(graphicsData._glVbo, 2, 2, tao_ogl_resources::vao_typ_float, false, kVertSize, reinterpret_cast<void*>(6 * sizeof (float)));
        graphicsData._glVao.SetVertexAttribPointer(graphicsData._glVbo, 3, 3, tao_ogl_resources::vao_typ_float, false, kVertSize, reinterpret_cast<void*>(8 * sizeof (float)));
        graphicsData._glVao.SetVertexAttribPointer(graphicsData._glVbo, 4, 3, tao_ogl_resources::vao_typ_float, false, kVertSize, reinterpret_cast<void*>(11 * sizeof (float)));

        graphicsData._glVao.EnableVertexAttrib(0);
        graphicsData._glVao.EnableVertexAttrib(1);
        graphicsData._glVao.EnableVertexAttrib(2);
        graphicsData._glVao.EnableVertexAttrib(3);
        graphicsData._glVao.EnableVertexAttrib(4);

        graphicsData._glVao.SetIndexBuffer(graphicsData._glEbo);

        return _meshesGraphicsData.insert(std::move(graphicsData));
    }

    GenKey<ImageTextureGraphicsData>  PbrRenderer::CreateGraphicsData(ImageTexture& image)
    {
        int w, h, c;
        void* data = stbi_load(image._path.c_str(), &w, &h, &c, 0);

        if(!data)
        {
            throw runtime_error(std::format("Failed to load texture data at {}: {}", image._path, stbi_failure_reason()));
        }

        tao_ogl_resources::ogl_texture_internal_format ifmt;
        tao_ogl_resources::ogl_texture_format fmt;
        switch(c)
        {
            case(1): ifmt = tao_ogl_resources::tex_int_for_red;  fmt = tao_ogl_resources::tex_for_red; break;
            case(2): ifmt = tao_ogl_resources::tex_int_for_rg;   fmt = tao_ogl_resources::tex_for_rg; break;

            case(3): ifmt = image._accountForGamma ? tao_ogl_resources::tex_int_for_srgb8        : tao_ogl_resources::tex_int_for_rgb ; fmt = tao_ogl_resources::tex_for_rgb; break;
            case(4): ifmt = image._accountForGamma ? tao_ogl_resources::tex_int_for_srgb8_alpha8 : tao_ogl_resources::tex_int_for_rgba; fmt = tao_ogl_resources::tex_for_rgba; break;
        }

        ImageTextureGraphicsData gd{._glTexture = _renderContext->CreateTexture2D()};

        gd._glTexture.TexImage(0, ifmt, w, h, fmt, tao_ogl_resources::tex_typ_unsigned_byte, data);

        stbi_image_free(data);

        return _texturesGraphicsData.insert(std::move(gd));
    }

    GenKey<EnvironmentTextureGraphicsData>  PbrRenderer::CreateGraphicsData(EnvironmentLight& image)
    {
        int w, h, c;
        void* data = stbi_loadf(image._path.c_str(), &w, &h, &c, 3);

        tao_ogl_resources::ogl_texture_internal_format ifmt {tex_int_for_rgb16f};
        tao_ogl_resources::ogl_texture_format fmt           {tex_for_rgb};

        // needed only to create the cube version of the environment
        auto env2DTex = _renderContext->CreateTexture2D();
        env2DTex.TexImage(0, ifmt, w, h, fmt, tao_ogl_resources::tex_typ_float, data);

        EnvironmentTextureGraphicsData gd = CreateEnvironmentTextures(env2DTex);

        stbi_image_free(data);

        return _environmentTexturesGraphicsData.insert(std::move(gd));
    }

    GenKey<Mesh> PbrRenderer::AddMesh(Mesh& mesh)
    {
        auto key = _meshes.insert(mesh);

        _meshes.at(key)._graphicsData = CreateGraphicsData(mesh);

        return key;
    }

    GenKey<ImageTexture> PbrRenderer::AddImageTexture(ImageTexture& texture)
    {
        auto key = _textures.insert(texture);

        _textures.at(key)._graphicsData = CreateGraphicsData(texture);

        return key;
    }

    GenKey<EnvironmentLight> PbrRenderer::AddEnvironmentTexture(const char* path)
    {
        EnvironmentLight envLight{path};
        auto key = _environmentTextures.insert(envLight);

        _environmentTextures.at(key)._graphicsData = CreateGraphicsData(envLight);

        return key;
    }

    GenKey<PbrMaterial> PbrRenderer::AddMaterial(const PbrMaterial& material)
    {
        if(!PbrMaterial::CheckKey(material._diffuseTex, _textures))     throw std::runtime_error("Invalid texture key for `Diffuse` texture.");
        if(!PbrMaterial::CheckKey(material._metalnessMap, _textures))   throw std::runtime_error("Invalid texture key for `Metalness` texture.");
        if(!PbrMaterial::CheckKey(material._normalMap, _textures))      throw std::runtime_error("Invalid texture key for `Normal` texture.");
        if(!PbrMaterial::CheckKey(material._roughnessMap, _textures))   throw std::runtime_error("Invalid texture key for `Roughness` texture.");

        auto key = _materials.insert(material);

        return key;
    }

    void PbrRenderer::SetCurrentEnvironment(const GenKey<tao_pbr::EnvironmentLight> &environment)
    {
        if(!_environmentTextures.keyValid(environment))
            throw std::runtime_error("The given key is not valid.");

        _currentEnvironment = environment;
    }

    void PbrRenderer::WriteTransfromToShaderBuffer(const MeshRenderer& mesh, int offset)
    {
        WriteTransfromToShaderBuffer(std::vector<MeshRenderer>{mesh}, offset);
    }

    void PbrRenderer::WriteTransfromToShaderBuffer(const std::vector<MeshRenderer>& meshes, int offset)
    {
        const int trDataBlkSize = sizeof(transform_gl_data_block);

        std::vector<transform_gl_data_block> dataTra(meshes.size());

        for (int i = 0; i < meshes.size(); i++)
        {
            glm::mat4 model = meshes[i]._transformation.matrix();
            glm::mat3 normal = glm::transpose(glm::inverse(model));

            dataTra[i] = (transform_gl_data_block
                    {
                            .modelMatrix = model,
                            .normalMatrix = normal
                    });

        }

        BufferDataPacker pack{};
        pack.AddDataArray(dataTra);

        // Since we need to glBindBufferRange we should take
        // GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT into account.
        auto data = pack.InterleavedBuffer(_renderContext->UniformBufferOffsetAlignment());
        _shaderBuffers.transformUbo.OglBuffer().SetSubData(offset, meshes.size()*_transformDataBlockAlignment, data.data());
    }

    void PbrRenderer::WriteMaterialToShaderBuffer(const MeshRenderer& mesh, int offset)
    {
        WriteMaterialToShaderBuffer((std::vector<MeshRenderer>{mesh}), offset);
    }

    void PbrRenderer::WriteMaterialToShaderBuffer(const std::vector<MeshRenderer>& meshes, int offset)
    {
        const int matDataBlockSize = sizeof(material_gl_data_block);

        std::vector<material_gl_data_block> dataMat(meshes.size());

        for (int i = 0; i < meshes.size(); i++)
        {

            const PbrMaterial &mat = _materials.at(meshes[i]._material);

            dataMat[i] = material_gl_data_block
                    {
                            .diffuse = glm::vec4(mat._diffuse, 1.0f),
                            .emission = glm::vec4(mat._emission, 1.0f),
                            .roughness = mat._roughness,
                            .metalness = mat._metalness,

                            .has_diffuse_tex    = mat._diffuseTex.has_value(),
                            .has_emission_tex   = mat._emissionTex.has_value(),
                            .has_normal_tex     = mat._normalMap.has_value(),
                            .has_roughness_tex  = mat._roughnessMap.has_value(),
                            .has_merged_rough_metal = mat._mergedMetalRough,
                            .has_metalness_tex  = mat._metalnessMap.has_value(),
                            .has_occlusion_tex  = mat._occlusionMap.has_value()
                    };
        }

        BufferDataPacker pack{};
        pack.AddDataArray(dataMat);

        // Since we need to glBindBufferRange we should take
        // GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT into account.
        auto data = pack.InterleavedBuffer(_renderContext->UniformBufferOffsetAlignment());
        _shaderBuffers.materialUbo.OglBuffer().SetSubData(offset, meshes.size() * _materialDataBlockAlignment, data.data());
    }

    GenKey<MeshRenderer> PbrRenderer::AddMeshRenderer(const Transformation& transform, const GenKey<Mesh>& mesh, const GenKey<PbrMaterial> &material)
    {
        if(!_meshes.keyValid(mesh))        throw std::runtime_error("Invalid `mesh` key.");
        if(!_materials.keyValid(material)) throw std::runtime_error("Invalid `material` key.");

        MeshRenderer mr(this, mesh, material, transform);
        mr._aabb = tao_math::BoundingBox<float, 3>::ComputeBbox(
                _meshes.at(mesh)._positions,
                mr._transformation.matrix());

        auto key = _meshRenderers.insert(mr);

        // Write transform and material to their buffers.
        // Resize and copy back old data if necessary.
        // ----------------------------------------------
        int rdrCount = _meshRenderers.vector().size();

        // Transformations
        if( _shaderBuffers.transformUbo.Resize(rdrCount*_transformDataBlockAlignment))
            WriteTransfromToShaderBuffer(_meshRenderers.vector(), 0);                                    // resized, need to re-write everything
        else
            WriteTransfromToShaderBuffer(_meshRenderers.at(key), key.Index*_transformDataBlockAlignment); // possibly overwrite existing old data

        // Materials
        if( _shaderBuffers.materialUbo.Resize(rdrCount*_materialDataBlockAlignment))
            WriteMaterialToShaderBuffer(_meshRenderers.vector(), 0);                                    // resized, need to re-write everything
        else
            WriteMaterialToShaderBuffer(_meshRenderers.at(key), key.Index*_materialDataBlockAlignment);  // possibly overwrite existing old data

        return key;
    }

    // TODO: this "CPU-GPU synced buffer" should become an entity on its own
    template<typename T, typename G>
    GenKey<T> AddToCollectionSyncGpu(GenKeyVector<T>& genKeyedCollection, ResizableSsbo& gpuBuffer, const T& elemToAdd, std::function<G(const T&)> converter)
    {
        auto key = genKeyedCollection.insert(elemToAdd);

        int totalElementsCnt = genKeyedCollection.vector().size();
        // this works with SSBOs and std430(I hope...), for UBOs
        // we should query  the gpu  offset alignment.
        int gpuRequiredCapacity = totalElementsCnt * sizeof(G) /* which should exactly match gpu-side size...*/;

        if(gpuBuffer.Resize(gpuRequiredCapacity))
        {
            // There wasn't enough space, gpu buffer resized.
            // Need to copy all the elements from the start.
            vector<G> gpuData(totalElementsCnt);
            for(int i=0;i<gpuData.size(); i++) gpuData[i] = converter(genKeyedCollection.vector()[i]);
            gpuBuffer.OglBuffer().SetSubData(0, gpuData.size()*sizeof(G), gpuData.data());
        }
        else
        {
            // There was enough space to insert a new element
            // in the gpu buffer.
            G elem = converter(elemToAdd);
            gpuBuffer.OglBuffer().SetSubData((totalElementsCnt-1)*sizeof(G), sizeof(G), &elem);
        }

        return key;
    }
    // TODO: this "CPU-GPU synced buffer" should become an entity on its own
    template<typename T, typename G>
    void WriteToCollectionSyncGpu(GenKeyVector<T>& genKeyedCollection, GenKey<T>& where, ResizableSsbo& gpuBuffer, const T& newVal, std::function<G(const T&)> converter)
    {
        if(where.Index>=genKeyedCollection.vector().size())
            throw runtime_error("Invalid index");

        genKeyedCollection.at(where) = newVal;

        G elem = converter(newVal);
        gpuBuffer.OglBuffer().SetSubData((where.Index)*sizeof(G), sizeof(G), &elem);
    }

    void PbrRenderer::UpdateDirectionalLight(GenKey<DirectionalLight> key, const DirectionalLight& value)
    {
        std::function<directional_light_gl_data_block(const DirectionalLight&)> converter =
                static_cast<directional_light_gl_data_block(*)(const DirectionalLight&)>(ToGraphicsData);

        WriteToCollectionSyncGpu(_directionalLights, key, _shaderBuffers.directionalLightsSsbo, value, converter);
    }

    GenKey<DirectionalLight> PbrRenderer::AddLight(const DirectionalLight &directionalLight)
    {
        std::function<directional_light_gl_data_block(const DirectionalLight&)> converter =
                static_cast<directional_light_gl_data_block(*)(const DirectionalLight&)>(ToGraphicsData);

        return AddToCollectionSyncGpu(_directionalLights, _shaderBuffers.directionalLightsSsbo, directionalLight, converter);
    }

    void PbrRenderer::UpdateSphereLight(GenKey<SphereLight> key, const SphereLight& value)
    {
        std::function<sphere_light_gl_data_block(const SphereLight&)> converter =
                static_cast<sphere_light_gl_data_block(*)(const SphereLight&)>(ToGraphicsData);

        WriteToCollectionSyncGpu(_sphereLights, key, _shaderBuffers.sphereLightsSsbo, value, converter);
    }

    GenKey<SphereLight> PbrRenderer::AddLight(const SphereLight &sphereLight)
    {
        std::function<sphere_light_gl_data_block(const SphereLight&)> converter =
                static_cast<sphere_light_gl_data_block(*)(const SphereLight&)>(ToGraphicsData);

        return AddToCollectionSyncGpu(_sphereLights, _shaderBuffers.sphereLightsSsbo, sphereLight, converter);
    }

    void PbrRenderer::UpdateRectLight(GenKey<RectLight> key, const RectLight& value)
    {
        std::function<rect_light_gl_data_block(const RectLight&)> converter =
                static_cast<rect_light_gl_data_block(*)(const RectLight&)>(ToGraphicsData);

        WriteToCollectionSyncGpu(_rectLights, key, _shaderBuffers.rectLightsSsbo, value, converter);
    }

    GenKey<RectLight> PbrRenderer::AddLight(const RectLight &rectLigth)
    {
        std::function<rect_light_gl_data_block(const RectLight&)> converter =
                static_cast<rect_light_gl_data_block(*)(const RectLight&)>(ToGraphicsData);

        return AddToCollectionSyncGpu(_rectLights, _shaderBuffers.rectLightsSsbo, rectLigth, converter);
    }

    void PbrRenderer::ReloadShaders()
    {
        InitShaders();
    }

    void PbrRenderer::Resize(int newWidth, int newHeight)
    {
        if(newWidth <= 0 || newHeight <= 0)
            throw std::runtime_error("Invalid size.");

        if(newWidth==_windowWidth && newHeight==_windowHeight)
            return;

        _windowWidth  = newWidth;
        _windowHeight = newHeight;

        ResizeGBuffer(newWidth, newHeight);
        ResizeOutputBuffer(newWidth, newHeight);
    }

    OglTexture2D& PbrRenderer::GetGlTexture(const GenKey<ImageTexture>& tex)
    {
        return _texturesGraphicsData.at(_textures.at(tex)._graphicsData.value())._glTexture;
    }
    void PbrRenderer::BindMaterialTextures(const GenKey<PbrMaterial>& mat)
    {
        if(auto tex = _materials.at(mat)._diffuseTex) // --- Diffuse
        {
            GetGlTexture(tex.value()).BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + GPASS_TEX_BINDING_DIFFUSE));
            _linearSamplerRepeat.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + GPASS_TEX_BINDING_DIFFUSE));
        }
        if(auto tex = _materials.at(mat)._normalMap) // --- Normal
        {
            GetGlTexture(tex.value()).BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + GPASS_TEX_BINDING_NORMALS));
            _linearSamplerRepeat.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + GPASS_TEX_BINDING_NORMALS));
        }
        if(auto tex = _materials.at(mat)._roughnessMap) // --- Roughness
        {
            GetGlTexture(tex.value()).BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + GPASS_TEX_BINDING_ROUGHNESS));
            _linearSamplerRepeat.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + GPASS_TEX_BINDING_ROUGHNESS));
        }
        if(auto tex = _materials.at(mat)._metalnessMap) // --- Metalness
        {
            GetGlTexture(tex.value()).BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + GPASS_TEX_BINDING_METALNESS));
            _linearSamplerRepeat.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + GPASS_TEX_BINDING_METALNESS));
        }
        if(auto tex = _materials.at(mat)._emissionTex) // --- Emission
        {
            GetGlTexture(tex.value()).BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + GPASS_TEX_BINDING_EMISSION));
            _linearSamplerRepeat.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + GPASS_TEX_BINDING_EMISSION));
        }
        if(auto tex = _materials.at(mat)._occlusionMap) // --- Occlusion
        {
            GetGlTexture(tex.value()).BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + GPASS_TEX_BINDING_OCCLUSION));
            _linearSamplerRepeat.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + GPASS_TEX_BINDING_OCCLUSION));
        }
    }

    string GetUniformArrayName(const char* name, int index)
    {
        return std::format("{}[{}]", name, to_string(index));
    }

    PbrRenderer::pbrRendererOut PbrRenderer::Render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, float near, float far)
    {
        _renderContext->MakeCurrent();

        // loading per-frame data
        frame_gl_data_block frameGlDataBlock
        {
                .eyePosition        = inverse(viewMatrix) * vec4(0.0, 0.0, 0.0, 1.0),
                .viewportSize       = vec2(_windowWidth, _windowHeight),
                .taaJitter          = vec2(0.0),
                .doGamma            = 1,
                .gamma              = 2.2,
                .radianceMinLod = PRE_CUBE_MIN_LOD,
                .radianceMaxLod = PRE_CUBE_MAX_LOD,
                .doTaa          = 0
        };
        _frameDataUbo.SetSubData(0, sizeof(frame_gl_data_block), &frameGlDataBlock);
        _frameDataUbo.Bind(UBO_BINDING_FRAME_DATA);


        /// Shadow Pass
        ////////////////////////////////////////////
        for(int i=0;i<MAX_DIR_SHADOW_COUNT;i++)
        {
            if(_directionalLights.indexValid(i))
            CreateShadowMap(_directionalShadowMaps[i], _directionalLights.vector()[i], DIR_SHADOW_RES, DIR_SHADOW_RES);
        }

        for(int i=0;i<MAX_SPHERE_SHADOW_COUNT;i++)
        {
            if(_sphereLights.indexValid(i))
            CreateShadowMap(_sphereShadowMaps[i],_sphereLights.vector()[i], POINT_SHADOW_RES);
        }

        for(int i=0;i<MAX_RECT_SHADOW_COUNT;i++)
        {
            if(_rectLights.indexValid(i))
                CreateShadowMap(_rectShadowMaps[i], _rectLights.vector()[i], POINT_SHADOW_RES);
        }

        _renderContext->SetViewport(0, 0, _windowWidth, _windowHeight);

        _renderContext->SetDepthState       (DEFAULT_DEPTH_STATE);
        _renderContext->SetRasterizerState  (DEFAULT_RASTERIZER_STATE);
        _renderContext->SetBlendState       (DEFAULT_BLEND_STATE);

        // loading lights data
        lights_gl_data_block lightsGlDataBlock
        {
            .doEnvironment= _currentEnvironment.has_value(),
            .environmentIntensity = 0.25f,
            .directionalLightsCnt = static_cast<int>(_directionalLights.vector().size()),
            .sphereLightsCnt      = static_cast<int>(_sphereLights.vector().size()),
            .rectLightsCnt        = static_cast<int>(_rectLights.vector().size())
        };
        _lightsDataUbo.SetSubData(0, sizeof(lights_gl_data_block), &lightsGlDataBlock);
        _lightsDataUbo.Bind(LIGHTPASS_UBO_BINDING_LIGHTS_DATA);

        // loading view data
        camera_gl_data_block cameraGlDataBlock
        {
            .viewMatrix = viewMatrix,
            .projectionMatrix = projectionMatrix,
            .near = near,
            .far = far
        };
        _shaderBuffers.cameraUbo.SetSubData(0, sizeof(camera_gl_data_block), &cameraGlDataBlock);
        _shaderBuffers.cameraUbo.Bind(GPASS_UBO_BINDING_CAMERA);

        /// Geometry Pass
        ////////////////////////////////////////////

#ifdef ENABLE_GPU_PROFILING
        auto swg = _gpuStopwatch.Start("GPass");
#endif

        _gBuffer.gBuff.Bind(ogl_framebuffer_binding::fbo_read_draw);
        _renderContext->ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        _renderContext->ClearDepth(1.0f);

        _shaders.gPass.UseProgram();

        for(int i=0;i<_meshRenderers.vector().size(); i++)
        {
            if(!_meshRenderers.indexValid(i)) continue;

            auto meshKey = _meshRenderers.vector()[i]._mesh;
            auto matKey = _meshRenderers.vector()[i]._material;
            auto meshDataKey = _meshes.at(meshKey)._graphicsData;

            if(!meshDataKey.has_value()) throw std::runtime_error("The mesh has no graphics data.");

            auto& gfxData = _meshesGraphicsData.at(meshDataKey.value());
            gfxData._glVao.Bind();

            _shaderBuffers.transformUbo.OglBuffer().BindRange(GPASS_UBO_BINDING_TRANSFORM, i*_transformDataBlockAlignment, sizeof(transform_gl_data_block));
            _shaderBuffers.materialUbo.OglBuffer().BindRange(GPASS_UBO_BINDING_MATERIAL, i*_materialDataBlockAlignment, sizeof(material_gl_data_block));

            BindMaterialTextures(matKey);

            _renderContext->DrawElements(pmt_type_triangles, gfxData._indicesCount, idx_typ_unsigned_int, nullptr);
        }

        OglFramebuffer<OglTexture2D>::UnBind(fbo_read_draw);

#ifdef ENABLE_GPU_PROFILING
        PerfCounters.GPassTime = _gpuStopwatch.Stop<tao_instrument::Stopwatch::MILLISECONDS>(swg);
#endif

        /// Light Pass
        ////////////////////////////////////////////

#ifdef ENABLE_GPU_PROFILING
        auto swl = _gpuStopwatch.Start("LightPass");
#endif
        _renderContext->SetDepthState(DEPTH_STATE_OFF);

        _outBuffer.buff.Bind(fbo_read_draw);
        _renderContext->ClearColor(0.1f, 0.1f, 0.1f, 0.0f);

        _shaders.lightPass.UseProgram();

        // Bind GBuffer and samplers
        _gBuffer.texColor0.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_GBUFF0));
        _gBuffer.texColor1.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_GBUFF1));
        _gBuffer.texColor2.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_GBUFF2));
        _gBuffer.texColor3.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_GBUFF3));

        _pointSampler.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_GBUFF0));
        _pointSampler.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_GBUFF1));
        _pointSampler.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_GBUFF2));
        _pointSampler.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_GBUFF3));

        // Bind ambient IBL textures and samplers
        if(_currentEnvironment.has_value())
        {
            EnvironmentLight& currEnvTex = _environmentTextures.at(_currentEnvironment.value());
            EnvironmentTextureGraphicsData& currEnvData = _environmentTexturesGraphicsData.at(currEnvTex._graphicsData.value());

            _envBRDFLut                     .BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_ENV_BRDF_LUT));
            currEnvData._irradianceCube     .BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_ENV_IRRADIANCE));
            currEnvData._prefilteredEnvCube .BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_ENV_PREFILTERED));

            _pointSampler           .BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_ENV_BRDF_LUT));
            _pointSampler           .BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_ENV_IRRADIANCE));
            _linearMipLinearSampler .BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_ENV_PREFILTERED));
        }

        // Bind ltc LUTs
        // TODO: if(areaLightsCnt>0)
        {
            _ltcLut1.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_LTC_LUT_1));
            _ltcLut2.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_LTC_LUT_2));
            _linearSampler.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_LTC_LUT_1));
            _linearSampler.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_LTC_LUT_2));
        }

        // Bind lights SSBOs
        _shaderBuffers.directionalLightsSsbo.OglBuffer().Bind(LIGHTPASS_BUFFER_BINDING_DIR_LIGHTS);
        _shaderBuffers.sphereLightsSsbo.OglBuffer()     .Bind(LIGHTPASS_BUFFER_BINDING_SPHERE_LIGHTS);
        _shaderBuffers.rectLightsSsbo.OglBuffer()       .Bind(LIGHTPASS_BUFFER_BINDING_RECT_LIGHTS);


        // Directional Shadow data
        for(int i=0;i<MAX_SPHERE_SHADOW_COUNT; i++)
        {
            if(!_directionalLights.indexValid(i)) continue;

            auto texUnit = static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_DIR_SHADOW_MAP + i);

            _directionalShadowMaps[i].shadowMap.BindToTextureUnit(texUnit);
            _pointSampler.BindToTextureUnit(texUnit);

            _shaders.lightPass.SetUniformMatrix4(GetUniformArrayName(LIGHTPASS_NAME_DIR_SHADOW_MATRIX, i).c_str(), glm::value_ptr(_directionalShadowMaps[i].shadowMatrix));
            _shaders.lightPass.SetUniform(GetUniformArrayName(LIGHTPASS_NAME_DO_DIR_SHADOW , i).c_str(), true);
            _shaders.lightPass.SetUniform(GetUniformArrayName(LIGHTPASS_NAME_DIR_SHADOW_POS, i).c_str(),
                                                                                                        _directionalShadowMaps[i].lightPos.x,
                                                                                                        _directionalShadowMaps[i].lightPos.y,
                                                                                                        _directionalShadowMaps[i].lightPos.z);
            _shaders.lightPass.SetUniform(GetUniformArrayName(LIGHTPASS_NAME_DIR_SHADOW_SIZE, i).c_str(),
                                                                                                        _directionalShadowMaps[i].shadowSize.x,
                                                                                                        _directionalShadowMaps[i].shadowSize.y,
                                                                                                        _directionalShadowMaps[i].shadowSize.z,
                                                                                                        _directionalShadowMaps[i].shadowSize.w);
        }

        // Sphere Shadow data
        for(int i=0;i<MAX_SPHERE_SHADOW_COUNT; i++)
        {
            if(!_sphereLights.indexValid(i)) continue;

            auto texUnit = static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_SPHERE_SHADOW_MAP + i);

            _shaders.lightPass.SetUniform(GetUniformArrayName(LIGHTPASS_NAME_DO_SPHERE_SHADOW  , i).c_str(), true);
            _shaders.lightPass.SetUniform(GetUniformArrayName(LIGHTPASS_NAME_SPHERE_SHADOW_RES , i).c_str(), POINT_SHADOW_RES, POINT_SHADOW_RES);
            _shaders.lightPass.SetUniform(GetUniformArrayName(LIGHTPASS_NAME_SPHERE_SHADOW_SIZE, i).c_str(),
                                                                                                            _sphereShadowMaps[i].shadowSize.x,
                                                                                                            _sphereShadowMaps[i].shadowSize.z,
                                                                                                            _sphereShadowMaps[i].shadowSize.y,
                                                                                                            _sphereShadowMaps[i].shadowSize.w);

            _sphereShadowMaps[i].shadowMapColor.BindToTextureUnit(texUnit);
            _pointSampler.BindToTextureUnit(texUnit);
        }

        // Rect Shadow data
        for(int i=0;i<MAX_RECT_SHADOW_COUNT; i++)
        {
            if(!_rectLights.indexValid(i)) continue;

            auto texUnit = static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_RECT_SHADOW_MAP + i);

            _shaders.lightPass.SetUniform(GetUniformArrayName(LIGHTPASS_NAME_DO_RECT_SHADOW    , i).c_str(), true);
            _shaders.lightPass.SetUniform(GetUniformArrayName(LIGHTPASS_NAME_RECT_SHADOW_RADIUS, i).c_str(), glm::min(_rectLights.vector()[i].size.x, _rectLights.vector()[i].size.y) * 0.5f);
            _shaders.lightPass.SetUniform(GetUniformArrayName(LIGHTPASS_NAME_RECT_SHADOW_RES   , i).c_str(), POINT_SHADOW_RES, POINT_SHADOW_RES);
            _shaders.lightPass.SetUniform(GetUniformArrayName(LIGHTPASS_NAME_RECT_SHADOW_SIZE  , i).c_str(),
                                          _rectShadowMaps[i].shadowSize.x,
                                          _rectShadowMaps[i].shadowSize.z,
                                          _rectShadowMaps[i].shadowSize.y,
                                          _rectShadowMaps[i].shadowSize.w);

            _rectShadowMaps[i].shadowMapColor.BindToTextureUnit(texUnit);
            _pointSampler.BindToTextureUnit(texUnit);
        }

        _fsQuad.vao.Bind();
        _renderContext->DrawElements(pmt_type_triangles, 6, idx_typ_unsigned_int, nullptr);

        // reset Framebuffer and texture bindings
        OglFramebuffer<OglTexture2D>::UnBind(fbo_read_draw);
        OglTexture2D::UnBindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_GBUFF0));
        OglTexture2D::UnBindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_GBUFF1));
        OglTexture2D::UnBindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_GBUFF2));
        OglTexture2D::UnBindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_GBUFF3));

        if(_currentEnvironment.has_value())
        {
            OglTexture2D::UnBindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_ENV_BRDF_LUT));
            OglTexture2D::UnBindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_ENV_BRDF_LUT));
            OglTexture2D::UnBindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_ENV_BRDF_LUT));
        }

        // TODO: if(areaLightsCnt>0)
        {
            _ltcLut1.UnBindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_LTC_LUT_1));
            _ltcLut2.UnBindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_LTC_LUT_2));
        }

        // TODO: unbind all the textures and buffers !!!

#ifdef ENABLE_GPU_PROFILING
        PerfCounters.LightPassTime = _gpuStopwatch.Stop<tao_instrument::Stopwatch::MILLISECONDS>(swl);
#endif

        return pbrRendererOut
        {
            ._colorTexture = &_outBuffer.texColor,
            ._depthTexture = &_gBuffer  .texDepth
        };
    }


    void ComputeShaderNumGroups(int countX, int countY, int countZ, int groupSizeX, int groupSizeY, int groupSizeZ, int& numGroupsX, int& numGroupsY, int& numGroupsZ)
    {
        numGroupsX = countX/groupSizeX + (countX%groupSizeX > 0);
        numGroupsY = countY/groupSizeY + (countY%groupSizeY > 0);
        numGroupsZ = countZ/groupSizeZ + (countZ%groupSizeZ > 0);
    }

    void PbrRenderer::InitEnvBRDFLut()
    {
        constexpr int kLutRes = 512;

        _envBRDFLut.TexImage(0, tex_int_for_rg16f, kLutRes, kLutRes, tex_for_rg, tex_typ_float, nullptr);

        // a sampler obj will be used, this is only to make it complete (default min filter is mipmap linear)
        _envBRDFLut.SetFilterParams(ogl_tex_filter_params{.min_filter = tex_min_filter_linear, .mag_filter = tex_mag_filter_linear});
        _envBRDFLut.BindToImageUnit(0,0, image_access_write, image_format_rg16f );

        _computeShaders.generateEnvBRDFLut.UseProgram();
        _computeShaders.generateEnvBRDFLut.SetUniform(PROCESS_ENV_ENVLUT_TEX_NAME, 0);

        int numGrpX, numGrpY, numGrpZ;
        ComputeShaderNumGroups(
                kLutRes, kLutRes, 1,
                PROCESS_ENV_GROUP_SIZE_X, PROCESS_ENV_GROUP_SIZE_Y, PROCESS_ENV_GROUP_SIZE_Z,
                numGrpX, numGrpY, numGrpZ
                );

        _renderContext->DispatchCompute(numGrpX, numGrpY, numGrpY);

        _envBRDFLut.UnBindToImageUnit(0);

        _renderContext->MemoryBarrier(static_cast<ogl_barrier_bit>(texture_fetch_barrier_bit | shader_image_access_barrier_bit));
    }

    void PbrRenderer::InitLtcLut()
    {
        // Those LUTs comes pre-computed from: https://github.com/selfshadow/ltc_code
        // This is ok as long as the area lights implementation matches theirs.
        auto ltc1Path = std::string{}.append(RESOURCES_DIR).append("/ltc_1.dds");
        auto ltc2Path = std::string{}.append(RESOURCES_DIR).append("/ltc_2.dds");

        gli::texture ltc1Tex= gli::load_dds(ltc1Path);
        gli::texture ltc2Tex= gli::load_dds(ltc2Path);

        if(ltc1Tex.empty())
            throw std::runtime_error(std::string("Error loading texture at ").append(ltc1Path));

        if(ltc2Tex.empty())
            throw std::runtime_error(std::string("Error loading texture at ").append(ltc1Path));

        ogl_tex_filter_params linearFilter
        {
                .min_filter = tex_min_filter_linear,
                .mag_filter = tex_mag_filter_linear
        };

        _ltcLut1.TexImage(0, tex_int_for_rgba16f, ltc1Tex.extent(0).x, ltc1Tex.extent(0).y, tex_for_rgba, tex_typ_half_float, ltc1Tex.data());
        _ltcLut2.TexImage(0, tex_int_for_rgba16f, ltc2Tex.extent(0).x, ltc1Tex.extent(0).y, tex_for_rgba, tex_typ_half_float, ltc2Tex.data());
        _ltcLut1.SetFilterParams(linearFilter);
        _ltcLut2.SetFilterParams(linearFilter);
    }

    EnvironmentTextureGraphicsData PbrRenderer::CreateEnvironmentTextures(tao_ogl_resources::OglTexture2D &env)
    {
        EnvironmentTextureGraphicsData res
        {
            ._envCube{_renderContext->CreateTextureCube()},
            ._irradianceCube{_renderContext->CreateTextureCube()},
            ._prefilteredEnvCube{_renderContext->CreateTextureCube()}
        };

        // Initialize each face level 0
        for(ogl_texture_cube_target face = tex_tar_cube_map_positive_x; face <=tex_tar_cube_map_negative_z; face = static_cast<ogl_texture_cube_target>(face+1))
        {
            res._envCube             .TexImage(face, 0, tex_int_for_rgba16f, ENV_CUBE_RES, ENV_CUBE_RES, 0,  tex_for_rgba, tex_typ_float, nullptr);
            res._irradianceCube      .TexImage(face, 0, tex_int_for_rgba16f, IRR_CUBE_RES, IRR_CUBE_RES, 0,  tex_for_rgba, tex_typ_float, nullptr);
            res._prefilteredEnvCube  .TexImage(face, 0, tex_int_for_rgba16f, PRE_CUBE_RES, PRE_CUBE_RES, 0,  tex_for_rgba, tex_typ_float, nullptr);
        }

        res._prefilteredEnvCube.GenerateMipmap();

        // --- !!! IMPORTANT !!! ---------------------------------------------------------------------------
        // Mh....apparently (with my current NVidia drivers) writes via ImageStore on a texture that is
        // 'texture incomplete` has no effect. The debug layer didn't say anything. Only NSight graphics
        // detected the issue. Is this by OGL specs???
        // -------------------------------------------------------------------------------------------------
        res._envCube             .SetFilterParams(ogl_tex_filter_params{.min_filter = tex_min_filter_nearest, .mag_filter = tex_mag_filter_nearest});
        res._irradianceCube      .SetFilterParams(ogl_tex_filter_params{.min_filter = tex_min_filter_nearest, .mag_filter = tex_mag_filter_nearest});
        res._prefilteredEnvCube  .SetFilterParams(ogl_tex_filter_params{.min_filter = tex_min_filter_linear_mip_linear, .mag_filter = tex_mag_filter_linear});

        _linearSampler.BindToTextureUnit(tex_unit_0);

        // Generate ENVIRONMENT cube map
        // ------------------------------------------------------------------------------------------------------
        /* tex unit 0 */ env                 .BindToTextureUnit(tex_unit_0);
        /* img unit 0 */ res._envCube        .BindToImageUnit(0, 0, true, 0, image_access_write, image_format_rgba16f);

        _computeShaders.generateEnvironmentCube.UseProgram();
        _computeShaders.generateEnvironmentCube.SetUniform(PROCESS_ENV_ENV2D_TEX_NAME  , 0);
        _computeShaders.generateEnvironmentCube.SetUniform(PROCESS_ENV_ENVCUBE_TEX_NAME, 0);

        int grpCntX, grpCntY, grpCntZ;
        ComputeShaderNumGroups(
                ENV_CUBE_RES, ENV_CUBE_RES, 6/*cube map faces*/,
                PROCESS_ENV_GROUP_SIZE_X, PROCESS_ENV_GROUP_SIZE_Y, PROCESS_ENV_GROUP_SIZE_Z,
                grpCntX, grpCntY, grpCntZ
                );

        _renderContext->DispatchCompute(grpCntX, grpCntY, grpCntZ);

        env                 .UnBindToTextureUnit(tex_unit_0);
        res._envCube        .UnBindToImageUnit(0);

        _renderContext->MemoryBarrier(static_cast<ogl_barrier_bit>(texture_fetch_barrier_bit | shader_image_access_barrier_bit));

        // Generate IRRADIANCE cube map
        // ------------------------------------------------------------------------------------------------------
        /* tex unit 0 */ res._envCube         .BindToTextureUnit(tex_unit_0); // linear sampler should be bound!!!
        /* img unit 0 */ res._irradianceCube  .BindToImageUnit(0, 0, true, 0, image_access_write, image_format_rgba16f);

        _computeShaders.generateIrradianceCube.UseProgram();
        _computeShaders.generateIrradianceCube.SetUniform(PROCESS_ENV_ENVCUBE_TEX_NAME  , 0);
        _computeShaders.generateIrradianceCube.SetUniform(PROCESS_ENV_IRRCUBE_TEX_NAME, 0);

        ComputeShaderNumGroups(
                IRR_CUBE_RES, IRR_CUBE_RES, 6/*cube map faces*/,
                PROCESS_ENV_GROUP_SIZE_X, PROCESS_ENV_GROUP_SIZE_Y, PROCESS_ENV_GROUP_SIZE_Z,
                grpCntX, grpCntY, grpCntZ
        );
        _renderContext->DispatchCompute(grpCntX, grpCntY, grpCntZ);

        res._envCube         .UnBindToTextureUnit(tex_unit_0);
        res._irradianceCube  .UnBindToImageUnit(0);

        _renderContext->MemoryBarrier(static_cast<ogl_barrier_bit>(texture_fetch_barrier_bit | shader_image_access_barrier_bit));

        // Generate PREFILTERED ENVIRONMENT cube map
        // ------------------------------------------------------------------------------------------------------
        _computeShaders.generatePrefilteredEnvCube.UseProgram();
        _computeShaders.generatePrefilteredEnvCube.SetUniform(PROCESS_ENV_ENVCUBE_TEX_NAME, 0);
        _computeShaders.generatePrefilteredEnvCube.SetUniform(PROCESS_ENV_PRECUBE_TEX_NAME, 0);

        for(int mip=PRE_CUBE_MIN_LOD, resolution = PRE_CUBE_RES; mip<=PRE_CUBE_MAX_LOD; resolution = resolution>>1, mip++ )
        {
            float roughness = static_cast<float>(mip)/(PRE_CUBE_MAX_LOD - PRE_CUBE_MIN_LOD);
            _computeShaders.generatePrefilteredEnvCube.SetUniform(PROCESS_ENV_ROUGHNESS_NAME, roughness);

            /* img unit 0 */ res._prefilteredEnvCube.BindToImageUnit(0, mip, true, 0, image_access_write,image_format_rgba16f);
            /* tex unit 0 */ res._envCube           .BindToTextureUnit(tex_unit_0); // linear sampler should be bound!!!

            ComputeShaderNumGroups(
                    resolution, resolution, 6/*cube map faces*/,
                    PROCESS_ENV_GROUP_SIZE_X, PROCESS_ENV_GROUP_SIZE_Y, PROCESS_ENV_GROUP_SIZE_Z,
                    grpCntX, grpCntY, grpCntZ
            );

            _renderContext->DispatchCompute(grpCntX, grpCntY, grpCntZ);
            res._prefilteredEnvCube.UnBindToImageUnit(0);
        }

        res._envCube             .UnBindToTextureUnit(tex_unit_0);

        _renderContext->MemoryBarrier(static_cast<ogl_barrier_bit>(texture_fetch_barrier_bit | shader_image_access_barrier_bit));

        return res;
    }

    void PbrRenderer::CreateShadowMap(DirectionalShadowMap &shadowMapData, const tao_pbr::DirectionalLight &l, int shadowMapWidth, int shadowMapHeight)
    {
        // Compute scene Bbox
        int mrCnt = _meshRenderers.vector().size();
        vector<vec3> pts{}; pts.reserve(mrCnt*2);
        for(int i=0;i<mrCnt; i++)
        {
            if(!_meshRenderers.indexValid(i)) continue;

            pts.push_back(_meshRenderers.vector()[i]._aabb.Min);
            pts.push_back(_meshRenderers.vector()[i]._aabb.Max);
        }

        auto sceneAABB = BoundingBox<float, 3>::ComputeBbox(pts);

        // Use the enclosing sphere to determine the camera position
        vec3  sceneCenter = sceneAABB.Center();
        float sceneRadius = sceneAABB.Diagonal()*0.5;

        vec3  viewDir    = vec3{l.transformation.matrix()[2]};
        vec3  viewPos    = sceneCenter-viewDir*sceneRadius;
        float viewNear   = 1e-3; // mh...
        float viewFar    = sceneRadius*2.0;
        mat4  viewMatrix = lookAt(viewPos, sceneCenter,vec3{l.transformation.matrix()[1]});
        mat4  projMatrix = ortho(-sceneRadius, sceneRadius, -sceneRadius, sceneRadius, viewNear, viewFar);

        shadowMapData.shadowMatrix = projMatrix*viewMatrix;
        shadowMapData.shadowSize   = glm::vec4(sceneRadius * 2.0f, sceneRadius * 2.0f, viewNear, viewFar);
        shadowMapData.lightPos     = viewPos;

        // set view data
        camera_gl_data_block cameraGlDataBlock
        {
            .viewMatrix         = viewMatrix,
            .projectionMatrix   = projMatrix,
            .near               = viewNear,
            .far                = viewFar
        };
        _shaderBuffers.cameraUbo.SetSubData(0, sizeof(camera_gl_data_block), &cameraGlDataBlock);
        _shaderBuffers.cameraUbo.Bind(GPASS_UBO_BINDING_CAMERA);

        _renderContext->SetViewport(0, 0, shadowMapWidth, shadowMapHeight);

        shadowMapData.shadowFbo.Bind(fbo_read_draw);

        _renderContext->SetDepthState(DEFAULT_DEPTH_STATE);
        _renderContext->SetRasterizerState(RASTERIZER_STATE_SHADOW_MAP);

        _renderContext->ClearDepth(1.0f);

        _shaders.gPass.UseProgram(); // using the gPass shader for now....
        for(int i=0;i<_meshRenderers.vector().size(); i++)
        {
            if(!_meshRenderers.indexValid(i)) continue;

            auto meshKey = _meshRenderers.vector()[i]._mesh;
            auto meshDataKey = _meshes.at(meshKey)._graphicsData;

            if(!meshDataKey.has_value()) throw std::runtime_error("The mesh has no graphics data.");

            auto& gfxData = _meshesGraphicsData.at(meshDataKey.value());
            gfxData._glVao.Bind();

            _shaderBuffers.transformUbo.OglBuffer().BindRange(GPASS_UBO_BINDING_TRANSFORM, i*_transformDataBlockAlignment, sizeof(transform_gl_data_block));
            _renderContext->DrawElements(pmt_type_triangles, gfxData._indicesCount, idx_typ_unsigned_int, nullptr);
        }

        shadowMapData.shadowFbo.UnBind(fbo_read_draw);
    }

    float ComputeSphereLightRadius(const vec3& intensity, float tolerance=0.0002)
    {
        // Without am artificial falloff the light contribution
        // will never reach exactly 0. Here we are computing a
        // distance beyond which the energy received from the light
        // is negligible (<tol).
        //
        // from: https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
        // E = phi/4PId^2(NoL) - page 44, Remark
        // taking the max for the dot product (1.0) we have:
        // max(light.r, g, b)/(4*PI*d^2) < tol -> d>sqrt(max(rgb)/4*PI*tol)
        //
        // NOTE:    with a small tolerance (which is good) the radius is
        //          big...maybe we should "enforce" a falloff in the
        //          lighting computation.

        return glm::sqrt(glm::max(glm::max(intensity.r, intensity.g), intensity.b) / (4.0*pi<float>()*tolerance));
    }

    void PbrRenderer::CreateShadowMap(SphereShadowMap &shadowMapData, const vec3& position, const vec3& direction, const vec3& intensity, float radius, int shadowMapResolution)
    {
        float rMin = glm::max(1e-3f, radius);
        float rMax = rMin + ComputeSphereLightRadius(intensity);
        vec3 viewPos = position;
        vec3 viewDirections[6] =
        {
            vec3{1.0, 0.0, 0.0},
            vec3{-1.0, 0.0, 0.0},
            vec3{0.0, 1.0, 0.0},
            vec3{0.0, -1.0,  0.0},
            vec3{0.0, 0.0, 1.0},
            vec3{0.0, 0.0, -1.0}
        };

        mat4 projMatrix = perspective(0.5f * pi<float>(), 1.0f, rMin, rMax);


        // see: https://registry.khronos.org/OpenGL/specs/gl/glspec33.core.pdf
        // page 169
        // big LOL......
        mat4 shadowMatrices[6] =
        {
            projMatrix * lookAt(viewPos, viewPos + viewDirections[0], vec3(0.0, -1.0, 0.0)),
            projMatrix * lookAt(viewPos, viewPos + viewDirections[1], vec3(0.0, -1.0, 0.0)),
            projMatrix * lookAt(viewPos, viewPos + viewDirections[2], vec3(0.0, 0.0, 1.0)),
            projMatrix * lookAt(viewPos, viewPos + viewDirections[3], vec3(0.0, 0.0, -1.0)),
            projMatrix * lookAt(viewPos, viewPos + viewDirections[4], vec3(0.0, -1.0, 0.0)),
            projMatrix * lookAt(viewPos, viewPos + viewDirections[5], vec3(0.0, -1.0, 0.0)),
        };

        shadowMapData.shadowCenter = viewPos;
        shadowMapData.shadowSize = vec4{ rMin*2.0f, rMin*2.0f  ,rMin, rMax}; // size x, size y, z min, zmax

        _renderContext->SetViewport(0, 0, shadowMapResolution, shadowMapResolution);

        shadowMapData.shadowFbo.Bind(fbo_read_draw);

        _renderContext->SetBlendState(DEFAULT_BLEND_STATE);
        _renderContext->SetDepthState(DEFAULT_DEPTH_STATE);
        _renderContext->ClearDepth(1.0f);
        _renderContext->ClearColor(rMax, 0.0, 0.0, 0.0);

        // Setting uniforms (6 transform matrices, light position)
        _shaders.pointShadowMap.UseProgram();
        _shaders.pointShadowMap.SetUniform(POINT_SHADOWS_NAME_LIGHT_POS, viewPos.x, viewPos.y, viewPos.z);
        for(int i=0;i<6;i++)
        {
            string name = string{POINT_SHADOWS_NAME_VIEWPROJ}
                    .append("[").append(to_string(i)).append("]");

            _shaders.pointShadowMap.SetUniformMatrix4(name.c_str(), value_ptr(shadowMatrices[i]));
        }

        for (int i = 0; i < _meshRenderers.vector().size(); i++)
        {
            if (!_meshRenderers.indexValid(i)) continue;

            auto meshKey = _meshRenderers.vector()[i]._mesh;
            auto meshDataKey = _meshes.at(meshKey)._graphicsData;

            if (!meshDataKey.has_value()) throw std::runtime_error("The mesh has no graphics data.");

            auto &gfxData = _meshesGraphicsData.at(meshDataKey.value());
            gfxData._glVao.Bind();

            _shaderBuffers.transformUbo.OglBuffer().BindRange(GPASS_UBO_BINDING_TRANSFORM,
                                                              i * _transformDataBlockAlignment,
                                                              sizeof(transform_gl_data_block));
            _renderContext->DrawElements(pmt_type_triangles, gfxData._indicesCount, idx_typ_unsigned_int, nullptr);
        }

        shadowMapData.shadowFbo.UnBind(fbo_read_draw);
    }

    void PbrRenderer::CreateShadowMap(SphereShadowMap &shadowMapData, const tao_pbr::SphereLight &l, int shadowMapResolution)
    {
        CreateShadowMap(shadowMapData, vec3{l.transformation.matrix()[3]}, vec3{l.transformation.matrix()[2]}, l.intensity, l.radius, shadowMapResolution);
    }

    void PbrRenderer::CreateShadowMap(SphereShadowMap &shadowMapData, const tao_pbr::RectLight &l, int shadowMapResolution)
    {
        // TODO: currently we limit the shadow frustum for sphere lights with a formula that shouldn't work for rect lights
        // TODO: add ComputeRectLightRadius method
        CreateShadowMap(shadowMapData, vec3{l.transformation.matrix()[3]}, vec3{l.transformation.matrix()[2]}, l.intensity, glm::min(l.size.x, l.size.y) * 0.5, shadowMapResolution);
    }

}