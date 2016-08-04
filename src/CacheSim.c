/* File: CacheSim.c
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
 
/********************************
 *     1. Includes              *
 *******************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include "CacheSim.h"

/********************************
 *     2. Structs & Globals     *
 ********************************/

/* Block
 *
 * Holds an integer that states the validity of the bit (0 = invalid,
 * 1 = valid), the tag being held, and another integer that states if
 * the bit is dirty or not (0 = clean, 1 = dirty). Also holds the
 * timestamp for when the block was most recently updated (0 = oldest)
 */

struct Block_ {
    int valid;
    char* tag;
    int dirty;
    int timestamp;
};

/* Way
 *
 * Holds an integer about which way number it is (1 - 4),
 * and an array of blocks.
 */

struct Way_ {
	int waynum;
    Block* blocks;
};

/* Cache
 *
 * Cache object that holds all the data about cache access as well as 
 * the sizes and an array of ways.
 *
 * @param   reads           # of attempted reads by the trace input
 * @param 	read_hits 		# of reads that hit successfully in the cache
 * @param   read_misses 	# of reads that missed in the cache
 * @param   writes          # of attempted writes by the trace input
 * @param 	write_hits 		# of writes that hit successfully in the cache
 * @param 	write_misses 	# of writes that missed in the cache
 * @param 	cycles 			# of cycle currently elapsed in simulation
 * @param 	stream_ins 		# of stream-in operations from memory to cache
 * @param 	stream_outs 	# of stream_out operations from cache to memory
 * @param 	evictions 		# of valid blocks evicted from cache by LRU policy
 * @param   cache_size      total size of the cache in bytes
 * @param   block_size      how big each block of data is in bytes
 * @param 	associativity	# of ways
 * @param   ways          	pointer to the actual array of ways
 */


struct Cache_ {
    int reads;
    int read_hits;
    int read_misses;
    int writes;
    int write_hits;
    int write_misses;
    int cycles;
    int stream_ins;
    int stream_outs;
    int evictions;
    int cache_size;
    int block_size;
    int associativity;
    Way* ways;
};

// global variables for binary address bits

int bitsTag = 0;
int bitsIndex = 0;
int bitsOffset = 0;

// global variable for counting memory accesses

int mem_accesses = 0;

// global variables for debug flags

bool VERSION_DEBUG = false;
bool TRACE_DEBUG = false;
bool DUMP_DEBUG = false;

/********************************
 *     3. Utility Functions     *
 ********************************/
 
/* htoi
 *
 * Converts hexidecimal memory locations to unsigned integers.
 * No real error checking is performed. This function will skip
 * over any non-recognized characters.
 */
 
unsigned int htoi(const char str[]) {

    /* Local Variables */
    unsigned int result;
    int i;

    i = 0;
    result = 0;
    
    if(str[i] == '0' && str[i+1] == 'x') {
        i = i + 2;
    }

    while(str[i] != '\0') {

        result = result * 16;

        if(str[i] >= '0' && str[i] <= '9') {
            result = result + (str[i] - '0');
        }

        else if(tolower(str[i]) >= 'a' && tolower(str[i]) <= 'f') {
            result = result + (tolower(str[i]) - 'a') + 10;
        }

        i++;
    }

    return result;
}

/* getBinary
 *
 * Converts an unsigned integer into a string containing its
 * 32 bit long binary representation.
 *
 *
 * @param   num         number to be converted
 *
 * @result  char*       binary string
 */
 
char *getBinary(unsigned int num) {

    char* bstring;
    int i;
    
    /* Calculate the Binary String */
    
    bstring = (char*) malloc(sizeof(char) * 33);
    assert(bstring != NULL);
    
    bstring[32] = '\0';
    
    for( i = 0; i < 32; i++ ) {
        bstring[32 - 1 - i] = (num == ((1 << i) | num)) ? '1' : '0';
    }
    
    return bstring;
}

