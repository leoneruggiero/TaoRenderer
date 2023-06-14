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

out VS_OUT
{
    vec4  v_color;
}
vs_out;

void main()
{
    gl_Position     = f_projMat * f_viewMat * i_transform[gl_InstanceID] * vec4(v_position, 1.0);
    vs_out.v_color  = v_color*i_color[gl_InstanceID];
}