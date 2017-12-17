#ifndef CACHE_STORAGE_H_
#define CACHE_STORAGE_H_

#include <stdint.h>
#include <stdio.h>

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
	TypeName(const TypeName&); \
	void operator=(const TypeName&)

// Storage access stats
typedef struct StorageStats_ {
	uint64_t access_counter;
	uint64_t miss_num;
	uint64_t access_cycle; // cycles
	uint64_t replace_num; // Evict old lines
	uint64_t fetch_num; // Fetch lower layer
	uint64_t prefetch_num; // Prefetch

	StorageStats_ ()
	{
		access_counter = 0;
		miss_num = 0;
		access_cycle = 0;
		replace_num = 0;
		fetch_num = 0;
		prefetch_num = 0;
	}
} StorageStats;

// Storage basic config
typedef struct StorageLatency_ {
	uint64_t hit_latency; // In cycles
	uint64_t bus_latency; // Added to each request

	StorageLatency_ ()
	{
		hit_latency = 0;
		bus_latency = 0;
	}
	StorageLatency_ (uint64_t bl, uint64_t hl) 
	{
		bus_latency = bl;
		hit_latency = hl;
	}
} StorageLatency;

class Storage {
protected:
	StorageStats stats_;
	StorageLatency latency_;

public:
	Storage() {}
	~Storage() {}

	// Sets & Gets
	void SetStats(StorageStats ss) { stats_ = ss; }
	void GetStats(StorageStats &ss) { ss = stats_; }
	void SetLatency(StorageLatency sl) { latency_ = sl; }
	void GetLatency(StorageLatency &sl) { sl = latency_; }
	
	uint64_t print_info()
	{
		double miss_rate = (double) stats_.miss_num / stats_.access_counter;
		miss_rate *= 100.0;

		printf("access_counter:\t%ld\n", stats_.access_counter);
		printf("miss_num:\t%ld\n", stats_.miss_num);
		printf("miss_rate:\t%3.16f%%\n", miss_rate);
		printf("access_cycle:\t%ld\n", stats_.access_cycle);
		printf("replace_num:\t%ld\n", stats_.replace_num);
		printf("fetch_num:\t%ld\n", stats_.fetch_num);
		
		return stats_.access_cycle;
	}

	virtual void HandleRequest(uint64_t addr, int read, int replace_method) = 0;
};

#endif //CACHE_STORAGE_H_ 
