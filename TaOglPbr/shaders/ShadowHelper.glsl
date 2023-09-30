#define SQRT_TWO    1.41421356237
#define SQRT_TWO_2  0.70710678118
#define PI          3.14159265359
#define PI2         6.28318530718

const vec2 SMP_GRID_9[]=vec2[9]
(
    vec2(  0.00, 0.00 ),
    vec2(  -SQRT_TWO_2, -SQRT_TWO_2),
    vec2(  -1.00, 0.00),
    vec2(  -SQRT_TWO_2, SQRT_TWO_2),
    vec2(  0.00, 1.00 ),
    vec2(  SQRT_TWO_2, SQRT_TWO_2),
    vec2(  1.00, 0.00),
    vec2(  SQRT_TWO_2, -SQRT_TWO_2),
    vec2(  0.0, -1.00)
);

const vec2 SMP_VOGEL_16[]=vec2[16]
(
    vec2(0.176777, 0.000000),
    vec2(-0.225780, 0.206818),
    vec2(0.034587, -0.393769),
    vec2(0.284530, 0.371204),
    vec2(-0.522210, -0.092451),
    vec2(0.494753, -0.314594),
    vec2(-0.165602, 0.615488),
    vec2(-0.315404, -0.607676),
    vec2(0.684568, 0.250232),
    vec2(-0.712353, 0.293773),
    vec2(0.343624, -0.733602),
    vec2(0.253402, 0.809035),
    vec2(-0.764550, -0.443524),
    vec2(0.897228, -0.196803),
    vec2(-0.547909, 0.778489),
    vec2(-0.125948, -0.976159)
);

vec2 Rotate2D(vec2 p, float angle)
{
    float s = sin(angle);
    float c = cos(angle);

    return vec2(p.x*c-p.y*s, p.x*s+p.y*c);
}

// from: https://www.gamedev.net/tutorials/programming/graphics/contact-hardening-soft-shadows-made-fast-r4906/
float InterleavedGradientNoise(vec2 position_screen)
{
    vec3 magic = vec3(0.06711056f, 0.00583715f, 52.9829189f);
    return fract(magic.z * fract(dot(position_screen, magic.xy)));
}

float SlopeBiasForPCSS(vec3 worldNormal, vec3 worldLightDirection, float sampleOffsetWorld)
{
    float angle = acos(dot(worldNormal, worldLightDirection));
    float zOffsetWorld = sampleOffsetWorld * tan(clamp(angle, 0.0, PI/2.5));
    return zOffsetWorld;
}

float FindBlockerDistance_DirectionalLight(
    vec3 surfPos,vec3 surfNormal, sampler2D shadowMap,
    mat4 shadowTransform, vec4 shadowSize,
    vec3 lightDir, float searchWidth,
    float bias /* base bias added to a slope bias computed here  */)
{
    int blockers = 0;
    float avgBlockerDistance = 0;

    vec4 smpLS = shadowTransform *vec4(surfPos, 1.0);
    smpLS.xyz/=smpLS.w;
    smpLS.xyz=smpLS.xyz*0.5+0.5;
    float refVal = smpLS.z - bias;

    float zScale = shadowSize.w-shadowSize.z; // far-near
    vec2 searchWidthUV = searchWidth/shadowSize.xy;

    float coneBias = SlopeBiasForPCSS(surfNormal, lightDir, 1.0);

    const int samplesCnt=9;
    for(int i = 0 ; i< samplesCnt; i++)
    {

        float coneBiasSmp = coneBias*searchWidth*length(SMP_GRID_9[i])/zScale;
        vec2 offset = searchWidthUV*SMP_GRID_9[i];
        float closestDepth = texture(shadowMap, smpLS.xy+offset).r;

        float diff = (refVal - closestDepth);

        if (diff>coneBiasSmp+bias)
        {
            blockers++;
            avgBlockerDistance +=diff*zScale;
        }
    }

	float avgDist = blockers > 0
    ? avgBlockerDistance / blockers
    : -1.0;

    return avgDist;
}

float FindBlockerDistance_SphereLight(
    vec3 surfPos,vec3 surfNormal, samplerCube shadowMap,
    vec3 lightPos, float searchRadius,
    float bias /* base bias added to a slope bias computed here  */)
{

    int blockers = 0;
    float avgBlockerDistance = 0;

    vec3  dir = (surfPos-lightPos);
    float dist = length(dir);
    vec3  dirNrm = dir/dist;

    float searchW = searchRadius;

    float refVal = dist;

    vec3 right = normalize(cross(dirNrm, vec3(0.156, 1.066, 2.781)));
    vec3 up = cross(dirNrm, right);

    const int samplesCnt=9;
    for(int i = 0 ; i< samplesCnt; i++)
    {

        vec3 offset = SMP_GRID_9[i].x*right + SMP_GRID_9[i].y*up;
        vec3 smpDir= dirNrm + searchW*offset;

        float closestDepth = texture(shadowMap, smpDir).r;

        float diff = (refVal - closestDepth);

        if (diff>bias)
        {
            blockers++;
            avgBlockerDistance +=diff;
        }
    }

    float avgDist = blockers > 0
    ? avgBlockerDistance / blockers
    : -1.0;

    return avgDist;
}

