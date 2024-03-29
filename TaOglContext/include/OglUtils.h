#pragma once

#include <exception>
#include <string>

#include "glad/glad.h"

#define GL_CALL(f)\
while (glGetError() != GL_NO_ERROR){}\
f;\
if(GLenum e=glGetError(); e != GL_NO_ERROR){throw GlException(e);}\


class GlException : public std::exception
{
public:
	GLenum gl_error_code;
	explicit GlException(GLenum error):gl_error_code(error)
	{
		_message = "OpenGL API error: " + ResolveGlErrorCode(error);
	}
	[[nodiscard]] const char* what() const noexcept override { return _message.c_str(); }

private:
	std::string _message;

    static constexpr std::string ResolveGlErrorCode(GLenum error)
    {
        std::string msg{};
        switch(error)
        {
            case(GL_INVALID_ENUM)       : msg = std::string{"INVALID ENUM"};        break;
            case(GL_INVALID_VALUE)      : msg = std::string{"INVALID VALUE"};       break;
            case(GL_INVALID_OPERATION)  : msg = std::string{"INVALID OPERATION"};   break;
            default                     : msg = std::string{};                          break;
        }

        return msg.append("(").append(std::to_string(error)).append(")");
    }
};

namespace  tao_render_context
{
	enum ogl_primitive_type
	{
		pmt_type_points						= GL_POINTS,
		pmt_type_line_strip					= GL_LINE_STRIP,
		pmt_type_line_loop					= GL_LINE_LOOP,
		pmt_type_lines						= GL_LINES,
		pmt_type_line_strip_adjacency		= GL_LINE_STRIP_ADJACENCY,
		pmt_type_line_adjacency				= GL_LINES_ADJACENCY,
		pmt_type_triangle_strip				= GL_TRIANGLE_STRIP,
		pmt_type_triangle_fan				= GL_TRIANGLE_FAN,
		pmt_type_triangles					= GL_TRIANGLES,
		pmt_type_triangle_strip_adjacency	= GL_TRIANGLE_STRIP_ADJACENCY,
		pmt_type_triangles_adjacency		= GL_TRIANGLES_ADJACENCY,
		pmt_type_patches					= GL_PATCHES
	};
}

namespace tao_ogl_resources
{
	enum ogl_indices_type
	{
		idx_typ_unsigned_byte	= GL_UNSIGNED_BYTE,
		idx_typ_unsigned_int	= GL_UNSIGNED_INT,
		idx_typ_unsigned_short  = GL_UNSIGNED_SHORT
	};

	enum ogl_buffer_usage
	{
		buf_usg_stream_draw = GL_STREAM_DRAW,
		buf_usg_stream_read = GL_STREAM_READ,
		buf_usg_stream_copy = GL_STREAM_COPY,
		buf_usg_static_draw = GL_STATIC_DRAW,
		buf_usg_static_read = GL_STATIC_READ,
		buf_usg_static_copy = GL_STATIC_COPY,
		buf_usg_dynamic_draw = GL_DYNAMIC_DRAW,
		buf_usg_dynamic_read = GL_DYNAMIC_READ,
		buf_usg_dynamic_copy = GL_DYNAMIC_COPY
	};

	enum ogl_vertex_attrib_type
	{
		vao_typ_byte = GL_BYTE,
		vao_typ_unsigned_byte = GL_UNSIGNED_BYTE,
		vao_typ_short = GL_SHORT,
		vao_typ_unsigned_short = GL_UNSIGNED_SHORT,
		vao_typ_int = GL_INT,
		vao_typ_unsigned_int = GL_UNSIGNED_INT,
		vao_typ_half_float = GL_HALF_FLOAT,
		vao_typ_float = GL_FLOAT,
		vao_typ_double = GL_DOUBLE,
		vao_typ_fixed = GL_FIXED,

		/// ... some other esoteric formats... ///
	};

