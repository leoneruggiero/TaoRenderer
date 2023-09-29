#include "RenderContextUtils.h"
#include "glm/vec3.hpp"
#include <regex>
#include <fstream>
#include <sstream>
#include <iostream>

namespace tao_render_context
{

    std::string FileNotFoundExceptionMsg(const char *pre, const char *filePath)
    {
        return std::string{}
                .append(pre)
                .append("unable to find file: ")
                .append(filePath);
    }

    std::string GenericExceptionMsg(const char *pre)
    {
        return std::string{}
                .append(pre)
                .append("a generic error has occurred.");
    }

    std::regex IncludeDirectiveRgx()
    {
        //! #include ".../.../.../fileName.glsl"
        return std::regex{std::string{}
                                  .append(R"(^)")
                                  .append("//! #include")
                                  .append(R"(\s+"(.+/)*(\w+.glsl)\")")
        };
    }

    std::regex GlslVersionRgx()
    {
        // #version ...
        return std::regex{std::string{}
                                  .append(R"(^\s*#version)")
        };
    }

    std::string
    ShaderLoader::LoadShader(const char *shaderSourceFile, const char *shaderSourceDir, const char *shaderIncludeDir)
    {
        auto shaderSourcePath = std::string()
                .append(shaderSourceDir)
                .append("/")
                .append(shaderSourceFile);

        std::ifstream srcStream(shaderSourcePath);

        if (!srcStream.is_open())
            throw std::runtime_error(FileNotFoundExceptionMsg(EXC_PREAMBLE, shaderSourcePath.c_str()));

        std::string line{};
        std::stringstream outStr{};

        // scan the file line by line
        // replace include directives
        // with include files.
        while (getline(srcStream, line)) {
            std::smatch res{};

            if (regex_match(line, res, IncludeDirectiveRgx()))
                outStr << LoadShader(
                        res[2].str().c_str(),     // include file name
                        shaderIncludeDir,          // source file dir
                        shaderIncludeDir                        // include file dir
                );

            else
                outStr << line << std::endl;
        }

        return outStr.str();
    }


    std::string ShaderLoader::DefineConditional(const std::string &shaderSource, const std::vector<std::string> &definitions)
    {
        std::string line{};
        std::stringstream inStr{shaderSource};
        std::stringstream outStr{};

        getline(inStr, line);

        // if the first line is #version ...
        if (regex_search(line, GlslVersionRgx())) {
            outStr << line << std::endl; // append #version

            // append all the symbols
            for (const std::string &def: definitions)
                outStr << DEFINE_DIRECTIVE << " " << def << std::endl;

            // copy the rest of the shader as is
            while (getline(inStr, line)) outStr << line << std::endl;
        } else
            throw std::runtime_error{"Cannot define conditional symbols. The shader is ill-formed."};

        return outStr.str();
    }

    std::string ShaderLoader::ReplaceSymbols(const std::string &shaderSource, const std::vector<std::pair<std::string, std::string>> &replacements)
    {
        std::string copy{shaderSource};

        for(const auto& pair : replacements)
        {
            std::string r = pair.first;
            copy = regex_replace(copy, regex{r}, pair.second);
        }

        return copy;
    }


    // Resizable VBO
    /////////////////////////////////
    tao_ogl_resources::OglVertexBuffer CreateEmptyVbo(tao_render_context::RenderContext& rc, tao_ogl_resources::ogl_buffer_usage usg)
    {
        return rc.CreateVertexBuffer(nullptr, 0, usg);
    }
    void ResizeVbo(tao_ogl_resources::OglVertexBuffer& buff, unsigned int newCapacity, tao_ogl_resources::ogl_buffer_usage usg)
    {
        buff.SetData(newCapacity, nullptr, usg);
    }

    // Resizable EBO
    /////////////////////////////////
    tao_ogl_resources::OglIndexBuffer  CreateEmptyEbo(RenderContext& rc, tao_ogl_resources::ogl_buffer_usage usg)
    {
        auto ebo = rc.CreateIndexBuffer();
        ebo.SetData(0, nullptr, usg);
        return ebo;
    }