/* formatBinary
 *
 * Converts a binary string to a formatted version for easier parsing.
 * The format is determined by the bitsTag, bitsIndex, and bitsOffset variables.
 *
 * Ex. Format:
 *  -----------------------------------------------------
 * | Tag: 18 bits | Index: 10 bits | Byte Select: 5 bits |
 *  -----------------------------------------------------
 *
 * Ex. Result:
 * 101010101010101010 1010101010 10101
 *
 * @param   bstring     binary string to be converted
 *
 * @result  char*       formated binary string
 */

char *formatBinary(char *bstring) {

    char *formatted;
    int i;
    
    /* Format for Output */
    
    formatted = (char *) malloc(sizeof(char) * 35);
    assert(formatted != NULL);
    
    formatted[ADDRESS_SIZE+2] = '\0';
    
    for(i = 0; i < bitsTag; i++) {
        formatted[i] = bstring[i];
    }
    
    formatted[bitsTag] = ' ';
    
    for(i = bitsTag + 1; i < bitsIndex + bitsTag + 1; i++) {
        formatted[i] = bstring[i - 1];
    }
    
    formatted[bitsIndex + bitsTag + 1] = ' ';
    
    for(i = bitsIndex + bitsTag + 2; i < bitsOffset + bitsIndex + bitsTag + 2; i++) {
        formatted[i] = bstring[i - 2];
    }

    return formatted;
}

/* btoi
 *
 * Converts a binary string to an integer. Returns 0 on error.
 *
 * source: http://www.daniweb.com/software-development/c/code/216372
 *
 * @param   bin     binary string to convert
 *
 * @result  int     decimal representation of binary string
 */

int btoi(char *bin) {

    int  b, k, m, n;
    int  len, sum;

    sum = 0;
    len = strlen(bin) - 1;

    for(k = 0; k <= len; k++) {
        n = (bin[k] - '0');

        if ((n > 1) || (n < 0)) {
            return 0;
        }

        for(b = 1, m = len; m > k; m--) {
            b *= 2;
        }

        sum = sum + n * b;
    }
    return(sum);
}

/* parseMemoryAddress
 *
 * Helper function that takes in a hexadecimal address in
 * the format of "0x00000000" and spits out the decimal, 
 * binary, and formatted binary equivalents. Also, it
 * calculates the corresponding tag, index, and offset.
 *
 * @param       address         Hexadecimal memory address
 *
 * @return      void
 */

void parseMemoryAddress(char *address) {

    unsigned int dec;
    char *bstring, *bformatted, *tag, *index, *offset;
    int i;
    
    dec = htoi(address);
    bstring = getBinary(dec);
    bformatted = formatBinary(bstring);
    
    if(TRACE_DEBUG) {
        printf("Hex: %s\n", address);
        printf("Decimal: %u\n", dec);
        printf("Binary: %s\n", bstring);
        printf("Formatted: %s\n", bformatted);
    }
    
    i = 0;
    
    tag = (char *) malloc( sizeof(char) * (bitsTag + 1) );
    assert(tag != NULL);
    tag[bitsTag] = '\0';
    
    for(i = 0; i < bitsTag; i++) {
        tag[i] = bformatted[i];
    }
    
    index = (char *) malloc( sizeof(char) * (bitsIndex + 1) );
    assert(index != NULL);
    index[bitsIndex] = '\0';
    
    for(i = bitsTag + 1; i < bitsIndex + bitsTag + 1; i++) {
        index[i - bitsTag - 1] = bformatted[i];
    }
    
    offset = (char *) malloc( sizeof(char) * (bitsOffset + 1) );
    assert(offset != NULL);
    offset[bitsOffset] = '\0';
    
    for(i = bitsIndex + bitsTag + 2; i < bitsOffset + bitsIndex + bitsTag + 2; i++) {
        offset[i - bitsIndex - bitsTag - 2] = bformatted[i];
    }
    
    printf("Tag: %s (%i)\n", tag, btoi(tag));
    printf("Index: %s (%i)\n", index, btoi(index));
    printf("Offset: %s (%i)\n", offset, btoi(offset));
}

/********************************
 *        4. Main Function      *
 ********************************/
 
