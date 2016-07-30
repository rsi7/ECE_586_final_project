/* File: sim.c
 * 
 * This is a program that simulates a cache using a trace file 
 * 
 * Usage: Usage: ./CacheSim.exe [-h] <trace file>
 *
 *
 * <trace file> is the file location that contains a memory access trace.
 *
 */
 
/********************************
 *     1. Includes              *
 *******************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include "sim.h"

/********************************
 *     2. Structs & Globals     *
 ********************************/

/* Block
 *
 * Holds an integer that states the validity of the bit (0 = invalid,
 * 1 = valid), the tag being held, and another integer that states if
 * the bit is dirty or not (0 = clean, 1 = dirty).
 */

struct Block_ {
    int valid;
    char* tag;
    int dirty;
    int timestamp;
};

/* Way
 *
 * Holds an integer about which way number it is (1 - 8),
 * and integer for the number of blocks, and an array of blocks.
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
 * @param   hits            # of cache accesses that hit valid data
 * @param   misses          # of cache accesses that missed valid data
 * @param   reads           # of reads from main memory
 * @param   writes          # of writes from main memory
 * @param   cache_size      Total size of the cache in bytes
 * @param   block_size      How big each block of data should be
 * @param 	associativity	# of ways
 * @param   ways          	The actual array of ways
 */


struct Cache_ {
    int hits;
    int misses;
    int reads;
    int writes;
    int evictions;
    int cache_size;
    int block_size;
    int associativity;
    Way* ways;
};

// global variables for cache bits

int bitsTag = 0;
int bitsIndex = 0;
int bitsOffset = 0;

// global variable for counting memory accesses

int mem_accesses = 0;

/********************************
 *     3. Utility Functions     *
 ********************************/
 
/* Function List:
 *
 * 1) htoi
 * 2) getBinary
 * 3) formatBinary
 * 4) btoi
 * 5) parseMemoryAddress
 */

/* htoi
 *
 * Converts hexidecimal memory locations to unsigned integers.
 * No real error checking is performed. This function will skip
 * over any non recognized characters.
 */
 
unsigned int htoi(const char str[])
{
    /* Local Variables */
    unsigned int result;
    int i;

    i = 0;
    result = 0;
    
    if(str[i] == '0' && str[i+1] == 'x')
    {
        i = i + 2;
    }

    while(str[i] != '\0')
    {
        result = result * 16;
        if(str[i] >= '0' && str[i] <= '9')
        {
            result = result + (str[i] - '0');
        }
        else if(tolower(str[i]) >= 'a' && tolower(str[i]) <= 'f')
        {
            result = result + (tolower(str[i]) - 'a') + 10;
        }
        i++;
    }

    return result;
}

/* getBinary
 *
 * Converts an unsigned integer into a string containing it's
 * 32 bit long binary representation.
 *
 *
 * @param   num         number to be converted
 *
 * @result  char*       binary string
 */
 
char *getBinary(unsigned int num)
{
    char* bstring;
    int i;
    
    /* Calculate the Binary String */
    
    bstring = (char*) malloc(sizeof(char) * 33);
    assert(bstring != NULL);
    
    bstring[32] = '\0';
    
    for( i = 0; i < 32; i++ )
    {
        bstring[32 - 1 - i] = (num == ((1 << i) | num)) ? '1' : '0';
    }
    
    return bstring;
}

/* formatBinary
 *
 * Converts a 32 bit long binary string to a formatted version
 * for easier parsing. The format is determined by the bitsTag, bitsIndex,
 * and bitsOffset variables.
 *
 * Ex. Format:
 *  -----------------------------------------------------
 * | Tag: 18 bits | Index: 12 bits | Byte Select: 4 bits |
 *  -----------------------------------------------------
 *
 * Ex. Result:
 * 000000000010001110 101111011111 00
 *
 * @param   bstring     binary string to be converted
 *
 * @result  char*       formated binary string
 */

char *formatBinary(char *bstring)
{
    char *formatted;
    int i;
    
    /* Format for Output */
    
    formatted = (char *) malloc(sizeof(char) * 35);
    assert(formatted != NULL);
    
    formatted[34] = '\0';
    
    for(i = 0; i < bitsTag; i++)
    {
        formatted[i] = bstring[i];
    }
    
    formatted[bitsTag] = ' ';
    
    for(i = bitsTag + 1; i < bitsIndex + bitsTag + 1; i++)
    {
        formatted[i] = bstring[i - 1];
    }
    
    formatted[bitsIndex + bitsTag + 1] = ' ';
    
    for(i = bitsIndex + bitsTag + 2; i < bitsOffset + bitsIndex + bitsTag + 2; i++)
    {
        formatted[i] = bstring[i - 2];
    }

    return formatted;
}

