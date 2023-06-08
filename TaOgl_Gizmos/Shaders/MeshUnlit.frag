#version 430 core

#define GIZOMS_MESH
//! #include "./Include/UboDefs.glsl"

in VS_OUT
{
    vec4 v_color;
    vec2 v_texCoord;
}
fs_in;

out vec4 FragColor;

void main()
{
    FragColor = fs_in.v_color;
}