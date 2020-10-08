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

        leb_DiamondParent diamond = leb_DecodeDiamondParent_Square(node);
        DecodeTriangleVertices(diamond.base, triangleVertices);
        bool shouldMergeBase = LevelOfDetail(triangleVertices).x < 1.0;
        DecodeTriangleVertices(diamond.top, triangleVertices);
        bool shouldMergeTop = LevelOfDetail(triangleVertices).x < 1.0;

        if (shouldMergeBase && shouldMergeTop) {
            leb_MergeNode_Square(cbtID, node, diamond);
        }
    }
}