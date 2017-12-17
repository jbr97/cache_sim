#include "cache.h"
#include "def.h"

// Main access process
// [in]	addr: access address
// [in]	bytes: target number of bytes
// [in]	read: 0|1 for write|read; 3|4 for write|read in prefetch
// [i|o] content: in|out data
// [out] hit: 0|1 for miss|hit
// [out] cycle: total access cycle

void Cache::HandleRequest(uint64_t addr, int read, int replace_method) 
{
	uint64_t addr_tag;
	int addr_set;
	int victim;
	int vicbuf; 
	uint64_t weight;
	
	++stats_.access_counter;
	PartitionAlgorithm(addr, addr_tag, addr_set);
	
	// Bypass?
	if (!BypassDecision(addr_tag))  {
		// calc bus latency
		stats_.access_cycle += latency_.bus_latency;
		// Miss?
		if (ReplaceDecision(addr, victim, weight, replace_method)) { // HIT
			// hit latency
			stats_.access_cycle += latency_.hit_latency;
			// set weight
			set_[addr_set].line_[victim].weight = weight;
			// decide whether write back|through
			if (read == CACHE_WRITE && config_.write_through == 0)
				set_[addr_set].line_[victim].Init(CACHE_WB);
			else if (read == CACHE_WRITE && config_.write_through == 1)
				lower_ -> HandleRequest(addr, CACHE_WRITE, replace_method);
		}
		else { // MISS
			++stats_.miss_num;
			BypassUpdatestat(addr_tag, victim);
			// Prefetch?
			if (PrefetchDecision(addr, vicbuf)) { // need to prefetch
				if (vicbuf >= 0) { // if prefetch is legal
					stats_.prefetch_num++;
					PrefetchAlgorithm(addr, vicbuf);
				}
				// finish the replacement
				ReplaceAlgorithm(addr, victim, weight, read, replace_method);
			}
			else { // already prefetched
				ReplaceAlgorithm(addr, victim, weight, read|(read<<1), replace_method);
			}
		}
	}
	else { // BYPASS
		lower_ -> HandleRequest(addr, read, replace_method);
	}
}

int Cache::BypassDecision(uint64_t addr_tag) 
{
	int bypass_shiftbit = config_.bypass_shiftbit;
	double bypass_threshold = config_.bypass_threshold;

	if (bypass_shiftbit >= 0) {
		uint64_t bypass_tag = (addr_tag >> bypass_shiftbit);
		bypass_cnt[bypass_tag]++;
		if (bypass_cnt[bypass_tag] > 100) {
			double bypass_MR = (double) bypass_miss[bypass_tag] / bypass_cnt[bypass_tag];
			if (bypass_MR > bypass_threshold)
				return TRUE;
		}
	}
	return FALSE;
}

void Cache::BypassUpdatestat(uint64_t addr_tag, int victim)
{
	int bypass_shiftbit = config_.bypass_shiftbit;

	if (bypass_shiftbit >= 0) {
		uint64_t bypass_tag = (addr_tag >> bypass_shiftbit);
		++bypass_miss[bypass_tag];
	}
}

int Cache::PrefetchDecision(uint64_t addr, int &vicbuf) 
{
	vicbuf = -1;

	for (int i = 0; i < config_.pf_buf_num; ++i) {
		for(int j = 0; j < 4; ++j)
			if (pf_buf[i][j] == (addr >> config_.block_bit)) return FALSE;
		if (vicbuf == -1 || pf_buf_info[i] < pf_buf_info[vicbuf])
			vicbuf = i;
	}
	return TRUE;
}

void Cache::PrefetchAlgorithm(uint64_t addr, int vicbuf) 
{
	for (int i = 0; i < 4; ++i)
		pf_buf[vicbuf][i] = (addr >> config_.block_bit) + i + 1;
	pf_buf_info[vicbuf] = stats_.access_counter;
}

void Cache::PartitionAlgorithm(uint64_t addr, uint64_t &addr_tag, int &addr_set)
{
	int tag_bit = config_.block_bit + config_.set_bit;
	
	addr_tag = (addr & (ADDR_MASK << tag_bit)) >> tag_bit;
	addr_set = (addr & ~(ADDR_MASK << tag_bit)) >> config_.block_bit;
}

