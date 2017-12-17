#include <stdio.h>
#include <assert.h>
#include <algorithm>
#include "cache.h"
#include "memory.h"

#define EXE_CNT 100
#define BYPASS_SET 0x4

FILE *ftrace;
int trace_tot;
std::pair<uint64_t, char> trace_request[1000010];

int level;
CacheConfig config[10];
StorageLatency latency_cycles[10];

int method_cnt;
std::pair<uint64_t, int> acctot[110];
std::pair<double, int> MR[10][110];
std::pair<double, int> accAMAT[110];

StorageLatency get_latency(int size)
{
	switch(size)
	{
		case 32: return StorageLatency(0, 3);
		case 256: return StorageLatency(6, 4);
	}
	return StorageLatency(-1, -1);
}

int get_pf_buf_num(int size)
{
	switch(size)
	{
		case 32: return 64;
		case 256: return 1024;
	}
	return -1;
}

int ilog2(int x)
{
	int res = 0;
	while(x >>= 1)
		++res;
	return res;
}

const char * Retrieve_name(int replace_method)
{
	const char *res = NULL;
	switch (replace_method)
	{
		case 0x20: res = "LRU"; break;
		case 0x21: res = "MRU"; break;
		case 0x22: res = "RR"; break;
		case 0x23: res = "SLRU"; break;
		case 0x24: res = "LFU"; break;
		case 0x25: res = "LFRU"; break;
		case 0x26: res = "LFUDA"; break;
		case 0x27: res = "ARC"; break;
		case 0x28: res = "FIFO"; break;
		case 0x29: res = "LIFO"; break;
		case 0x2A: res = ""; break;
		case 0x2B: res = ""; break;
		case 0x2C: res = ""; break;
		case 0x2D: res = ""; break;
		case 0x2E: res = ""; break;
		case 0x2F: res = "GREEDY"; break;
		default: res = "NULL"; break;	
	}

	return res;
}

void Try_differ_RM(const char *tracefile, int replace_method)
{
	Cache *cache_lists[10];
	Memory *Main_memory;

	ftrace = fopen(tracefile, "r");
	Main_memory = new Memory;
	cache_lists[level] = new Cache(config[level], Main_memory, Main_memory, latency_cycles[level]);
	for (int i = level - 1; i >= 1; i--)
		cache_lists[i] = new Cache(config[i], cache_lists[i+1], Main_memory, latency_cycles[i]);
	
	trace_tot = 0;
	while (true) {
		char t;
		uint64_t addr;
		
		if (fscanf(ftrace, "%c%lx\n", &t, &addr) == EOF)
			break;
		trace_request[trace_tot++] = std::pair<uint64_t, char> (addr, t);
	}
	printf("trace_tot = %d\n", trace_tot);

	printf("Executing...\n");
	printf("\033[0;32;32m" "Using replace policy: %s" "\033[m" "\n", Retrieve_name(replace_method));
	// warm up
	for (int i = 1; i <= EXE_CNT; ++i) {
		for (int j = 0; j < trace_tot; ++j) {
			char t; 
			uint64_t addr;

			t = trace_request[j].second;
			addr = trace_request[j].first; 
			cache_lists[1] -> HandleRequest(addr, t == 'r', replace_method);
		}
	}
	
	// clear stats
	StorageStats zerostats;
	Main_memory -> SetStats(zerostats);
	for (int i = 1; i <= level; ++i) {
		cache_lists[i] -> SetStats(zerostats);
		cache_lists[i] -> BypassClear();
	}
	
	// re-execute
	for (int i = 1; i <= EXE_CNT / 10; ++i) {
	for (int j = 0; j < trace_tot; ++j) {
		char t; 
		uint64_t addr;

		t = trace_request[j].second;
		addr = trace_request[j].first; 
		cache_lists[1] -> HandleRequest(addr, t == 'r', replace_method);
		
	}
	}
	
	// print_info
	uint64_t tot = 0;
	for (int i = 1; i <= level; i++) {
		printf("Level %d Cache info:\n", i);
		tot += cache_lists[i] -> print_info();

		StorageStats nwstats;
		cache_lists[i] -> GetStats(nwstats);

		double nwMR = (double) nwstats.miss_num / nwstats.access_counter;
		MR[i][method_cnt].first = nwMR * 100.0;
		MR[i][method_cnt].second = replace_method;
	}
	printf("Memory info\n");
	tot += Main_memory -> print_info();
	printf("Total Cycles:\t%ld\n", tot);

	double AMAT = 100;
	for (int i = level; i >= 1; --i) {
		double nwMR = MR[i][method_cnt].first / 100.0;
		double miss_latency = latency_cycles[i].bus_latency;
		AMAT = latency_cycles[i].hit_latency + nwMR * (miss_latency + AMAT);
	}
	printf("AMAT:\t%.7f\n", AMAT);
	accAMAT[method_cnt].first = AMAT;
	accAMAT[method_cnt].second = replace_method;

	acctot[method_cnt].first = tot;
	acctot[method_cnt].second = replace_method;

	++method_cnt;
	fclose(ftrace);
	printf("\n");
}

