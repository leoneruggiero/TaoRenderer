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

    void PbrRenderer::InitShaders()
    {
        _shaders.gPass = _renderContext->CreateShaderProgram(
                tao_render_context::ShaderLoader::LoadShader(GPASS_VERT_SOURCE, SHADER_SRC_DIR, SHADER_SRC_DIR).c_str(),
                tao_render_context::ShaderLoader::LoadShader(GPASS_FRAG_SOURCE, SHADER_SRC_DIR, SHADER_SRC_DIR).c_str()
        );
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

    void PbrRenderer::Render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, float near, float far)
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


        ///// Geometry Pass
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

            // transform and materials are stored in a single buffer
            _shaderBuffers.transformUbo.OglBuffer().BindRange(GPASS_UBO_BINDING_TRANSFORM, i*sizeof(transform_gl_data_block), sizeof(transform_gl_data_block));
            _shaderBuffers.materialUbo.OglBuffer().BindRange(GPASS_UBO_BINDING_MATERIAL, i*sizeof(material_gl_data_block), sizeof(material_gl_data_block));

            _renderContext->DrawElements(pmt_type_triangles, gfxData._indicesCount, idx_typ_unsigned_int, nullptr);
        }

    }
}