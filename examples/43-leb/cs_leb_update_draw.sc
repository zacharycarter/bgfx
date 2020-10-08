#include "bgfx_compute.sh"

BUFFER_RO(u_CbtBuffer, uint, 0);
BUFFER_WR(indirectBuffer, uvec4, 1);

#include "cbt.sh"

uniform int u_CbtID = 0;

NUM_THREADS(1u, 1u, 1u)
void main()
{
    const int cbtID = u_CbtID;
    uint nodeCount = cbt_NodeCount(cbtID);
	drawIndexedIndirect(indirectBuffer, 0u, 3 << (2 * 3), nodeCount, 0u, 0u, 0u);
}
