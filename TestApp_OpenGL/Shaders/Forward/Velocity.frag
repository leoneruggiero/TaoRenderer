#version 430 core
    
//! #include "../Include/UboDefs.glsl"

in VS_OUT
{
    vec3 curr_pos_clip;
    vec3 prev_pos_clip;
}fs_in;

out vec4 FragColor;

// Currently considering camera motion only.
void main()
{
    // perspective divide
    vec2 prevPos_ss = fs_in.prev_pos_clip.xy/max(1E-9,fs_in.prev_pos_clip.z);
    vec2 currPos_ss = fs_in.curr_pos_clip.xy/max(1E-9,fs_in.curr_pos_clip.z);

    // NDC to UV (0, 1)
    prevPos_ss = prevPos_ss.xy * 0.5 + 0.5;
    currPos_ss = currPos_ss.xy * 0.5 + 0.5;

    FragColor = vec4(prevPos_ss.xy -currPos_ss.xy, 0.0, 0.0);
}