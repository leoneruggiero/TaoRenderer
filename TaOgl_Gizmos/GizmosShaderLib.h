#pragma once
#include <string>
#include <regex>
#include "RenderContext.h"

namespace tao_gizmos
{
    using namespace std;
    using namespace tao_ogl_resources;
    using namespace tao_render_context;

    enum class gizmos_shader_type
    {
        points,
	    lines,
        lineStrip,
        mesh
    };
    enum class gizmos_shader_modifier
    {
        none,
        selection
    };

    class GizmosShaderLib
    {

    private:

        static constexpr const char* INCLUDE_DIRECTIVE   = "//! #include";
        static constexpr const char* DEFINE_DIRECTIVE    = "#define";
        static constexpr const char* SELECTION_SYMBOL    = "SELECTION";

        static constexpr const char* LINE_STRIP_VERT_SRC = "LineStrip.vert";
        static constexpr const char* LINE_STRIP_GEO_SRC  = "LineStrip.geom";
        static constexpr const char* LINE_STRIP_FRAG_SRC = "LineStrip.frag";

        static constexpr const char* POINTS_GEO_SRC      = "Points.geom";
        static constexpr const char* POINTS_VERT_SRC     = "Points.vert";
        static constexpr const char* POINTS_FRAG_SRC     = "Points.frag";

        static constexpr const char* LINES_VERT_SRC      = "Lines.vert";
        static constexpr const char* LINES_GEO_SRC       = "Lines.geom";
        static constexpr const char* LINES_FRAG_SRC      = "Lines.frag";

        static constexpr const char* MESH_VERT_SRC       = "Mesh.vert";
        static constexpr const char* MESH_FRAG_SRC       = "Mesh.frag";
        static constexpr const char* EXC_PREAMBLE        = "GizmosShaderLibrary: ";

        static string FileNotFoundExceptionMsg(const char* filePath)
        {
            return string{}
                .append(EXC_PREAMBLE)
                .append("Unable to find file: ")
                .append(filePath);
        }
        static string GenericExceptionMsg()
        {
            return string{}
                .append(EXC_PREAMBLE)
                .append("A generic error has occurred.");
        }
        static regex IncludeDirectiveRgx()
        {
            //! #include ".../.../.../fileName.glsl"
            return regex{ string{}
            .append(R"(^)")
            .append(INCLUDE_DIRECTIVE)
            .append(R"(\s+"(.+/)*(\w+.glsl)\")")
            };
        }
        static regex GlslVersionRgx()
        {
            // #version ...
            return regex{ string{}
            .append(R"(^\s*#version)")
            };
        }
        static string PreProcessShaderSource(const char* sourceFilePath);
        static void   DefineConditional     (string& shaderSource, const vector<string>& definitions);
        static void   LoadShaderSource      (gizmos_shader_type shaderType, gizmos_shader_modifier modifier, const char* shaderSrcDir, string* vertSrc, string* geomSrc, string* fragSrc );

    public:
        [[nodiscard]] static OglShaderProgram CreateShaderProgram(RenderContext& rc, gizmos_shader_type shaderType, gizmos_shader_modifier modifier, const char* shaderSrcDir);
    };

}
