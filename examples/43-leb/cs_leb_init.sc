#include "bgfx_compute.sh"

BUFFER_WR(indirectBuffer, uvec4, 0);

NUM_THREADS(1u, 1u, 1u)
void main()
{
    drawIndexedIndirect(indirectBuffer, 0u, 0u, 0u, 0u, 0u, 0u);
	dispatchIndirect(indirectBuffer, 1u, 2u, 1u, 1u);
}