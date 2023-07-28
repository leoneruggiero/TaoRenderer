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

}