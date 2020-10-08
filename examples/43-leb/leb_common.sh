#include "bgfx_compute.sh"
#include "matrices.sh"
#include "fcull.sh"
#include "uniforms.sh"

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
float TriangleLevelOfDetail(in const vec4 patchVertices[3])
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
 * DisplacementVarianceTest -- Checks if the height variance criteria is met
 *
 * Terrains tend to have locally flat regions, which don't need large amounts
 * of polygons to be represented faithfully. This function checks the
 * local flatness of the terrain.
 *
 */
/*bool DisplacementVarianceTest(in const vec4 patchVertices[3])
{
#define P0 patchVertices[0].xy
#define P1 patchVertices[1].xy
#define P2 patchVertices[2].xy
    vec2 P = (P0 + P1 + P2) / 3.0;
    vec2 dx = (P0 - P1);
    vec2 dy = (P2 - P1);
    vec2 dmap = texture2DGrad(u_DmapSampler, P, dx, dy).rg;
    float dmapVariance = clamp(dmap.y - dmap.x * dmap.x, 0.0, 1.0);

    return (dmapVariance >= sqrt(0.1 / 64.0 / 1.0));
#undef P0
#undef P1
#undef P2
}*/

/*******************************************************************************
 * FrustumCullingTest -- Checks if the triangle lies inside the view frutsum
 *
 * This function depends on FrustumCulling.glsl
 *
 */
bool FrustumCullingTest(in const vec4 patchVertices[3])
{
    vec4 bmin = min(min(patchVertices[0], patchVertices[1]), patchVertices[2]);
    vec4 bmax = max(max(patchVertices[0], patchVertices[1]), patchVertices[2]);
    return frustumCullingTest(u_modelViewProj, bmin.xyz, bmax.xyz);
}

/*******************************************************************************
 * LevelOfDetail -- Computes the level of detail of associated to a triangle
 *
 * The first component is the actual LoD value. The second value is 0 if the
 * triangle is culled, and one otherwise.
 *
 */
vec2 LevelOfDetail(in const vec4 patchVertices[3])
{
    // culling test
    if (!FrustumCullingTest(patchVertices))
        return vec2(0.0f, 0.0f);
        // return vec2(0.0f, 1.0f);

    // variance test
    // if (!DisplacementVarianceTest(patchVertices))
        // return vec2(0.0f, 1.0f);

    // compute triangle LOD
    return vec2(TriangleLevelOfDetail(patchVertices), 1.0f);
}

/*******************************************************************************
 * BarycentricInterpolation -- Computes a barycentric interpolation
 *
 */
vec2 BarycentricInterpolation(in vec2 v[3], in vec2 u)
{
    // return v[0] + u.x * (v[1] - v[0]) + u.y * (v[2] - v[0]);
    return v[1] + u.x * (v[2] - v[1]) + u.y * (v[0] - v[1]);
}
vec4 BarycentricInterpolation(in vec4 v[3], in vec2 u)
{
    // return v[0] + u.x * (v[1] - v[0]) + u.y * (v[2] - v[0]);
    return v[1] + u.x * (v[2] - v[1]) + u.y * (v[0] - v[1]);
}