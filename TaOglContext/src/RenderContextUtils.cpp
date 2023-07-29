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


    void ShaderLoader::DefineConditional(std::string &shaderSource, const std::vector<std::string> &definitions)
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

        shaderSource = outStr.str();
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

}