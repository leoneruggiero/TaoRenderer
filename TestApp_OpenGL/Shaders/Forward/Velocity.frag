#version 430 core
    
//! #include "../Include/UboDefs.glsl"

uniform mat4 u_currVP;
uniform mat4 u_currModel;

uniform mat4 u_prevVP;
uniform mat4 u_prevModel;

in VS_OUT
{
    vec3 worldNormal;
    vec3 fragPosWorld;
    vec2 textureCoordinates;
}fs_in;

out vec4 FragColor;

// Currently considering camera motion only.
void main()
{
    vec4 posWorld4 = vec4(fs_in.fragPosWorld, 1.0);

    // World to camera to clip 
    vec4 prevPos_ss = u_prevVP * posWorld4;
    vec4 currPos_ss = u_currVP * posWorld4;

    // clip to NDC
    prevPos_ss.xy /= prevPos_ss.w;
    currPos_ss.xy /= currPos_ss.w;

    // NDC to UV (0, 1)
    prevPos_ss.xy = prevPos_ss.xy * 0.5 + 0.5;
    currPos_ss.xy = currPos_ss.xy * 0.5 + 0.5;

    FragColor = vec4(prevPos_ss.xy-currPos_ss.xy, 0.0, 0.0);
}