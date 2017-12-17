CACHE simulator
===
Author: Jiang Borui  
Date: 2017/11/25 

### How to compiler and run

```
$ cd /DIR/TO/THE/SIMULATOR/
$ make
$ ./simulator /DIR/TO/THE/TRACEFILE
```

Then the simulator will run to terminate and print the cache infomations like:  
```
Level ... Cache info:
access_counter: ...
miss_num: 		...
miss_rate: 		...%
access_cycle: 	...
replace_num:	...
fetch_num: 		...

Level ...

...

Main memory ...
...
```

### File composition

* main.cc
	* main simulator, which init the cache from cmdline.  
	* the cache config file, format:  
		$cache_level  
		$cache_size(KB) $cache_associativity $cache_block_size(byte) $cache_write_mode(0:write_back, 1:write_through)  	
	
* cache.cc
	* cache functions & cache class defination  
	
* cache.h
	* cache execute functions and replace algorithm, etc  
	
* def.h
	* def.h  
	
* memory.cc
	* main memory (differ to cache) execute functions, etc  
	
* memory.h
	* main memory functions & memory class defination  
	
* storage.h
	* the base class of memory & cache.  
	