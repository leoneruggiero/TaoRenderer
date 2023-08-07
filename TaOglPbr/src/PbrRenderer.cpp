#include "PbrRenderer.h"
#include "TaOglPbrConfig.h"
#include "stb_image.h"

namespace tao_pbr
{

    using namespace  tao_ogl_resources;
    using namespace  tao_render_context;

    void PbrRenderer::InitGBuffer(int width, int height)
    {
        _gBuffer.texColor0.TexImage(0, tex_int_for_rgba16f, width, height,tex_for_rgba, tex_typ_float, nullptr);
        _gBuffer.texColor1.TexImage(0, tex_int_for_rgba16f, width, height,tex_for_rgba, tex_typ_float, nullptr);
        _gBuffer.texColor2.TexImage(0, tex_int_for_rgba16f, width, height,tex_for_rgba, tex_typ_float, nullptr);
        _gBuffer.texColor3.TexImage(0, tex_int_for_rgba16f, width, height,tex_for_rgba, tex_typ_float, nullptr);
        _gBuffer.texDepth.TexImage(0, tex_int_for_depth_stencil, width, height, tex_for_depth, tex_typ_float, nullptr);

        // TODO: https://www.khronos.org/opengl/wiki/Common_Mistakes#Creating_a_complete_texture
        // -------------------------------------------------------------------------------------
        _gBuffer.texColor0.SetFilterParams({tex_min_filter_nearest, tex_mag_filter_linear});
        _gBuffer.texColor1.SetFilterParams({tex_min_filter_nearest, tex_mag_filter_linear});
        _gBuffer.texColor2.SetFilterParams({tex_min_filter_nearest, tex_mag_filter_linear});
        _gBuffer.texColor3.SetFilterParams({tex_min_filter_nearest, tex_mag_filter_linear});
        // -------------------------------------------------------------------------------------

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

    void PbrRenderer::InitOutputBuffer(int width, int height)
    {
        _outBuffer.texColor.TexImage(0, tex_int_for_rgba16f, width, height,tex_for_rgba, tex_typ_float, nullptr);
        _outBuffer.buff.AttachTexture(fbo_attachment_color0, _outBuffer.texColor, 0);

        const ogl_framebuffer_read_draw_buffs drawBuffs = fbo_read_draw_buff_color0;
        _outBuffer.buff.SetDrawBuffers(1, &drawBuffs);
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
                ShaderLoader::LoadShader(LIGHTPASS_FRAG_SOURCE, SHADER_SRC_DIR, SHADER_SRC_DIR).c_str()
        );

        // Set uniforms
        lightPassShader.UseProgram();
        lightPassShader.SetUniform(LIGHTPASS_NAME_GBUFF0, LIGHTPASS_TEX_BINDING_GBUFF0);
        lightPassShader.SetUniform(LIGHTPASS_NAME_GBUFF1, LIGHTPASS_TEX_BINDING_GBUFF1);
        lightPassShader.SetUniform(LIGHTPASS_NAME_GBUFF2, LIGHTPASS_TEX_BINDING_GBUFF2);
        lightPassShader.SetUniform(LIGHTPASS_NAME_GBUFF3, LIGHTPASS_TEX_BINDING_GBUFF3);


        // Compute Shaders
        // --------------------------------------
        auto processEnvShader = _renderContext->CreateShaderProgram(
                ShaderLoader::LoadShader(PROCESS_ENV_COMPUTE_SOURCE, SHADER_SRC_DIR, SHADER_SRC_DIR).c_str()
                );

        // if CreateShaderProgram() throws the
        // current shader is not affected.
        _shaders.gPass                      = std::move(gPassShader);
        _shaders.lightPass                  = std::move(lightPassShader);
        _computeShaders.processEnvironment  = std::move(processEnvShader);
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
        void* data = stbi_load(image._path.c_str(), &w, &h, &c, image._numChannels);

        tao_ogl_resources::ogl_texture_internal_format ifmt;
        tao_ogl_resources::ogl_texture_format fmt;
        switch(image._numChannels)
        {
            case(1): ifmt = tao_ogl_resources::tex_int_for_red;  fmt = tao_ogl_resources::tex_for_red; break;
            case(2): ifmt = tao_ogl_resources::tex_int_for_rg;   fmt = tao_ogl_resources::tex_for_rg; break;
            case(3): ifmt = tao_ogl_resources::tex_int_for_rgb;  fmt = tao_ogl_resources::tex_for_rgb; break;
            case(4): ifmt = tao_ogl_resources::tex_int_for_rgba; fmt = tao_ogl_resources::tex_for_rgba; break;
        }

        ImageTextureGraphicsData gd{._glTexture = _renderContext->CreateTexture2D()};

        gd._glTexture.TexImage(0, ifmt, w, h, fmt, tao_ogl_resources::tex_typ_unsigned_byte, data);

        stbi_image_free(data);

        return _texturesGraphicsData.insert(std::move(gd));
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

    GenKey<PbrMaterial> PbrRenderer::AddMaterial(const PbrMaterial& material)
    {
        if(!PbrMaterial::CheckKey(material._diffuseTex, _textures))     throw std::runtime_error("Invalid texture key for `Diffuse` texture.");
        if(!PbrMaterial::CheckKey(material._metalnessMap, _textures))   throw std::runtime_error("Invalid texture key for `Metalness` texture.");
        if(!PbrMaterial::CheckKey(material._normalMap, _textures))      throw std::runtime_error("Invalid texture key for `Normal` texture.");
        if(!PbrMaterial::CheckKey(material._roughnessMap, _textures))   throw std::runtime_error("Invalid texture key for `Roughness` texture.");

        auto key = _materials.insert(material);

        return key;
    }

    void PbrRenderer::WriteTransfromToShaderBuffer(const MeshRenderer& mesh)
    {
        WriteTransfromToShaderBuffer(std::vector<MeshRenderer>{mesh});
    }

    void PbrRenderer::WriteTransfromToShaderBuffer(const std::vector<MeshRenderer>& meshes)
    {
        const int trDataBlkSize = sizeof(transform_gl_data_block);

        std::vector<transform_gl_data_block> dataTra(meshes.size());

        for (int i = 0; i < meshes.size(); i++)
        {
            glm::mat4 model = meshes[i].transformation.transformation();
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
        _shaderBuffers.transformUbo.OglBuffer().SetSubData(0, meshes.size()*_transformDataBlockAlignment, data.data());
    }

    void PbrRenderer::WriteMaterialToShaderBuffer(const MeshRenderer& mesh)
    {
        WriteMaterialToShaderBuffer((std::vector<MeshRenderer>{mesh}));
    }

    void PbrRenderer::WriteMaterialToShaderBuffer(const std::vector<MeshRenderer>& meshes)
    {
        const int matDataBlockSize = sizeof(material_gl_data_block);

        std::vector<material_gl_data_block> dataMat(meshes.size());

        for (int i = 0; i < meshes.size(); i++)
        {

            const PbrMaterial &mat = _materials.at(meshes[i].material);

            dataMat[i] = material_gl_data_block
                    {
                            .diffuse = glm::vec4(mat._diffuse, 1.0f),
                            .emission = glm::vec4(mat._emission, 1.0f),
                            .roughness = mat._roughness,
                            .metalness = mat._metalness,
                            .has_diffuse_tex = mat._diffuseTex.has_value(),
                            .has_emission_tex = mat._emissionTex.has_value(),
                            .has_normal_tex = mat._normalMap.has_value(),
                            .has_roughness_tex = mat._roughnessMap.has_value(),
                            .has_metalness_tex = mat._metalnessMap.has_value(),
                            .has_occlusion_tex = mat._occlusionMap.has_value()
                    };
        }

        BufferDataPacker pack{};
        pack.AddDataArray(dataMat);

        // Since we need to glBindBufferRange we should take
        // GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT into account.
        auto data = pack.InterleavedBuffer(_renderContext->UniformBufferOffsetAlignment());
        _shaderBuffers.materialUbo.OglBuffer().SetSubData(0, meshes.size() * _materialDataBlockAlignment, data.data());
    }

    GenKey<MeshRenderer> PbrRenderer::AddMeshRenderer(const MeshRenderer& meshRenderer)
    {
        if(!_meshes.keyValid(meshRenderer.mesh))        throw std::runtime_error("Invalid `mesh` key.");
        if(!_materials.keyValid(meshRenderer.material)) throw std::runtime_error("Invalid `material` key.");

        auto key = _meshRenderers.insert(meshRenderer);

        // Write transform and material to their buffers.
        // Resize and copy back old data if necessary.
        // ----------------------------------------------
        int rdrCount = _meshRenderers.vector().size();

        // Transformations
        if( _shaderBuffers.transformUbo.Resize(rdrCount*_transformDataBlockAlignment))
            WriteTransfromToShaderBuffer(_meshRenderers.vector()); // resized, need to re-write everything
        else
            WriteTransfromToShaderBuffer(_meshRenderers.at(key));

        // Materials
        if( _shaderBuffers.materialUbo.Resize(rdrCount*_materialDataBlockAlignment))
            WriteMaterialToShaderBuffer(_meshRenderers.vector()); // resized, need to re-write everything
        else
            WriteMaterialToShaderBuffer(_meshRenderers.at(key));

        return key;
    }

    void PbrRenderer::ReloadShaders()
    {
        InitShaders();
    }

    const OglFramebuffer<OglTexture2D>& PbrRenderer::Render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, float near, float far)
    {
        _renderContext->MakeCurrent();
        _renderContext->SetViewport(0, 0, _windowWidth, _windowHeight);

        _renderContext->SetDepthState(DEFAULT_DEPTH_STATE);
        _renderContext->SetRasterizerState(DEFAULT_RASTERIZER_STATE);
        _renderContext->SetBlendState(DEFAULT_BLEND_STATE);

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
        _gBuffer.gBuff.Bind(ogl_framebuffer_binding::fbo_read_draw);
        _renderContext->ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        _renderContext->ClearDepthStencil(1.0f, 0);

        _shaders.gPass.UseProgram();

        for(int i=0;i<_meshRenderers.vector().size(); i++)
        {
            if(!_meshRenderers.indexValid(i)) continue;

            auto meshKey = _meshRenderers.vector()[i].mesh;
            auto meshDataKey = _meshes.at(meshKey)._graphicsData;

            if(!meshDataKey.has_value()) throw std::runtime_error("The mesh has no graphics data.");

            auto& gfxData = _meshesGraphicsData.at(meshDataKey.value());
            gfxData._glVao.Bind();

            _shaderBuffers.transformUbo.OglBuffer().BindRange(GPASS_UBO_BINDING_TRANSFORM, i*_transformDataBlockAlignment, sizeof(transform_gl_data_block));
            _shaderBuffers.materialUbo.OglBuffer().BindRange(GPASS_UBO_BINDING_MATERIAL, i*_materialDataBlockAlignment, sizeof(material_gl_data_block));

            _renderContext->DrawElements(pmt_type_triangles, gfxData._indicesCount, idx_typ_unsigned_int, nullptr);
        }

        OglFramebuffer<OglTexture2D>::UnBind(fbo_read_draw);

        /// Light Pass
        ////////////////////////////////////////////
        _renderContext->SetDepthState(DEPTH_STATE_OFF);


        _outBuffer.buff.Bind(fbo_read_draw);
        _renderContext->ClearColor(0.0f, 0.0f, 0.0f, 0.0f);

        _shaders.lightPass.UseProgram();

        _gBuffer.texColor0.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_GBUFF0));
        _gBuffer.texColor1.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_GBUFF1));
        _gBuffer.texColor2.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_GBUFF2));
        _gBuffer.texColor3.BindToTextureUnit(static_cast<ogl_texture_unit>(tex_unit_0 + LIGHTPASS_TEX_BINDING_GBUFF3));

        _fsQuad.vao.Bind();
        _renderContext->DrawElements(pmt_type_triangles, 6, idx_typ_unsigned_int, nullptr);

        OglFramebuffer<OglTexture2D>::UnBind(fbo_read_draw);

        return _outBuffer.buff;
    }

    PbrRenderer::EnvironmentTextures PbrRenderer::CreateEnvironmentTextures(tao_ogl_resources::OglTexture2D &env, unsigned int outputResolution)
    {
        EnvironmentTextures res
        {
            .envCube{_renderContext->CreateTextureCube()},
            .irradianceCube{_renderContext->CreateTextureCube()}
        };

        // Initialize each face level 0
        for(ogl_texture_cube_target face = tex_tar_cube_map_negative_x; face <=tex_tar_cube_map_negative_z; face = static_cast<ogl_texture_cube_target>(face+1))
        {
            res.envCube         .TexImage(face, 0, tex_int_for_rgba16f, outputResolution, outputResolution, 0,  tex_for_rgba, tex_typ_float, nullptr);
            res.irradianceCube  .TexImage(face, 0, tex_int_for_rgba16f, outputResolution, outputResolution, 0,  tex_for_rgba, tex_typ_float, nullptr);
        }

        /* tex unit 0 */ env                 .BindToTextureUnit(tex_unit_0);
        /* img unit 0 */ res.envCube         .BindToImageUnit(0, 0, true, 0, image_access_rw, image_format_rgba16f);
        /* img unit 1 */ res.irradianceCube  .BindToImageUnit(1, 0, true, 0, image_access_write, image_format_rgba16f);

        _computeShaders.processEnvironment.UseProgram();
        _computeShaders.processEnvironment.SetUniform(PROCESS_ENV_IN_TEX_NAME       , 0);
        _computeShaders.processEnvironment.SetUniform(PROCESS_ENV_OUT_ENV_TEX_NAME  , 0);
        _computeShaders.processEnvironment.SetUniform(PROCESS_ENV_OUT_IRR_TEX_NAME  , 1);

        // TODO: profile!!! is it better to batch in bigger groups?
        _renderContext->DispatchCompute(outputResolution, outputResolution, 6 /* one for each cube face */);

        // TODO: renderContext->MemotyBarrier(...params...) !!! lazy
        GL_CALL(glMemoryBarrier(GL_ALL_BARRIER_BITS));

        env                 .UnBindToTextureUnit(tex_unit_0);
        res.envCube         .UnBindToImageUnit(0);
        res.irradianceCube  .UnBindToImageUnit(1);


    }
}