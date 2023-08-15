//? #version 430 core

#define PI     3.14159265359
#define PI_2   1.57079632679
#define PI_INV 0.31830988618

#define F0_DIELECTRIC vec3(0.04)

#define SATURATE(a) (clamp( (a) , 0.0, 1.0))
#define CLAMPED_DOT(a, b) (SATURATE(dot((a), (b))))

// from: https://learnopengl.com/PBR/IBL/Specular-IBL
// ---------------------------------------------------------------------------
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}
// ----------------------------------------------------------------------------

vec3 ClampHDRValue(vec3 val, float maxIntensity)
{
    float intensity = length(val);

    // some HDR images have discontinuities with
    // insanely high values (>1e3).
    // so by clamping the intensity we get smooth irradiance
    // maps even with this limited amount of samples
    return (val/intensity) * min(intensity, maxIntensity);
}

// from: https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// ------------------------------------------------------------------------------------------------------
// The following have been taken almost directly from the formulas
// in the article. Notice the different derivation for `k`
// in the SpecularG_IBL and SpecularG_Direct functions.
// -------------------------------------------------------------------------------------------------------
vec3 DiffuseBRDF(vec3 cdiff)
{
    return cdiff*PI_INV; // labert diffuse
}

// normal distribution
float SpecularD(float NoH, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float den = NoH*NoH*(a2-1.0) + 1.0;
    return a2/(PI*den*den);
}

// geometry for analytic lights
float SpecularG_Direct(float NoV, float roughness)
{
    roughness = (roughness+1.0)*0.5; // remap before squaring (SpecularG paragraph)
    float a = (roughness*roughness);
    float k = a*0.5;
    return (NoV)/(NoV*(1.0-k)+k);
}
float SpecularG_Direct(float NoL, float NoV, float roughness)
{
    return
        SpecularG_Direct(NoL, roughness) * //  normal - light shadowing
        SpecularG_Direct(NoV, roughness);  //  normal - view  shadowing
}

// geometry for IBL
// same as the other one except for the remapping
float SpecularG_IBL(float NoV, float roughness)
{
    float a = (roughness*roughness);
    float k = a*0.5;
    return (NoV)/(NoV*(1.0-k)+k);
}
float SpecularG_IBL(float NoL, float NoV, float roughness)
{
    return
    SpecularG_IBL(NoL, roughness) * // normal - light shadowing
    SpecularG_IBL(NoV, roughness);  // normal - view  shadowing
}

// Fresnel Shlick's approx
vec3 SpecularF(vec3 F0, float VoH)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - VoH, 0.0, 1.0), 5.0);
}

vec3 SpecularBRDF(float NoL, float NoV, float NoH, float VoH, vec3 F0, float roughness)
{
    return
        SpecularD(NoH, roughness) * SpecularF(F0, VoH) * SpecularG_Direct(NoL, NoV, roughness)
        /(4.0 * NoL * NoV + 0.00001);

}

vec3 SpecularBRDF(vec3 viewDirection, vec3 lightDirection, vec3 surfaceNormal, vec3 F0, float roughness)
{
    vec3  h   = normalize(viewDirection + lightDirection);
    float NoL = CLAMPED_DOT(surfaceNormal, lightDirection);
    float NoV = abs(dot(surfaceNormal, viewDirection))+1e-5;
    float NoH = CLAMPED_DOT(surfaceNormal, h);
    float VoH = CLAMPED_DOT(viewDirection, h);

    return SpecularBRDF(NoL, NoV, NoH, VoH, F0, roughness);
}

