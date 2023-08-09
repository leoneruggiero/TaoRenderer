//? #version 430 core

#define PI     3.14159265359
#define PI_2   1.57079632679
#define PI_INV 0.31830988618
#define saturate(a) min(max(0.0, (a)), 1.0)

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

// Slightly modified Fresnel Shlick's approx
// From: https://seblagarde.wordpress.com/2012/06/03/spherical-gaussien-approximation-for-blinn-phong-phong-and-fresnel/
// I'm sure saving one or two instructions will make all the difference in this project lol
vec3 SpecularF(vec3 F0, float VoH)
{
    return F0 + (1.0 - F0) * exp2((-5.55473 * VoH - 6.98316) * VoH);
}

vec3 SpecularBRDF(vec3 l, vec3 v, vec3 n, vec3 F0, float roughness)
{
    vec3 h = normalize(l+v);
    float NoL = dot(n, l);
    float NoV = dot(n, v);
    float NoH = dot(n, h);
    float VoH = dot(v, h);

    return
        SpecularD(NoH, roughness) * SpecularF(F0, VoH) * SpecularG_Direct(NoL, NoV, roughness) /
            4.0 * NoL * NoV;

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
        float NoL = saturate( dot( N, L ) );
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
        float NoL = saturate( L.z );
        float NoH = saturate( H.z );
        float VoH = saturate( dot( V, H ) );
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
