#version 430 core
#define GIZMOS_POINTS
//! #include "./Include/UboDefs.glsl"

layout(location = 0) in vec3  v_position;
layout(location = 1) in vec4  v_color;
layout(location = 2) in vec4  v_texCoord; // rect, .xy = bl, .zw = tr 

out VS_OUT
{
    vec4  v_color;
    vec4  v_texCoord;
}
vs_out;

void main()
{
    gl_Position = f_projMat * f_viewMat * vec4(v_position, 1.0);
    vs_out.v_color      = v_color;
    vs_out.v_texCoord   = v_texCoord;
}