/*
 * Algorithm:
 *
 *  1. Validate input arguments
 *  2. Open the trace file for reading
 *  3. Create a new Cache object
 *  4. Read a line from the file
 *  5. Parse the line and read or write accordingly
 *  6. If the line is "#eof" continue, otherwise go back to step 4 
 *  7. Print the results
 *  8. Destroy the Cache object
 *  9. Close the file
 */

int main(int argc, char **argv) {

	int counter, i, j;
    Cache cache;
    FILE *file;
    char mode, address[100];
    
    /* Technically a line shouldn't be longer than 104 characters, but
       allocate extra space in the buffer just in case */

    char buffer[LINELENGTH];

    /* Help Menu
     *
     * If the help flag is present or there is not the correct # of args,
     * print the usage menu and return.
     *
     * There must be at least 2 args (./CacheSim and <file location>),
     * and no more than 3 extra debug args (-v, -t, -d).
     */
     
    if(argc < 2 || argc > 5 || strcmp(argv[1], "-h") == 0) {
        fprintf(stderr, "Usage: ./CacheSim <trace file> [-v] [-t] [-d] \n\n");
        return -1;
    }
    
    /* Check if there's more than two arguments
     * If so, use if-else statements to set the appropriate flags
     * Unrecognized args will terminate the program with an error message
     */

    if (argc >= 3) {

    		for (i = 3; i <= argc; i++) {

    			if (strcmp(argv[i - 1], "-v") == 0) {
    				VERSION_DEBUG = true;
    			}
    			else if (strcmp(argv[i-1], "-t") == 0) {
    				TRACE_DEBUG = true;
    			}
    			else if (strcmp(argv[i-1], "-d") == 0) {
    				DUMP_DEBUG = true;
    			}
    			else {
    		        fprintf(stderr, "\nIncorrect arguments: ./CacheSim <trace file> [-v] [-t] [-d]\n\n");
    		        return -1;
    			}
    		}
    }

    /* calculate other cache parameters */

    bitsOffset = floor(log2(BLOCK_SIZE));
    bitsIndex = floor(log2(NUMBER_OF_SETS));
    bitsTag = ADDRESS_SIZE - (bitsOffset + bitsIndex);

    /* Open the file for reading. */
    file = fopen( argv[1], "r" );

    if( file == NULL ) {
        fprintf(stderr, "ERROR: Could not open file. Check <file location> argument.\n");
        return -1;
    }

    /* If [-v] arg was specified, print version header information */

    if (VERSION_DEBUG) {
    	printf("\n\n************************ CacheSim v1.0 ************************\n");
    	printf("************************* Rehan Iqbal *************************\n");
    	printf("************************* PSU ECE 586 *************************\n");
    }

    /* Call createCache function, which allocates memory & returns pointer to Cache object */
    cache = createCache(CACHE_SIZE, BLOCK_SIZE, ASSOCIATIVITY);

    counter = 0;
    
    while( fgets(buffer, LINELENGTH, file) != NULL ) {

    	/* Check for #eof statement - skip text processing once encountered */
        if(buffer[0] != '#') {

        	i = 0;

            /* keep processing line until you hit newline character */
            while(buffer[i] != '\n') {

            	/* capture the r/w character into mode variable*/
            	mode = buffer[i];

            	/* push index forward to hex address */
            	i += 2;
            	j = 0;

            	/* keep storing hex address in buffer
            	 * until space or newline is encountered */

            	while(buffer[i] != ' ' && buffer[i] != '\n') {
            		address[j] = buffer[i];
            		i++;
            		j++;
            	}

            	/* terminate address buffer with NULL for helper function */
            	address[j] = '\0';

            	/* if last character is NULL, leave index where it is
            	 * otherwise, move index forward to next memory access*/
            	if(buffer[i] != '\n'){
            		i++;
            	}
            
            	/* print address if debug flag is set */
            	if(TRACE_DEBUG) printf("\nAccess %i: Mode %c -- Address %s\n\n", counter+1, mode, address);

            	mem_accesses++;
            
            	/* call read function with address buffer */
            	if(mode == 'r') {
            		readFromCache(cache, address);
            	}

            	/* call write function with address buffer */
            	else if(mode == 'w') {
            		writeToCache(cache, address);
            	}

            	/* if no valid mode detected, terminate program
            	 * after freeing cache memory & closing file safely */

            	else {
            		printf("Error on memory access %i! Check trace file input.\n", counter);
            		fclose(file);
            		destroyCache(cache);
            		cache = NULL;
                
            		return -1;
            	}

            	counter++;
            }
        }
    }

    /* Call printCache function to print cache statistics and dump information */
    printCache(cache);
    
    /* Close the file, destroy the cache. */
    
    fclose(file);
    destroyCache(cache);
    cache = NULL;
    
    return 0;
}

