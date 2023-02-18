#version 430 core
    
//! #include "../Include/UboDefs.glsl"

in VS_OUT
{
    vec3 curr_pos_ndc;
    vec3 prev_pos_ndc;
}fs_in;

out vec4 FragColor;

// Currently considering camera motion only.
void main()
{
    // NDC to UV (0, 1)
    vec3 prevPos_ss = fs_in.prev_pos_ndc.xyz * 0.5 + 0.5;
    vec3 currPos_ss = fs_in.curr_pos_ndc.xyz * 0.5 + 0.5;

    FragColor = vec4(prevPos_ss.xyz-currPos_ss.xyz, 0.0);
}