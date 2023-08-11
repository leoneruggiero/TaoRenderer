#version 430 core

#define GBUFF_READ

//! #include "GPassHelper.glsl"
//! #include "UboDefs.glsl"
//! #include "PbrHelper.glsl"

out layout (location = 0) vec4 FragColor;

layout(location = 4) uniform sampler2D   envBrdfLut;
layout(location = 5) uniform samplerCube envIrradiance;
layout(location = 6) uniform samplerCube envPrefiltered;

void main()
{
    // Gbuff data
    vec3 posWorld;
    vec3 nrmWorld;
    vec3 albedo;
    vec3 emission;
    float roughness;
    float metalness;
    float occlusion;

    ivec2 fragCoord = ivec2(gl_FragCoord.xy-0.5);
    ReadGBuff(fragCoord, albedo, emission, posWorld, nrmWorld, roughness, metalness, occlusion);

    vec4 col = vec4(albedo, 1.0);

    vec3 LIGHT_DEBUG = 1.0 * vec3(1.0);

    if(posWorld!=vec3(0.0))
    {
        vec3 lightDir = normalize(LIGHT_DEBUG);
        vec3 viewDir = normalize(f_eyeWorldPos.xyz - posWorld);
        vec3 halfVec = normalize(viewDir + lightDir);
        vec3 F0 = mix(vec3(0.04), albedo.rgb, metalness);
        vec3 diffuse = albedo.rgb * LIGHT_DEBUG * saturate(dot(nrmWorld, lightDir));
        diffuse = mix(diffuse, vec3(0.0), metalness);
        vec3 specularD = LIGHT_DEBUG * SpecularBRDF(lightDir, viewDir, nrmWorld, F0, roughness);
        vec3 kd = 1.0-SpecularF(F0, saturate(dot(viewDir , nrmWorld)));

        col = vec4( kd * diffuse + specularD, 1.0);




        // TODO: that's not true....
        float envIntensity =  0.02; /**f_environmentIntensity*/
        vec3 ambientD = envIntensity* texture(envIrradiance, nrmWorld.xzy * vec3(1,-1,1)).rgb;
        ambientD = mix(ambientD, vec3(0.0), metalness);

        vec3 reflDir = reflect(-viewDir, nrmWorld);
        reflDir = reflDir.xzy * vec3(1,-1,1);

        float lod = f_radianceMinLod + roughness * (f_radianceMaxLod - f_radianceMinLod);
        vec3 reflection = textureLod(envPrefiltered, normalize(reflDir), lod).rgb;
        float NoV = saturate(dot(nrmWorld, viewDir));
        vec2 brdfLut    = texture(envBrdfLut, vec2(roughness, NoV)).xy;


        vec3 ambientS = envIntensity * reflection*(F0*brdfLut.x + brdfLut.y);
        col += vec4(ambientS + ambientD, 1.0);
        //col = vec4((F0*brdfLut.x + brdfLut.y), 1.0);
    }

    if(f_doGamma)
    {
        col=pow(col,vec4(1.0/f_gamma));
    }

    FragColor =  col;
}