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
    struct compute_shader
    {
        static GLuint Create();
        static void Destroy(GLuint);
        static constexpr const char* to_string = "compute shader";
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
	struct pixel_pack_buffer
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "shader storage buffer";
	};
	struct pixel_unpack_buffer
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "shader storage buffer";
	};
	struct texture_1D
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "texture 1D";
	};
	struct texture_2D
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "texture 2D";
	};
	struct texture_2D_multisample
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "texture 2D multisample";
	};
	struct texture_cube
	{
		static GLuint Create();
		static void Destroy(GLuint);
		static constexpr const char* to_string = "texture cube";
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
    struct query
    {
        static GLuint Create();
        static void Destroy(GLuint);
        static constexpr const char* to_string = "query";
    };

	template<typename T>
	concept ogl_resource =
		std::is_same_v<T, vertex_shader>			||
		std::is_same_v<T, fragment_shader>			||
		std::is_same_v<T, geometry_shader>			||
        std::is_same_v<T, compute_shader>			||
		std::is_same_v<T, shader_program>			||
		std::is_same_v<T, vertex_buffer_object>		||
		std::is_same_v<T, vertex_attrib_array>		||
		std::is_same_v<T, index_buffer>				||
		std::is_same_v<T, uniform_buffer>			||
		std::is_same_v<T, shader_storage_buffer>	||
		std::is_same_v<T, pixel_pack_buffer>		||
		std::is_same_v<T, pixel_unpack_buffer>		||
		std::is_same_v<T, texture_1D>				||
		std::is_same_v<T, texture_2D>				||
		std::is_same_v<T, texture_2D_multisample>	||
		std::is_same_v<T, texture_cube>				||
		std::is_same_v<T, framebuffer>				||
		std::is_same_v<T, sampler>                  ||
        std::is_same_v<T, query>;

	template<typename T>
	concept ogl_shader =
		std::is_same_v<T, vertex_shader>	||
		std::is_same_v<T, fragment_shader>	||
		std::is_same_v<T, geometry_shader>  ||
        std::is_same_v<T, compute_shader>;

	template<typename T>
	concept ogl_buffer =
		std::is_same_v<T, vertex_buffer_object> ||
		std::is_same_v<T, index_buffer>			||
		std::is_same_v<T, uniform_buffer>		||
		std::is_same_v<T, pixel_pack_buffer>	||
		std::is_same_v<T, pixel_unpack_buffer>	||
		std::is_same_v<T, shader_storage_buffer>;

	template<typename T>
	concept ogl_texture =
		std::is_same_v<T, texture_1D>			||
		std::is_same_v<T, texture_2D>			||
		std::is_same_v<T, texture_cube>			||
		std::is_same_v<T, texture_2D_multisample>;

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

	template<typename Tex> requires ogl_texture<typename Tex::ogl_resource_type>
		class OglFramebuffer;

	/// Fragment Shader
	//////////////////////////////////////
	class OglFragmentShader
	{
		friend class tao_render_context::RenderContext;
		friend class OglShaderProgram;

	public:
        typedef fragment_shader ogl_resource_type;
        void Compile(const char* source);

    private:
        OglResource<ogl_resource_type> _ogl_obj;
        OglFragmentShader(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}
	};

	/// Geometry Shader
	//////////////////////////////////////
	class OglGeometryShader
	{
		friend class tao_render_context::RenderContext;
		friend class OglShaderProgram;
	public:
        typedef geometry_shader ogl_resource_type;
        void Compile(const char* source);

    private:
        OglResource<geometry_shader> _ogl_obj;
        OglGeometryShader(OglResource<geometry_shader>&& shader) :_ogl_obj(std::move(shader)) {}
	};

    /// Compute Shader
    //////////////////////////////////////
    class OglComputeShader
    {
        friend class tao_render_context::RenderContext;
        friend class OglShaderProgram;
    public:
        typedef compute_shader ogl_resource_type;
        void Compile(const char* source);

    private:
        OglResource<compute_shader> _ogl_obj;
        OglComputeShader(OglResource<compute_shader>&& shader) :_ogl_obj(std::move(shader)) {}
    };

	/// Shader Program
	//////////////////////////////////////
	class OglShaderProgram
	{
		friend class tao_render_context::RenderContext;

	public:
        typedef shader_program ogl_resource_type;

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

    };

	/// VBO
	//////////////////////////////////////
	class OglVertexBuffer
	{
		friend class tao_render_context::RenderContext;

	public:
        typedef vertex_buffer_object ogl_resource_type;
        void Bind();
		static void UnBind();
		void SetData(GLsizeiptr size, const void* data, ogl_buffer_usage usage);
		void SetSubData(GLintptr offset, GLsizeiptr size, const void* data);

    private:
        OglResource<ogl_resource_type> _ogl_obj;
        OglVertexBuffer(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}

    };

	/// EBO
	//////////////////////////////////////
	class OglIndexBuffer
	{
		friend class tao_render_context::RenderContext;

	public:
        typedef index_buffer ogl_resource_type;
        void Bind();
		static void UnBind();
		void SetData(GLsizeiptr size, const void* data, ogl_buffer_usage usage);
		void SetSubData(GLintptr offset, GLsizeiptr size, const void* data);

    private:
        OglResource<ogl_resource_type> _ogl_obj;
        OglIndexBuffer(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}

	};

	/// VAO
	//////////////////////////////////////
	class OglVertexAttribArray
	{
		friend class tao_render_context::RenderContext;

	public:
        typedef vertex_attrib_array ogl_resource_type;
        void Bind();
		static void UnBind();
		void EnableVertexAttrib(GLuint index);
		void DisableVertexAttrib(GLuint index);
		void SetVertexAttribPointer(OglVertexBuffer& vertexBuffer, GLuint index, GLint size, ogl_vertex_attrib_type type, GLboolean normalized, GLsizei stride, const void* pointer, GLuint divisor=0);
		void SetIndexBuffer(OglIndexBuffer& indexBuffer);

    private:
        OglResource<ogl_resource_type> _ogl_obj;
        OglVertexAttribArray(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}

    };

	/// UBO
	//////////////////////////////////////
	class OglUniformBuffer
	{
		friend class tao_render_context::RenderContext;

	public:
        typedef uniform_buffer ogl_resource_type;
        void Bind(GLuint index);
        void BindRange(GLuint index, GLintptr offset, GLsizeiptr size);
		void SetData(GLsizeiptr size, const void* data, ogl_buffer_usage usage);
		void SetSubData(GLintptr offset, GLsizeiptr size, const void* data);

    private:
        OglResource<ogl_resource_type> _ogl_obj;
        OglUniformBuffer(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}

    };

	/// SSBO
	//////////////////////////////////////
	class OglShaderStorageBuffer
	{
        friend class tao_render_context::RenderContext;

    public:
        typedef shader_storage_buffer ogl_resource_type;
        void Bind(GLuint index);
        void BindRange(GLuint index, GLintptr offset, GLsizeiptr size);
        void SetData(GLsizeiptr size, const void* data, ogl_buffer_usage usage);
        void SetSubData(GLintptr offset, GLsizeiptr size, const void* data);

    private:
		OglResource<ogl_resource_type> _ogl_obj;
		OglShaderStorageBuffer(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}
	};

	/// PBO (pack)
	//////////////////////////////////////
	class OglPixelPackBuffer
	{
		friend class tao_render_context::RenderContext;

	public:
        typedef pixel_pack_buffer ogl_resource_type;
		void Bind();
		static void UnBind();
		void BufferStorage(GLsizeiptr size, const void* data, ogl_buffer_flags flags);
		void* MapBuffer(ogl_map_flags access);
		void UnmapBuffer();

    private:
        OglResource<ogl_resource_type> _ogl_obj;
        OglPixelPackBuffer(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}

    };

	/// PBO (unpack)
	//////////////////////////////////////
	class OglPixelUnpackBuffer
	{
		friend class tao_render_context::RenderContext;

	public:
        typedef pixel_unpack_buffer ogl_resource_type;

        void Bind();
		static void UnBind();

    private:
        OglResource<ogl_resource_type> _ogl_obj;
        OglPixelUnpackBuffer(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}

    };

	/// Texture1D
	//////////////////////////////////////
	class OglTexture1D
	{
		friend class tao_render_context::RenderContext;

	public:
        typedef texture_1D ogl_resource_type;
        void Bind();
		static void UnBind();
		void BindToTextureUnit  (ogl_texture_unit unit);
		static void UnBindToTextureUnit(ogl_texture_unit unit);
        void BindToImageUnit  (GLuint unit, GLint level, ogl_image_access access, ogl_image_format format);
        static void UnBindToImageUnit(GLuint unit);
		void TexImage(GLint level, ogl_texture_internal_format internalFormat, GLsizei width, GLsizei height, GLint border, ogl_texture_format format, ogl_texture_data_type type, const void* data);
		void GenerateMipmap();
		void SetFilterParams(ogl_tex_filter_params params);
		void SetLodParams(ogl_tex_lod_params params);
		void SetWrapParams(ogl_tex_wrap_params params);
    private:
        OglResource<ogl_resource_type> _ogl_obj;
        OglTexture1D(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}
	};

	/// Texture2D
	//////////////////////////////////////
	class OglTexture2D
	{
		template<typename Tex> requires ogl_texture<typename Tex::ogl_resource_type>
		friend class OglFramebuffer;
		friend class tao_render_context::RenderContext;

	public:
        typedef texture_2D ogl_resource_type;
        void Bind();
		static void UnBind();
		void BindToTextureUnit(ogl_texture_unit unit);
		static void UnBindToTextureUnit(ogl_texture_unit unit);
        void BindToImageUnit  (GLuint unit, GLint level, ogl_image_access access, ogl_image_format format);
        static void UnBindToImageUnit(GLuint unit);
		void TexImage(GLint level, ogl_texture_internal_format internalFormat, GLsizei width, GLsizei height, ogl_texture_format format, ogl_texture_data_type type, const void* data);
		void GenerateMipmap();
		void SetDepthStencilMode(ogl_texture_depth_stencil_tex_mode mode);
		void SetCompareParams(ogl_tex_compare_params params);
		void SetFilterParams(ogl_tex_filter_params params);
		void SetLodParams(ogl_tex_lod_params params);
		void SetWrapParams(ogl_tex_wrap_params params);
		void SetParams(ogl_tex_params params);

    private:
        OglResource<ogl_resource_type> _ogl_obj;
        OglTexture2D(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}
	};

	/// Texture2D Multisample
	//////////////////////////////////////
	class OglTexture2DMultisample
	{
		template<typename Tex> requires ogl_texture<typename Tex::ogl_resource_type>
		friend class OglFramebuffer;
		friend class tao_render_context::RenderContext;

	public:
        typedef  texture_2D_multisample ogl_resource_type;

        void Bind();
		static void UnBind();
		void BindToTextureUnit(ogl_texture_unit unit);
		static void UnBindToTextureUnit(ogl_texture_unit unit);
        void BindToImageUnit  (GLuint unit, GLint level, ogl_image_access access, ogl_image_format format);
        static void UnBindToImageUnit(GLuint unit);
		void TexImage(GLsizei samples, ogl_texture_internal_format internalFormat, GLsizei width, GLsizei height, GLboolean fixedSampleLocation);

    private:
        OglResource<ogl_resource_type> _ogl_obj;
        OglTexture2DMultisample(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}
	};

	/// Texture Cube
	//////////////////////////////////////
	class OglTextureCube
	{
		template<typename Tex> requires ogl_texture<typename Tex::ogl_resource_type>
		friend class OglFramebuffer;
		friend class tao_render_context::RenderContext;

    public:
        typedef texture_cube ogl_resource_type;
        void Bind();
		static void UnBind();
		void BindToTextureUnit(ogl_texture_unit unit);
		static void UnBindToTextureUnit(ogl_texture_unit unit);
        void BindToImageUnit  (GLuint unit, GLint level, GLboolean  layered, GLint layer, ogl_image_access access, ogl_image_format format);
        static void UnBindToImageUnit(GLuint unit);
		void TexImage(ogl_texture_cube_target target, GLint level, ogl_texture_internal_format internalFormat, GLsizei width, GLsizei height, GLint border, ogl_texture_format format, ogl_texture_data_type type, const void* data);
		void GenerateMipmap();
		void SetDepthStencilMode(ogl_texture_depth_stencil_tex_mode mode);
		void SetCompareParams(ogl_tex_compare_params params);
		void SetFilterParams(ogl_tex_filter_params params);
		void SetLodParams(ogl_tex_lod_params params);
		void SetWrapParams(ogl_tex_wrap_params params);
		void SetParams(ogl_tex_params params);

    private:
        OglResource<ogl_resource_type> _ogl_obj;
        OglTextureCube(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}

    };

	/// Sampler
	//////////////////////////////////////
	class OglSampler
	{
		friend class tao_render_context::RenderContext;

	public:
        typedef sampler ogl_resource_type;
        void BindToTextureUnit(ogl_texture_unit unit);
		static void UnBindToTextureUnit(ogl_texture_unit unit);
		void SetCompareParams(ogl_sampler_compare_params params);
		void SetFilterParams (ogl_sampler_filter_params params);
		void SetLodParams    (ogl_sampler_lod_params params);
		void SetWrapParams   (ogl_sampler_wrap_params params);
		void SetParams       (ogl_sampler_params params);

    private:
        OglResource<ogl_resource_type> _ogl_obj;
        OglSampler(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}
	};

	/// FBO 
	//////////////////////////////////////
	template<typename Tex> requires ogl_texture<typename Tex::ogl_resource_type> 
	class OglFramebuffer
	{
		friend class tao_render_context::RenderContext;

	public:
        typedef framebuffer ogl_resource_type;
        void Bind(ogl_framebuffer_binding target);
		static void UnBind(ogl_framebuffer_binding target);
		void AttachTexture(ogl_framebuffer_attachment attachment, const Tex& texture, GLint level);
		void SetDrawBuffers(GLsizei n, const ogl_framebuffer_read_draw_buffs* buffs);
		void SetReadBuffer(ogl_framebuffer_read_draw_buffs buff);

        template<typename TSrc> requires ogl_texture<typename TSrc::ogl_resource_type>
        void CopyFrom(const OglFramebuffer<TSrc>* src, GLint width, GLint height, ogl_framebuffer_copy_mask mask);

        template<typename TDst> requires ogl_texture<typename TDst::ogl_resource_type>
		void CopyTo  (OglFramebuffer<TDst>* dst, GLint width, GLint height, ogl_framebuffer_copy_mask mask) const;

        template<typename TSrc> requires ogl_texture<typename TSrc::ogl_resource_type>
        void CopyFrom(const OglFramebuffer<TSrc>* src, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, ogl_framebuffer_copy_mask mask, ogl_framebuffer_copy_filter filter);

        template<typename TDst> requires ogl_texture<typename TDst::ogl_resource_type>
        void CopyTo  (OglFramebuffer<TDst>* dst, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, ogl_framebuffer_copy_mask mask, ogl_framebuffer_copy_filter filter) const;

        // Internal use only !!!
        void CopyFrom(GLint id, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, ogl_framebuffer_copy_mask mask, ogl_framebuffer_copy_filter filter);

        // Internal use only !!!
        void CopyTo  (GLint id, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, ogl_framebuffer_copy_mask mask, ogl_framebuffer_copy_filter filter) const;


    private:
        OglResource<ogl_resource_type> _ogl_obj;
        OglFramebuffer(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}

    };

    /// Fence
    //////////////////////////////////////
	class OglFence
	{
		// RenderContext is the one in charge of calling the private constructor
		friend class tao_render_context::RenderContext; 

	public:
		OglFence(const OglFence&) = delete;

		OglFence& operator=(const OglFence&) = delete;

		OglFence(OglFence&& other) noexcept
		{
			_id = other._id;
			other._id = 0;
		}

		OglFence& operator=(OglFence&& other) noexcept
		{
			if (this != &other)
			{
				Destroy();

				_id = other._id;
				other._id = nullptr;
			}
			return *this;
		}

		ogl_wait_sync_result ClientWaitSync(ogl_wait_sync_flags flags, unsigned long long timeout);

		~OglFence() { Destroy(); }

	private:

		GLsync _id;

		void Create(ogl_sync_condition condition);
		void Destroy();
		
		explicit OglFence(ogl_sync_condition condition) { Create(condition); }
	};

    /// Query
    //////////////////////////////////////
    class OglQuery
    {
        friend class tao_render_context::RenderContext;

    public:
        typedef query ogl_resource_type;

        void GetIntegerv(ogl_query_param pname, GLint * params);
        void GetInteger64v(ogl_query_param pname, GLint64 * params);
        void QueryCounter(ogl_query_counter_target target);
    private:
        OglResource<ogl_resource_type> _ogl_obj;
        OglQuery(OglResource<ogl_resource_type>&& shader) :_ogl_obj(std::move(shader)) {}
    };
}