	enum ogl_texture_unit
	{
		tex_unit_0  = GL_TEXTURE0,
		tex_unit_1  = GL_TEXTURE1,
		tex_unit_2  = GL_TEXTURE2,
		tex_unit_3  = GL_TEXTURE3,
		tex_unit_4  = GL_TEXTURE4,
		tex_unit_5  = GL_TEXTURE5,
		tex_unit_6  = GL_TEXTURE6,
		tex_unit_7  = GL_TEXTURE7,
		tex_unit_8  = GL_TEXTURE8,
		tex_unit_9  = GL_TEXTURE9,
		tex_unit_10 = GL_TEXTURE10,
		tex_unit_11 = GL_TEXTURE11,
		tex_unit_12 = GL_TEXTURE12,
		tex_unit_13 = GL_TEXTURE13,
		tex_unit_14 = GL_TEXTURE14,
		tex_unit_15 = GL_TEXTURE16,
	};

    enum ogl_image_access
    {
        image_access_read = GL_READ_ONLY,
        image_access_write = GL_WRITE_ONLY,
        image_access_rw    = GL_READ_WRITE,
    };

    enum ogl_image_format
    {
        image_format_rgba32f = GL_RGBA32F,
        image_format_rgba16f = GL_RGBA16F,
        image_format_rg16f   = GL_RG16F,
        image_format_r32f    = GL_R32F
    };

	enum ogl_texture_cube_target
	{
		tex_tar_cube_map_positive_x = GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		tex_tar_cube_map_negative_x = GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		tex_tar_cube_map_positive_y = GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		tex_tar_cube_map_negative_y = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
		tex_tar_cube_map_positive_z = GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
		tex_tar_cube_map_negative_z = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
	};

	enum ogl_texture_format
	{
		tex_for_red = GL_RED,
		tex_for_rg = GL_RG,
		tex_for_rgb = GL_RGB,
		tex_for_bgr = GL_BGR,
		tex_for_rgba = GL_RGBA,
		tex_for_brga = GL_BGRA,
		tex_for_red_integer = GL_RED_INTEGER,
		tex_for_rg_integer = GL_RG_INTEGER,
		tex_for_rgb_integer = GL_RGB_INTEGER,
		tex_for_brg_integer = GL_BGR_INTEGER,
		tex_for_rgba_integer = GL_RGBA_INTEGER,
		tex_for_brga_integer = GL_BGRA_INTEGER,
		tex_for_stencil_index = GL_STENCIL_INDEX,
		tex_for_depth = GL_DEPTH_COMPONENT,
		tex_for_depth_stencil = GL_DEPTH_STENCIL
	};

	enum ogl_read_pixels_format
	{
		read_pix_for_red = GL_RED,
		read_pix_for_green = GL_GREEN,
		read_pix_for_blue = GL_BLUE,
		read_pix_for_rgb = GL_RGB,
		read_pix_for_bgr = GL_BGR,
		read_pix_for_rgba = GL_RGBA,
		read_pix_for_brga = GL_BGRA,
		read_pix_for_depth = GL_DEPTH_COMPONENT,
		read_pix_for_depth_stencil = GL_DEPTH_STENCIL
	};

	enum ogl_texture_data_type
	{
		tex_typ_unsigned_byte = GL_UNSIGNED_BYTE,
		tex_typ_byte = GL_BYTE,
		tex_typ_usigned_short = GL_UNSIGNED_SHORT,
		tex_typ_short = GL_SHORT,
		tex_typ_unsigned_int = GL_UNSIGNED_INT,
		tex_typ_int = GL_INT,
		tex_typ_float = GL_FLOAT,
        tex_typ_half_float = GL_HALF_FLOAT,

		/// ... some other esoteric formats... ///
	};

