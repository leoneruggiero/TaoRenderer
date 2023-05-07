#version 430 core
#define GIZMOS_LINES    
//! #include "./Include/UboDefs.glsl"

in GS_OUT
{
    vec4 v_color;
}fs_in;

uniform sampler2D s2D_colorTexture;

out vec4 FragColor;

void main()
{
    vec4 color = fs_in.v_color;
    
    FragColor = color;
}