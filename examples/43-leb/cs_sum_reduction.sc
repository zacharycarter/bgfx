#include "bgfx_compute.sh"
#include "uniforms.sh"

BUFFER_RW(u_CbtBuffer, uint, 0);

#include "cbt.sh"

uniform int u_CbtID = 0;

NUM_THREADS(256u, 1u, 1u)
void main()
{
    const int cbtID = u_CbtID;
    uint cnt = (1u << u_pass_id);
    uint threadID = gl_GlobalInvocationID.x;

    if (threadID < cnt) {
        uint nodeID = threadID + cnt;
        uint x0 = cbt_HeapRead(cbtID, cbt_CreateNode(nodeID << 1u     , u_pass_id + 1));
        uint x1 = cbt_HeapRead(cbtID, cbt_CreateNode(nodeID << 1u | 1u, u_pass_id + 1));

        cbt__HeapWrite(cbtID, cbt_CreateNode(nodeID, u_pass_id), x0 + x1);
    }
}