	enum ogl_texture_internal_format
	{
		tex_int_for_depth = GL_DEPTH_COMPONENT,
        tex_int_for_depth_16 = GL_DEPTH_COMPONENT16,
        tex_int_for_depth_24 = GL_DEPTH_COMPONENT24,
        tex_int_for_depth_32f = GL_DEPTH_COMPONENT32F,
		tex_int_for_depth_stencil = GL_DEPTH_STENCIL,
		tex_int_for_red = GL_RED,
		tex_int_for_rg = GL_RG,
		tex_int_for_rgb = GL_RGB,
		tex_int_for_rgba = GL_RGBA,
		tex_int_for_r8 = GL_R8,
		tex_int_for_r8_snorm = GL_R8_SNORM,
		tex_int_for_r16 = GL_R16,
		tex_int_for_r16_snorm = GL_R16_SNORM,
		tex_int_for_rg8 = GL_RG8,
		tex_int_for_rg8_snorm = GL_RG8_SNORM,
		tex_int_for_rg16 = GL_RG16,
		tex_int_for_rg16_snorm = GL_RG16_SNORM,
		tex_int_for_r3_g3_b2 = GL_R3_G3_B2,
		tex_int_for_rgb4 = GL_RGB4,
		tex_int_for_rgb5 = GL_RGB5,
		tex_int_for_rgb8 = GL_RGB8,
		tex_int_for_rgb8_snorm = GL_RGB8_SNORM,
		tex_int_for_rgb10 = GL_RGB10,
		tex_int_for_rgb12 = GL_RGB12,
		tex_int_for_rgb16_snorm = GL_RGB16_SNORM,
		tex_int_for_rgba2 = GL_RGBA2,
		tex_int_for_rgba4 = GL_RGBA4,
		tex_int_for_rgb5_a1 = GL_RGB5_A1,
		tex_int_for_rgba8 = GL_RGBA8,
		tex_int_for_rgba8_snorm = GL_RGBA8_SNORM,
		tex_int_for_rgb10_a2 = GL_RGB10_A2,
		tex_int_for_rgb10_a2_ui = GL_RGB10_A2UI,
		tex_int_for_rgba12 = GL_RGBA12,
		tex_int_for_rgba16 = GL_RGBA16,
		tex_int_for_srgb8 = GL_SRGB8,
		tex_int_for_srgb8_alpha8 = GL_SRGB8_ALPHA8,
		tex_int_for_r16f = GL_R16F,
		tex_int_for_rg16f = GL_RG16F,
		tex_int_for_rgb16f = GL_RGB16F,
		tex_int_for_rgba16f = GL_RGBA16F,
		tex_int_for_r32f = GL_R32F,
		tex_int_for_rg32f = GL_RG32F,
		tex_int_for_rgb32f = GL_RGB32F,
		tex_int_for_rgba32f = GL_RGBA32F,
		tex_int_for_r11f_g11_b10f = GL_R11F_G11F_B10F,
		tex_int_for_rgb9_e5 = GL_RGB9_E5,
		tex_int_for_r8i = GL_R8I,
		tex_int_for_r8ui = GL_R8UI,
		tex_int_for_r16i = GL_R16I,
		tex_int_for_r16_ui = GL_R16UI,
		tex_int_for_r32i = GL_R32I,
		tex_int_for_r32ui = GL_R32UI,
		tex_int_for_rg8i = GL_RG8I,
		tex_int_for_rg8ui = GL_RG8UI,
		tex_int_for_rg16i = GL_RG16I,
		tex_int_for_rg16ui = GL_RG16UI,
		tex_int_for_rg32i = GL_RG32I,
		tex_int_for_rg32ui = GL_RG32UI,
		tex_int_for_rgb8i = GL_RGB8I,
		tex_int_for_rgb8ui = GL_RGB8UI,
		tex_int_for_rgb16i = GL_RGB16I,
		tex_int_for_rgb16ui = GL_RGB16UI,
		tex_int_for_rgb32i = GL_RGB32I,
		tex_int_for_rgb32ui = GL_RGB32UI,
		tex_int_for_rgba8i = GL_RGBA8I,
		tex_int_for_rgba8ui = GL_RGBA8UI,
		tex_int_for_rgba16i = GL_RGBA16I,
		tex_int_for_rgba16ui = GL_RGBA16UI,
		tex_int_for_rgba32i = GL_RGBA32I,
		tex_int_for_rgba32ui = GL_RGBA32UI,

		/// ... some other esoteric formats... ///
	};

