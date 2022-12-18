#version 430 core

//! #include "../Include/UboDefs.glsl"
//! #include "../Include/DebugViz.glsl"

#define HISTORY_WEIGHT 0.9

//#define UNJITTER

uniform sampler2D t_MainColor;
uniform sampler2D t_HistoryColor;
uniform sampler2D t_VelocityBuffer;
uniform sampler2D t_DepthBuffer;

in VS_OUT
{
    vec3 worldNormal;
    vec3 fragPosWorld;
    vec2 textureCoordinates;
}fs_in;



// From: https://github.com/playdeadgames/temporal/blob/master/Assets/Shaders/TemporalReprojection.shader
vec4 clip_aabb(vec3 aabb_min, vec3 aabb_max, vec4 p, vec4 q)
	{
		// note: only clips towards aabb center (but fast!)
		vec3 p_clip = 0.5 * (aabb_max + aabb_min);
		vec3 e_clip = 0.5 * (aabb_max - aabb_min) + 0.00000001;

		vec4 v_clip = q - vec4(p_clip, p.w);
		vec3 v_unit = v_clip.xyz / e_clip;
		vec3 a_unit = abs(v_unit);
		float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

		
		if (ma_unit > 1.0)	
				return vec4(p_clip, p.w) + v_clip / ma_unit;
		else
			return q;// point inside aabb

	}

vec2 find_closest_fragment_3x3(vec2 uv)
{
	vec2 texelSize = 1.0/textureSize(t_DepthBuffer, 0);

	vec2 du = vec2(texelSize.x, 0.0);
	vec2 dv = vec2(0.0, texelSize.y);

	vec3 dtl = vec3(-1, -1, texture(t_DepthBuffer, uv - dv - du).x);
	vec3 dtc = vec3( 0, -1, texture(t_DepthBuffer, uv - dv).x);
	vec3 dtr = vec3( 1, -1, texture(t_DepthBuffer, uv - dv + du).x);
	vec3 dml = vec3(-1,  0, texture(t_DepthBuffer, uv - du).x);
	vec3 dmc = vec3( 0,  0, texture(t_DepthBuffer, uv).x);
	vec3 dmr = vec3( 1,  0, texture(t_DepthBuffer, uv + du).x);
	vec3 dbl = vec3(-1,  1, texture(t_DepthBuffer, uv + dv - du).x);
	vec3 dbc = vec3( 0,  1, texture(t_DepthBuffer, uv + dv).x);
	vec3 dbr = vec3( 1,  1, texture(t_DepthBuffer, uv + dv + du).x);

	vec3 dmin = dtl;

	if (dmin.z > dtc.z) dmin = dtc;
	if (dmin.z > dtr.z) dmin = dtr;
	if (dmin.z > dml.z) dmin = dml;
	if (dmin.z > dmc.z) dmin = dmc;
	if (dmin.z > dmr.z) dmin = dmr;
	if (dmin.z > dbl.z) dmin = dbl;
	if (dmin.z > dbc.z) dmin = dbc;
	if (dmin.z > dbr.z) dmin = dbr;

	return vec2(uv + texelSize.xy * dmin.xy);
}

vec4 temporal_reprojection(vec2 fragPos_uv, vec2 velocity)
	{
		
		vec2 texelSize = vec2(1.0)/textureSize(t_MainColor, 0); 
		vec4 texel0 = texture(t_MainColor, fragPos_uv 
		
		#ifdef UNJITTER
		- texelSize * (f_taa_jitter-0.5)
		#endif
		);
		
		// Get history color considering the velocity of the fragment:
		// uv offset from current frame to previous frame.
		vec4 texel1 = texture(t_HistoryColor, fragPos_uv + velocity);

		vec2 uv = fragPos_uv 
		 #ifdef UNJITTER
		 - texelSize * (f_taa_jitter*-0.5)
		 #endif
		;

		vec2 du = vec2(texelSize.x, 0.0);
		vec2 dv = vec2(0.0, texelSize.y);

		// Get 9 samples in a 3x3 region centered at fragPos_uv
		vec4 ctl = texture(t_MainColor, uv - dv - du);
		vec4 ctc = texture(t_MainColor, uv - dv);
		vec4 ctr = texture(t_MainColor, uv - dv + du);
		vec4 cml = texture(t_MainColor, uv - du);
		vec4 cmc = texture(t_MainColor, uv);
		vec4 cmr = texture(t_MainColor, uv + du);
		vec4 cbl = texture(t_MainColor, uv + dv - du);
		vec4 cbc = texture(t_MainColor, uv + dv);
		vec4 cbr = texture(t_MainColor, uv + dv + du);

		vec4 cmin = min(ctl, min(ctc, min(ctr, min(cml, min(cmc, min(cmr, min(cbl, min(cbc, cbr))))))));
		vec4 cmax = max(ctl, max(ctc, max(ctr, max(cml, max(cmc, max(cmr, max(cbl, max(cbc, cbr))))))));

		vec4 cavg = (ctl + ctc + ctr + cml + cmc + cmr + cbl + cbc + cbr) / 9.0;
		
		
		// Clamp to neighbourhood of current sample
		//
		// See the Playdead's presentation:
		// clamping instead of clipping should avoid clustering at the corners
		// of the color-space aabbox.
		texel1 = clip_aabb(cmin.xyz, cmax.xyz, clamp(cavg, cmin, cmax), texel1);
		
		// output
		return mix(texel0, texel1, HISTORY_WEIGHT);
	}


	layout (location = 0) out vec4 toScreen;
	

void main()
{
    
    vec2 fragCoord_uv = fs_in.textureCoordinates.xy;
	vec2 texelSize = 1.0/textureSize(t_MainColor, 0);
	
	// Don't sample the velocity buffer with the fragment uv,
	// instead find the closest (z) neighbor inside a 3x3 region
	// so that sub-pixel features on edges moves too.

	vec2 c_frag = find_closest_fragment_3x3(fragCoord_uv.xy
	//#ifdef UNJITTER
	- texelSize * (f_taa_jitter-0.5)
	//#endif
	);

	vec2 ss_vel = texture(t_VelocityBuffer, c_frag.xy).xy;

	vec4 resolvedCol = temporal_reprojection(fragCoord_uv, ss_vel);

	#ifdef DEBUG_VIZ
	
	 if(uDebug_viewport.z == DEBUG_TAA)
	 {
	 	ivec2 intFC = ivec2(gl_FragCoord.xy); // trunc
	 	int spacing = 20;
	 	bool dither = intFC%spacing == ivec2(0);
	 	bool crop = all(lessThan(c_frag.xy, vec2(0.54)) && greaterThan(c_frag.xy, vec2(0.46)));
	 	if(dither && crop)
	 	{
	 		DViz_DrawLineSS(c_frag.xy, c_frag.xy + ss_vel.xy, vec4(1.0, 0.0, 1.0, 0.7), f_viewMat, f_projMat);
	 	}
	 }
	#endif

    toScreen = resolvedCol;

	//vec4 texel0 = texture(t_MainColor, fragCoord_uv, 0);	
	//vec4 texel1 = texture(t_HistoryColor, fragCoord_uv, 0);
	//vec4 texel0 = texelFetch(t_MainColor, fragCoord_tex);	
	//vec4 texel1 = texelFetch(t_HistoryColor, fragCoord_tex, 0);
	//FragColor = mix (texel0, texel1, 0.9);

	//FragColor = mix(FragColor, vec4(abs(ss_vel)*100.0, 0.0, 1.0), 0.5);

	//FragColor = vec4(fragCoord_uv, 0.0, 1.0);

}