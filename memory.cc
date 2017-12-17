#include "memory.h"

void Memory::HandleRequest(uint64_t addr, int read, int replace_method) {
	++stats_.access_counter;
//	if (!prefetch)
	stats_.access_cycle += latency_.hit_latency + latency_.bus_latency;
}

