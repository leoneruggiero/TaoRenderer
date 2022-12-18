#version 430 core

//! #include "../Include/UboDefs.glsl"

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 textureCoordinates;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec3 bitangent;



out VS_OUT
{
    vec3 worldNormal;
    vec3 fragPosWorld;
    vec2 textureCoordinates;
    mat3 TBN;
}vs_out;

void main()
{
    vec4 fragPosWorld = o_modelMat * vec4(position, 1.0);

    vec4 clip = f_projMat * f_viewMat * fragPosWorld;
    
    // Jitter sample for TAA.
    // The jitter value is in the range [0, 1]
    // Offset max 0.5 pixels.
    if(f_doTaa)
        clip.xy+=((f_taa_jitter.xy*2-1) / f_viewportSize.xy) * clip.w;

    gl_Position = clip;
    vs_out.fragPosWorld = fragPosWorld.xyz;
    vs_out.worldNormal = normalize((o_normalMat * vec4(normal, 0.0f)).xyz);
    vs_out.textureCoordinates = textureCoordinates;

    // TBN for normal mapping
    // ----------------------
    vec3 T = normalize(vec3(o_normalMat * vec4(tangent,   0.0)));
    vec3 N = normalize(vec3(o_normalMat * vec4(normal, 0.0)));
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

    vs_out.TBN = mat3(T, B, N);        
       
}