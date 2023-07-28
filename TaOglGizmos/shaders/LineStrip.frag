#version 430 core
#define GIZMOS_LINE_STRIP    
//! #include "UboDefs.glsl"

in GS_OUT
{
    vec4 v_color;
    
    noperspective
    vec2 v_texCoord;
}fs_in;

uniform sampler2D s2D_patternTexture;

out vec4 FragColor;

void main()
{
    vec4 color = fs_in.v_color;

    #ifndef SELECTION
    if(o_hasTexture) 
    {
        vec2 tc = vec2(mod(fs_in.v_texCoord.x, 1.0), fs_in.v_texCoord.y);
        color *= texture(s2D_patternTexture, tc);
    }
    #endif
   
    FragColor = color;
}