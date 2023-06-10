#version 430 core

#define GIZMOS_MESH
//! #include "./Include/UboDefs.glsl"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec4 v_color;
layout(location = 3) in vec2 v_texCoord;

layout(std430, binding = 0) buffer instanceBuff
{
    INST_DATA buff_inst_data[];
};

out VS_OUT
{
    vec3 v_position;
    vec3 v_normal;
    vec4 v_color;
    vec2 v_texCoord;
}
vs_out;

void main()
{
    vec4 worldPosition   = buff_inst_data[gl_InstanceID].i_transform * vec4(v_position, 1.0);
    mat3 normalMat       = mat3(buff_inst_data[gl_InstanceID].i_normal_transform);
    
    gl_Position         = f_projMat * f_viewMat * worldPosition;
    vs_out.v_position   = worldPosition.xyz;
    vs_out.v_normal     = normalMat * v_normal;
    vs_out.v_color      = v_color*buff_inst_data[gl_InstanceID].i_color;
    vs_out.v_texCoord   = v_texCoord;
}