vec3 ImportanceSampleGGX( vec2 Xi, float Roughness, vec3 N )
{
    float a = Roughness * Roughness;
    float Phi = 2 * PI * Xi.x;
    float CosTheta = sqrt( (1 - Xi.y) / ( 1 + (a*a - 1) * Xi.y ) );
    float SinTheta = sqrt( 1 - CosTheta * CosTheta );
    vec3 H;
    H.x = SinTheta * cos( Phi );
    H.y = SinTheta * sin( Phi );
    H.z = CosTheta;
    vec3 UpVector = abs(N.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
    vec3 TangentX = normalize( cross( UpVector, N ) );
    vec3 TangentY = cross( N, TangentX );
    // Tangent to world space
    return TangentX * H.x + TangentY * H.y + N * H.z;
}

vec3 PrefilterEnvMap( samplerCube EnvMap, float Roughness, vec3 R )
{
    vec3 N = R;
    vec3 V = R;
    vec3 PrefilteredColor = vec3(0.0);
    float TotalWeight = 0.0;
    const uint NumSamples = 1024;
    for( uint i = 0; i < NumSamples; i++ )
    {
        vec2 Xi = Hammersley( i, NumSamples );
        vec3 H = ImportanceSampleGGX( Xi, Roughness, N );
        vec3 L = 2 * dot( V, H ) * H - V;
        float NoL = SATURATE( dot( N, L ) );
        if( NoL > 0 )
        {
            PrefilteredColor += ClampHDRValue(texture( EnvMap , L).rgb, 10.0) * NoL;
            TotalWeight += NoL;
        }
    }
    return PrefilteredColor / TotalWeight;
}

vec2 IntegrateBRDF( float Roughness, float NoV )
{
    vec3 V;
    vec3 N = vec3(0.0, 0.0, 1.0);
    V.x = sqrt( 1.0f - NoV * NoV ); // sin
    V.y = 0;
    V.z = NoV; // cos
    float A = 0;
    float B = 0;
    const uint NumSamples = 1024;
    for( uint i = 0; i < NumSamples; i++ )
    {
        vec2 Xi = Hammersley( i, NumSamples );
        vec3 H = ImportanceSampleGGX( Xi, Roughness, N );
        vec3 L = 2 * dot( V, H ) * H - V;
        float NoL = SATURATE( L.z );
        float NoH = SATURATE( H.z );
        float VoH = SATURATE( dot( V, H ) );
        if( NoL > 0 )
        {
            float G = SpecularG_IBL( NoV, NoL, Roughness );
            float G_Vis = G * VoH / (NoH * NoV);
            float Fc = pow( 1 - VoH, 5 );
            A += (1 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    return vec2( A, B ) / NumSamples;
}
// -------------------------------------------------------------------------------------------------------


// from: https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
// ------------------------------------------------------------------------------------------------------
float ComputeSphereLightIlluminance(vec3 surfacePosition, vec3 surfaceNormal, vec3 lightPosition, float radius)
{
    vec3 Lunormalized = lightPosition - surfacePosition;
    vec3 L = normalize(Lunormalized);
    float sqrDist = dot(Lunormalized, Lunormalized);

     // Analytical solution with horizon
     // Tilted patch to sphere equation
     float H = sqrt (sqrDist);
     float h = H / radius;
     float x = sqrt (h * h - 1.0);
     float Beta = acos ( dot ( surfaceNormal , L));
     float y = - x * (1.0 / tan (Beta ));


     float illuminance = 0.0;
     if (h * cos (Beta) > 1.0)
        illuminance = cos (Beta) / (h * h);
     else
     {
        illuminance = (1.0 / (PI * h * h)) *
        (cos (Beta ) * acos (y) - x * sin(Beta) * sqrt(1.0 - y * y)) +
        PI_INV * atan (sin (Beta) * sqrt(1.0 - y * y) / x);
     }

     illuminance *= PI;

    return illuminance;
}

float RectangleSolidAngle ( vec3 worldPos , vec3 p0 , vec3 p1 , vec3 p2 , vec3 p3 )
 {
    vec3 v0 = p0 - worldPos ;
    vec3 v1 = p1 - worldPos ;
    vec3 v2 = p2 - worldPos ;
    vec3 v3 = p3 - worldPos ;

    vec3 n0 = normalize ( cross (v0 , v1 ));
    vec3 n1 = normalize ( cross (v1 , v2 ));
    vec3 n2 = normalize ( cross (v2 , v3 ));
    vec3 n3 = normalize ( cross (v3 , v0 ));

    float g0 = acos ( dot (-n0 , n1 ));
    float g1 = acos ( dot (-n1 , n2 ));
    float g2 = acos ( dot (-n2 , n3 ));
    float g3 = acos ( dot (-n3 , n0 ));

    return g0 + g1 + g2 + g3 - 2.0 * PI ;
 }

struct ReprPointRectRes
{
    vec3 point;
    vec3 pointClamped;
    float overlap;
};


vec3 GetSpecularDominantDirArea( vec3 surfNormal, vec3 reflDir, float NdotV , float roughness )
{
    // Simple linear approximation
    float lerpFactor = (1 - roughness );
    return normalize ( mix(surfNormal, reflDir, lerpFactor ));
}

float RectOverlapFactor(vec2 lightSize, vec2 intersectPos,float dist, float roughness)
{
    // Some nonsense to evaluate the overlap between the reflection cone
    // and the area light...TODO
    float r = tan(0.6*PI_2*roughness + 0.005)*dist;

    vec2 overlap =
    min( lightSize, intersectPos+vec2(r))-
    max(-lightSize, intersectPos-vec2(r));

    float overlapFac = abs(overlap.x*overlap.y)/(r*r);

    if(any(lessThan(overlap, vec2(0.0))))
        overlapFac = 0.0;

    return overlapFac;
}

ReprPointRectRes RepresentativePointOnRect(vec3 viewDirection, vec3 surfPosition, vec3 surfNormal, float surfRoughness, vec3 lightPosition, vec3 ligthAxisX, vec3 ligthAxisY, vec3 ligthAxisZ, vec2 lightSize)
{
    vec3 reflDir     = reflect(-viewDirection, surfNormal);
    float NoV        = dot(surfNormal, viewDirection);
    vec3 dominantDir =  reflDir;//GetSpecularDominantDirArea(surfNormal, reflDir, NoV, surfRoughness);

    // ray-plane intersection
    vec3 intersection = surfPosition + dominantDir*(dot(lightPosition-surfPosition, -ligthAxisZ)/dot(dominantDir, -ligthAxisZ));

    vec2 intersection2D = vec2(
        dot(intersection-lightPosition, ligthAxisX),
        dot(intersection-lightPosition, ligthAxisY));

    // clamping the point to be inside the area
    vec2 intersection2DClamped = vec2(
      clamp(intersection2D.x, -0.5*lightSize.x,0.5*lightSize.x),
      clamp(intersection2D.y, -0.5*lightSize.y,0.5*lightSize.y));

    ReprPointRectRes res;
    res.point        =  lightPosition + intersection2D.x*ligthAxisX + intersection2D.y*ligthAxisY;
    res.pointClamped =  lightPosition + intersection2DClamped.x*ligthAxisX + intersection2DClamped.y*ligthAxisY;

    // TODO: as physically accurate as a unicorn...looks nice enough for now
    res.overlap      = RectOverlapFactor(lightSize, intersection2D, length(surfPosition-intersection), surfRoughness);

    return res;
}

vec3 RepresentativePointOnSphere(vec3 viewDirection, vec3 surfPosition, vec3 surfNormal, float surfRoughness, vec3 lightPosition, float lightRadius)
{
    vec3 reflDir     = reflect(-viewDirection, surfNormal);
    float NoV        = dot(surfNormal, viewDirection);
    vec3 dominantDir =  reflDir; //GetSpecularDominantDirArea(surfNormal, reflDir, NoV, surfRoughness);

    vec3 toCenter = lightPosition - surfPosition;
    vec3 proj = dot(toCenter, dominantDir)*dominantDir;
    vec3 centerToProj = proj-toCenter;
    vec3 mrPoint = surfPosition + toCenter + centerToProj * min(1.0, lightRadius/length(centerToProj));

    return mrPoint;
}

float SpecularD_Area(float NoH, float roughness, float a)
{
    float den = NoH*NoH*(a-1.0) + 1.0;
    return a/(PI*den*den);
}

vec3 SpecularBRDF_Area(float NoL, float NoV, float NoH, float VoH, vec3 F0, float roughness, float a)
{
    return
    SpecularD_Area(NoH, roughness, a) * SpecularF(F0, VoH) * SpecularG_Direct(NoL, NoV, roughness)
    /(4.0 * NoL * NoV + 0.00001);

}

vec3 SpecularBRDF_Area(vec3 viewDirection, vec3 lightDirection, vec3 surfaceNormal, vec3 F0, float roughness, float a)
{
    vec3  h   = normalize(viewDirection + lightDirection);
    float NoL = CLAMPED_DOT(surfaceNormal, lightDirection);
    float NoV = abs(dot(surfaceNormal, viewDirection))+1e-5;
    float NoH = CLAMPED_DOT(surfaceNormal, h);
    float VoH = CLAMPED_DOT(viewDirection, h);

    return SpecularBRDF_Area(NoL, NoV, NoH, VoH, F0, roughness, a);
}

float ComputeSphereLightIlluminanceW(vec3 surfacePosition, vec3 surfaceNormal, vec3 lightPosition, float radius)
{

    vec3 Lunormalized = lightPosition - surfacePosition;
    float dist = length(Lunormalized);
    vec3 L = Lunormalized / dist;

    float sqrDist = dot(Lunormalized, Lunormalized);

    float cosTheta = clamp(dot(surfaceNormal, L), -0.999, 0.999); // Clamp to avoid edge case


    float sqrLightRadius = radius*radius;
    float sinSigmaSqr = min(sqrLightRadius / sqrDist, 0.9999f);

    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    float illuminance = 0.0f;
    // Note: Following test is equivalent to the original formula.
    // There is 3 phase in the curve: cosTheta > sqrt(sinSigmaSqr),
    // cosTheta > -sqrt(sinSigmaSqr) and else it is 0
    // The two outer case can be merge into a cosTheta * cosTheta > sinSigmaSqr
    // and using saturate(cosTheta) instead.
    if (cosTheta * cosTheta > sinSigmaSqr)
    {
        illuminance = PI * sinSigmaSqr * SATURATE(cosTheta);
    }
    else
    {
        float x = sqrt(1.0f / sinSigmaSqr - 1.0f); // For a disk this simplify to x = d / r
        float y = -x * (cosTheta / sinTheta);
        float sinThetaSqrtY = sinTheta * sqrt(1.0f - y * y);
        illuminance = (cosTheta * acos(y) - x * sinThetaSqrtY) * sinSigmaSqr + atan(sinThetaSqrtY / x);
    }

    return max(illuminance, 0.0f);
}
// -----------------------------------------------------------------------------------------------------------

