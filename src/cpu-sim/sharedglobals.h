#ifndef SHAREDGLOBALS_H
#define SHAREDGLOBALS_H

#if SIMICS_INTEG
long long last_posted_event_cycle=0; 
#endif

FILE * fp_dbg;

int  MEM_SHORT_CIRCUIT=0;
long long  cpu_global_clock=0;
long long cpu_global_clock_start=0;
int instruction_passes=0;

/* END of TRACE file indicator */
bool trace_end= false;
long long refs_read = 0;
/* TRACE FILE GLOBALS */
#if SC_INTEG
buffer_t buffer[TRACE_BUFFER_SIZE];
#else
char buffer[TRACE_BUFFER_SIZE][200];
#endif

unsigned refs[128];
unsigned commit_refs[128];
bool thread_done[128];
bool end_sim = false;

int store_index=0, end = TRACE_BUFFER_SIZE ,commit_index = 0;
int buffer_front=0, buffer_rear=0;
bool flag = true;
long long last_timestamp=0;

/* Simulation Globals and their defaults */
int DEBUG_CPU = 1;
int PRINT = 0;
int INTERACTIVE = 0;
int STAT = 0;
int RECORD = 0;
int EP_RECORD_NOC = 1;
int RECORD_NOC = 0;
int CACHE_QOS = 1;
int priority_levels=4;
int priority_occ_limit[128]; 
long long pri_occ[128];
long long pri_occ_limit[128];
int thread_per_proc = 4; 
int llc_shared = 1;
int numcpus = 4;
int num_llc_banks =4;
int llc_bank_map[MAX_CORES];
int llc_bank_power_2 = 32;
int llc_bank_start[MAX_CORES];
int numclevels = 2;
int cpus_per_node = 4;
float core_raw_cpi = 1;

int core_transfer_time = 7;
int mem_transfer_time = 300;
int flc_lookup_time = 1;
int mlc_lookup_time = 6;
int hop_time = 2;
int llc_transfer_time = 14;
int llc_lookup_time = 2;
int memory_access_time = 4;
int llc_victim_lookup_time = 4;
int mlc_victim_lookup_time = 4;

int cache_bq_size[3] = {8,16,512};
int cache_rq_size[3] = {8,16,512};
int nic_queue_size=64;
//int tag_shift;
int bank_shift;
long long bank_mask;

long long transaction_count = 0 ;
long long insts_executed = 0;
long long tr_commited = 0;
int threads_per_proc = 4; 

char output_file_name[150];
int  output_file_defined = 0;

std::vector<std::string> trace_files;
std::string noc_file;
std::vector<CLEVEL> clevel;
std::vector<TRANSTYPE> tr_pool;
std::vector<nic_entry_t> nic_queue[64][64];
CACHE *MEMORY;

gzFile multi_trace_fp;
gzFile out_fp;
gzFile rec_noc_fp;

double countj=0, jump = 0;

/* Stat Globals */


long long stat_interval=1000000;
long long per_core_icn_msgs[128];
long long per_core_icn_lat[128];


int cpu_network_layout[128] = {0};
int cache_network_layout[128] = {0};
int inverse_cpu_network_layout[128] = {0};
int inverse_cache_network_layout[128] = {0};
int mem_network_layout[128] = {0};
int cmp_network_nodes = 16;
long long total_icn_latency = 0;
long long short_circuit_icn_latency=30;
long long ipc_print_freq=1000;
bool icn_event_flag=false;
bool icn_func_sim[128]={false};
long long total_icn_msgs = 0;
long long mlc_busy_time=0;
long long llc_busy_time=0;
long long reads = 0;
long long writes = 0;
long long total_resp_time = 0;
long long mlc_queue_wait = 0;
long long mem_queue_wait[128];
long long llc_queue_wait = 0;
long long snoop_wait = 0;
unsigned long long mlc_hits=0, mlc_misses=0; 
unsigned long long llc_hits=0, llc_misses=0;
long long llc_misses_per_core[128];
long long llc_refs_per_core[128];
long long misses_per_core[128];
float mlc_occ=0, llc_occ=0;
long long conflict_pri_misses = 0;
long long addr_msgs = 0;
long long data_msgs = 0;
long long wb_msgs = 0;
long long snoop_msgs = 0;


int COMPRESS_NOX=0;
int COMPRESS_CSIM=0;

//Compression Ratio Stats
long long compress_count = 0;
long long compress_linesize = 0;
long long pattern_zero = 0;
long long pattern_4b_se = 0;
long long pattern_8b_se = 0;
long long pattern_16b_se = 0;
long long pattern_16b_padded = 0;
long long pattern_16b_se_half = 0;
long long pattern_8b_rep = 0;
long long pattern_32b=0;

int llc_linesize = 0;

long long set_compacts=0;
long long set_tot_dead_times=0;
long long bank_compacts=0;
long long bank_tot_compact_delay=0;

// NOC INTEG
long long sim_clock;
int verbose;
unsigned int num_ejt_msg, num_inj_msg;
void sim_result();
int sim_init(int argc, char *argv[]);
int nox_sim(long long);

long long csim_inj_msgs=0;
long long total_csim_nic_qdelay=0;

char mp_trace_file_name[128][200];
bool pepsi_end;
processor_t processor[128];
int finished_processors[MAX_PASS];
char benchmark[200];

//Memory Controllers through Interconnect

long long mem_controller_mask=3;
int mem_controller_shift=30;
int num_mem_controllers=4;

long max_physical_pages_per_bank=0;
int bank_map = CACHE_LINE_INTERLEAVING;
int bank_proximity[128][128];

// Onur added
bool start_gpu_execution = false;
bool cpu_max_insn_struck = false;
unsigned long long gpu_sim_cycle_start = 0;

// Onur added
unsigned long long newflits = 0;
unsigned long long deletedflits = 0;

// Onur added
long long unsigned cpu_instructions_to_simulate = 10000;

//UNT added
unsigned long long totsimcycle = 0;
unsigned long long simcycle = 0;
int _cpu_vcs = 2;

//Energy
double p_buffer = 0.0;
double p_routing = 0.0;
double p_vc_arb = 0.0;
double p_sw_arb = 0.0;
double p_xbar = 0.0;
double p_link = 0.0;
double p_retrans = 0.0;
double p_static = 8.8015;
double p_photonic = 0.0;
double p_tot = 0.0;
bool coRun = false;

#endif
