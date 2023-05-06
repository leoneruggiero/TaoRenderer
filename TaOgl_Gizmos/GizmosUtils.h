#pragma once
#include <glm\glm.hpp>

namespace tao_gizmos
{
	// see glsl source: UboDefs.glsl

	constexpr unsigned int FRAME_DATA_BINDING		 = 0;
	constexpr unsigned int POINTS_OBJ_DATA_BINDING	 = 1;
	constexpr unsigned int LINES_OBJ_DATA_BINDING	 = 2;

	constexpr const char* POINTS_OBJ_DATA_BLOCK_NAME = "blk_PerObjectData";
	constexpr const char* LINES_OBJ_BLOCK_NAME		 = "blk_PerObjectData";
	constexpr const char* FRAME_DATA_BLOCK_NAME		 = "blk_PerFrameData";

	constexpr const char* POINTS_TEX_ATLAS_NAME      = "s2D_colorTexture";

	#pragma pack(4)
	struct points_obj_data_block
	{
		unsigned int	size;
		int				snap;
		int				has_texture;
	};

	#pragma pack(4)
	struct lines_obj_data_block
	{
		glm::mat4	model_matrix;
		float		size;
	};

	#pragma pack(4)
	struct frame_data_block
	{
		glm::mat4 view_matrix;
		glm::mat4 projection_matrix;
		glm::vec2 viewport_size;
		glm::vec2 viewport_size_inv;
		glm::vec2 near_far;
	};

}