	enum ogl_texture_min_filter
	{
		tex_min_filter_nearest = GL_NEAREST,
		tex_min_filter_linear = GL_LINEAR,
		tex_min_filter_nearest_mip_nearest = GL_NEAREST_MIPMAP_NEAREST,
		tex_min_filter_linear_mip_nearest = GL_LINEAR_MIPMAP_NEAREST,
		tex_min_filter_nearest_mip_linear = GL_NEAREST_MIPMAP_LINEAR,
		tex_min_filter_linear_mip_linear = GL_LINEAR_MIPMAP_LINEAR
	};

	enum ogl_texture_mag_filter
	{
		tex_mag_filter_nearest = GL_NEAREST,
		tex_mag_filter_linear = GL_LINEAR
	};

	enum ogl_texture_wrap
	{
		tex_wrap_clamp_to_edge = GL_CLAMP_TO_EDGE,
		tex_wrap_clamp_to_border = GL_CLAMP_TO_BORDER,
		tex_wrap_mirrored_repeat = GL_MIRRORED_REPEAT,
		tex_wrap_repeat = GL_REPEAT,
		tex_wrap_mirror_clamp_to_edge = GL_MIRROR_CLAMP_TO_EDGE
	};

	enum ogl_texture_compare_mode
	{
		tex_cmp_mode_compare_ref_to_texture = GL_COMPARE_REF_TO_TEXTURE,
		tex_cmp_mode_none = GL_NONE
	};

	enum ogl_texture_compare_func
	{
		tex_cmp_func_lequal = GL_LEQUAL,
		tex_cmp_func_gequal = GL_GEQUAL,
		tex_cmp_func_less = GL_LESS,
		tex_cmp_func_greater = GL_GREATER,
		tex_cmp_func_equal = GL_EQUAL,
		tex_cmp_func_notequal = GL_NOTEQUAL,
		tex_cmp_func_always = GL_ALWAYS,
		tex_cmp_func_never = GL_NEVER
	};

	enum ogl_texture_depth_stencil_tex_mode
	{
		tex_ds_tex_mode_depth_component = GL_DEPTH_COMPONENT,
		tex_ds_tex_mode_stencil_index = GL_STENCIL_INDEX
	};

	struct ogl_tex_compare_params
	{
		// todo: find the defaults
		ogl_texture_compare_mode compare_mode = tex_cmp_mode_none;
		ogl_texture_compare_func compare_func = tex_cmp_func_lequal;
	};

	struct ogl_tex_lod_params
	{
		float lod_bias = 0.0f;
		float min_lod = -1000.0f;
		float max_lod = 1000.0f;
		int base_level = 0;
		int max_level = 1000;
	};

	struct ogl_tex_filter_params
	{
		ogl_texture_min_filter min_filter = tex_min_filter_nearest_mip_linear;
		ogl_texture_mag_filter mag_filter = tex_mag_filter_linear;
	};

	struct ogl_tex_wrap_params
	{
		ogl_texture_wrap wrap_s = tex_wrap_repeat;
		ogl_texture_wrap wrap_t = tex_wrap_repeat;
		ogl_texture_wrap wrap_r = tex_wrap_repeat;
	};

	struct ogl_tex_params
	{
		ogl_tex_filter_params filter_params;
		ogl_tex_wrap_params wrap_params;
		ogl_tex_lod_params lod_params;
		ogl_tex_compare_params compare_params;
		ogl_texture_depth_stencil_tex_mode depth_stencil_tex_mode = tex_ds_tex_mode_depth_component;
	};

	enum ogl_sampler_min_filter
	{
		sampler_min_filter_nearest = GL_NEAREST,
		sampler_min_filter_linear = GL_LINEAR,
		sampler_min_filter_nearest_mip_nearest = GL_NEAREST_MIPMAP_NEAREST,
		sampler_min_filter_linear_mip_nearest = GL_LINEAR_MIPMAP_NEAREST,
		sampler_min_filter_nearest_mip_linear = GL_NEAREST_MIPMAP_LINEAR,
		sampler_min_filter_linear_mip_linear = GL_LINEAR_MIPMAP_LINEAR
	};