/* 
** cache replace decision function:
** Args: 
**	addr: address id
**	victim: which line(block) will be replaced
**	weight: the cache weight for replace policies
**	replace_method: a macro defination in the set $CACHE_RM$
** Return defination:
**	True: Hit, victim saves the hit line(block)
**	False: Miss, victimd saves the line need to be replaced
*/
int Cache::ReplaceDecision(uint64_t addr, int &victim, uint64_t &weight, int replace_method)
{
	uint64_t addr_tag;
	int addr_set;
	int cold_line = -1;
	victim = -1;
	
	PartitionAlgorithm(addr, addr_tag, addr_set);
	if (replace_method == CACHE_RM_LRU) {
		for (int i = 0; i < config_.associativity; ++i) {
			if (set_[addr_set].line_[i].valid) {
				if (set_[addr_set].line_[i].tag == addr_tag) {
					victim = i;
					weight = stats_.access_counter;
					return TRUE;
				}

				// LRU
				if (victim == -1 
				 || set_[addr_set].line_[i] < set_[addr_set].line_[victim])
					victim = i;
			}
			else if (cold_line == -1)
				cold_line = i;
		}
		if (cold_line != -1) 
			victim = cold_line;
		weight = stats_.access_counter;
		return FALSE;
	}
	// else if (replace_method == CACHE_RM_TLRU) { } // network cache applications
	else if (replace_method == CACHE_RM_MRU) {
		for (int i = 0; i < config_.associativity; ++i) {
			if (set_[addr_set].line_[i].valid) {
				if (set_[addr_set].line_[i].tag == addr_tag) {
					victim = i;
					weight = stats_.access_counter;
					return TRUE;
				}

				// MRU
				if (victim == -1 
				 || set_[addr_set].line_[i] > set_[addr_set].line_[victim])
					victim = i;
			}
			else if (cold_line == -1)
				cold_line = i;
		}
		if (cold_line != -1)
			victim = cold_line;
		weight = stats_.access_counter;
		return FALSE;
	}
	// else if (replace_method == CACHE_RM_PLRU) { } // higher MR but lower victim-choose latency
	else if (replace_method == CACHE_RM_RR) {
		weight = 0;
		for (int i = 0; i < config_.associativity; ++i) {
			if (set_[addr_set].line_[i].valid
			 && set_[addr_set].line_[i].tag == addr_tag) {
				victim = i;
				return TRUE;
			}
			if (cold_line == -1
			&& !set_[addr_set].line_[i].valid)
				cold_line = i;
		}
		// RR
		if (cold_line != -1)
			victim = cold_line;
		else
			victim = rand()%config_.associativity;
		return FALSE;

	}
	else if (replace_method == CACHE_RM_SLRU) {
		int SLRU_lim = config_.associativity / 2;
		
		for (int i = 0; i < config_.associativity; ++i) {
			if (set_[addr_set].line_[i].valid
			&& set_[addr_set].line_[i].tag == addr_tag) {
				victim = i;
				if (set_[addr_set].line_[i].weight & 1) // protected
					weight = (stats_.access_counter << 1) | 1;
				else { // probationary
					int protected_num = 0;
					int pro_victim = -1;

					for (int j = 0; j < config_.associativity; ++j)
					if (set_[addr_set].line_[j].valid
					&& (set_[addr_set].line_[j].weight & 1)) {
						++protected_num;
						if (pro_victim == -1
						|| set_[addr_set].line_[j] < set_[addr_set].line_[pro_victim])
							pro_victim = j;
					}
					if (protected_num >= SLRU_lim) 
						set_[addr_set].line_[pro_victim].weight ^= 1;
					// level up to protected
					weight = (stats_.access_counter << 1) | 1;
				}
				return TRUE;
			}

			if (cold_line == -1
			&& !set_[addr_set].line_[i].valid)
				cold_line = i;

			// choose victim from probationary
			if ((set_[addr_set].line_[i].weight & 1) == 0 
			&& (victim == -1 || set_[addr_set].line_[i] < set_[addr_set].line_[victim]))
				victim = i;
		}

		// probationary
		weight = stats_.access_counter << 1;
		if (cold_line != -1)
			victim = cold_line;
		return FALSE;
	}
	else if (replace_method == CACHE_RM_LFU) {
		for (int i = 0; i < config_.associativity; ++i) {
			if (set_[addr_set].line_[i].valid) {
				if (set_[addr_set].line_[i].tag == addr_tag) {
					victim = i;
					weight = set_[addr_set].line_[i].weight + 1;
					return TRUE;
				}
				// LFU
				if (victim == -1 
				 || set_[addr_set].line_[i] < set_[addr_set].line_[victim])
					victim = i;
			}
			else if (cold_line == -1)
				cold_line = i;
		}
		weight = 1;
		if (cold_line != -1)
			victim = cold_line;
		return FALSE;
	}
	else if (replace_method == CACHE_RM_LFRU) {
		int LFRU_lim = config_.associativity / 2;
		
		for (int i = 0; i < config_.associativity; ++i) {
			if (set_[addr_set].line_[i].valid
			&& set_[addr_set].line_[i].tag == addr_tag) {
				victim = i;
				if (set_[addr_set].line_[i].weight & 1) // protected
					weight = set_[addr_set].line_[i].weight + 2;
				else { // probationary
					int protected_num = 0;
					int pro_victim = -1;

					for (int j = 0; j < config_.associativity; ++j)
					if (set_[addr_set].line_[j].valid
					&& (set_[addr_set].line_[j].weight & 1)) {
						++protected_num;
						if (pro_victim == -1
						|| set_[addr_set].line_[j] < set_[addr_set].line_[pro_victim])
							pro_victim = j;
					}
					if (protected_num >= LFRU_lim) 
						set_[addr_set].line_[pro_victim].weight ^= 1;
					// level up to protected
					weight = (set_[addr_set].line_[i].weight + 2) | 1;
				}
				return TRUE;
			}

			if (cold_line == -1
			&& !set_[addr_set].line_[i].valid)
				cold_line = i;

			// choose victim from probationary
			if ((set_[addr_set].line_[i].weight & 1) == 0 
			&& (victim == -1 || set_[addr_set].line_[i] < set_[addr_set].line_[victim]))
				victim = i;
		}

		// probationary
		weight = 2;
		if (cold_line != -1)
			victim = cold_line;
		return FALSE;
	}
	else if (replace_method == CACHE_RM_LFUDA) {
		for (int i = 0; i < config_.associativity; ++i) {
			if (set_[addr_set].line_[i].valid) {
				if (set_[addr_set].line_[i].tag == addr_tag) {
					victim = i;
					weight = set_[addr_set].line_[i].weight + 1;
					return TRUE;
				}
				// LFU
				if (victim == -1 
				 || set_[addr_set].line_[i] < set_[addr_set].line_[victim]) {
					victim = i;
					weight = set_[addr_set].line_[i].weight + 1;
				}
			}
			else if (cold_line == -1)
				cold_line = i;
		}
		if (cold_line != -1) {
			victim = cold_line;
			weight = 1;
		}
		return FALSE;
	}
	// else if (replace_method == CACHE_RM_LIRS) { } // page replace policy
	else if (replace_method == CACHE_RM_ARC) {
		int &ARC_lim = set_[addr_set].ARC_lim;
		
		for (int i = 0; i < config_.associativity; ++i) {
			if (set_[addr_set].line_[i].valid
			&& set_[addr_set].line_[i].tag == addr_tag) {
				victim = i;

				// ghost hit: maintain ARC_lim ( 1~config_associativity-1 )
				if (set_[addr_set].B1_exist(victim) && ARC_lim > 1)
					--ARC_lim;
				if (set_[addr_set].B2_exist(victim) && ARC_lim < config_.associativity-1)
					++ARC_lim;

				if (set_[addr_set].line_[i].weight & 1) // protected
					weight = ((stats_.access_counter << 1) | 1) + (1LL << 32);
				else { // probationary
					int protected_num;
					do {
						protected_num = 0;
						int pro_victim = -1;

						for (int j = 0; j < config_.associativity; ++j)
						if (set_[addr_set].line_[j].valid
						&& (set_[addr_set].line_[j].weight & 1)) {
							++protected_num;
							if (pro_victim == -1
							|| (set_[addr_set].line_[j].weight>>32) < (set_[addr_set].line_[pro_victim].weight>>32))
								pro_victim = j;
						}
						if (protected_num >= ARC_lim) {
							--protected_num;
							set_[addr_set].B2_push(pro_victim);
							set_[addr_set].line_[pro_victim].weight ^= 1;
							set_[addr_set].line_[pro_victim].weight = (uint32_t) set_[addr_set].line_[pro_victim].weight;
						}
					} while (protected_num >= ARC_lim);
					
					// level up to protected
					weight = ((stats_.access_counter << 1) | 1) + (1LL<<32);
				}
				return TRUE;
			}

			if (cold_line == -1
			&& !set_[addr_set].line_[i].valid)
				cold_line = i;

			// choose victim from probationary
			if ((set_[addr_set].line_[i].weight & 1) == 0 
			&& (victim == -1 || set_[addr_set].line_[i] < set_[addr_set].line_[victim]))
				victim = i;
		}

		// probationary
		weight = stats_.access_counter << 1;
		if (cold_line != -1)
			victim = cold_line;
		else
			set_[addr_set].B1_push(victim);

		return FALSE;
	}
	// else if (replace_method == CACHE_RM_CAR) { } // page cache replace policy
	// else if (replace_method == CACHE_RM_MQ) { } // second-level-buffer cache replace policy
	// else if (replace_method == CACHE_RM_PANNIER) { } // for flash caching mechanism
	else if (replace_method == CACHE_RM_FIFO) {
		for (int i = 0; i < config_.associativity; ++i) {
			if (set_[addr_set].line_[i].valid) {
				if (set_[addr_set].line_[i].tag == addr_tag) {
					for (int j = i; j < config_.associativity-1; ++j) {
						if(!set_[addr_set].line_[j+1].valid) break;
						std::swap(set_[addr_set].line_[j], set_[addr_set].line_[j+1]);
					}
					victim = config_.associativity-1;
					return TRUE;
				}
			}
			else {
				victim = i;
				return FALSE;
			}
		}
		for (int i = 0; i < config_.associativity-1; ++i)
			std::swap(set_[addr_set].line_[i], set_[addr_set].line_[i+1]);
		victim = config_.associativity-1;
		return FALSE;
	}
	else if (replace_method == CACHE_RM_LIFO) {
		for (int i = 0; i < config_.associativity; ++i) {
			if (set_[addr_set].line_[i].valid) {
				if (set_[addr_set].line_[i].tag == addr_tag) {
					for (int j = i; j < config_.associativity-1; ++j) {
						if(!set_[addr_set].line_[j+1].valid) break;
						std::swap(set_[addr_set].line_[j], set_[addr_set].line_[j+1]);
					}
					victim = config_.associativity-1;
					return TRUE;
				}
			}
			else {
				victim = i;
				return FALSE;
			}
		}
		victim = config_.associativity-1;
		return FALSE;
	}

	printf("Error 1:\n");
	printf("Trace back when matching replace_method(%d)\n", replace_method);
	printf("No such Replace Method!\n");
	throw;

	return -1;
}

