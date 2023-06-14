#version 430 core
#define GIZMOS_LINE_STRIP
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

layout(std430, binding = 2) buffer buff
{
    float b_screenDst[];
};


out VS_OUT
{
    vec4  v_color;
    vec2  v_texCoord;
}
vs_out;

void main()
{
    gl_Position         = f_projMat * f_viewMat * i_transform[gl_InstanceID] * vec4(v_position, 1.0);
    vs_out.v_color      = v_color * i_color[gl_InstanceID];
    vs_out.v_texCoord   = vec2(b_screenDst[o_vertCount * gl_InstanceID + gl_VertexID]/o_patternSize, 0.0);
}