#version 430 core
#define GIZMOS_POINTS    
//! #include "./Include/UboDefs.glsl"

in GS_OUT
{
    vec4 v_color;
    vec2 v_texCoord;
}fs_in;

uniform sampler2D s2D_colorTexture;

out vec4 FragColor;

void main()
{
    vec4 color = fs_in.v_color;
    
    #ifndef SELECTION
    if(o_hasTexture) color *= texture(s2D_colorTexture, fs_in.v_texCoord);
    #endif
    
    FragColor = color;
}