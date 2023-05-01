#include "RenderContext.h"
#include <glad/glad.h>
namespace tao_render_context
{

	// ReSharper disable CppMemberFunctionMayBeStatic

	void RenderContext::ClearColor(float red, float green, float blue, float alpha)
	{
		GL_CALL(glClearColor(red, green, blue, alpha));
		GL_CALL(glClear(ogl_clear_mask::color_bit));  
	}

	void RenderContext::ClearDepth(float value)
	{
		GL_CALL(glClearDepth(value));
		GL_CALL(glClear(ogl_clear_mask::depth_bit));
	}

	void RenderContext::ClearStencil(int value)
	{
		GL_CALL(glClearStencil(value));
		GL_CALL(glClear(ogl_clear_mask::stencil_bit));
	}

	void RenderContext::ClearDepthStencil(double depth_val, int stencil_val)
	{
		GL_CALL(glClearStencil(stencil_val));
		GL_CALL(glClearDepth(depth_val)); 
		GL_CALL(glClear(ogl_clear_mask::depth_bit | ogl_clear_mask::stencil_bit));
	}

	void RenderContext::SetDepthState(ogl_depth_state state)
	{
		GL_CALL(
		if (state.depth_test_enable)glEnable (GL_DEPTH_TEST); 
		else						glDisable(GL_DEPTH_TEST);)
		GL_CALL(glDepthFunc(state.depth_func);								);
		GL_CALL(glDepthRange(state.depth_range_near, state.depth_range_far););
	}

	void RenderContext::DrawArrays(ogl_primitive_type mode, GLint first, GLsizei count)
	{
		GL_CALL(glDrawArrays(mode, first, count));
	}

	OglVertexShader RenderContext::CreateVertexShader()	
	{
		return OglVertexShader{ OglResource<vertex_shader>{} };
	}
	OglVertexShader RenderContext::CreateVertexShader(const char* source) 
	{
		auto v = OglVertexShader{ OglResource<vertex_shader>{} };
		v.Compile(source);
		return v;
	}
	OglFragmentShader RenderContext::CreateFragmentShader() 
	{
		return OglFragmentShader{ OglResource<fragment_shader>{} };
	}
	OglFragmentShader RenderContext::CreateFragmentShader(const char* source) 
	{
		auto f = OglFragmentShader{ OglResource<fragment_shader>{} };
		f.Compile(source);
		return f;
	}
	OglGeometryShader RenderContext::CreateGeometryShader() 
	{
		return OglGeometryShader{ OglResource<geometry_shader>{} };
	}
	OglGeometryShader RenderContext::CreateGeometryShader(const char* source) 
	{
		auto g = OglGeometryShader{ OglResource<geometry_shader>{} };
		g.Compile(source);
		return g;
	}
	OglShaderProgram RenderContext::CreateShaderProgram() 
	{
		return OglShaderProgram{ OglResource<shader_program>{} };
	}
	OglShaderProgram RenderContext::CreateShaderProgram(const char* vertSource, const char* geomSource, const char* fragSource) 
	{
		if (!vertSource || !fragSource)
			throw std::exception("CreateShaderProgram: null vertex or fragment shader source code.");
		

		auto p = OglShaderProgram{ OglResource<shader_program>{} };
		auto v = OglVertexShader{ OglResource<vertex_shader>() };
		auto f = OglFragmentShader{ OglResource<fragment_shader>() };

		v.Compile(vertSource);
		f.Compile(fragSource);

		p.AttachShader(v);
		p.AttachShader(f);

		if(geomSource) // geom stage is optional
		{
			auto g = OglGeometryShader{ OglResource<geometry_shader>() };
			g.Compile(geomSource);
			p.AttachShader(g);
		}

		p.LinkProgram();

		return p;
	}
	OglShaderProgram RenderContext::CreateShaderProgram(const char* vertSource, const char* fragSource) 
	{
		return CreateShaderProgram(vertSource, nullptr, fragSource);
	}

	static constexpr int VertexAttribTypeSize(ogl_vertex_attrib_type format)
	{
		switch (format)
		{
		case(ogl_vertex_attrib_type::vao_typ_byte):				return 1;  break;
		case(ogl_vertex_attrib_type::vao_typ_unsigned_byte):	return 1;  break;
		case(ogl_vertex_attrib_type::vao_typ_short):			return 16; break;
		case(ogl_vertex_attrib_type::vao_typ_unsigned_short):	return 16; break;
		case(ogl_vertex_attrib_type::vao_typ_int):				return 32; break;
		case(ogl_vertex_attrib_type::vao_typ_unsigned_int):		return 32; break;
		case(ogl_vertex_attrib_type::vao_typ_half_float):		return 16; break;
		case(ogl_vertex_attrib_type::vao_typ_double):			return 64; break;
		case(ogl_vertex_attrib_type::vao_typ_fixed):			return 64; break;
		default: return -1; break;
		}
	}
	OglVertexBuffer RenderContext::CreateVertexBuffer()
	{
		return OglVertexBuffer{ OglResource<vertex_buffer_object>{} };
	}
	OglVertexBuffer RenderContext::CreateVertexBuffer(const void* data, int size, ogl_buffer_usage usage)
	{
		auto vb = OglVertexBuffer{OglResource<vertex_buffer_object>{}} ;

		vb.SetData(size, data, usage);

		return vb;
	}
	OglVertexAttribArray RenderContext::CreateVertexAttribArray()
	{
		return OglVertexAttribArray{ OglResource<vertex_attrib_array>{} };
	}
	OglVertexAttribArray RenderContext::CreateVertexAttribArray(const vector<pair<shared_ptr<OglVertexBuffer>, vertex_buffer_layout_desc>> vertexDataSrc)
	{
		auto vao = OglVertexAttribArray{OglResource<vertex_attrib_array>{}};

		// foreach vbo V (multiple vbos for a vao)
		// foreach attribute A in V 
		for (int vbo = 0; vbo < vertexDataSrc.size(); vbo++)
		for (int att = 0; att < vertexDataSrc[vbo].second.attrib_count; att++)
		{
			const vertex_attribute_desc* atDsc = &vertexDataSrc[vbo].second.attrib_descriptors[att];
			vao.EnableVertexAttrib(atDsc->index); 
			vao.SetVertexAttribPointer(*vertexDataSrc[vbo].first, atDsc->index, atDsc->element_count, atDsc->element_type, false,atDsc->stride , reinterpret_cast<const void*>(atDsc->offset));
			// attributes are enabled but never disabled,
			// see https://community.khronos.org/t/should-i-disable-a-vertex-attrib-array-if-a-program-doesnt-use-it/109656/4
		}

		return vao;
	}
	OglUniformBuffer RenderContext::CreateUniformBuffer()
	{
		return OglUniformBuffer{ OglResource<uniform_buffer>{} };
	}
	OglTexture2D RenderContext::CreateTexture2D()
	{
		return OglTexture2D{ OglResource<texture_2D>{} };
	}

	OglSampler RenderContext::CreateSampler()
	{
		return OglSampler{ OglResource<sampler>{} };
	}

	OglSampler RenderContext::CreateSampler(ogl_sampler_params params)
	{
		auto s = OglSampler{ OglResource<sampler>{} };

		s.SetParams(params);

		return s;
	}

	OglFramebufferTex2D RenderContext::CreateFramebufferTex2D()
	{
		return OglFramebufferTex2D{ OglResource<framebuffer>{} };
	}

	// ReSharper restore CppMemberFunctionMayBeStatic
}