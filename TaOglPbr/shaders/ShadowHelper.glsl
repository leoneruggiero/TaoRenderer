const vec2 SMP_GRID_16[]=vec2[16]
(
    vec2( -1.00, -1.00 ),
    vec2( -0.34, -1.00 ),
    vec2(  0.34, -1.00 ),
    vec2(  1.00, -1.00 ),

    vec2( -1.00, -0.34 ),
    vec2( -0.34, -0.34 ),
    vec2(  0.34, -0.34 ),
    vec2(  1.00, -0.34 ),

    vec2( -1.00, 0.34 ),
    vec2( -0.34, 0.34 ),
    vec2(  0.34, 0.34 ),
    vec2(  1.00, 0.34 ),

    vec2( -1.00, 1.00 ),
    vec2( -0.34, 1.00 ),
    vec2(  0.34, 1.00 ),
    vec2(  1.00, 1.00 )
);

const vec2 SMP_POISSON_16[]=vec2[16]
(
    vec2( -0.94201624, -0.39906216 ),
    vec2( 0.94558609, -0.76890725 ),
    vec2( -0.094184101, -0.92938870 ),
    vec2( 0.34495938, 0.29387760 ),
    vec2( -0.91588581, 0.45771432 ),
    vec2( -0.81544232, -0.87912464 ),
    vec2( -0.38277543, 0.27676845 ),
    vec2( 0.97484398, 0.75648379 ),
    vec2( 0.44323325, -0.97511554 ),
    vec2( 0.53742981, -0.47373420 ),
    vec2( -0.26496911, -0.41893023 ),
    vec2( 0.79197514, 0.19090188 ),
    vec2( -0.24188840, 0.99706507 ),
    vec2( -0.81409955, 0.91437590 ),
    vec2( 0.19984126, 0.78641367 ),
    vec2( 0.14383161, -0.14100790 )
);

const vec2 SMP_POISSON_64[]=vec2[64]
(
vec2(-0.613392, 0.617481),
vec2(0.170019, -0.040254),
vec2(-0.299417, 0.791925),
vec2(0.645680, 0.493210),
vec2(-0.651784, 0.717887),
vec2(0.421003, 0.027070),
vec2(-0.817194, -0.271096),
vec2(-0.705374, -0.668203),
vec2(0.977050, -0.108615),
vec2(0.063326, 0.142369),
 vec2(0.203528, 0.214331),
 vec2(-0.667531, 0.326090),
 vec2(-0.098422, -0.295755),
 vec2(-0.885922, 0.215369),
 vec2(0.566637, 0.605213),
 vec2(0.039766, -0.396100),
 vec2(0.751946, 0.453352),
 vec2(0.078707, -0.715323),
 vec2(-0.075838, -0.529344),
 vec2(0.724479, -0.580798),
 vec2(0.222999, -0.215125),
 vec2(-0.467574, -0.405438),
 vec2(-0.248268, -0.814753),
 vec2(0.354411, -0.887570),
 vec2(0.175817, 0.382366),
 vec2(0.487472, -0.063082),
 vec2(-0.084078, 0.898312),
 vec2(0.488876, -0.783441),
 vec2(0.470016, 0.217933),
 vec2(-0.696890, -0.549791),
 vec2(-0.149693, 0.605762),
 vec2(0.034211, 0.979980),
 vec2(0.503098, -0.308878),
 vec2(-0.016205, -0.872921),
 vec2(0.385784, -0.393902),
 vec2(-0.146886, -0.859249),
 vec2(0.643361, 0.164098),
 vec2(0.634388, -0.049471),
 vec2(-0.688894, 0.007843),
 vec2(0.464034, -0.188818),
 vec2(-0.440840, 0.137486),
 vec2(0.364483, 0.511704),
 vec2(0.034028, 0.325968),
 vec2(0.099094, -0.308023),
 vec2(0.693960, -0.366253),
 vec2(0.678884, -0.204688),
 vec2(0.001801, 0.780328),
 vec2(0.145177, -0.898984),
 vec2(0.062655, -0.611866),
 vec2(0.315226, -0.604297),
 vec2(-0.780145, 0.486251),
 vec2(-0.371868, 0.882138),
 vec2(0.200476, 0.494430),
 vec2(-0.494552, -0.711051),
 vec2(0.612476, 0.705252),
 vec2(-0.578845, -0.768792),
 vec2(-0.772454, -0.090976),
 vec2(0.504440, 0.372295),
 vec2(0.155736, 0.065157),
 vec2(0.391522, 0.849605),
 vec2(-0.620106, -0.328104),
 vec2(0.789239, -0.419965),
 vec2(-0.545396, 0.538133),
 vec2(-0.178564, -0.596057)
);


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

    const int samplesCnt=16;
    for(int i = 0 ; i< samplesCnt; i++)
    {

        float coneBiasSmp = coneBias*searchWidth*length(SMP_POISSON_16[i])/zScale;
        vec2 offset = searchWidthUV*SMP_POISSON_16[i];
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

    const int samplesCnt=16;
    for(int i = 0 ; i< samplesCnt; i++)
    {

        vec3 offset = SMP_POISSON_16[i].x*right + SMP_POISSON_16[i].y*up;
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

    const int samplesCnt=64;
    for (int i = 0; i < samplesCnt; i++)
    {
        float coneBiasSmp = coneBias*filterSize*length(SMP_POISSON_64[i]);
        vec2 offset  = filterSizeUV* SMP_POISSON_64[i];

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

    const int samplesCnt=64;
    for(int i = 0 ; i< samplesCnt; i++)
    {

        vec3 offset = SMP_POISSON_64[i].x*right + SMP_POISSON_64[i].y*up;
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