	enum ogl_sampler_mag_filter
	{
		sampler_mag_filter_nearest = GL_NEAREST,
		sampler_mag_filter_linear = GL_LINEAR
	};

	enum ogl_sampler_wrap
	{
		sampler_wrap_clamp_to_edge = GL_CLAMP_TO_EDGE,
		sampler_wrap_clamp_to_border = GL_CLAMP_TO_BORDER,
		sampler_wrap_mirrored_repeat = GL_MIRRORED_REPEAT,
		sampler_wrap_repeat = GL_REPEAT,
		sampler_wrap_mirror_clamp_to_edge = GL_MIRROR_CLAMP_TO_EDGE
	};

	enum ogl_sampler_compare_mode
	{
		sampler_cmp_mode_compare_ref_to_texture = GL_COMPARE_REF_TO_TEXTURE,
		sampler_cmp_mode_none = GL_NONE
	};

	enum ogl_sampler_compare_func
	{
		sampler_cmp_func_lequal = GL_LEQUAL,
		sampler_cmp_func_gequal = GL_GEQUAL,
		sampler_cmp_func_less = GL_LESS,
		sampler_cmp_func_greater = GL_GREATER,
		sampler_cmp_func_equal = GL_EQUAL,
		sampler_cmp_func_notequal = GL_NOTEQUAL,
		sampler_cmp_func_always = GL_ALWAYS,
		sampler_cmp_func_never = GL_NEVER
	};

	struct ogl_sampler_compare_params
	{
		// todo: find the defaults
		ogl_sampler_compare_mode compare_mode = sampler_cmp_mode_none;
		ogl_sampler_compare_func compare_func = sampler_cmp_func_lequal;
	};

	struct ogl_sampler_lod_params
	{
		float lod_bias = 0.0f;
		float min_lod = -1000.0f;
		float max_lod = 1000.0f;
	};

	struct ogl_sampler_filter_params
	{
		ogl_sampler_min_filter min_filter = sampler_min_filter_nearest_mip_linear;
		ogl_sampler_mag_filter mag_filter = sampler_mag_filter_linear;
	};

	struct ogl_sampler_wrap_params
	{
		ogl_sampler_wrap wrap_s = sampler_wrap_repeat;
		ogl_sampler_wrap wrap_t = sampler_wrap_repeat;
		ogl_sampler_wrap wrap_r = sampler_wrap_repeat;
	};

	struct ogl_sampler_params
	{
		ogl_sampler_filter_params filter_params;
		ogl_sampler_wrap_params wrap_params;
		ogl_sampler_lod_params lod_params;
		ogl_sampler_compare_params compare_params;
	};

	enum ogl_framebuffer_binding
	{
		fbo_read = GL_READ_FRAMEBUFFER,
		fbo_draw = GL_DRAW_FRAMEBUFFER,
		fbo_read_draw = GL_FRAMEBUFFER
	};

	enum ogl_framebuffer_attachment
	{
		fbo_attachment_depth = GL_DEPTH_ATTACHMENT,
		fbo_attachment_stencil = GL_STENCIL_ATTACHMENT,
		fbo_attachment_depth_stencil = GL_DEPTH_STENCIL_ATTACHMENT,
		fbo_attachment_color0 = GL_COLOR_ATTACHMENT0
	};

	enum ogl_framebuffer_read_draw_buffs
	{
        fbo_read_draw_buff_none   = GL_NONE,
		fbo_read_draw_buff_color0 = GL_COLOR_ATTACHMENT0
	};

	enum ogl_framebuffer_copy_mask
	{
		fbo_copy_mask_color_bit = GL_COLOR_BUFFER_BIT,
		fbo_copy_mask_depth_bit = GL_DEPTH_BUFFER_BIT,
		fbo_copy_mask_stencil_bit = GL_STENCIL_BUFFER_BIT
	};

	enum ogl_framebuffer_copy_filter
	{
		fbo_copy_filter_nearest = GL_NEAREST,
		fbo_copy_filter_linear = GL_LINEAR,
	};

	enum ogl_clear_mask
	{
		color_bit = GL_COLOR_BUFFER_BIT,
		depth_bit = GL_DEPTH_BUFFER_BIT,
		stencil_bit = GL_STENCIL_BUFFER_BIT,
	};

