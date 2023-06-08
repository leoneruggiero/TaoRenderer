#version 430 core

#define GIZMOS_MESH
//! #include "./Include/UboDefs.glsl"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec4 v_color;
layout(location = 3) in vec2 v_texCoord;


struct INST_DATA
{
    mat4 i_transform;
    vec4 i_color;
};

layout(std430, binding = 0) buffer buff
{
    INST_DATA[] buff_instance_data;
};

out VS_OUT
{
    vec4 v_color;
    vec2 v_texCoord;
}
vs_out;

void main()
{
    gl_Position = f_projMat * f_viewMat * buff_instance_data[gl_InstanceID].i_transform * vec4(v_position, 1.0);

    vs_out.v_color      = v_color*buff_instance_data[gl_InstanceID].i_color;
    vs_out.v_texCoord   = v_texCoord;
}