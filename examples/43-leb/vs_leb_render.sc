$input a_position
$output v_texcoord0, v_wpos

#include "leb_common.sh"
#include "fcull.sh"

SAMPLER2D(u_DmapSampler, 0);
BUFFER_RO(u_CbtBuffer, uint, 1);

#include "leb.sh"

/*******************************************************************************
 * GenerateVertex -- Computes the final vertex position
 *
 */
struct VertexAttribute {
    vec4 position;
    vec2 texCoord;
};

void TessellateTriangle(
    in const vec2 texCoords[3],
    in vec2 tessCoord,
    out VertexAttribute attr
) {
    vec2 texCoord = BarycentricInterpolation(texCoords, tessCoord);
    vec4 position = vec4(texCoord.xy, 0, 1);

    position.z = u_DmapFactor * texture2DLod(u_DmapSampler, texCoord, 0.0).r;

    attr.position = position;
    attr.texCoord = texCoord;
}

void main()
{
    const int cbtID = 0;
    uint nodeID = gl_InstanceID;
    cbt_Node node = cbt_DecodeNode(cbtID, nodeID);
    vec4 triangleVertices[3]; 
    DecodeTriangleVertices(node, triangleVertices);
    vec2 triangleTexCoords[3];
    triangleTexCoords[0] = triangleVertices[0].xy;
    triangleTexCoords[1] = triangleVertices[1].xy;
    triangleTexCoords[2] = triangleVertices[2].xy;

    // compute final vertex attributes
    VertexAttribute attrib;
    
    TessellateTriangle(
        triangleTexCoords,
        a_position.xy,
        attrib
    );

    gl_Position = mul(u_modelViewProj, attrib.position);

    v_texcoord0  = attrib.texCoord;
    v_wpos  = vec3(float(u_CbtBuffer[nodeID]), triangleVertices[1].y, triangleVertices[1].z);
}