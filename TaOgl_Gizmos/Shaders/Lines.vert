#version 430 core
#define GIZMOS_LINES
//! #include "./Include/UboDefs.glsl"

layout(location = 0) in vec3  v_position;
layout(location = 1) in vec4  v_color;

out VS_OUT
{
    vec4  v_color;
}
vs_out;

void main()
{
    gl_Position = f_projMat * f_viewMat * vec4(v_position, 1.0);
    vs_out.v_color      = v_color;
}