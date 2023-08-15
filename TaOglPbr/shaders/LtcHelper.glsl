// from: https://github.com/selfshadow/ltc_code
// ------------------------------------------------------------------------------------------------------
vec3 LTC_IntegrateEdgeVec(vec3 v1, vec3 v2)
{
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206*y)*y;
    float b = 3.4175940 + (4.1616724 + y)*y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5*inversesqrt(max(1.0 - x*x, 1e-7)) - v;

    return cross(v1, v2)*theta_sintheta;
}

float LTC_IntegrateEdge(vec3 v1, vec3 v2)
{
    return LTC_IntegrateEdgeVec(v1, v2).z;
}

vec3 LTC_Evaluate(
    vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4], bool twoSided, sampler2D ltc_2)
{
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N*dot(V, N));
    T2 = cross(N, T1);

    // rotate area light in (T1, T2, N) basis
    Minv = Minv * transpose(mat3(T1, T2, N));

    // polygon (allocate 5 vertices for clipping)
    vec3 L[5];
    L[0] = Minv * ( points[0] - P);
    L[1] = Minv * ( points[1] - P);
    L[2] = Minv * ( points[2] - P);
    L[3] = Minv * ( points[3] - P);

    // integrate
    float sum = 0.0;

    vec3 dir = points[0].xyz - P;
    vec3 lightNormal = cross(points[1] - points[0], points[3] - points[0]);
    bool behind = (dot(dir, lightNormal) < 0.0);

    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);

    vec3 vsum = vec3(0.0);

    vsum += LTC_IntegrateEdgeVec(L[0], L[1]);
    vsum += LTC_IntegrateEdgeVec(L[1], L[2]);
    vsum += LTC_IntegrateEdgeVec(L[2], L[3]);
    vsum += LTC_IntegrateEdgeVec(L[3], L[0]);

    float len = length(vsum);
    float z = vsum.z/len;

    if (behind)
    z = -z;

    vec2 uv = vec2(z*0.5 + 0.5, len);
    uv = uv*LTC_LUT_SCALE + LTC_LUT_BIAS;

    float scale = texture(ltc_2, uv).w;

    sum = len*scale;

    if (behind && !twoSided)
    sum = 0.0;

    vec3 Lo_i = vec3(sum, sum, sum);

    return Lo_i;
}

void LTC_InitRectPoints(RectLight l, out vec3 points[4])
{
    vec3 ex = 0.5*l.size.x*l.axisX;
    vec3 ey = 0.5*l.size.y*l.axisY;

    points[3] = l.position - ex - ey;
    points[2] = l.position + ex - ey;
    points[1] = l.position + ex + ey;
    points[0] = l.position - ex + ey;
}

// ------------------------------------------------------------------------------------------------------