/********************************
 *     5. Cache Functions       *
 ********************************/
 
/* Function List:
 *
 * 1) createCache
 * 2) destroyCache
 * 3) readFromCache
 * 4) writeToCache
 * 5) printCache
 */


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

Cache createCache(int cache_size, int block_size, int associativity) {

	Cache cache;
    int i = 0;
    int j = 0;
    
    /* Validate Inputs */
    if(cache_size <= 0) {
        fprintf(stderr, "Error: Cache size must be greater than 0 bytes!\n");
        return NULL;
    }
    
    if(block_size <= 0) {
        fprintf(stderr, "Error: Block size must be greater than 0 bytes!\n");
        return NULL;
    }
    
    
    /* Lets make a cache!
     * allocate all the memory needed for a cache structure */

    cache = (Cache) malloc( sizeof( struct Cache_ ) );

    if(cache == NULL) {
        fprintf(stderr, "Error: could not allocate memory for cache.\n");
        return NULL;
    }
    
    /* Initialize the cache parameters */

    cache->reads = 0;
    cache->read_hits = 0;
    cache->read_misses = 0;

    cache->writes = 0;
    cache->write_hits = 0;
    cache->write_misses = 0;

    cache->cycles = 0;

    cache->stream_ins = 0;
    cache->stream_outs = 0;
    cache->evictions = 0;

    cache->cache_size = cache_size;
    cache->block_size = block_size;
    cache->associativity = associativity;

    /* Allocate all the memory for ALL ways */

	cache->ways = (Way*) malloc( sizeof(Way) * associativity );
	assert(cache->ways != NULL);

    for(j = 0; j < associativity; j++) {

    	/* Allocate memory for this INDIVIDUAL way */

		cache->ways[j] = (Way) malloc( sizeof( struct Way_ ) );
		assert(cache->ways[j] != NULL);

    	cache->ways[j]->waynum = j;

		/* Allocate space for ALL Blocks */

		cache->ways[j]->blocks = (Block*) malloc( sizeof(Block) * NUMBER_OF_SETS );
		assert(cache->ways[j]->blocks != NULL);

		/* By default insert blocks where valid = 0 */
		for(i = 0; i < NUMBER_OF_SETS; i++) {

			/* Allocate memory for this INDIVIDUAL block */

			cache->ways[j]->blocks[i] = (Block) malloc( sizeof( struct Block_ ) );
			assert(cache->ways[j]->blocks[i] != NULL);

			/* Initialize parameters for this block */

			cache->ways[j]->blocks[i]->valid = 0;
			cache->ways[j]->blocks[i]->dirty = 0;
			cache->ways[j]->blocks[i]->tag = NULL;
			cache->ways[j]->blocks[i]->timestamp = 0;

		}
    }
    
    return cache;
}

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

void destroyCache(Cache cache) {

    int i;
    int j;
    
    if(cache != NULL) {

    	/* Deallocate ALL ways in this cache */
    	for (j = 0; j < ASSOCIATIVITY; j++) {

    		/* Deallocate ALL blocks in this way */
			for( i = 0; i < NUMBER_OF_SETS; i++ ) {

				/* Check if the block entry is NULL */

				if(cache->ways[j]->blocks[i]->tag != NULL) {
					free(cache->ways[j]->blocks[i]->tag);
				}

				/* Deallocate this INDIVIDUAL block */
				free(cache->ways[j]->blocks[i]);
			}

			/* Deallocate this INDIVIDUAL way */

			free(cache->ways[j]->blocks);
			free(cache->ways[j]);
    	}

        free(cache->ways);
        free(cache);
    }

    return;
}

