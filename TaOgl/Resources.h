#pragma once

#include <iostream>
#include <concepts>
#include <string>
#include <type_traits>

#include "OglUtils.h"

namespace tao_render_context
{
	class RenderContext;
}
namespace tao_ogl_resources
{
	/////////////////////
	// Wrapped OGL resources inspired by:
	// https://stackoverflow.com/questions/17161013/raii-wrapper-for-opengl-objects
	//

	struct vertex_shader
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "vertex shader";
	};
	struct fragment_shader
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "fragment shader";
	};
	struct geometry_shader
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "geometry shader";
	};
	struct shader_program
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "shader program";
	};
	struct vertex_buffer_object
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "vertex buffer object";
	};
	struct vertex_attrib_array
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "vertex attrib array";
	};
	struct index_buffer
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "index buffer";
	};
	struct uniform_buffer
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "uniform buffer";
	};
	struct shader_storage_buffer
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "shader storage buffer";
	};
	struct texture_1D
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "texture";
	};
	struct texture_2D
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "texture";
	};
	struct texture_cube
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "texture";
	};
	struct framebuffer
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "framebuffer";
	};
	struct sampler
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "sampler";
	};

	template<typename T>
	concept ogl_resource =
		std::is_same_v<T, vertex_shader> ||
		std::is_same_v<T, fragment_shader> ||
		std::is_same_v<T, geometry_shader> ||
		std::is_same_v<T, shader_program> ||
		std::is_same_v<T, vertex_buffer_object> ||
		std::is_same_v<T, vertex_attrib_array> ||
		std::is_same_v<T, index_buffer> ||
		std::is_same_v<T, uniform_buffer> ||
		std::is_same_v<T, shader_storage_buffer> ||
		std::is_same_v<T, texture_1D> ||
		std::is_same_v<T, texture_2D> ||
		std::is_same_v<T, texture_cube> ||
		std::is_same_v<T, framebuffer> ||
		std::is_same_v<T, sampler>;

	template<typename T>
	concept ogl_shader =
		std::is_same_v<T, vertex_shader> ||
		std::is_same_v<T, fragment_shader> ||
		std::is_same_v<T, geometry_shader>;

	template<typename T>
	concept ogl_buffer =
		std::is_same_v<T, vertex_buffer_object> ||
		std::is_same_v<T, index_buffer> ||
		std::is_same_v<T, uniform_buffer> ||
		std::is_same_v<T, shader_storage_buffer>;

	template <typename T> requires ogl_resource<T>
	class OglResource
	{

		// RenderContext is the one in charge of calling the private constructor
		friend class tao_render_context::RenderContext; 

	public:
		using type = T;

		OglResource(const OglResource&) = delete;

		OglResource& operator=(const OglResource&) = delete;

		OglResource(OglResource&& other) noexcept
		{
			_id = other._id;
			other._id = 0;
		}

		OglResource& operator=(OglResource&& other) noexcept
		{
			if (this != &other)
			{
				Destroy();

				_id = other._id;
				other._id = 0;
			}
			return *this;
		}

		~OglResource() { Destroy(); }

		unsigned int ID() const { return _id; }

	private:

		GLuint _id;

		void Create();
		void Destroy();
		
		OglResource() { Create(); }
	};

	template <typename T> requires ogl_resource<T>
	void OglResource<T>::Create(){_id = T::Create();}

	template <typename T> requires ogl_resource<T>
	void OglResource<T>::Destroy()
	{
		// Skip if the resources has already been freed
		// or never initialized properly
		if (_id)
		{
			T::Destroy(_id);
			_id = 0;
		}
	}

	/// Vertex Shader
	//////////////////////////////////////
	class OglVertexShader
	{
		using ogl_resource_type = vertex_shader;

		friend class tao_render_context::RenderContext;
		friend class OglShaderProgram;
	private:
		OglResource<ogl_resource_type> _ogl_obj;
		OglVertexShader(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}
	public:
		void Compile(const char* source);
	};

	/// Fragment Shader
	//////////////////////////////////////
	class OglFragmentShader
	{
		using ogl_resource_type = fragment_shader;

		friend class tao_render_context::RenderContext;
		friend class OglShaderProgram;
	private:
		OglResource<ogl_resource_type> _ogl_obj;
		OglFragmentShader(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}
	public:
		void Compile(const char* source);
	};

	/// Geometry Shader
	//////////////////////////////////////
	class OglGeometryShader
	{
		using ogl_resource_type = geometry_shader;

		friend class tao_render_context::RenderContext;
		friend class OglShaderProgram;
	private:
		OglResource<geometry_shader> _ogl_obj;
		OglGeometryShader(OglResource<geometry_shader>&& shader) :_ogl_obj(std::move(shader)) {}
	public:
		void Compile(const char* source);
	};

	/// Shader Program
	//////////////////////////////////////
	class OglShaderProgram
	{
		using ogl_resource_type = shader_program;

		friend class tao_render_context::RenderContext;
	private:
		OglResource<ogl_resource_type> _ogl_obj;
		OglShaderProgram(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}
		void AttachShader(GLuint shader);

		template <typename T> static void SetUniform(GLint location, T v0);
		template <typename T> static void SetUniform(GLint location, T v0, T v1);
		template <typename T> static void SetUniform(GLint location, T v0, T v1, T v2);
		template <typename T> static void SetUniform(GLint location, T v0, T v1, T v2, T v3);

		static void SetUniformMatrix2(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
		static void SetUniformMatrix3(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
		static void SetUniformMatrix4(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
		
	public:
		template <typename T> requires ogl_shader<typename T::ogl_resource_type>
		void AttachShader(const T& shader) { AttachShader(shader._ogl_obj.ID()); }
		void LinkProgram();
		void UseProgram();
		GLint GetUniformLocation(const char* name) const;

		template <typename T> void SetUniform(const char* name, T v0)					{ SetUniform(GetUniformLocation(name), v0); }
		template <typename T> void SetUniform(const char* name, T v0, T v1)				{ SetUniform(GetUniformLocation(name), v0, v1); }
		template <typename T> void SetUniform(const char* name, T v0, T v1, T v2)		{ SetUniform(GetUniformLocation(name), v0, v1, v2); }
		template <typename T> void SetUniform(const char* name, T v0, T v1, T v2, T v3) { SetUniform(GetUniformLocation(name), v0, v1, v2, v3); }

		// ReSharper disable CppMemberFunctionMayBeConst
		void SetUniformMatrix2(const char* name, const GLfloat* value) { SetUniformMatrix2(GetUniformLocation(name), 1, false, value); }
		void SetUniformMatrix3(const char* name, const GLfloat* value) { SetUniformMatrix3(GetUniformLocation(name), 1, false, value); }
		void SetUniformMatrix4(const char* name, const GLfloat* value) { SetUniformMatrix4(GetUniformLocation(name), 1, false, value); }
		// ReSharper restore CppMemberFunctionMayBeConst

		void SetUniformBlockBinding(const char* name, GLuint uniformBlockBinding);
		
	};

	/// VBO
	//////////////////////////////////////
	class OglVertexBuffer
	{
		using ogl_resource_type = vertex_buffer_object;

		friend class tao_render_context::RenderContext;
		friend class OglVertexAttribArray;
	private:
		OglResource<ogl_resource_type> _ogl_obj;
		OglVertexBuffer(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}

	public:
		void Bind();
		static void UnBind();
		void SetData(GLsizeiptr size, const void* data, ogl_buffer_usage usage);
		void SetSubData(GLintptr offset, GLsizeiptr size, const void* data);
	};

	/// EBO
	//////////////////////////////////////
	class OglIndexBuffer
	{
		using ogl_resource_type = index_buffer;

		friend class tao_render_context::RenderContext;
		friend class OglVertexAttribArray;
	private:
		OglResource<ogl_resource_type> _ogl_obj;
		OglIndexBuffer(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}

	public:
		void Bind();
		static void UnBind();
		void SetData(GLsizeiptr size, const void* data, ogl_buffer_usage usage);
		void SetSubData(GLintptr offset, GLsizeiptr size, const void* data);

	};

	/// VAO
	//////////////////////////////////////
	class OglVertexAttribArray
	{
		using ogl_resource_type = vertex_attrib_array;

		friend class tao_render_context::RenderContext;
		friend class OglVertexAttribArray;
	private:
		OglResource<ogl_resource_type> _ogl_obj;
		OglVertexAttribArray(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}

	public:
		void Bind();
		static void UnBind();
		void EnableVertexAttrib(GLuint index);
		void DisableVertexAttrib(GLuint index);
		void SetVertexAttribPointer(OglVertexBuffer& vertexBuffer, GLuint index, GLint size, ogl_vertex_attrib_type type, GLboolean normalized, GLsizei stride, const void* pointer, GLuint divisor=0);
		void SetIndexBuffer(OglIndexBuffer indexBuffer);
	};

	/// UBO
	//////////////////////////////////////
	class OglUniformBuffer
	{
		using ogl_resource_type = uniform_buffer;

		friend class tao_render_context::RenderContext;
		friend class OglVertexAttribArray;
	private:
		OglResource<ogl_resource_type> _ogl_obj;
		OglUniformBuffer(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}

	public:
		void Bind(GLuint index);
		void SetData(GLsizeiptr size, const void* data, ogl_buffer_usage usage);
		void SetSubData(GLintptr offset, GLsizeiptr size, const void* data);

	};

	/// SSBO
	//////////////////////////////////////
	class OglShaderStorageBuffer
	{
		using ogl_resource_type = shader_storage_buffer;

		friend class tao_render_context::RenderContext;
		friend class OglVertexAttribArray;
	private:
		OglResource<ogl_resource_type> _ogl_obj;
		OglShaderStorageBuffer(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}

	public:
		void Bind(GLuint index);
		void SetData(GLsizeiptr size, const void* data, ogl_buffer_usage usage);
		void SetSubData(GLintptr offset, GLsizeiptr size, const void* data);

	};

	/// Texture1D
	//////////////////////////////////////
	class OglTexture1D
	{
		using ogl_resource_type = texture_1D;

		friend class tao_render_context::RenderContext;

	private:
		OglResource<ogl_resource_type> _ogl_obj;
		OglTexture1D(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}
	public:
		void Bind();
		static void UnBind();
		void BindToTextureUnit  (ogl_texture_unit unit);
		static void UnBindToTextureUnit(ogl_texture_unit unit);
		void TexImage(GLint level, ogl_texture_internal_format internalFormat, GLint width, GLint height, GLint border, ogl_texture_format format, ogl_texture_data_type type, const void* data);
		void GenerateMipmap();
		void SetFilterParams(ogl_tex_filter_params params);
		void SetLodParams(ogl_tex_lod_params params);
		void SetWrapParams(ogl_tex_wrap_params params);
	};

	/// Texture2D
	//////////////////////////////////////
	class OglTexture2D
	{
		using ogl_resource_type = texture_2D;

		friend class OglFramebufferTex2D;
		friend class tao_render_context::RenderContext;

	private:
		OglResource<ogl_resource_type> _ogl_obj;
		OglTexture2D(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}
	public:
		void Bind();
		static void UnBind();
		void BindToTextureUnit(ogl_texture_unit unit);
		static void UnBindToTextureUnit(ogl_texture_unit unit);
		void TexImage(GLint level, ogl_texture_internal_format internalFormat, GLint width, GLint height, ogl_texture_format format, ogl_texture_data_type type, const void* data);
		void GenerateMipmap();
		void SetDepthStencilMode(ogl_texture_depth_stencil_tex_mode mode);
		void SetCompareParams(ogl_tex_compare_params params);
		void SetFilterParams(ogl_tex_filter_params params);
		void SetLodParams(ogl_tex_lod_params params);
		void SetWrapParams(ogl_tex_wrap_params params);
		void SetParams(ogl_tex_params params);
	};

	/// Texture Cube
	//////////////////////////////////////
	class OglTextureCube
	{
		using ogl_resource_type = texture_cube;

		friend class tao_render_context::RenderContext;

	private:
		OglResource<ogl_resource_type> _ogl_obj;
		OglTextureCube(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}
	public:
		void Bind();
		static void UnBind();
		void BindToTextureUnit(ogl_texture_unit unit);
		static void UnBindToTextureUnit(ogl_texture_unit unit);
		void TexImage(ogl_texture_cube_target target, GLint level, ogl_texture_internal_format internalFormat, GLint width, GLint height, GLint border, ogl_texture_format format, ogl_texture_data_type type, const void* data);
		void GenerateMipmap();
		void SetDepthStencilMode(ogl_texture_depth_stencil_tex_mode mode);
		void SetCompareParams(ogl_tex_compare_params params);
		void SetFilterParams(ogl_tex_filter_params params);
		void SetLodParams(ogl_tex_lod_params params);
		void SetWrapParams(ogl_tex_wrap_params params);
		void SetParams(ogl_tex_params params);
	};

	/// Sampler
	//////////////////////////////////////
	class OglSampler
	{
		using ogl_resource_type = sampler;

		friend class tao_render_context::RenderContext;

	private:
		OglResource<ogl_resource_type> _ogl_obj;
		OglSampler(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}
	public:
		void BindToTextureUnit(ogl_texture_unit unit);
		static void UnBindToTextureUnit(ogl_texture_unit unit);
		void SetCompareParams(ogl_sampler_compare_params params);
		void SetFilterParams (ogl_sampler_filter_params params);
		void SetLodParams    (ogl_sampler_lod_params params);
		void SetWrapParams   (ogl_sampler_wrap_params params);
		void SetParams       (ogl_sampler_params params);
	};

	/// FBO - Texture2D
	//////////////////////////////////////
	class OglFramebufferTex2D
	{
		using ogl_resource_type = framebuffer;

		friend class tao_render_context::RenderContext;

	private:
		OglResource<ogl_resource_type> _ogl_obj;
		OglFramebufferTex2D(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}
	public:
		void Bind(ogl_framebuffer_binding target);
		static void UnBind(ogl_framebuffer_binding target);
		void AttachTex2D(ogl_framebuffer_attachment attachment, const OglTexture2D& texture, GLint level);
		void SetDrawBuffers(GLsizei n, const ogl_framebuffer_read_draw_buffs* buffs);
		void SetReadBuffer(ogl_framebuffer_read_draw_buffs buff);
		void CopyFrom(const OglFramebufferTex2D& src, GLint width, GLint height, ogl_framebuffer_copy_mask mask);
		void CopyTo  (const OglFramebufferTex2D& dst, GLint width, GLint height, ogl_framebuffer_copy_mask mask);
		void CopyFrom(const OglFramebufferTex2D& src, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, ogl_framebuffer_copy_mask mask, ogl_framebuffer_copy_filter filter);
		void CopyTo  (const OglFramebufferTex2D& dst, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, ogl_framebuffer_copy_mask mask, ogl_framebuffer_copy_filter filter);
	};

}
