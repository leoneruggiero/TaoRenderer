//? #version 430 core

#ifdef GBUFF_WRITE
layout (location = 0) out vec4 gBuff0;
layout (location = 1) out vec4 gBuff1;
layout (location = 2) out vec4 gBuff2;
layout (location = 3) out vec4 gBuff3;

void WriteGBuff(vec3 albedo, vec3 emission, vec3 position, vec3 normal, float roughness, float metalness, float occlusion)
{
    gBuff0 = vec4(position, roughness);
    gBuff1 = vec4(normal, metalness);
    gBuff2 = vec4(albedo, occlusion);
    gBuff3 = vec4(emission, 0.0f);
}
#endif

#ifdef GBUFF_READ

uniform sampler2D gBuff0;
uniform sampler2D gBuff1;
uniform sampler2D gBuff2;
uniform sampler2D gBuff3;

void ReadGBuff(ivec2 texCoord, out vec3 albedo, out vec3 emission, out vec3 position, out vec3 normal, out float roughness, out float metalness, out float occlusion)
{
    vec4 gData0 = texelFetch(gBuff0, texCoord, 0).rgba;
    vec4 gData1 = texelFetch(gBuff1, texCoord, 0).rgba;
    vec4 gData2 = texelFetch(gBuff2, texCoord, 0).rgba;
    vec4 gData3 = texelFetch(gBuff3, texCoord, 0).rgba;

    position    = gData0.rgb;
    roughness   = gData0.a;
    normal      = gData1.rgb;
    metalness   = gData1.a;
    albedo      = gData2.rgb;
    occlusion   = gData2.a;
    emission    = gData3.rgb;
}
#endif