/* readFromCache
 *
 * Function that reads data from a cache. Returns 0 on failure
 * or 1 on success. 
 *
 * @param       cache       target cache struct
 * @param       address     hexidecimal address
 *
 * @return      success     0
 * @return      failure     -1
 */

int readFromCache(Cache cache, char* address) {

    unsigned int dec;
    char *bstring, *bformatted, *tag, *index, *offset;
    int i;
    int j;
    int LRU;
    int LRU_access_num;
    bool noHit = true;

    Block block;
    
    
    /* Validate inputs */
    if(cache == NULL) {
        fprintf(stderr, "Error: Must supply a valid cache to write to!\n");
        return -1;
    }
    
    if(address == NULL) {
        fprintf(stderr, "Error: Must supply a valid memory address!\n");
        return -1;
    }
    
    /* Convert and parse necessary values */
    
    dec = htoi(address);
    bstring = getBinary(dec);
    bformatted = formatBinary(bstring);
    
    /* Print cache address bits for debugging if [-t] arg was specified */

    if(TRACE_DEBUG) {
        printf("\tHex: %s\n", address);
        printf("\tDecimal: %u\n", dec);
        printf("\tBinary: %s\n", bstring);
        printf("\tFormatted: %s\n\n", bformatted);
    }
        
    i = 0;
    
    tag = (char *) malloc( sizeof(char) * (bitsTag + 1) );
    assert(tag != NULL);
    tag[bitsTag] = '\0';
    
    for(i = 0; i < bitsTag; i++) {
        tag[i] = bformatted[i];
    }
    
    index = (char *) malloc( sizeof(char) * (bitsIndex + 1) );
    assert(index != NULL);
    index[bitsIndex] = '\0';
    
    for(i = bitsTag + 1; i < bitsIndex + bitsTag + 1; i++) {
        index[i - bitsTag - 1] = bformatted[i];
    }
    
    offset = (char *) malloc( sizeof(char) * (bitsOffset + 1) );
    assert(offset != NULL);
    offset[bitsOffset] = '\0';
    
    for(i = bitsIndex + bitsTag + 2; i < bitsOffset + bitsIndex + bitsTag + 2; i++) {
        offset[i - bitsIndex - bitsTag - 2] = bformatted[i];
    }
    
    /* Print tag + index + offset for debugging if [-t] arg was specified */

    if(TRACE_DEBUG) {
        printf("\tTag: %s (%i)\n", tag, btoi(tag));
        printf("\tIndex: %s (%i)\n", index, btoi(index));
        printf("\tOffset: %s (%i)\n\n", offset, btoi(offset));
    }
    
    /* Get the block */
	if(TRACE_DEBUG) printf("\tAttempting to read data from cache slot %i.\n", btoi(index));

	/* Increment an attempted read access */
	cache->reads++;

	/* Check through ALL 4 ways of the cache for that particular block */
    for(j = 0; j < ASSOCIATIVITY; j++) {

		block = cache->ways[j]->blocks[btoi(index)];

		/* if there's a cache hit (valid && tags match) */

		if(block->valid == 1 && strcmp(block->tag, tag) == 0) {

			cache->read_hits++;
			cache->cycles += 1;
			noHit = false;
			block->timestamp = mem_accesses;
			if (TRACE_DEBUG) printf("\tCache hit on Way %d. Block timestamp updated to %d.\n", j, mem_accesses);
			free(tag);
		}

    }

	/* Otherwise, cache miss - implement LRU here */

	if (noHit == true) {

		cache->read_misses++;
		cache->stream_ins++;
		cache->cycles +=51;

        LRU_access_num = mem_accesses;

        /* Search through blocks in all 4 ways to find LRU */

        for (j = 0; j < ASSOCIATIVITY; j++) {

            block = cache->ways[j]->blocks[btoi(index)];

            if(block->timestamp < LRU_access_num) {
            	LRU_access_num = block->timestamp;
            	LRU = j;
            }
        }

        /* evict LRU and update cache statistics */

		if (TRACE_DEBUG) printf("\tCache miss - eviction on Way %d. Block timestamp updated to %d.\n", LRU, mem_accesses);
        block = cache->ways[LRU]->blocks[btoi(index)];

        /* if data was dirty, need to stream-out and reset dirty bit */

		if(block->dirty == 1) {
			cache->stream_outs++;
			cache->cycles += 50;
			block->dirty = 0;
		}

		block->valid = 1;
        block->timestamp = mem_accesses;

        /* if non-null data got evicted, log an eviction */

		if(block->tag != NULL) {
			cache->evictions++;
			free(block->tag);
		}

		/* replace victim tag with incoming block's tag */
		block->tag = tag;

	}
    
    free(bstring);
    free(bformatted);
    free(offset);
    free(index);
    return 0;
}