	enum ogl_depth_func
	{
		depth_func_equal = GL_EQUAL,
		depth_func_not_equal = GL_NOTEQUAL,
		depth_func_less = GL_LESS,
		depth_func_less_equal = GL_LEQUAL,
		depth_func_greater = GL_GREATER,
		depth_func_greate_equal = GL_GEQUAL,
		depth_func_always = GL_ALWAYS,
		depth_func_never = GL_NEVER,
	};

	struct ogl_depth_state
	{
		bool depth_test_enable		= false;
		bool depth_write_enable		= true;
		ogl_depth_func depth_func	= depth_func_always;
		double depth_range_near		= 0.0;
		double depth_range_far		= 1.0;
	};

	enum ogl_blend_func
	{
		blend_func_add = GL_FUNC_ADD,
		blend_func_subtract = GL_FUNC_SUBTRACT,
		blend_func_reverese_subtract = GL_FUNC_REVERSE_SUBTRACT,
		blend_func_min = GL_MIN,
		blend_func_max = GL_MAX
	};

	enum ogl_blend_factor
	{
		blend_fac_zero					= GL_ZERO,
		blend_fac_one					= GL_ONE,
		blend_fac_src_col				= GL_SRC_COLOR,
		blend_fac_one_minus_src_col		= GL_ONE_MINUS_SRC_COLOR,
		blend_fac_dst_col				= GL_DST_COLOR,
		blend_fac_one_minus_dst_col		= GL_ONE_MINUS_DST_COLOR,
		blend_fac_src_alpha				= GL_SRC_ALPHA,
		blend_fac_one_minus_src_alpha	= GL_ONE_MINUS_SRC_ALPHA,
		blend_fac_dst_alpha				= GL_DST_ALPHA,
		blend_fac_one_minus_dst_alpha	= GL_ONE_MINUS_DST_ALPHA,
		blend_fac_const_col				= GL_CONSTANT_COLOR,
		blend_fac_one_minus_const_col	= GL_ONE_MINUS_CONSTANT_COLOR,
		blend_fac_const_alpha			= GL_CONSTANT_ALPHA,
		blend_fac_one_minus_const_alpha = GL_ONE_MINUS_CONSTANT_ALPHA,
		blend_fac_alpha_sat				= GL_SRC_ALPHA_SATURATE,
		blend_fac_src1_col				= GL_SRC1_COLOR,
		blend_fac_one_minus_src1_col	= GL_ONE_MINUS_SRC1_COLOR,
		blend_fac_src1_alpha			= GL_SRC1_ALPHA,
		blend_fac_one_minus_src1_alpha	= GL_ONE_MINUS_SRC1_ALPHA
	};

	struct ogl_blend_equation
	{
		ogl_blend_func   blend_func			= blend_func_add;
		ogl_blend_factor blend_factor_src	= blend_fac_one;
		ogl_blend_factor blend_factor_dst	= blend_fac_one;
	};

	struct ogl_color_mask
	{
		bool mask_red = true;
		bool mask_green = true;
		bool mask_blue = true;
		bool mask_alpha = true;
	};

	constexpr ogl_color_mask mask_none
	{
		.mask_red = false,
		.mask_green = false,
		.mask_blue = false,
		.mask_alpha = false
	};
	constexpr ogl_color_mask mask_rgb
	{
		.mask_red = true,
		.mask_green = true,
		.mask_blue = true,
		.mask_alpha = false
	};
	constexpr ogl_color_mask mask_all
	{
		.mask_red = true,
		.mask_green = true,
		.mask_blue = true,
		.mask_alpha = true
	};

	struct ogl_blend_state
	{
		bool blend_enable = false;
		ogl_blend_equation blend_equation_rgb;
		ogl_blend_equation blend_equation_alpha;
		float blend_const_color_r = 0.0f;
		float blend_const_color_g = 0.0f;
		float blend_const_color_b = 0.0f;
		float blend_const_color_a = 0.0f;
		ogl_color_mask color_mask = mask_all;
	};

