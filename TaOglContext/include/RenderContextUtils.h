#pragma once

#include "OglUtils.h"
namespace tao_render_context
{
	using namespace  tao_ogl_resources;

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
}