/* writeToCache
 *
 * Function that writes data to the cache. Returns 0 on failure or
 * 1 on success. Frees any old tags that already existed in the
 * target slot.
 *
 * @param       cache       target cache struct
 * @param       address     hexidecimal address
 *
 * @return      success     0
 * @return      error       -1
 */

int writeToCache(Cache cache, char* address) {

    unsigned int dec;
    char *bstring, *bformatted, *tag, *index, *offset;
    int i = 0;
    int j = 0;
    int LRU;
    int LRU_access_num;
    bool noHit = true;
    
    Block block;

    /* Validate inputs */
    if(cache == NULL) {
        fprintf(stderr, "Error: Must supply a valid cache to write to!\n");
        return -1;
    }
    
    if(address == NULL) {
        fprintf(stderr, "Error: Must supply a valid memory address!\n");
        return -1;
    }
    
    /* Convert and parse necessary values */
    
    dec = htoi(address);
    bstring = getBinary(dec);
    bformatted = formatBinary(bstring);
    
    if(TRACE_DEBUG) {
        printf("\tHex: %s\n", address);
        printf("\tDecimal: %u\n", dec);
        printf("\tBinary: %s\n", bstring);
        printf("\tFormatted: %s\n\n", bformatted);
    }
        
    i = 0;
    
    tag = (char *) malloc( sizeof(char) * (bitsTag + 1) );
    assert(tag != NULL);
    tag[bitsTag] = '\0';
    
    for(i = 0; i < bitsTag; i++) {
        tag[i] = bformatted[i];
    }
    
    index = (char *) malloc( sizeof(char) * (bitsIndex + 1) );
    assert(index != NULL);
    index[bitsIndex] = '\0';
    
    for(i = bitsTag + 1; i < bitsIndex + bitsTag + 1; i++) {
        index[i - bitsTag - 1] = bformatted[i];
    }
    
    offset = (char *) malloc( sizeof(char) * (bitsOffset + 1) );
    assert(offset != NULL);
    offset[bitsOffset] = '\0';
    
    for(i = bitsIndex + bitsTag + 2; i < bitsOffset + bitsIndex + bitsTag + 2; i++) {
        offset[i - bitsIndex - bitsTag - 2] = bformatted[i];
    }
    
    if(TRACE_DEBUG) {
        printf("\tTag: %s (%i)\n", tag, btoi(tag));
        printf("\tIndex: %s (%i)\n", index, btoi(index));
        printf("\tOffset: %s (%i)\n\n", offset, btoi(offset));
    }
    
    /* Get the block */
    
    if(TRACE_DEBUG) printf("\tAttempting to write data to cache slot %i.\n", btoi(index));

    /* Log another attempted write access */

    cache->writes++;

    /* Check through ALL 4 ways for that particular block */

    for (j = 0; j < ASSOCIATIVITY; j++) {

        block = cache->ways[j]->blocks[btoi(index)];

        /* if there was a write hit (valid bit set && tags match) */

        if(block->valid == 1 && strcmp(block->tag, tag) == 0) {

            noHit = false;
            block->dirty = 1;
            block->timestamp = mem_accesses;
            cache->write_hits++;
            cache->cycles += 1;
			if (TRACE_DEBUG) printf("\tCache hit on Way %d. Block timestamp updated to %d.\n", j, mem_accesses);
            free(tag);
        }
    }


    /* cache miss - implement LRU here */

    if (noHit == true) {

        cache->write_misses++;
        cache->stream_ins++;
        cache->cycles +=51;
        
        LRU_access_num = mem_accesses;

        /* search through all 4 ways looking for LRU block */

        for (j = 0; j < ASSOCIATIVITY; j++) {

            block = cache->ways[j]->blocks[btoi(index)];

            if(block->timestamp < LRU_access_num) {
            	LRU_access_num = block->timestamp;
            	LRU = j;
            }
        }

        /* evict the LRU block & update cache statistics */

		if (TRACE_DEBUG) printf("\tCache miss - eviction on Way %d. Block timestamp updated to %d.\n", LRU, mem_accesses);
		block = cache->ways[LRU]->blocks[btoi(index)];

		/* if eviction target was dirty, must stream-out */

        if(block->dirty == 1) {
            cache->stream_outs++;
            cache->cycles += 50;
        }        
        
        /* allocate-on-write policy -> overwritten data is in cache
         * must set the dirty & valid bits */

        block->dirty = 1;
        block->valid = 1;
        block->timestamp = mem_accesses;
        
        /* if evicted data was non-null, log an eviction */

        if(block->tag != NULL) {
        	cache->evictions++;
            free(block->tag);
        }
        
        /* replace victim tag with new block's tag */
        block->tag = tag;
    }
    
    free(bstring);
    free(bformatted);
    free(offset);
    free(index);
    return 0;
}

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