	constexpr ogl_blend_state no_blend
	{
		.blend_enable = false,
		.color_mask = mask_all
	};

	constexpr ogl_blend_state alpha_interpolate
	{
		.blend_enable = true,
		.blend_equation_rgb =
	{
			.blend_func		  = blend_func_add,
			.blend_factor_src = blend_fac_src_alpha,
			.blend_factor_dst = blend_fac_one_minus_src_alpha
		},
		.blend_equation_alpha
	{
			.blend_func		  = blend_func_add,
			.blend_factor_src = blend_fac_one,
			.blend_factor_dst = blend_fac_src_alpha
		},
		.color_mask = mask_all
	};

	enum ogl_front_face
	{
		front_face_ccw = GL_CCW,
		front_face_cw  = GL_CW
	};

	enum ogl_cull_mode
	{
		cull_mode_front			 = GL_FRONT,
		cull_mode_back			 = GL_BACK,
		cull_mode_front_and_back = GL_FRONT_AND_BACK
	};

	enum ogl_polygon_mode
	{
		polygon_mode_fill  = GL_FILL,
		polygon_mode_line  = GL_LINE,
		polygon_mode_point = GL_POINT
	};

	struct ogl_rasterizer_state
	{
		bool			 culling_enable		= false;
		ogl_front_face   front_face			= front_face_ccw;
		ogl_cull_mode    cull_mode			= cull_mode_back;
		ogl_polygon_mode polygon_mode       = polygon_mode_fill;

        float polygon_offset_factor         = 0.0f;
        float polygon_offset_units          = 0.0f;

		bool  multisample_enable			= false;
		bool  alpha_to_coverage_enable		= false;

	};

	enum ogl_buffer_flags
	{
		buffer_flags_dynamic_storage	= GL_DYNAMIC_STORAGE_BIT,
		buffer_flags_map_read			= GL_MAP_READ_BIT,
		buffer_flags_map_write			= GL_MAP_WRITE_BIT,
		buffer_flags_map_persistent		= GL_MAP_PERSISTENT_BIT,
		buffer_flags_map_coherent		= GL_MAP_COHERENT_BIT,
		buffer_flags_client_storage		= GL_CLIENT_STORAGE_BIT
	};

	enum ogl_map_flags
	{
		map_flags_read_only		= GL_READ_ONLY,
		map_flags_write_only	= GL_WRITE_ONLY,
		map_flags_read_write	= GL_READ_WRITE
	};

	enum ogl_sync_condition
	{
		sync_condition_gpu_commands_complete = GL_SYNC_GPU_COMMANDS_COMPLETE
	};

	enum ogl_wait_sync_flags
	{
		wait_sync_flags_none = 0,
		wait_sync_flags_flush_commands = GL_SYNC_FLUSH_COMMANDS_BIT
	};

	enum ogl_wait_sync_result
	{
		wait_sync_res_already_signaled = GL_ALREADY_SIGNALED,
		wait_sync_res_timeout_expired = GL_TIMEOUT_EXPIRED ,
		wait_sync_res_condition_satisfied = GL_CONDITION_SATISFIED ,
		wait_sync_res_failed = GL_WAIT_FAILED,
	};

    struct vertex_attribute_desc
    {
        int index;
        int element_count;
        int offset;
        int stride;
        ogl_vertex_attrib_type element_type;

    };

    struct vertex_buffer_layout_desc
    {
        int attrib_count;
        vertex_attribute_desc* attrib_descriptors;
    };

    enum ogl_debug_output_severity
    {
        dbg_out_severity_low        = GL_DEBUG_SEVERITY_LOW,
        dbg_out_severity_medium     = GL_DEBUG_SEVERITY_MEDIUM,
        dbg_out_severity_high       = GL_DEBUG_SEVERITY_HIGH,
        dbg_out_severity_dontcare   = GL_DONT_CARE
    };

