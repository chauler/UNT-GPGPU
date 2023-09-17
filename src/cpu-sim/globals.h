#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include "cache.h"
#include "cpu_engine.h"
#include "processor.h"
#include "defines.h"
#include <string>

#define MAX_COLS    8
#define MAX_ROWS    8 
#define MAX_NODES   (MAX_COLS * MAX_ROWS)


#if SIMICS_INTEG
extern long long last_posted_event_cycle; 
#endif

/* Simulation Globals */
extern int  MEM_SHORT_CIRCUIT;
extern int DEBUG_CPU;
extern int INTERACTIVE;
extern int STAT;
extern int PRINT;
extern int RECORD;
extern int RECORD_NOC;
extern int EP_RECORD_NOC;
extern int CACHE_QOS;
extern long long pri_occ[128];
extern long long pri_occ_limit[128];
extern int priority_levels;
extern int priority_occ_limit[128]; 
extern int llc_shared;
extern int thread_per_proc;
extern int numcpus;
extern int num_llc_banks;
extern int llc_bank_map[MAX_CORES];
extern int llc_bank_power_2;
extern int llc_bank_start[MAX_CORES];
extern int numclevels;
extern float core_raw_cpi;
extern int cpus_per_node;

extern int rob_head_ptr;
extern int rob_tail_ptr;

extern int core_transfer_time;
extern int flc_lookup_time;
extern int mlc_lookup_time;
extern int hop_time;
extern int llc_transfer_time;
extern int llc_lookup_time;
extern int memory_access_time;
extern int mem_transfer_time;
extern int llc_victim_lookup_time;
extern int mlc_victim_lookup_time;
extern long long  transaction_count;
extern long long tr_commited ;
extern int threads_per_proc; 

extern int cache_bq_size[3] ;
extern int cache_rq_size[3] ;
extern int nic_queue_size;

extern char output_file_name[100];
extern int  output_file_defined;

extern std::vector<std::string> trace_files;
extern std::vector<CLEVEL> clevel;
extern std::vector<TRANSTYPE> tr_pool;
extern std::vector<nic_entry_t> nic_queue[64][64];
extern CACHE *MEMORY;

extern long long cpu_global_clock;
extern long long cpu_global_clock_start;
extern long long sim_start_clock;

extern gzFile multi_trace_fp;
extern gzFile out_fp;
extern gzFile rec_noc_fp;

//extern int tag_shift;
extern int bank_shift;
extern long long bank_mask;

/* Stat Globals */

extern long long stat_interval;
extern long long per_core_icn_msgs[128];
extern long long per_core_icn_lat[128];

extern int cpu_network_layout[128];
extern int cache_network_layout[128];
extern int inverse_cpu_network_layout[128];
extern int inverse_cache_network_layout[128];
extern int mem_network_layout[128];
extern int cache_network_layout[128];
extern int cmp_network_nodes;
extern long long total_icn_latency;
extern long long short_circuit_icn_latency;
extern long long ipc_print_freq;
extern bool icn_event_flag;
extern bool icn_func_sim[128];
extern long long total_icn_msgs;
extern long long mlc_busy_time;
extern long long llc_busy_time;
extern long long reads;
extern long long writes;
extern long long mlc_queue_wait;
extern long long mem_queue_wait[128];
extern long long icnq_wait;
extern long long llc_queue_wait;
extern long long snoop_wait;
extern long long mlc_hits, mlc_misses; 
extern long long conflict_pri_misses;
extern long long total_resp_time;
extern long long llc_hits, llc_misses;
extern long long misses_per_core[128];
extern float mlc_occ, llc_occ;

extern unsigned refs[128];
extern unsigned commit_refs[128];
extern bool thread_done[128];
extern bool end_sim;
extern long long csim_inj_msgs;
extern long long total_csim_nic_qdelay;
extern long long compress_count;
extern long long compress_linesize;
extern long long pattern_zero;
extern long long pattern_4b_se;
extern long long pattern_8b_se;
extern long long pattern_16b_se;
extern long long pattern_16b_padded;
extern long long pattern_16b_se_half;
extern long long pattern_8b_rep;
extern long long pattern_32b;

extern int llc_linesize;
extern int COMPRESS_NOX;
extern int COMPRESS_CSIM;

extern long long addr_msgs;
extern long long data_msgs;
extern long long wb_msgs;
extern long long snoop_msgs;

extern FILE *fp_dbg;

extern std::string noc_file;
extern long long insts_executed;
extern bool flag;
extern double countj, jump;

extern long long set_compacts;
extern long long set_tot_dead_times;
extern long long bank_compacts;
extern long long bank_tot_compact_delay;

extern char mp_trace_file_name[128][200];
extern bool pepsi_end;
extern processor_t processor[128];
extern int finished_processors[MAX_PASS];
extern char benchmark[200];

//Memory Controllers through Interconnect
extern long long mem_controller_mask;
extern int mem_controller_shift;
extern int num_mem_controllers;

extern long max_physical_pages_per_bank;
extern int bank_map;
extern int bank_proximity[128][128];

extern bool profile_run;
extern long long addr1_mask, addr2_mask;
extern int addr1_shift, addr2_shift;

// Onur added
extern bool start_gpu_execution;
extern bool cpu_max_insn_struck;
extern unsigned long long gpu_sim_cycle_start;

// Onur added
extern unsigned long long newflits;
extern unsigned long long deletedflits;

// Onur added
extern long long unsigned cpu_instructions_to_simulate;

//UNT added
extern unsigned long long totsimcycle;
extern unsigned long long simcycle;
extern int _cpu_vcs;

extern double p_buffer;
extern double p_routing;
extern double p_vc_arb;
extern double p_sw_arb;
extern double p_xbar;
extern double p_link;
extern double p_retrans;
extern double p_static;
extern double p_photonic;
extern double p_tot;
extern bool coRun;

#endif
