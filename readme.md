# ECE 586 - CacheSim Project    
	
## Description:
This project simulates a single-level blocking cache using a trace file. The cache is assumed to be fixed size, allocate-on-write, and write-back.

`Usage: ./CacheSim <trace file> [-v] [-t] [-d]`

`[-v]` will include program version information in the output.
`[-t]` will include information about the trace accesses in the output (r/w, tag, offset, etc.)
`[-d]` will dump the final cache contents in the output (valid, dirty, tag, etc.)

Trace file must be specified immediately after program executable.
Debug commands can be in any order. For example:

```
./CacheSim "C:\folder\trace.txt"
./CacheSim "C:\folder\trace.txt" -t -v
./CacheSim "C:\folder\trace.txt" -v -d -t
```

## Structure & Compiling:
The project is structured as follows:      
	src/
	    * CacheSim.c
	    * CacheSim.h
	traces/
	    * trace_0.txt
	    * trace_1.txt
	    * trace_2.txt
	    * trace_3.txt
	    * trace_4.txt
	    * trace_5.txt
	    * trace_6.txt
	    * trace_7.txt
	    * trace_8.txt
	    * trace_9.txt
	    * trace_10.txt
	output/
	    * trace_0_output.txt
	    * trace_1_output.txt
	    * trace_2_output.txt
	    * trace_3_output.txt
	    * trace_4_output.txt
	    * trace_5_output.txt
	    * trace_6_output.txt
	    * trace_7_output.txt
	    * trace_8_output.txt
	    * trace_9_output.txt
	    * trace_10_output.txt

## Design & Implementation:
The main algorithm was the following:
    1. Validate input arguments   
    2. Open the trace file for reading   
    3. Create a new Cache object
    4. Read a line from the file
    5. Parse the line and read or write accordingly
    6. If the line is "#eof" continue, otherwise go back to step 4
    7. Print the results
    8. Destroy the Cache object
    9. Close the file   

The functions were separated into three main groups: the main function, cache functions, and utility functions. The main function executed the algorithm listed above. The utility functions were used to convert hexadecimal memory addresses to various binary and decimal equivalents.     

The five main cache functions were: 

    1) createCache
    2) destroyCache
    3) readFromCache
    4) writeToCache
    5) printCache