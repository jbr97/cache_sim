#ifndef CACHE_MEMORY_H_
#define CACHE_MEMORY_H_

#include <stdint.h>
#include "storage.h"

class Memory: public Storage {
private:
	// Memory implement

	DISALLOW_COPY_AND_ASSIGN(Memory);

public:
	Memory()
	{ 
		latency_.bus_latency = 0;
		latency_.hit_latency = 100;
	}
	
	~Memory() {}

	// Main access process
	void HandleRequest(uint64_t addr, int read, int replace_method);
};

#endif //CACHE_MEMORY_H_ 
