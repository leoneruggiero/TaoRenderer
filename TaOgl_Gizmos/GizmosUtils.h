#pragma once
#include <glm\glm.hpp>

namespace tao_gizmos
{
	// see glsl source: UboDefs.glsl

	constexpr unsigned int FRAME_DATA_BINDING		   = 0;
	constexpr unsigned int POINTS_OBJ_DATA_BINDING	   = 1;
	constexpr unsigned int LINES_OBJ_DATA_BINDING	   = 2;
	constexpr unsigned int LINE_STRIP_OBJ_DATA_BINDING = 3;
	constexpr unsigned int MESH_OBJ_DATA_BINDING       = 4;

	constexpr const char* POINTS_OBJ_DATA_BLOCK_NAME  = "blk_PerObjectData";
	constexpr const char* LINES_OBJ_BLOCK_NAME		  = "blk_PerObjectData";
	constexpr const char* LINE_STRIP_OBJ_BLOCK_NAME   = "blk_PerObjectData";
	constexpr const char* MESH_OBJ_BLOCK_NAME		  = "blk_PerObjectData";
	constexpr const char* FRAME_DATA_BLOCK_NAME		  = "blk_PerFrameData";

	constexpr const char* POINTS_TEX_ATLAS_NAME       = "s2D_colorTexture";
	constexpr const char* LINE_PATTERN_TEX_NAME		  = "s2D_patternTexture";
	constexpr const char* LINE_STRIP_PATTERN_TEX_NAME = "s2D_patternTexture";

	constexpr unsigned int INSTANCE_DATA_STATIC_SSBO_BINDING	 = 0;
	constexpr unsigned int INSTANCE_DATA_DYNAMIC_SSBO_BINDING	 = 1;
	constexpr unsigned int LINE_STRIP_SSBO_BINDING_SCREEN_LENGTH = 2;
	
	// #pragma pack(4) TODO
	struct points_obj_data_block
	{
		unsigned int	size;
		int				snap;
		int				has_texture;
	};

	// #pragma pack(4) TODO
	struct lines_obj_data_block
	{
		unsigned int size;
		int			 has_texture;
		unsigned int pattern_size;
	};

	// #pragma pack(4) TODO
	struct line_strip_obj_data_block
	{
		unsigned int size;
		int			 has_texture;
		unsigned int pattern_size;
		unsigned int vert_count;
	};

	// #pragma pack(4) TODO
	struct mesh_obj_data_block
	{
		glm::vec2 color;
		int		  has_texture;
	};

	// #pragma pack(4) TODO
	struct frame_data_block
	{
		glm::mat4 view_matrix;
		glm::mat4 projection_matrix;
		glm::vec4 view_position;
		glm::vec4 view_direction;
		glm::vec2 viewport_size;
		glm::vec2 viewport_size_inv;
		glm::vec2 near_far;
	};

}