float PCF_DirectionalLight(
    vec3 surfPos,vec3 surfNormal, sampler2DShadow shadowMap,
    mat4 shadowTransform, vec4 shadowSize,
    vec3 lightDir, float filterSize,
    float bias /* base bias added to a slope bias computed here  */)
{

    vec4 smpLS = shadowTransform *vec4(surfPos, 1.0);
    smpLS.xyz/=smpLS.w;
    smpLS.xyz=smpLS.xyz*0.5+0.5;
    float refVal = smpLS.z - bias;

    vec2 filterSizeUV = filterSize/shadowSize.xy;
    float sum = 0;

    float coneBias = SlopeBiasForPCSS(surfNormal, lightDir, 1.0);

    const int samplesCnt=16;
    for (int i = 0; i < samplesCnt; i++)
    {
        vec2 offsetDir = SMP_VOGEL_16[i];
        float coneBiasSmp = coneBias*filterSize*length(offsetDir);

        // random rotation (better noise than banding)
        float rnd = InterleavedGradientNoise(gl_FragCoord.xy-0.5);
        vec2 offset = filterSizeUV * Rotate2D(offsetDir, rnd*PI2);

        sum+= texture(shadowMap, vec3(smpLS.xy+offset, refVal-coneBiasSmp), 0.0).r;

    }

    return sum / samplesCnt;
}

float PCF_SphereLight(
    vec3 surfPos,vec3 surfNormal, samplerCube shadowMap,
    vec3 lightPos, float filterRadius,
    float bias /* base bias added to a slope bias computed here  */)
{
    float sum=0.0;

    vec3  dir = (surfPos-lightPos);
    float dist = length(dir);
    vec3  dirNrm = dir/dist;

    float searchW = filterRadius;

    float refVal = dist;

    vec3 right = normalize(cross(dirNrm, vec3(0.156, 1.066, 2.781)));
    vec3 up = cross(dirNrm, right);

    const int samplesCnt=16;
    for(int i = 0 ; i< samplesCnt; i++)
    {
        // random rotation (better noise than banding)
        float rnd = InterleavedGradientNoise(gl_FragCoord.xy-0.5);
        vec2 smpVogel = Rotate2D(SMP_VOGEL_16[i], rnd*PI2);
        vec3 offset = smpVogel.x*right + smpVogel.y*up;
        vec3 smpDir= dirNrm + searchW*offset;

        float closestDepth = texture(shadowMap, smpDir).r;

        float diff = (refVal - closestDepth);

        if (diff<bias)
        {
           sum++;
        }
    }

    return sum/samplesCnt;
}

float PCSS_DirectionalLight(
    vec3 surfPos,vec3 surfNormal,
    sampler2D shadowMap,sampler2DShadow shadowMapCompare,
    mat4 shadowTransform,
    vec4 shadowSize, vec3 lightPos,
    vec3 lightDir, float angle,
    float bias /* base bias added to a slope bias computed here  */)
{
    float tanAngle = tan(angle*0.5);

    // blocker search
    float searchW = dot(lightPos-surfPos, lightDir)*tanAngle;
    float blockerDistance = FindBlockerDistance_DirectionalLight(surfPos,surfNormal, shadowMap,
                                                                 shadowTransform, shadowSize,
                                                                 lightDir, searchW, bias);

    if (blockerDistance == -1) // no blocker found
    return 1.0;

    // filtering
    float pcfRadius = blockerDistance*tanAngle;

    return PCF_DirectionalLight(
        surfPos, surfNormal, shadowMapCompare,
        shadowTransform ,shadowSize,
        lightDir, pcfRadius,
        bias);
}

float PCSS_SphereLight(
    vec3 surfPos,vec3 surfNormal, samplerCube shadowMap,
    vec3 lightPos, float lightRadius,
    float bias /* base bias added to a slope bias computed here  */)
{

    // blocker search
    float searchW = lightRadius*0.1; // ???
    float blockerDistance = FindBlockerDistance_SphereLight(surfPos,surfNormal, shadowMap, lightPos, searchW, bias);


    if (blockerDistance == -1) // no blocker found
    return 1.0;

    // filtering
    float pcfRadius = blockerDistance * searchW/length(surfPos-lightPos);

    return PCF_SphereLight(surfPos,surfNormal, shadowMap, lightPos, pcfRadius, bias);
}