#ifndef __DEFINES_H__
#define __DEFINES_H__

//Onur added
#define CLINE_CTRL_SIZE 8
#define CLINE_REQUEST_SIZE 128   //in bytes // FIXXX this should not be hardcoded
#define CLINE_RESPONSE_SIZE 64   //in bytes // FIXXX this should not be hardcoded


#define MILLION 1000000
#define KILO 1024
#define MEGA KILO*KILO
#define GIGA MEGA*KILO

#define MAX_CORES 128
#define MAX_LLC_BANKS 128
#define CORES_LOG FloorLog2(CORES)
#define ADDR_SIZE 36
#define FLC 0
#define MLC 1
#define LLC 2
#define TRUE 1
#define FALSE 0
#define BLOCKED -1
#define INFINITE_CYCLE 100000
#define MEM 400   //Just some adhoc huge number. It was 8. Changed to 400 by Nachi
#define MAX_TR 8000
#define TRACE_BUFFER_SIZE 10000 
#define SOFTWARE_BKPT 0
#define SC_INTEG 0
#define SIMICS_INTEG 0
#define SHORT_CIRCUIT 0
#define BYPASS_MEM_BUS 1
#define CLOCK_FREQ_SCALE 1LL
#define SHORT_CIRCUIT_ICN_LAT (short_circuit_icn_latency)
#define OOO_COMMIT_WINDOW 3

// EP statistics 
#define EP_STAT 0
#define EP_CLUSTER_SIZE 4
#define EP_NODES 16
#define MAX_EP_COUNT_RANGE 8
#define MAX_EP_TIME_RANGE 500
#define MAX_EP_COUNT_RANGE 8
#define MAX_EP_TIME_RANGE 500
#define EP_STAT_INTERVAL 1000 // in hundreds of cycles
#define EP_START_REC_INT 0 
#define EP_END_REC_INT 9000 
#define MAX_OUTSTANDING_REFS 64

// Compression defines
//#define COMPRESS_CSIM 0
//#define COMPRESS_NOX 0
#define MAX_SET_SEGMENTS (assoc*linesize)/SET_SEG_SIZE 
//FIXME change to 4 for compression simulations code is hardcoded for 128 byte
//linesize
#define SET_SEG_SIZE 8 
#define DECOMPRESSION_LATENCY 1
#define COMPRESSION_LATENCY 2
#define COMPACT_DELAY 0 
#define CADDR_SIZE 1

#define FLIT_WIDTH 64
#define FLIT_SIZE_BYTES (FLIT_WIDTH/8)

#define CLINE_SIZE_BYTES (64)

#define CLINE_SIZE (((CLINE_SIZE_BYTES)*8)/FLIT_WIDTH)


#define DECODE_BANK(addr, cpuid) (((((addr) >> bank_shift) & (bank_mask)) % llc_bank_map[(cpuid)]) + llc_bank_start[(cpuid)])
//#define DECODE_BANK(addr, cpuid)    (((addr) >> bank_shift) & bank_mask)

#define DECODE_MEM_CTRLR(addr, cpuid) (int)((((addr) >> mem_controller_shift) & (mem_controller_mask)) % num_mem_controllers)

//#define DECODE_SET(addr) (level == MLC ? ((addr >> set_shift) & set_mask) : ((((addr & addr1_mask)>>addr1_shift | ((addr & addr2_mask) >> addr2_shift)) >> set_shift) & set_mask))
#define DECODE_SET(addr) ((addr >> set_shift) & set_mask)

#define SERVER_TIME 6

#define MEMORY_BYPASS 1
#endif 
