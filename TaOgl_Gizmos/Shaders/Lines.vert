#version 430 core
#define GIZMOS_LINES
//! #include "./Include/UboDefs.glsl"

layout(location = 0) in vec3  v_position;
layout(location = 1) in vec4  v_color;

layout(std430, binding = 0) buffer buff_instance_data_0
{
     vec4 i_color[];
};

layout(std430, binding = 1) buffer buff_instance_data_1
{
     mat4 i_transform[];
};

layout(std430, binding = 2) buffer buff_instance_data_2
{
     uint i_visible[];
};

#ifdef SELECTION
layout(std430, binding = 3) buffer buff_instance_data_3
{
     uint i_selectable[];
};
layout(std430, binding = 4) buffer buff_instance_data_4
{
     vec4 i_selection_color[];
};
#endif

out VS_OUT
{
    vec4  v_color;
    bool  v_visible;
}
vs_out;

void main()
{
    gl_Position     = f_projMat * f_viewMat * i_transform[gl_InstanceID] * vec4(v_position, 1.0);

    vs_out.v_color  = 
    #ifndef SELECTION
                      v_color * i_color[gl_InstanceID];
    #else
                      i_selection_color[gl_InstanceID] * vec4(vec3(i_selectable[gl_InstanceID]), 1.0);
    #endif

    vs_out.v_visible = i_visible[gl_InstanceID]>0;
}