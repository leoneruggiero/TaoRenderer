#version 430 core
#define GIZMOS_POINTS
//! #include "./Include/UboDefs.glsl"

layout(location = 0) in vec3  v_position;
layout(location = 1) in vec4  v_color;
layout(location = 2) in vec4  v_texCoord; // rect, .xy = bl, .zw = tr 

layout(std430, binding = 0) buffer buff_instance_data_0
{
     vec4 i_color[];
};

layout(std430, binding = 1) buffer buff_instance_data_1
{
     mat4 i_transform[];
};

#ifdef SELECTION
layout(std430, binding = 2) buffer buff_instance_data_2
{
     vec4 i_selection_color[];
};
#endif

out VS_OUT
{
    vec4  v_color;
    vec4  v_texCoord;
}
vs_out;

void main()
{
    gl_Position = f_projMat * f_viewMat * i_transform[gl_InstanceID] * vec4(v_position, 1.0);
    
    vs_out.v_color = 
    #ifndef SELECTION
                      v_color * i_color[gl_InstanceID];
    #else
                      i_selection_color[gl_InstanceID];
    #endif

    vs_out.v_texCoord   = v_texCoord;
}