    enum ogl_debug_output_type
    {
        dbg_out_type_error                  = GL_DEBUG_TYPE_ERROR,
        dbg_out_type_deprecated_behavior    = GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR,
        dbg_out_type_undefined_behavior     = GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR,
        dbg_out_type_portability            = GL_DEBUG_TYPE_PORTABILITY,
        dbg_out_type_performance            = GL_DEBUG_TYPE_PERFORMANCE,
        dbg_out_type_other                  = GL_DEBUG_TYPE_OTHER,
        dbg_out_type_dontcare               = GL_DONT_CARE
    };

    enum ogl_debug_output_source
    {
        dbg_out_src_api             = GL_DEBUG_SOURCE_API,
        dbg_out_src_window_system   = GL_DEBUG_SOURCE_WINDOW_SYSTEM,
        dbg_out_src_shader_compiler = GL_DEBUG_SOURCE_SHADER_COMPILER,
        dbg_out_src_third_party     = GL_DEBUG_SOURCE_THIRD_PARTY,
        dbg_out_src_application     = GL_DEBUG_SOURCE_APPLICATION,
        dbg_out_src_other           = GL_DEBUG_SOURCE_OTHER,
        dbg_out_src_dontcare        = GL_DONT_CARE,
    };

    struct ogl_debug_output_filter
    {
        ogl_debug_output_severity severity = dbg_out_severity_dontcare;
        ogl_debug_output_source   source   = dbg_out_src_dontcare;
        ogl_debug_output_type     type     = dbg_out_type_dontcare;
    };

    constexpr ogl_debug_output_filter ogl_debug_output_filter_errors_only
    {
        .severity =  dbg_out_severity_dontcare,
        .source = dbg_out_src_dontcare,
        .type = dbg_out_type_error,
    };

    constexpr ogl_debug_output_filter ogl_debug_output_filter_severe
    {
            .severity = static_cast<ogl_debug_output_severity>(dbg_out_severity_medium | dbg_out_severity_high),
            .source = dbg_out_src_dontcare,
            .type = dbg_out_type_error,
    };

    constexpr ogl_debug_output_filter ogl_debug_output_filter_everything
            {
                    .severity = dbg_out_severity_dontcare,
                    .source = dbg_out_src_dontcare,
                    .type = dbg_out_type_dontcare,
            };


    enum ogl_barrier_bit
    {
        vertex_atttrib_barrier_bit      = GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT,
        element_array_barrier_bit       =  GL_ELEMENT_ARRAY_BARRIER_BIT,
        uniform_barrier_bit             = GL_UNIFORM_BARRIER_BIT,
        texture_fetch_barrier_bit       = GL_TEXTURE_FETCH_BARRIER_BIT,
        shader_image_access_barrier_bit = GL_SHADER_IMAGE_ACCESS_BARRIER_BIT,
        command_barrier_bit             = GL_COMMAND_BARRIER_BIT,
        pixel_buffer_barrier_bit        = GL_PIXEL_BUFFER_BARRIER_BIT,
        texture_update_barrier_bit      = GL_TEXTURE_UPDATE_BARRIER_BIT,
        buffer_update_barrier_bit       = GL_BUFFER_UPDATE_BARRIER_BIT,
        client_mapped_buffer_barrier_bit= GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT,
        framebuffer_barrier_bit         = GL_FRAMEBUFFER_BARRIER_BIT,
        transform_feedback_barrier_bit  = GL_TRANSFORM_FEEDBACK_BARRIER_BIT,
        atomic_count_barrier_bit        = GL_ATOMIC_COUNTER_BARRIER_BIT,
        shader_storage_barrier_bit      = GL_SHADER_STORAGE_BARRIER_BIT,
        query_buffer_barrier_bit        = GL_QUERY_BUFFER_BARRIER_BIT,
        all_barrier_bit                 = GL_ALL_BARRIER_BITS
    };

    enum ogl_query_param
    {
        query_result = GL_QUERY_RESULT,
        query_result_available = GL_QUERY_RESULT_AVAILABLE,
        query_result_no_wait = GL_QUERY_RESULT_NO_WAIT,
        query_target = GL_QUERY_TARGET
    };

    enum ogl_query_counter_target
    {
        query_timestamp = GL_TIMESTAMP
    };
}
