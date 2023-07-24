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
		GL_CALL
		(
			if (state.depth_test_enable) glEnable (GL_DEPTH_TEST);
			else						 glDisable(GL_DEPTH_TEST);
		);

		GL_CALL(glDepthMask(state.depth_write_enable));
		GL_CALL(glDepthFunc(state.depth_func);								);
		GL_CALL(glDepthRange(state.depth_range_near, state.depth_range_far););
	}

	void RenderContext::SetBlendState(ogl_blend_state state)
	{
		// todo: dual source blending
		GL_CALL
		(
			if (state.blend_enable)	glEnable (GL_BLEND);
			else					glDisable(GL_BLEND);
		);
		GL_CALL
		(
			glBlendFuncSeparate(
				state.blend_equation_rgb.blend_factor_src,   // src rgb
				state.blend_equation_rgb.blend_factor_dst,	 // dst rgb
				state.blend_equation_alpha.blend_factor_src, // src alpha
				state.blend_equation_alpha.blend_factor_dst  // dst alpha
			);
		);
		GL_CALL
		(
			glBlendEquationSeparate(
				state.blend_equation_rgb.blend_func,		// func rgb
				state.blend_equation_alpha.blend_func		// func alpha
			);
		);
		GL_CALL(
			glBlendColor(
				state.blend_const_color_r,
				state.blend_const_color_g,
				state.blend_const_color_b,
				state.blend_const_color_a
			);
		);
		GL_CALL(
			glColorMask(
				state.color_mask.mask_red,
				state.color_mask.mask_green,
				state.color_mask.mask_blue,
				state.color_mask.mask_alpha
			);
		)
	}

	void RenderContext::SetRasterizerState(ogl_rasterizer_state state)
	{
		GL_CALL(
			if (state.multisample_enable) glEnable(GL_MULTISAMPLE);
			else glDisable(GL_MULTISAMPLE);
		);
		GL_CALL(
			if (state.alpha_to_coverage_enable) glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
			else glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
		);
		GL_CALL(
			if (state.culling_enable) glEnable(GL_CULL_FACE);
			else glDisable(GL_CULL_FACE);
		);
		GL_CALL(glFrontFace(state.front_face););
		GL_CALL(glCullFace(state.cull_mode););
		GL_CALL(glPolygonMode(GL_FRONT_AND_BACK, state.polygon_mode););
	}

	void RenderContext::SetViewport(GLint x, GLint y, GLsizei width, GLsizei height)
	{
		GL_CALL(glViewport(x, y, width, height));
	}

	void RenderContext::ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, ogl_read_pixels_format format, ogl_texture_data_type type, void* data)
	{
		GL_CALL(glReadPixels(x, y, width, height, format, type, data));
	}

	void RenderContext::ReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, ogl_read_pixels_format format, ogl_texture_data_type type, GLsizei bufSize, void* data)
	{
		glReadnPixels(x, y, width, height, format, type, bufSize, data);
	}


	void RenderContext::DrawArrays(ogl_primitive_type mode, GLint first, GLsizei count)
	{
		GL_CALL(glDrawArrays(mode, first, count));
	}

	void RenderContext::DrawArraysInstanced(ogl_primitive_type mode, GLint first, GLsizei count, GLsizei instanceCount)
	{
		GL_CALL(glDrawArraysInstanced(mode, first,  count, instanceCount));
	}

	void RenderContext::DrawElementsInstanced(ogl_primitive_type mode, GLsizei count, ogl_indices_type type, const void* offset, GLsizei instanceCount)
	{
		GL_CALL(glDrawElementsInstanced( mode, count, type, offset, instanceCount));
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
			throw std::runtime_error("CreateShaderProgram: null vertex or fragment shader source code.");
		

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

	OglIndexBuffer RenderContext::CreateIndexBuffer()
	{
		return OglIndexBuffer{ OglResource<index_buffer>{} };
	}

	OglShaderStorageBuffer RenderContext::CreateShaderStorageBuffer()
	{
		return OglShaderStorageBuffer{ OglResource<shader_storage_buffer>{} };
	}

	OglPixelPackBuffer RenderContext::CreatePixelPackBuffer()
	{
		return OglPixelPackBuffer{ OglResource<pixel_pack_buffer>{} };
	}

	OglFence RenderContext::CreateFence()
	{
		return OglFence{ sync_condition_gpu_commands_complete };
	}

	OglTexture2D RenderContext::CreateTexture2D()
	{
		return OglTexture2D{ OglResource<texture_2D>{} };
	}

	OglTexture2DMultisample RenderContext::CreateTexture2DMultisample()
	{
		return OglTexture2DMultisample{ OglResource<texture_2D_multisample>{} };
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

	template<typename Tex> requires ogl_texture<typename Tex::ogl_resource_type>
	OglFramebuffer<Tex> RenderContext::CreateFramebuffer()
	{
		return OglFramebuffer<Tex>{ OglResource<framebuffer>{} };
	}

	// Forcing template instantiation
	template OglFramebuffer<OglTexture2D>			 RenderContext::CreateFramebuffer();
	template OglFramebuffer<OglTexture2DMultisample> RenderContext::CreateFramebuffer();
	template OglFramebuffer<OglTextureCube>			 RenderContext::CreateFramebuffer();

	// ReSharper restore CppMemberFunctionMayBeStatic
}