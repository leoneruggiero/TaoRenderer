#include "GizmosShaderLib.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>

namespace tao_gizmos
{
    string OneLevelUp(const char* path)
    {
        return string{}.append(path).append( "/../" );
    }

    string GizmosShaderLib::PreProcessShaderSource(const char* sourceFilePath)
    {
        ifstream srcStream(sourceFilePath);

        if (!srcStream.is_open())
            throw exception(FileNotFoundExceptionMsg(sourceFilePath).c_str());

        string line{};
        stringstream outStr{};

        // scan the file line by line
        // replace include directives
        // with include files.
        while (getline(srcStream, line))
        {
            smatch res{};

            if (regex_match(line, res, IncludeDirectiveRgx()))
                outStr << PreProcessShaderSource(string{}
                    .append(OneLevelUp(sourceFilePath))         // current file dir
                    .append(res[1].str())                       // include file dir 
                    .append(res[2].str()).c_str());             // include file name
            else
                outStr << line << endl;
        }

        return outStr.str();
    }

    void GizmosShaderLib::LoadShaderSource(gizmos_shader_type shaderType, const char* shaderSrcDir, string* vertSrc, string* geomSrc, string* fragSrc)
    {
        const char* vertSrcFile;
        const char* fragSrcFile;
        const char* geomSrcFile;
        switch (shaderType)
        {
        case(gizmos_shader_type::points):   vertSrcFile = POINTS_VERT_SRC;       geomSrcFile = POINTS_GEO_SRC;       fragSrcFile = POINTS_FRAG_SRC;       break;
        case(gizmos_shader_type::lines):    vertSrcFile = LINES_VERT_SRC;        geomSrcFile = LINES_GEO_SRC;        fragSrcFile = LINES_FRAG_SRC;        break;
        case(gizmos_shader_type::lineStrip):vertSrcFile = LINE_STRIP_VERT_SRC;   geomSrcFile = LINE_STRIP_GEO_SRC;   fragSrcFile = LINE_STRIP_FRAG_SRC;   break;
        default: throw exception(GenericExceptionMsg().c_str());
        }

        *vertSrc = PreProcessShaderSource(string{}.append(shaderSrcDir).append("/").append(vertSrcFile).c_str());
        *geomSrc = PreProcessShaderSource(string{}.append(shaderSrcDir).append("/").append(geomSrcFile).c_str());
        *fragSrc = PreProcessShaderSource(string{}.append(shaderSrcDir).append("/").append(fragSrcFile).c_str());
    }

    OglShaderProgram GizmosShaderLib::CreateShaderProgram(RenderContext& rc, gizmos_shader_type shaderType, const char* shaderSrcDir)
    {
        string vertSrc, geomSrc, fragSrc;
        LoadShaderSource(shaderType, shaderSrcDir, &vertSrc, &geomSrc, &fragSrc);

        return
            rc.CreateShaderProgram(vertSrc.c_str(), geomSrc.c_str(), fragSrc.c_str());
    }
}
