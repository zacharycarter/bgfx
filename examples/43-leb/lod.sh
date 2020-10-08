/*******************************************************************************
 * TriangleLevelOfDetail -- Computes the LoD assocaited to a triangle
 *
 * This function is used to garantee a user-specific pixel edge length in
 * screen space. The reference edge length is that of the longest edge of the
 * input triangle.In practice, we compute the LoD as:
 *      LoD = 2 * log2(EdgePixelLength / TargetPixelLength)
 * where the factor 2 is because the number of segments doubles every 2
 * subdivision level.
 */
float TriangleLevelOfDetail(in vec4 patchVertices[3])
{
    vec3 v0 = mul(u_modelView, patchVertices[0]).xyz;
    vec3 v2 = mul(u_modelView, patchVertices[2]).xyz;

#if 0 //  human-readable version
    vec3 edgeCenter = (v0 + v2); // division by 2 was moved to u_LodFactor
    vec3 edgeVector = (v2 - v0);
    float distanceToEdgeSqr = dot(edgeCenter, edgeCenter);
    float edgeLengthSqr = dot(edgeVector, edgeVector);

    return u_LodFactor + log2(edgeLengthSqr / distanceToEdgeSqr);
#else // optimized version
    float sqrMagSum = dot(v0, v0) + dot(v2, v2);
    float twoDotAC = 2.0f * dot(v0, v2);
    float distanceToEdgeSqr = sqrMagSum + twoDotAC;
    float edgeLengthSqr     = sqrMagSum - twoDotAC;

    return u_LodFactor + log2(edgeLengthSqr / distanceToEdgeSqr);
#endif
}

/*******************************************************************************
 * LevelOfDetail -- Computes the level of detail of associated to a triangle
 *
 * The first component is the actual LoD value. The second value is 0 if the
 * triangle is culled, and one otherwise.
 *
 */
vec2 LevelOfDetail(in vec4 patchVertices[3])
{
    // culling test
    vec4 bmin = min(min(patchVertices[0], patchVertices[1]), patchVertices[2]);
    vec4 bmax = max(max(patchVertices[0], patchVertices[1]), patchVertices[2]);
    if (!frustumCullingTest(u_modelViewProj, bmin.xyz, bmax.xyz))
        return vec2(0.0f, 0.0f);
        // return vec2(0.0f, 1.0f);

    // variance test
    // if (!DisplacementVarianceTest(patchVertices))
        // return vec2(0.0f, 1.0f);

    // compute triangle LOD
    return vec2(TriangleLevelOfDetail(patchVertices), 1.0f);
}