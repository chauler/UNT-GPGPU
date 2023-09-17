#ifndef __INSTRUCTION_WINDOW_H__
#define __INSTRUCTION_WINDOW_H__

#include <map>
#include <set>

//For MemFetch
#include "../gpgpu-sim/mem_fetch.h"

using namespace std;

void print_table();

#define MAX_INSTRUCTION_WINDOW_SIZE 512
#define MAX_TRACE_SIZE 1000
#define MAX_PASS_SUPPORTED 100
#define MAX_PASS 10000

#define CDLY_TYPE  1
#define READ_TYPE  2
#define WRITE_TYPE 3
#define NET_TYPE   4
#define MNET_TYPE  5

#define INTEL_TRACE_FORMAT 1
#define MSR_TRACE_FORMAT   2
#define PEPSI_TRACE_FORMAT 3
#define NOP_TRACE_FORMAT   4

#define MAX_MONITOR_INTERVALS 4
#define MONITOR_INTERVAL_LENGTH 1024
#define UNFAIRNESS_THRESHOLD 2000
#define BUFF_OCCUPANCY_INTERVAL 32
#define BASE_HOT_ROUTER_OCC_THRESHOLD 0.9


typedef struct {
  bool ready;
  int  type;
  long long icount;
  long long address;
  long long tr_id;
  long long min_retire_timestamp;   //The clock should reach at least this value for this instruction to be committed
  int oldest;
  int network_stall;
  
  short l1miss;
  short l2miss;
  short ot_packets;
  short ot_instructions;
  short hops;
 
  long long enter_time;
  long long measure_slack;
  

} reorder_buffer_t;

 void getAvgStats(int);
//bool compare_hash()

typedef struct  {
  public:
    int type;
    int cycles;
    long long address;
    short src;
    short dst;
    short l1miss;
    short l2miss;
    short l1wbx;
    short l2wbx;

} trace_buffer_t;

class processor_t {
  public :

    int procID;

    int window_size;
    int issue_width;
    int commit_width;
    int execution_width;

    int trace_format;
    char app_name[30];
    bool network_trace_mode;

    //points to the first empty slot in reorder_buffer
    int next;
    //points to the oldest instruction in reorder_buffer
    int oldest;
    //active entries in the reorder_buffer
    int num_entrys;
    int num_mem_entrys;
    int num_unfinished_mem_entrys;
    //total instruction issued so far
    long long instructions;
    int pass;
    //total instruction commited far
    long long retired_instructions;
    long long retired_instructions_after_warmup;
    long long mem_op_id;


    //trace handlers
    gzFile mp_trace_fp;
    gzFile record_trace_fp;
    int FastForwardPoint; 
    FILE *oracle_fp;
    trace_buffer_t mp_trace_buffer[MAX_TRACE_SIZE];
    int trace_next;

    reorder_buffer_t rob_array[MAX_INSTRUCTION_WINDOW_SIZE];
    //Stats
    int sample_interval;
    double sum_rob_occupancy;
    double sum_ot_occupancy;
    double sum_sjf_values;
    long long sum_slack; //ASIT
    double bank_spread;
    double bank_spread_counter;
    int bank_touch[64];

    // Onur windows stats
    unsigned long long last_gpu_sim_cycle;

    //Nachi's variables being used
    double l1misses;
    double l1accesses;
    double l2misses;
    double l2accesses;
    double mshr_hits;

    double l1misses_last_pass;
    double prior_l1misses;
    double l2misses_last_pass;
    double prior_l2misses;
    //double current_outstanding_packets;

    double IPC[MAX_PASS];
    double nspi[MAX_PASS];
    double nast[MAX_PASS];
    double curr_nast[MAX_PASS];
    double ast[MAX_PASS];
    double ast_alone[MAX_PASS];
    double nast_alone[MAX_PASS];
    double average_packet_latency;
    double thpt[MAX_PASS];
    double curr_thpt[MAX_PASS];
    double request_packets[MAX_PASS];
    double dram_request_packets[MAX_PASS];
    long long clock_last_pass;
    int current_l1misses_in_rob;    
    int max_concurrent_l1misses_in_rob;    
    int current_l2misses_in_rob;    
    int max_concurrent_l2misses_in_rob;    
    double sum_l1misses_in_rob;
    double sum_l2misses_in_rob;
    