/* btoi
 *
 * Converts a binary string to an integer. Returns 0 on error.
 *
 * src: http://www.daniweb.com/software-development/c/code/216372
 *
 * @param   bin     binary string to convert
 *
 * @result  int     decimal representation of binary string
 */

int btoi(char *bin)
{
    int  b, k, m, n;
    int  len, sum;

    sum = 0;
    len = strlen(bin) - 1;

    for(k = 0; k <= len; k++)
    {
        n = (bin[k] - '0'); 
        if ((n > 1) || (n < 0))
        {
            return 0;
        }
        for(b = 1, m = len; m > k; m--)
        {
            b *= 2;
        }
        sum = sum + n * b;
    }
    return(sum);
}

/* parseMemoryAddress
 *
 * Helper function that takes in a hexidecimal address in
 * the format of "0x00000000" and spits out the decimal, 
 * binary, and formatted binary equivilants. Also, it 
 * calculates the corresponding tag, index, and offset.
 *
 * @param       address         Hexidecimal memory address
 *
 * @return      void
 */

void parseMemoryAddress(char *address)
{
    unsigned int dec;
    char *bstring, *bformatted, *tag, *index, *offset;
    int i;
    
    dec = htoi(address);
    bstring = getBinary(dec);
    bformatted = formatBinary(bstring);
    
    if(DEBUG)
    {
        printf("Hex: %s\n", address);
        printf("Decimal: %u\n", dec);
        printf("Binary: %s\n", bstring);
        printf("Formatted: %s\n", bformatted);
    }
    
    i = 0;
    
    tag = (char *) malloc( sizeof(char) * (bitsTag + 1) );
    assert(tag != NULL);
    tag[bitsTag] = '\0';
    
    for(i = 0; i < bitsTag; i++)
    {
        tag[i] = bformatted[i];
    }
    
    index = (char *) malloc( sizeof(char) * (bitsIndex + 1) );
    assert(index != NULL);
    index[bitsIndex] = '\0';
    
    for(i = bitsTag + 1; i < bitsIndex + bitsTag + 1; i++)
    {
        index[i - bitsTag - 1] = bformatted[i];
    }
    
    offset = (char *) malloc( sizeof(char) * (bitsOffset + 1) );
    assert(offset != NULL);
    offset[bitsOffset] = '\0';
    
    for(i = bitsIndex + bitsTag + 2; i < bitsOffset + bitsIndex + bitsTag + 2; i++)
    {
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
 *  1. Validate inputs
 *  2. Open the trace file for reading
 *  3. Create a new cache object
 *  4. Read a line from the file
 *  5. Parse the line and read or write accordingly
 *  6. If the line is "#eof" continue, otherwise go back to step 4 
 *  7. Print the results
 *  8. Destroy the cache object
 *  9. Close the file
 */

int main(int argc, char **argv) {

    /* Local Variables */
    int counter, i, j;
    Cache cache;
    FILE *file;
    char mode, address[100];
    
    /* Technically a line shouldn't be longer than 25 characters, but
       allocate extra space in the buffer just in case */
    char buffer[LINELENGTH];

    /* Help Menu
     *
     * If the help flag is present or there is not exactly
     * two arguments (./CacheSim + file location),
     * print the usage menu and return.
     */
     
    if(argc != 2 || strcmp(argv[1], "-h") == 0) {
        fprintf(stderr, "Usage: ./CacheSim.exe [-h] <trace file>\n\n");
        return 0;
    }
    

    // calculate other cache parameters

    bitsOffset = floor(log2(BLOCK_SIZE));
    bitsIndex = floor(log2(NUMBER_OF_SETS));
    bitsTag = ADDRESS_SIZE - (bitsOffset + bitsIndex);

    /* Open the file for reading. */
    file = fopen( argv[1], "r" );
    if( file == NULL )
    {
        fprintf(stderr, "Error: Could not open file.\n");
        return 0; 
    }

    cache = createCache(CACHE_SIZE, BLOCK_SIZE, ASSOCIATIVITY);

    counter = 0;
    
    while( fgets(buffer, LINELENGTH, file) != NULL )
    {
        if(buffer[0] != '#')
        {
            i = 0;
            mode = buffer[i];

            i += 2;
            j = 0;
            
            while(buffer[i] != '\0')
            {
                address[j] = buffer[i];
                i++;
                j++;
            }
            
            address[j-1] = '\0';
            
            if(DEBUG) printf("\nLine %i: Mode %c -- Address %s\n\n", counter+1, mode, address);
            
            if(mode == '0') {
            	mem_accesses++;
                readFromCache(cache, address);
            }

            else if(mode == '1') {
            	mem_accesses++;
                writeToCache(cache, address);
            }

            else {
                printf("%i: ERROR!!!!\n", counter);
                fclose(file);
                destroyCache(cache);
                cache = NULL;
                
                return 0;
            }
            counter++;
        }
    }

    printCache(cache);
    
    /* Close the file, destroy the cache. */
    
    fclose(file);
    destroyCache(cache);
    cache = NULL;
    
    return 1;
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
 * @return  success         new Cache
 * @return  failure         NULL
 */

Cache createCache(int cache_size, int block_size, int associativity)
{
    /* Local Variables */
    Cache cache;
    int i = 0;
    int j = 0;
    
    /* Validate Inputs */
    if(cache_size <= 0)
    {
        fprintf(stderr, "Cache size must be greater than 0 bytes...\n");
        return NULL;
    }
    
    if(block_size <= 0)
    {
        fprintf(stderr, "Block size must be greater than 0 bytes...\n");
        return NULL;
    }
    
    
    /* Lets make a cache! */

    // allocate all the memory needed for a cache structure

    cache = (Cache) malloc( sizeof( struct Cache_ ) );
    if(cache == NULL)
    {
        fprintf(stderr, "Could not allocate memory for cache.\n");
        return NULL;
    }
    
    // take care of most parameters

    cache->hits = 0;
    cache->misses = 0;
    cache->reads = 0;
    cache->writes = 0;
    cache->evictions = 0;

    cache->cache_size = cache_size;
    cache->block_size = block_size;
    

    cache->associativity = associativity;

    // allocate all the memory for ALL ways
	cache->ways = (Way*) malloc( sizeof(Way) * associativity );
	assert(cache->ways != NULL);

    for(j = 0; j < associativity; j++) {

    	// allocate memory for this INDIVIDUAL way
		cache->ways[j] = (Way) malloc( sizeof( struct Way_ ) );
		assert(cache->ways[j] != NULL);

    	cache->ways[j]->waynum = j;

		// allocate space for ALL Blocks
		cache->ways[j]->blocks = (Block*) malloc( sizeof(Block) * NUMBER_OF_SETS );
		assert(cache->ways[j]->blocks != NULL);

		/* By default insert blocks where valid = 0 */

		for(i = 0; i < NUMBER_OF_SETS; i++) {

			// allocate memory for this INDIVIDUAL block
			cache->ways[j]->blocks[i] = (Block) malloc( sizeof( struct Block_ ) );
			assert(cache->ways[j]->blocks[i] != NULL);

			// take care of other parameters for this block
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

    	// deallocate ALL ways in this cache
    	for (j = 0; j < ASSOCIATIVITY; j++) {

    		// deallocate ALL blocks in this way
			for( i = 0; i < NUMBER_OF_SETS; i++ ) {

				// check if the block entry is NULL
				if(cache->ways[j]->blocks[i]->tag != NULL) {
					free(cache->ways[j]->blocks[i]->tag);
				}

				// deallocate this INDIVIDUAL block
				free(cache->ways[j]->blocks[i]);
			}

			// deallocate this INDIVIDUAL way
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
 * @return      success     1
 * @return      failure     0
 */

int readFromCache(Cache cache, char* address)
{
    unsigned int dec;
    char *bstring, *bformatted, *tag, *index, *offset;
    int i;
    int j;
    int LRU;
    int LRU_access_num;
    int noHit = 1;

    Block block;
    
    
    /* Validate inputs */
    if(cache == NULL)
    {
        fprintf(stderr, "Error: Must supply a valid cache to write to.\n");
        return 0;
    }
    
    if(address == NULL)
    {
        fprintf(stderr, "Error: Must supply a valid memory address.\n");
        return 0;
    }
    
    /* Convert and parse necessary values */
    
    dec = htoi(address);
    bstring = getBinary(dec);
    bformatted = formatBinary(bstring);
    
    if(DEBUG)
    {
        printf("\tHex: %s\n", address);
        printf("\tDecimal: %u\n", dec);
        printf("\tBinary: %s\n", bstring);
        printf("\tFormatted: %s\n\n", bformatted);
    }
        
    i = 0;
    
    tag = (char *) malloc( sizeof(char) * (bitsTag + 1) );
    assert(tag != NULL);
    tag[bitsTag] = '\0';
    
    for(i = 0; i < bitsTag; i++)
    {
        tag[i] = bformatted[i];
    }
    
    index = (char *) malloc( sizeof(char) * (bitsIndex + 1) );
    assert(index != NULL);
    index[bitsIndex] = '\0';
    
    for(i = bitsTag + 1; i < bitsIndex + bitsTag + 1; i++)
    {
        index[i - bitsTag - 1] = bformatted[i];
    }
    
    offset = (char *) malloc( sizeof(char) * (bitsOffset + 1) );
    assert(offset != NULL);
    offset[bitsOffset] = '\0';
    
    for(i = bitsIndex + bitsTag + 2; i < bitsOffset + bitsIndex + bitsTag + 2; i++)
    {
        offset[i - bitsIndex - bitsTag - 2] = bformatted[i];
    }
    
    if(DEBUG)
    {
        printf("\tTag: %s (%i)\n", tag, btoi(tag));
        printf("\tIndex: %s (%i)\n", index, btoi(index));
        printf("\tOffset: %s (%i)\n\n", offset, btoi(offset));
    }
    
    /* Get the block */
    
	if(DEBUG) printf("\tAttempting to read data from cache slot %i.\n", btoi(index));

    for(j = 0; j < ASSOCIATIVITY; j++) {

		block = cache->ways[j]->blocks[btoi(index)];

		// if there's a cache hit (valid && tags match)
		if(block->valid == 1 && strcmp(block->tag, tag) == 0) {
			cache->hits++;
			noHit = 0;
			block->timestamp = mem_accesses;
			printf("\tCache hit on Way %d - block timestamp updated to %d.\n", j, mem_accesses);
			free(tag);
		}

    }

	// otherwise, cache miss - implement LRU here
	if (noHit == 1) {

		cache->misses++;
		cache->reads++;

        LRU_access_num = mem_accesses;

        for (j = 0; j < ASSOCIATIVITY; j++) {

            block = cache->ways[j]->blocks[btoi(index)];

            if(block->timestamp < LRU_access_num) {
            	LRU_access_num = block->timestamp;
            	LRU = j;
            }
        }

		printf("\tCache miss - eviction on Way %d. Block timestamp update to %d.\n", LRU, mem_accesses);
        block = cache->ways[LRU]->blocks[btoi(index)];

		if(block->dirty == 1) {
			cache->writes++;
			block->dirty = 0;
		}

		block->valid = 1;
        block->timestamp = mem_accesses;

		if(block->tag != NULL) {
			cache->evictions++;
			free(block->tag);
		}

		block->tag = tag;

	}
    
    free(bstring);
    free(bformatted);
    free(offset);
    free(index);
    return 1;
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
 * @return      success     1
 * @return      error       0
 */

int writeToCache(Cache cache, char* address)
{
    unsigned int dec;
    char *bstring, *bformatted, *tag, *index, *offset;
    int i = 0;
    int j = 0;
    Block block;
    int LRU;
    int LRU_access_num;
    int noHit = 1;
    
    /* Validate inputs */
    if(cache == NULL)
    {
        fprintf(stderr, "Error: Must supply a valid cache to write to.\n");
        return 0;
    }
    
    if(address == NULL)
    {
        fprintf(stderr, "Error: Must supply a valid memory address.\n");
        return 0;
    }
    
    /* Convert and parse necessary values */
    
    dec = htoi(address);
    bstring = getBinary(dec);
    bformatted = formatBinary(bstring);
    
    if(DEBUG)
    {
        printf("\tHex: %s\n", address);
        printf("\tDecimal: %u\n", dec);
        printf("\tBinary: %s\n", bstring);
        printf("\tFormatted: %s\n\n", bformatted);
    }
        
    i = 0;
    
    tag = (char *) malloc( sizeof(char) * (bitsTag + 1) );
    assert(tag != NULL);
    tag[bitsTag] = '\0';
    
    for(i = 0; i < bitsTag; i++)
    {
        tag[i] = bformatted[i];
    }
    
    index = (char *) malloc( sizeof(char) * (bitsIndex + 1) );
    assert(index != NULL);
    index[bitsIndex] = '\0';
    
    for(i = bitsTag + 1; i < bitsIndex + bitsTag + 1; i++)
    {
        index[i - bitsTag - 1] = bformatted[i];
    }
    
    offset = (char *) malloc( sizeof(char) * (bitsOffset + 1) );
    assert(offset != NULL);
    offset[bitsOffset] = '\0';
    
    for(i = bitsIndex + bitsTag + 2; i < bitsOffset + bitsIndex + bitsTag + 2; i++)
    {
        offset[i - bitsIndex - bitsTag - 2] = bformatted[i];
    }
    
    if(DEBUG)
    {
        printf("\tTag: %s (%i)\n", tag, btoi(tag));
        printf("\tIndex: %s (%i)\n", index, btoi(index));
        printf("\tOffset: %s (%i)\n\n", offset, btoi(offset));
    }
    
    /* Get the block */
    
    if(DEBUG) printf("\tAttempting to write data to cache slot %i.\n", btoi(index));

    for (j = 0; j < ASSOCIATIVITY; j++) {

        block = cache->ways[j]->blocks[btoi(index)];

        if(block->valid == 1 && strcmp(block->tag, tag) == 0) {

            noHit = 0;
            block->dirty = 1;
            block->timestamp = mem_accesses;
            cache->hits++;
			printf("\tCache hit on Way %d - block timestamp updated to %d.\n", j, mem_accesses);
            free(tag);
        }
    }


    if (noHit == 1) {
        cache->misses++;
        cache->reads++;
        
        LRU_access_num = mem_accesses;

        for (j = 0; j < ASSOCIATIVITY; j++) {

            block = cache->ways[j]->blocks[btoi(index)];

            if(block->timestamp < LRU_access_num) {
            	LRU_access_num = block->timestamp;
            	LRU = j;
            }
        }

		printf("\tCache miss - eviction on Way %d. Block timestamp update to %d.\n", LRU, mem_accesses);
		block = cache->ways[LRU]->blocks[btoi(index)];

        if(block->dirty == 1) {
            cache->writes++;
        }        
        
        block->dirty = 1;
        block->valid = 1;
        block->timestamp = mem_accesses;
        
        if(block->tag != NULL) {
        	cache->evictions++;
            free(block->tag);
        }
        
        block->tag = tag;
    }
    
    free(bstring);
    free(bformatted);
    free(offset);
    free(index);
    return 1;
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

void printCache(Cache cache)
{
    int i;
    int j;

    int cache_total = (int) (cache->hits) + (cache->misses);
    char* tag;
    
    if(cache != NULL) {
    	if (DEBUG) {

    		for (j = 0; j < ASSOCIATIVITY; j++) {

    			printf("\n\n******** Way # %d ********\n\n", cache->ways[j]->waynum);

				for(i = 0; i < NUMBER_OF_SETS; i++) {
					tag = "NULL";
					if(cache->ways[j]->blocks[i]->tag != NULL) {
						tag = cache->ways[j]->blocks[i]->tag;
					}
					printf("\t[%i]: { valid: %i, tag: %s }\n", i, cache->ways[j]->blocks[i]->valid, tag);
				}
    		}
    	}

        printf("\nCache parameters:\n\n");

        printf("\tCache size: %d\n", cache->cache_size);
        printf("\tCache block size: %d\n", cache->block_size);
        printf("\tCache number of lines: %d\n", NUMBER_OF_SETS);
        printf("\tCache associativity: %d\n", ASSOCIATIVITY);

        printf("\nCache performance:\n\n");

        printf("\tCache hits: %d\n", cache->hits);
        printf("\tCache misses: %d\n", cache->misses);
        printf("\tCache total access: %d\n\n", cache_total);

        printf("\tCache hit ratio: %2.2f%%\n", ((float) (cache->hits) / (float) (cache_total) ) * 100);
        printf("\tCache miss ratio: %2.2f%%\n\n", ((float) (cache->misses) / (float) (cache_total) ) * 100);

        printf("\tMemory reads: %d\n", cache->reads);
        printf("\tMemory writes: %d\n\n", cache->writes);

        printf("\tCache evictions: %d\n\n", cache->evictions);

    }
}