void Cache::ReplaceAlgorithm(uint64_t addr, int victim, uint64_t weight, int read, int replace_method) 
{
	uint64_t addr_tag;
	int addr_set;

	PartitionAlgorithm(addr, addr_tag, addr_set);
	if((read&1) == CACHE_READ) { // cache_read
		if(set_[addr_set].line_[victim].valid == 1) {
			++stats_.replace_num;
			if(set_[addr_set].line_[victim].dirty == 1) {
				int tag_bit = config_.block_bit + config_.set_bit;
				uint64_t victim_addr = (set_[addr_set].line_[victim].tag << tag_bit) | (addr_set << config_.block_bit);
				// write back dirty
				lower_ -> HandleRequest(victim_addr, CACHE_WRITE, replace_method);
			}
		}
		// set cache info
		set_[addr_set].line_[victim].valid = 1;
		set_[addr_set].line_[victim].dirty = 0;
		set_[addr_set].line_[victim].tag = addr_tag;
		set_[addr_set].line_[victim].weight = weight;
		
		// read cache
		if ((read>>1) != CACHE_READ) // not prefetch
			lower_ -> HandleRequest(addr, CACHE_READ, replace_method);
		++stats_.fetch_num;
	}
	else if ((read&1) == CACHE_WRITE) { // cache_write
		if(config_.write_allocate == 0)
			memory_ -> HandleRequest(addr, CACHE_WRITE, replace_method);
		else {
			if(set_[addr_set].line_[victim].valid == 1) {
				++stats_.replace_num;
				if(set_[addr_set].line_[victim].dirty == 1) {
					int tag_bit = config_.block_bit + config_.set_bit;
					uint64_t victim_addr = (set_[addr_set].line_[victim].tag << tag_bit) | (addr_set << config_.block_bit);
					// write back dirty
					lower_ -> HandleRequest(victim_addr, CACHE_WRITE, replace_method);
				}
			}
			// set cache info
			set_[addr_set].line_[victim].valid = 1;
			set_[addr_set].line_[victim].dirty = 1;
			set_[addr_set].line_[victim].tag = addr_tag;
			set_[addr_set].line_[victim].weight = weight;
			
			// write cache
			lower_ -> HandleRequest(addr, CACHE_WRITE, replace_method);
			++stats_.fetch_num;
		}
	}
}

