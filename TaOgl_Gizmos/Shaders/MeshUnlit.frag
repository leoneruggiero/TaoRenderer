#version 430 core
    
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
    vec4 texColor = texture(s2D_colorTexture, fs_in.v_texCoord);
    FragColor = vec4(texColor.rgb, 1.0); //fs_in.v_color;
}