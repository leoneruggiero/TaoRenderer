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

    void GizmosShaderLib::DefineConditional(string& shaderSource, const vector<string>& definitions)
    {
        string line{};
        stringstream inStr{shaderSource};
        stringstream outStr{};
        
        getline(inStr, line);

        // if the first line is #version ...
        if (regex_search(line, GlslVersionRgx())) 
        {
            outStr << line << endl; // append #version

            // append all the symbols
            for (const string& def : definitions)
                outStr << DEFINE_DIRECTIVE << " " << def << endl;

            // copy the rest of the shader as is
            while (getline(inStr, line)) outStr << line << endl;
        }
        else
            throw std::exception{ "Cannot define conditional symbols. The shader is ill-formed." };

        shaderSource = outStr.str();
    }



    void GizmosShaderLib::LoadShaderSource(gizmos_shader_type shaderType, gizmos_shader_modifier modifier, const char* shaderSrcDir, string* vertSrc, string* geomSrc, string* fragSrc)
    {
        const char* vertSrcFile;
        const char* fragSrcFile;
        const char* geomSrcFile;
        switch (shaderType)
        {
        case(gizmos_shader_type::points):   vertSrcFile = POINTS_VERT_SRC;       geomSrcFile = POINTS_GEO_SRC;       fragSrcFile = POINTS_FRAG_SRC;       break;
        case(gizmos_shader_type::lines):    vertSrcFile = LINES_VERT_SRC;        geomSrcFile = LINES_GEO_SRC;        fragSrcFile = LINES_FRAG_SRC;        break;
        case(gizmos_shader_type::lineStrip):vertSrcFile = LINE_STRIP_VERT_SRC;   geomSrcFile = LINE_STRIP_GEO_SRC;   fragSrcFile = LINE_STRIP_FRAG_SRC;   break;
        case(gizmos_shader_type::mesh):     vertSrcFile = MESH_VERT_SRC;         geomSrcFile = nullptr;              fragSrcFile = MESH_FRAG_SRC;         break;
        default: throw exception(GenericExceptionMsg().c_str());
        }

		/* Not optional */*vertSrc = PreProcessShaderSource(string{}.append(shaderSrcDir).append("/").append(vertSrcFile).c_str()); 
        if(geomSrcFile)   *geomSrc = PreProcessShaderSource(string{}.append(shaderSrcDir).append("/").append(geomSrcFile).c_str()); 
        /* Not optional */*fragSrc = PreProcessShaderSource(string{}.append(shaderSrcDir).append("/").append(fragSrcFile).c_str());

        if(modifier == gizmos_shader_modifier::selection)
        {
            /* Not optional */  DefineConditional(*vertSrc, { SELECTION_SYMBOL });
            if (geomSrcFile)    DefineConditional(*geomSrc, { SELECTION_SYMBOL });
            /* Not optional */  DefineConditional(*fragSrc, { SELECTION_SYMBOL });
        }
    }

    OglShaderProgram GizmosShaderLib::CreateShaderProgram(RenderContext& rc, gizmos_shader_type shaderType, gizmos_shader_modifier modifier, const char* shaderSrcDir)
    {
        string vertSrc;
        string fragSrc;
        string geomSrc;
    	
        LoadShaderSource(shaderType, modifier, shaderSrcDir, &vertSrc, &geomSrc, &fragSrc);

        return
            rc.CreateShaderProgram(vertSrc.c_str(), geomSrc.empty() ? nullptr : geomSrc.c_str(), fragSrc.c_str());
    }
}