    void ResizeEbo(tao_ogl_resources::OglIndexBuffer& buff, unsigned int newCapacity, tao_ogl_resources::ogl_buffer_usage usg)
    {
        buff.SetData(newCapacity, nullptr, usg);
    }

    // Resizable SSBO
    /////////////////////////////////
    tao_ogl_resources::OglShaderStorageBuffer  CreateEmptySsbo(RenderContext& rc, tao_ogl_resources::ogl_buffer_usage usg)
    {
        auto ssbo = rc.CreateShaderStorageBuffer();
        ssbo.SetData(0, nullptr, usg);
        return ssbo;
    }

    void	ResizeSsbo(tao_ogl_resources::OglShaderStorageBuffer& buff, unsigned int newCapacity, tao_ogl_resources::ogl_buffer_usage usg)
    {
        buff.SetData(newCapacity, nullptr, usg);
    }

    // Resizable UBO
    /////////////////////////////////
    tao_ogl_resources::OglUniformBuffer  CreateEmptyUbo(RenderContext& rc, tao_ogl_resources::ogl_buffer_usage usg)
    {
        auto ubo = rc.CreateUniformBuffer();
        ubo.SetData(0, nullptr, usg);
        return ubo;
    }

    void ResizeUbo(tao_ogl_resources::OglUniformBuffer& buff, unsigned int newCapacity, tao_ogl_resources::ogl_buffer_usage usg)
    {
        buff.SetData(newCapacity, nullptr, usg);
    }

    unsigned int ResizeBufferPolicy(unsigned int currVertCapacity, unsigned int newVertCount) {
        // initialize vbo size to the required count
        if (currVertCapacity == 0)					return newVertCount;

            // never shrink the size (heuristic)
        else if (currVertCapacity >= newVertCount) return currVertCapacity;

            // currVertSize < newVertCount -> increment size by the
            // smallest integer multiple of the current size (heuristic)
        else 										return currVertCapacity * ((newVertCount / currVertCapacity) + 1);
    }


    WindowCompositor::WindowCompositor(RenderContext& renderContext, int width, int height)
                                                            :   _renderContext{&renderContext},
                                                                _vbo                {_renderContext->CreateVertexBuffer()},
                                                                _vao                {_renderContext->CreateVertexAttribArray()},
                                                                _shader             {_renderContext->CreateShaderProgram()},
                                                                _linearSampler      {_renderContext->CreateSampler()}
                                                                //_colorTexture       {_renderContext->CreateTexture2D()},
                                                                //_framebuffer        {_renderContext->CreateFramebuffer<OglTexture2D>()}
    {
        constexpr const char* kVertSource =
                R"(
            #version 430 core

            layout (location = 0) in vec2 v_position;
                                 out vec2 v_uv;
            void main()
            {
                v_uv        = v_position.xy*0.5+0.5;
                gl_Position = vec4(v_position.xy, 0.0, 1.0);
            }
        )";

        constexpr const char* kFragSource =
                R"(
            #version 430 core

                                 in vec2 v_uv;
            layout(location = 0) uniform sampler2D tex;
            layout(location = 0) out vec4 outColor;

