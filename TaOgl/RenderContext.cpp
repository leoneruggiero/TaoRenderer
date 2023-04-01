#include "RenderContext.h"

namespace render_context
{
	std::shared_ptr<OglVertexShader>	RenderContext::CreateVertexShader()	
	{
		return std::shared_ptr<OglVertexShader>(new OglVertexShader{ OglResource<vertex_shader>{} });
	}
	std::shared_ptr<OglVertexShader>	RenderContext::CreateVertexShader(const char* source) 
	{
		auto v = std::shared_ptr<OglVertexShader>(new OglVertexShader{ OglResource<vertex_shader>{} });
		v->Compile(source); // TODO
		return v;
	}

	std::shared_ptr<OglFragmentShader>	RenderContext::CreateFragmentShader() 
	{
		return std::shared_ptr<OglFragmentShader>(new OglFragmentShader{ OglResource<fragment_shader>{} });
	}
	std::shared_ptr<OglFragmentShader>	RenderContext::CreateFragmentShader(const char* source) 
	{
		auto f = std::shared_ptr<OglFragmentShader>(new OglFragmentShader{ OglResource<fragment_shader>{} });
		f->Compile(source);// TODO
		return f;
	}

	std::shared_ptr<OglGeometryShader>	RenderContext::CreateGeometryShader() 
	{
		return std::shared_ptr<OglGeometryShader>(new OglGeometryShader{ OglResource<geometry_shader>{} });
	}
	std::shared_ptr<OglGeometryShader>	RenderContext::CreateGeometryShader(const char* source) 
	{
		auto g = std::shared_ptr<OglGeometryShader>(new OglGeometryShader{ OglResource<geometry_shader>{} });
		g->Compile(source);// TODO
		return g;
	}

	std::shared_ptr<OglShaderProgram>	RenderContext::CreateShaderProgram() 
	{
		return std::shared_ptr<OglShaderProgram>(new OglShaderProgram{ OglResource<shader_program>{} });
	}
	std::shared_ptr<OglShaderProgram>	RenderContext::CreateShaderProgram(const char* vertSource, const char* geomSource, const char* fragSource) 
	{
		if (!vertSource || !fragSource)
			throw std::exception("CreateShaderProgram: null vertex or fragment shader source code.");
		

		auto p = std::shared_ptr<OglShaderProgram>(new OglShaderProgram{ OglResource<shader_program>{} });
		auto v = OglVertexShader{ OglResource<vertex_shader>() };
		auto f = OglFragmentShader{ OglResource<fragment_shader>() };

		v.Compile(vertSource);
		f.Compile(fragSource);

		p->AttachShader(v);
		p->AttachShader(f);

		if(geomSource) // geom stage is optional
		{
			auto g = OglGeometryShader{ OglResource<geometry_shader>() };
			g.Compile(geomSource);
			p->AttachShader(g);
		}

		GL_CALL(p->LinkProgram());

		return p;
	}
	std::shared_ptr<OglShaderProgram>	RenderContext::CreateShaderProgram(const char* vertSource, const char* fragSource) 
	{
		return CreateShaderProgram(vertSource, nullptr, fragSource);
	}
}