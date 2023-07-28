#version 430 core

//! #include "UboDefs.glsl"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_textureCoordinates;
layout(location = 3) in vec3 v_tangent;
layout(location = 4) in vec3 v_bitangent;

out VS_OUT
{
    vec3 worldNormal;
    vec3 fragPosWorld;
    vec2 textureCoordinates;
    mat3 TBN;
}vs_out;

void main()
{
    vec4 fragPosWorld = o_modelMat * vec4(v_position, 1.0);

    vec4 clip = f_projMat * f_viewMat * fragPosWorld;
    
    // Jitter sample for TAA.
    if(f_doTaa)
        clip.xy+=((f_taa_jitter.xy) / (0.5*f_viewportSize.xy)) * clip.w;

    gl_Position = clip;
    vs_out.fragPosWorld = fragPosWorld.xyz;
    vs_out.worldNormal = normalize((o_normalMat * vec4(v_normal, 0.0f)).xyz);
    vs_out.textureCoordinates = v_textureCoordinates;

    // TBN for normal mapping
    // ----------------------
    vec3 T = normalize(vec3(o_normalMat * vec4(v_tangent,0.0)));
    vec3 N = normalize(vec3(o_normalMat * vec4(v_normal, 0.0)));
    vec3 B = normalize(vec3(o_normalMat * vec4(v_bitangent, 0.0)));

    vs_out.TBN = mat3(T, B, N);        
       
}