            void main()
            {
                outColor = texture(tex, v_uv);
            }
        )";
        constexpr int   kNumVerts = 6;
        constexpr float kQuadVerts[]
        {
            -1.0f,-1.0f,
             1.0f,-1.0f,
             1.0f, 1.0f,

            -1.0f,-1.0f,
             1.0f, 1.0f,
            -1.0f, 1.0f,
        };

        // VBO and VAO
        // --------------------------
        _vbo.SetData(sizeof(float)*kNumVerts*2.0, &kQuadVerts, buf_usg_static_draw);
        _vao.SetVertexAttribPointer(_vbo, 0, 2, vao_typ_float, false, 0, nullptr);
        _vao.EnableVertexAttrib(0);

        // Shader
        // --------------------------
        _shader = std::move(_renderContext->CreateShaderProgram(kVertSource, kFragSource));

        // Framebuffer
        // --------------------------
        //_colorTexture.TexImage(0, tex_int_for_rgba16f, width, height, tex_for_rgba, tex_typ_float, nullptr);
        //_framebuffer.AttachTexture(fbo_attachment_color0, _colorTexture, 0);
        //ogl_framebuffer_read_draw_buffs drawBuffs[]{fbo_read_draw_buff_color0};
        //_framebuffer.SetDrawBuffers(1, drawBuffs);


        // Samplers
        // --------------------------
        _linearSampler.SetParams(ogl_sampler_params{
                .filter_params  {.min_filter = sampler_min_filter_linear, .mag_filter = sampler_mag_filter_linear},
                .wrap_params    { /* use defaults */ },
                .lod_params     { /* use defaults */ },
                .compare_params { /* use defaults */ }
        });

        // Pipeline states
        // --------------------------
        _depthState=
        {
            .depth_test_enable = false,
            .depth_write_enable = false,
            .depth_func = depth_func_always,
            .depth_range_near = 0.0,
            .depth_range_far = 1.0
        };

        _rasterizerState=
        {
            .culling_enable = false,
            .polygon_mode = polygon_mode_fill,
            .multisample_enable = false,
            .alpha_to_coverage_enable = false,
        };

        _blendStateBlendingOn=
        {
            .blend_enable = true,

            /* --- textures are assumed to contain pre-multiplied data --- */
            .blend_equation_rgb
            {
                .blend_factor_src = blend_fac_one,
                .blend_factor_dst = blend_fac_one_minus_src_alpha
            },
            .blend_equation_alpha
            {
                .blend_factor_src = blend_fac_one,
                .blend_factor_dst = blend_fac_one_minus_src_alpha
            },

            .color_mask = mask_all
        };

        _blendStateBlendingOff=
        {
            .blend_enable = false,
            .color_mask = mask_all,
        };
    }

    void WindowCompositor::Resize(int newWidth, int newHeight)
    {
        // _colorTexture.TexImage(0, tex_int_for_rgba16f, newWidth, newHeight, tex_for_rgba, tex_typ_float, nullptr);
    }

    WindowCompositor& WindowCompositor::AddLayer(tao_ogl_resources::OglTexture2D& layerTex, location loc, blend_option blend)
    {
       blendOperation operation
       {
           .texture = &layerTex,
           .location = loc,
           .operation = blend
       };

       _operations.push_back(operation);

        return *this;
    }

    void WindowCompositor::ClearLayers()
    {
        _operations.clear();
    }

    void WindowCompositor::GetResult( /* TODO: output framebuffer*/ )
    {
        if(_operations.empty())
            throw std::runtime_error("Cannot provide the result for an empty list of operations.");

        _renderContext->SetDepthState(_depthState);
        _renderContext->SetRasterizerState(_rasterizerState);

        // TODO --------------------------------------------------------------
        //_framebuffer.Bind(fbo_read_draw);
        OglFramebuffer<OglTexture2D>::UnBind(fbo_draw);
        _renderContext->ClearColor(0.0, 0.0, 0.0, 0.0);
        // -------------------------------------------------------------------

        _shader.UseProgram();
        _vao.Bind();

        for(int i=0; i<_operations.size(); i++)
        {
            blend_option  opt = _operations[i].operation;
            location      loc = _operations[i].location;
            OglTexture2D *tex = _operations[i].texture;

            switch(opt)
            {
                case(copy):         _renderContext->SetBlendState(_blendStateBlendingOff);  break;
                case(alpha_blend):  _renderContext->SetBlendState(_blendStateBlendingOn);   break;
            }

            _renderContext->SetViewport(loc.x, loc.y, loc.width, loc.height);

            tex->BindToTextureUnit(tex_unit_0);

            _renderContext->DrawArrays(pmt_type_triangles, 0, 6);

            tex->UnBindToTextureUnit(tex_unit_0);
        }

        _vao.UnBind();

        //_framebuffer.UnBind(ogl_framebuffer_binding::fbo_read_draw);
    }
}