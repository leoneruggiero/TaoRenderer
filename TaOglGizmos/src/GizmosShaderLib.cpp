#include "GizmosShaderLib.h"
#include "RenderContextUtils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>

namespace tao_gizmos
{
    OglShaderProgram GizmosShaderLib::CreateShaderProgram(RenderContext& rc, gizmos_shader_type shaderType, gizmos_shader_modifier modifier, const char* shaderSrcDir)
    {
        string vertSrc;
        string fragSrc;
        string geomSrc;

        const char* vertSrcFile;
        const char* fragSrcFile;
        const char* geomSrcFile;

        switch (shaderType)
        {
            case(gizmos_shader_type::points):   vertSrcFile = POINTS_VERT_SRC;       geomSrcFile = POINTS_GEO_SRC;       fragSrcFile = POINTS_FRAG_SRC;       break;
            case(gizmos_shader_type::lines):    vertSrcFile = LINES_VERT_SRC;        geomSrcFile = LINES_GEO_SRC;        fragSrcFile = LINES_FRAG_SRC;        break;
            case(gizmos_shader_type::lineStrip):vertSrcFile = LINE_STRIP_VERT_SRC;   geomSrcFile = LINE_STRIP_GEO_SRC;   fragSrcFile = LINE_STRIP_FRAG_SRC;   break;
            case(gizmos_shader_type::mesh):     vertSrcFile = MESH_VERT_SRC;         geomSrcFile = nullptr;              fragSrcFile = MESH_FRAG_SRC;         break;
            default: throw runtime_error("Unknown shader type.");
        }

        /* Not optional */vertSrc = ShaderLoader::LoadShader(vertSrcFile,shaderSrcDir,shaderSrcDir );
        if(geomSrcFile)   geomSrc = ShaderLoader::LoadShader(geomSrcFile,shaderSrcDir,shaderSrcDir );
        /* Not optional */fragSrc = ShaderLoader::LoadShader(fragSrcFile,shaderSrcDir,shaderSrcDir );

        if(modifier == gizmos_shader_modifier::selection)
        {
            /* Not optional */  ShaderLoader::DefineConditional(vertSrc, { SELECTION_SYMBOL });
            if (geomSrcFile)    ShaderLoader::DefineConditional(geomSrc, { SELECTION_SYMBOL });
            /* Not optional */  ShaderLoader::DefineConditional(fragSrc, { SELECTION_SYMBOL });
        }

        return
            rc.CreateShaderProgram(vertSrc.c_str(), geomSrc.empty() ? nullptr : geomSrc.c_str(), fragSrc.c_str());
    }
}
