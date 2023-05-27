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
	    lines,
        lineStrip,
        points
    };
    class GizmosShaderLib
    {

    private:

        static constexpr const char* INCLUDE_DIRECTIVE   = "//! #include";

        static constexpr const char* LINE_STRIP_VERT_SRC = "LineStrip.vert";
        static constexpr const char* LINE_STRIP_GEO_SRC  = "ThickLineStrip.geom";
        static constexpr const char* LINE_STRIP_FRAG_SRC = "LineStrip.frag";

        static constexpr const char* POINTS_GEO_SRC      = "ThickPoints.geom";
        static constexpr const char* POINTS_VERT_SRC     = "Points.vert";
        static constexpr const char* POINTS_FRAG_SRC     = "Points.frag";

        static constexpr const char* LINES_VERT_SRC      = "Lines.vert";
        static constexpr const char* LINES_GEO_SRC       = "ThickLines.geom";
        static constexpr const char* LINES_FRAG_SRC      = "Lines.frag";

        static constexpr const char* FRAG_SRC            = "MeshUnlit.frag";
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
        static string PreProcessShaderSource(const char* sourceFilePath);
        static void LoadShaderSource(gizmos_shader_type shaderType, const char* shaderSrcDir, string* vertSrc, string* geomSrc, string* fragSrc );

    public:
        [[nodiscard]] static OglShaderProgram CreateShaderProgram(RenderContext& rc, gizmos_shader_type shaderType, const char* shaderSrcDir);
    };

}
