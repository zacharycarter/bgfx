#include "bgfx_compute.sh"
#include "matrices.sh"
#include "uniforms.sh"



/*******************************************************************************
 * DisplacementVarianceTest -- Checks if the height variance criteria is met
 *
 * Terrains tend to have locally flat regions, which don't need large amounts
 * of polygons to be represented faithfully. This function checks the
 * local flatness of the terrain.
 *
 */
/*bool DisplacementVarianceTest(in vec4 patchVertices[3])
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