    //for phase stats:Asit
    double l1misses_phase_init;
    double l1misses_phase_end;
    long long network_phase_init;
    long long network_phase_end;
    long long network_phase;
    double total_l1misses;
    long long computation_phase;
    long long total_network_length;
    long long total_network_height;
    long long total_computation_phase;
    long long number_of_network_phase;
    long long number_of_network_phase_this_phase;
    float total_avg_network_height_per_network_length;
    float running_avg_height_per_net_len;
    float phase_running_avg_height_per_net_len;
    
    float running_avg_episode_height;
    float phase_running_avg_episode_height;
   
    int episode_cumulative_height;
    long long episode_length;
    long long total_episode_length;
    
    int comitted_mem_instr;
    int comitted_l1misses; 
    int comitted_l2misses; 
    
    //Profile Alone Stats
    double  peak_packets;
    double  cumulative_packets; 
    double  cumulative_nast;
    double  cumulative_ncpi;
    double  cumulative_rate;
    double  peak_rate;
    //Network Stats
    double hop_count;
    double packets;
    double packets_prior;
    long long packet_latency;
    long long blocking_cycles;
    long long buff_blocking_cycles;
    long long zero_load_cycles;
    long long priority_inversion_latency;
    double per_flow_packets[128];
    double local_packets;
    double ctrl_packet_latency;
    double ctrl_packets;
    double data_packet_latency;
    double data_packets;
  
    //Oracle from single app runs
    double oracleIPC[MAX_PASS];
    double oracleNast[MAX_PASS];
    double oracleNastCurrent[MAX_PASS];
    double oracleAst[MAX_PASS];
    double oracleThpt[MAX_PASS];
    double oracleThptCurrent[MAX_PASS];

    //Stall Stats
    double network_stall_cycles;
    double network_stall_cycles_last_pass;
    double memory_stall_cycles;
    double stall_alone;
    double network_stall_alone;
    
    double network_stall_cycles_prior;
    double memory_stall_cycles_prior;
    double stall_alone_prior;
     
    double memory_instructions;

    bool throttle_mode;
    
    vector<int> history_nast;

    set<unsigned long long> ot_MemRefs;
    //Tmp
    double sumlatency;
    

    //Network Interfact Daa
    std::list<mem_fetch*> cpu_response_fifo;

    //1. Polling
    bool isEmpty();
    bool isFull();
    bool isOldestReady();
    bool isOldestStallingNetwork();
    bool isPresent(long long address, long long id);
    bool isOutStanding(long long address, int index);
    //1.1 Get info
    int get_entry_id(long long address, int index);
    
    //2. Basic functions processor pipeline 
    void setReady(int entry, long long id);
    void doCycle();
    void retire();
    void fetch();
    void executeNonMemoryInstructions();
    void executeMemoryInstructions();

    //3. Trace reading
    void insertMemTransaction();
    void insertNetTransaction();
    void insertMemNetTransaction();
    void populateTraceBuffer();
    long long memory_map(long long vaddr);

    //4. Init
    void initProcessor(char *fname, int proc_id, int size, int iwidth, int cwidth, int ewidth);
   
    //5. Stall updates and stats
    void updateStallEstimate(int entry);
//    double returnSlowDownThisInterval();
//    double returnStallThisInterval();
//    double returnStallSharedThisInterval();
    int returnSlackPriority(int entry, bool l2miss);
//    double returnPacketsThisInterval();
//    double returnThptSlowdown(long cycles);
    
    //6. Profile and Warmup
    bool profile();
    void fastforward(char*);
    void NetworkTracer(long,long);
    void Reset();
    void ResetStats();
    
    //7. Stats
    void getStats(int);
    void getNetworkStats(int);
    void printOracleStats(int pass,int mp);
    void readOracleStats();
    void getNetworkCommunicationMatrix();
    void printCumulativeStats(int);

    //8. Network Interface
    void cpu_icnt_cycle();

};

bool mlc_transfer_overflow(int cpuid);
//All processor Stats
void profile_stats();
void getIPCMatrix(int pass);
void getStallMatrix(int pass);
void getNetworkStatsMatrix(int pass);
void monitorUnfairness();
void UpdatePerClassBuffOccupancy();
void UpdatePerNodeBuffOccupancy();
float AvgPerClassOccupany(int i);
double AvgPerNodeOccupancy(int i);
void ResetPerClassBuffOccupany();
void ResetPerNodeBuffOccupany();
double std_dev(double values[64], int num_values);
void findUniqueApps();
void getAppStats(int pass);
void getInstProgress();
#endif
