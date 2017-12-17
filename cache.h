#ifndef CACHE_CACHE_H_
#define CACHE_CACHE_H_

#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <map>
#include "storage.h"
#include "memory.h"

#define ADDR_MASK 0xFFFFFFFFFFFFFFFF

#define CACHE_INVALID	0x10
#define CACHE_WB	0x11

#define CACHE_READ	0x1
#define CACHE_WRITE	0x0

#define CACHE_RM_LRU	0x20
#define CACHE_RM_MRU	0x21
#define CACHE_RM_RR	0x22
#define CACHE_RM_SLRU	0x23
#define CACHE_RM_LFU	0x24
#define CACHE_RM_LFRU	0x25
#define CACHE_RM_LFUDA	0x26
#define CACHE_RM_ARC	0x27
#define CACHE_RM_FIFO	0x28
#define CACHE_RM_LIFO	0x29
#define CACHE_RM_GREEDY	0x2F


typedef struct CacheConfig_ {
	int size;
	int associativity;
	int set_num; // Number of cache sets
	int write_through; // 0|1 for back|through
	int write_allocate; // 0|1 for no-alc|alc

	int block_size;
	int block_bit;
	int set_bit;

	int bypass_shiftbit;
	double bypass_threshold;

	int pf_buf_num;
} CacheConfig;

typedef struct Line_ {
	int dirty;
	int valid;
	uint64_t tag;
	uint64_t weight;
	
	bool operator < (const struct Line_ &x) const
	{
		return weight < x.weight;
	}
	bool operator > (const struct Line_ &x) const
	{
		return weight > x.weight;
	}
	
	void Init(uint8_t init_type)
	{
		if (init_type == CACHE_INVALID) {
			valid = 0;
			dirty = 0;
			weight = 0;
			tag = 0;
		}
		else if (init_type == CACHE_WB) {
			dirty = 1;
		}
	}
} Line;

struct RR_queue {
	int head, items;
	uint64_t *keys;

	RR_queue()
	{
		keys = NULL;
		head = 0;
		items = 0;
	}

	bool empty()
	{
		return keys == NULL;
	}

	void Init(int len)
	{
		if (len <= 0) {
			puts("Invalid RR_queue length!");
			throw;
		}
		keys = new uint64_t[len];

		head = 0;
		items = len;
	}
	
	void push(uint64_t tag)
	{
		keys[head++] = tag;
		head %= items;
	}
	
	bool exist(uint64_t tag)
	{
		for (int i = 0; i < items; ++i)
			if (keys[i] == tag) return true;
		return false;
	}

};

typedef struct Set_{
	int ARC_lim;
	RR_queue B1_list;
	RR_queue B2_list;
	Line *line_;

	void B1_push(int x)
	{
		if (B1_list.empty()) B1_list.Init(8);
		B1_list.push(line_[x].tag);
	}
	void B2_push(int x)
	{
		if (B2_list.empty()) B2_list.Init(8);
		B2_list.push(line_[x].tag);
	}

	bool B1_exist(int x)
	{
		if (B1_list.empty()) return 0;
		return B1_list.exist(line_[x].tag);
	}
	bool B2_exist(int x)
	{
		if (B2_list.empty()) return 0;
		return B2_list.exist(line_[x].tag);
	}
} Set;

class Cache: public Storage {
private:
	// Bypassing
	int BypassDecision(uint64_t addr_tag);
	// Bypassing update
	void BypassUpdatestat(uint64_t addr_tag, int victim);
	// Partitioning
	void PartitionAlgorithm(uint64_t addr, uint64_t &addr_tag, int &addr_set);
	// Replacement
	int ReplaceDecision(uint64_t addr, int &victim, uint64_t &weight, int replace_method);
	// Replace Algorithms
	void ReplaceAlgorithm(uint64_t addr, int victim, uint64_t weight, int read, int replace_method);
	// Prefetching
	int PrefetchDecision(uint64_t addr, int &vicbuf);
	void PrefetchAlgorithm(uint64_t addr, int vicbuf);

	CacheConfig config_;
	Storage *lower_;
	Memory *memory_;
	Set *set_;

	// Bypass storage
	std::map<uint64_t, int> bypass_cnt, bypass_miss;

	// Prefetch buffer
	uint64_t **pf_buf, *pf_buf_info;
	
	DISALLOW_COPY_AND_ASSIGN(Cache);

public:
	void BypassClear()
	{
		bypass_cnt.clear();
		bypass_miss.clear();
	}

	Cache(CacheConfig config, Storage *lower, Memory *memory, StorageLatency latency)
	{
		SetConfig(config);
		SetLower(lower);
		memory_ = memory;
		latency_ = latency;
		
		set_ = new Set[config_.set_num];
		for (int i = 0; i < config_.set_num; i++) {
			set_[i].ARC_lim = config.associativity / 2;
			set_[i].line_ = new Line[config_.associativity];
			for (int j = 0; j < config_.associativity; j++) {
				set_[i].line_[j].Init(CACHE_INVALID);
			}
		}
		
		BypassClear();

		pf_buf = new uint64_t*[config_.pf_buf_num];
		pf_buf_info = new uint64_t[config_.pf_buf_num];
		for (int i = 0; i < config_.pf_buf_num; ++i) {
			pf_buf[i] = new uint64_t[4];
		}
	}
	
	~Cache() 
	{
		for (int i = 0; i < config_.set_num; i++)
			delete[] set_[i].line_;
		delete[] set_;
	}
	
	// Sets & Gets
	void SetConfig(CacheConfig config) { config_ = config; }
	void GetConfig(CacheConfig &config) { config = config_; }
	void SetLower(Storage *lower) { lower_ = lower; }

	void HandleRequest(uint64_t addr, int read, int replace_method);
};

#endif //CACHE_CACHE_H_ 