int main(int argc, char* argv[]) 
{
	printf("Cache Simulator started.\n");
	
	printf("Set Cache level: ");
	scanf("%d", &level);
	assert(level >= 1 && level <= 3);
	
	printf("Set Cache info for %d levels:\n", level);
	for (int i = 1; i <= level; ++i) {
		int cache_size;
		printf("Size(KB) | Associativity | block_size | write_mode\n");
		
		scanf("%d%d%d%d", &cache_size, &config[i].associativity, &config[i].block_size, &config[i].write_through);
		latency_cycles[i] = get_latency(cache_size);
		// prefetch config
		config[i].pf_buf_num = get_pf_buf_num(cache_size);
		config[i].size = (1LL << 10) * cache_size;
		config[i].set_num = config[i].size / (config[i].associativity * config[i].block_size);
		config[i].write_allocate = 1 - config[i].write_through;
		config[i].block_bit = ilog2(config[i].block_size);
		config[i].set_bit = ilog2(config[i].set_num);
		
		// bypass config
		if ((BYPASS_SET >> i)&1) {
			config[i].bypass_shiftbit = 32;
			config[i].bypass_threshold = 0.8;
		}
		else {
			config[i].bypass_shiftbit = -1;
		}

	}
	
	// replace method config
	method_cnt = 0;
	for (int RM = 0x20; RM <= 0x29; ++RM)
		Try_differ_RM(argv[1], RM);

	for (int i = 1; i <= level; ++i) {
		sort(MR[i], MR[i] + method_cnt);
		printf("Cache Level %d Ranklist:\n", i);
		for(int j = 0; j < method_cnt; ++j) {
			printf("\t| Rank: %2d\t", j + 1);
			printf("| miss rate:\t%7.3f%%\t", MR[i][j].first);
			printf("| With replace method:\t%6s\n", Retrieve_name(MR[i][j].second));
		}
	}
	sort(acctot, acctot + method_cnt);
	printf("Access time Ranklist:\n");
	for(int i = 0; i < method_cnt; ++i) {
		printf("\t| Rank: %2d\t", i + 1);
		printf("| access cycles:\t%7ld\t", acctot[i].first);
		printf("| With replace method:\t%6s\n", Retrieve_name(acctot[i].second));
	}

	sort(accAMAT, accAMAT + method_cnt);
	printf("AMAT Ranklist:\n");
	for(int i = 0; i < method_cnt; ++i) {
		printf("\t| Rank: %2d\t", i + 1);
		printf("| AMAT:\t\t%7.3f\t", accAMAT[i].first);
		printf("| With replace method:\t%6s\n", Retrieve_name(accAMAT[i].second));
	}
	
	return 0;
}
