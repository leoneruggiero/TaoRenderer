//? #version 430 core

void ClipLineOnNear(inout vec4 p0, inout vec4 p1, float near, out bool bothBehind)
{
    bothBehind = false;

    // a point on the near plane 
    // has clip coord .z = -near
    float clipPlane = -near;
    float p0zToNear = clipPlane - p0.z;
    float p1zToNear = clipPlane - p1.z;

    // if both points are  
    // behind the near plane skip the line.
    if(p0zToNear > 0.0 &&
       p1zToNear > 0.0  )
    {
        bothBehind = true;
        return;
    }

    // if both points are in front 
    // of the near plane, return.
    if(p0zToNear <= 0.0 &&
       p1zToNear <= 0.0  )
    {
        bothBehind = false;
        return;
    }

    vec3 m_clip = (p1.xyz - p0.xyz);
    m_clip.xy /= m_clip.z;

    vec4 p0_clip = p0;
    vec4 p1_clip = p1;

    // if one point is behind the near plane
    // we clip it to near
    if(p1zToNear > 0.0)
    {
        p1_clip.xy =  p0_clip.xy + m_clip.xy * p0zToNear ;
        p1_clip.z  =  clipPlane;
        p1_clip.w  = -clipPlane; // 1 for ortho
    }

    if(p0zToNear > 0.0)
    {
        p0_clip.xy =  p1_clip.xy + m_clip.xy * p1zToNear ;
        p0_clip.z  =  clipPlane;
        p0_clip.w  = -clipPlane; // 1 for ortho
    }

    p0 = p0_clip;
    p1 = p1_clip;
}