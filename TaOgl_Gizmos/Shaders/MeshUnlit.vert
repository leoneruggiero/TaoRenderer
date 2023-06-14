#version 430 core

#define GIZMOS_MESH
//! #include "./Include/UboDefs.glsl"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec4 v_color;
layout(location = 3) in vec2 v_texCoord;

struct NRM_MAT_COLOR
{
    mat4 i_normal_transform;
    vec4 i_color;
};

layout(std430, binding = 0) buffer buff_instance_data_0
{
     NRM_MAT_COLOR i_nrm_mat_col[];
};

layout(std430, binding = 1) buffer buff_instance_data_1
{
     mat4 i_transform[];
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
    vec4 worldPosition   = i_transform[gl_InstanceID]* vec4(v_position, 1.0);
    mat3 normalMat       = mat3(i_nrm_mat_col[gl_InstanceID].i_normal_transform);
    
    gl_Position         = f_projMat * f_viewMat * worldPosition;
    vs_out.v_position   = worldPosition.xyz;
    vs_out.v_normal     = normalMat * v_normal;
    vs_out.v_color      = v_color*i_nrm_mat_col[gl_InstanceID].i_color;
    vs_out.v_texCoord   = v_texCoord;
}