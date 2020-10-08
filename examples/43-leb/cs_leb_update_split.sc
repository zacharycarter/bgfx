#include "leb_common.sh"
#include "fcull.sh"
#include "lod.sh"

SAMPLER2D(u_DmapSampler, 0);
BUFFER_RW(u_CbtBuffer, uint, 1);

#include "leb.sh"

NUM_THREADS(256u, 1u, 1u)
void main()
{
    const int cbtID = 0;
    uint threadID = gl_GlobalInvocationID.x;

    if (threadID < cbt_NodeCount(cbtID))
    {
        cbt_Node node = cbt_DecodeNode(cbtID, threadID);

        vec4 triangleVertices[3];
        DecodeTriangleVertices(node, triangleVertices);

        vec2 targetLod = LevelOfDetail(triangleVertices);

        if (targetLod.x > 1.0)
        {
            leb_SplitNode_Square(cbtID, node);
        }
    }
}