/* File: CacheSim.h
 * 
 * This program simulates a single-level blocking cache using a trace file.
 * The cache is assumed to be fixed size, allocate-on-write, and write-back.
 * 
 * Usage: ./CacheSim <trace file> [-v] [-t] [-d]
 *
 * <trace file> is the file location that contains a memory access trace.
 *
 * [-v] will include program version information in the output.
 * [-t] will include information about the trace accesses in the output (r/w, tag, offset, etc.)
 * [-d] will dump the final cache contents in the output (valid, dirty, tag, etc.)
 *
 * Trace file must be specified immediately after program executable.
 * Debug commands can be in any order. For example:
 *
 * ./CacheSim "C:\folder\trace.txt"
 * ./CacheSim "C:\folder\trace.txt" -t -v
 * ./CacheSim "C:\folder\trace.txt" -v -d -t
 *
 */
 
#ifndef CACHESIM_H
#define CACHESIM_H

/* Constants */

/* Max Line Length in Trace */
#define LINELENGTH 128

/* Address Size */
#define ADDRESS_SIZE 32

/* Cache Parameters */
#define NUMBER_OF_SETS 1024
#define ASSOCIATIVITY 4
#define BLOCK_SIZE 32
#define CACHE_SIZE (NUMBER_OF_SETS * ASSOCIATIVITY * BLOCK_SIZE)

/* Typedefs */
typedef struct Cache_* Cache;
typedef struct Block_* Block;
typedef struct Way_* Way;

/* createCache
 *
 * Function to create a new cache struct.  Returns the new struct on success
 * and NULL on failure.
 *
 * @param   cache_size      size of cache in bytes
 * @param   block_size      size of each block in bytes
 * @param 	associativity 	# of ways
 *
 * @return  success         new Cache
 * @return  failure         NULL
 */

Cache createCache(int cache_size, int write_policy, int associativity);

/* destroyCache
 *
 * Function that destroys a created cache. Frees all allocated memory. If
 * you pass in NULL, nothing happens. So make sure to set your cache = NULL
 * after you destroy it to prevent a double free.
 *
 * @param   cache           cache object to be destroyed
 *
 * @return  void
 */

void destroyCache(Cache cache);

/* readFromCache
 *
 * Function that reads data from a cache. Returns 0 on failure
 * or 1 on success.
 *
 * @param       cache       target cache struct
 * @param       address     hexidecimal address
 *
 * @return      success     1
 * @return      failure     0
 */

int readFromCache(Cache cache, char* address);

/* writeToCache
 *
 * Function that writes data to the cache. Returns 0 on failure or
 * 1 on success. Frees any old tags that already existed in the
 * target slot.
 *
 * @param       cache       target cache struct
 * @param       address     hexidecimal address
 *
 * @return      success     1
 * @return      error       0
 */

int writeToCache(Cache cache, char* address);

/* printCache
 *
 * Prints out the values of each slot in the cache
 * as well as the hit, miss, read, write, and size
 * data.
 *
 * @param       cache       Cache struct
 *
 * @return      void
 */

void printCache(Cache cache);

#endif
/* CACHESIM_H */
