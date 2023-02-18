#version 430 core

//! #include "../Include/UboDefs.glsl"

layout(location = 0) in vec3 position;

uniform mat4 u_currVP;
uniform mat4 u_currModel;

uniform mat4 u_prevVP;
uniform mat4 u_prevModel;

out VS_OUT
{
    vec3 curr_pos_ndc;
    vec3 prev_pos_ndc;
}vs_in;

void main()
{
    // currently considering camera motion only...
    // TODO !!!
    vec4 curr_pos_world = o_modelMat * vec4(position, 1.0);
    vec4 curr_clip = u_currVP * curr_pos_world;
    
    vec4 prev_pos_world = o_modelMat * vec4(position, 1.0);
    vec4 prev_clip = u_prevVP * prev_pos_world;

    vec4 curr_clip_jittered = curr_clip;
    // Jitter sample for TAA.
    if(f_doTaa)
        curr_clip_jittered.xy+=((f_taa_jitter.xy) / (0.5*f_viewportSize.xy)) * curr_clip.w;

    gl_Position = curr_clip_jittered;
    
    vs_in.curr_pos_ndc = curr_clip.xyz/curr_clip.w;
    vs_in.prev_pos_ndc = prev_clip.xyz/prev_clip.w;
}