void printCache(Cache cache) {

    int i;
    int j;

    Block block;

    /* define some local integers to hold count totals */

    int cache_total = (int) (cache->reads) + (cache->writes);
    int cache_hits = (int) (cache->read_hits) + (cache->write_hits);
    int cache_misses = (int) (cache->read_misses) + (cache->write_misses);

    char* tag;
    
    if(cache != NULL) {

    	/* Printing cache contents at the end of simulation
    	 * if the debug flag was set */

    	if (DUMP_DEBUG) {
    		for (j = 0; j < ASSOCIATIVITY; j++) {
    			printf("\n\n******** Way # %d ********\n\n", cache->ways[j]->waynum);

				for(i = 0; i < NUMBER_OF_SETS; i++) {

					block = cache->ways[j]->blocks[i];
					tag = "NULL";

					if(block->tag != NULL) {
						tag = block->tag;
					}

					printf("\t[%i]: { valid: %i, dirty: %i, timestamp: %d, tag: %s }\n", i, block->valid, block->dirty, block->timestamp, tag);
				}
    		}
    	}

    	/* Printing cache statistics to the console */

        printf("\nCache parameters:\n\n");

        printf("\tCache size: %d\n", cache->cache_size);
        printf("\tCache block size: %d\n", cache->block_size);
        printf("\tCache number of lines: %d\n", NUMBER_OF_SETS);
        printf("\tCache associativity: %d\n", ASSOCIATIVITY);

        printf("\nCache performance:\n\n");

        printf("\tAttempted reads: %d\n", cache->reads);
        printf("\tCache read hits: %d\n", cache->read_hits);
        printf("\tCache read misses: %d\n\n", cache->read_misses);

        printf("\tAttempted writes: %d\n", cache->writes);
        printf("\tCache write hits: %d\n", cache->write_hits);
        printf("\tCache write misses: %d\n\n", cache->write_misses);

        printf("\tCache hits: %d\n", cache_hits);
        printf("\tCache misses: %d\n", cache_misses);
        printf("\tTotal accesses: %d\n\n", cache_total);

        printf("\tCache hit ratio: %2.2f%%\n", ((float) (cache_hits) / (float) (cache_total) ) * 100);
        printf("\tCache miss ratio: %2.2f%%\n\n", ((float) (cache_misses) / (float) (cache_total) ) * 100);

        printf("\tStream-in operations: %d\n", cache->stream_ins);
        printf("\tCache evictions: %d\n", cache->evictions);
        printf("\tStream-out operations: %d\n\n", cache->stream_outs);

        printf("\tCycles with cache: %d\n", cache->cycles);
        printf("\tCycles without cache: %d\n\n", 50*cache_total);

    }

}