/*****************************************************************************
 * File:        processor simulation
 * Description: This file consists of several functions that comprise the
 *              processor simulation core
 *
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <zlib.h>
#include <float.h>
#include <vector>
#include <algorithm>
#include <iomanip>

#include <math.h>

#include "processor.h"
#include "transaction.h"
#include "cache.h"
#include "pepsi_stats.h"
#include "globals.h"

//For Interconnect
#include "../intersim/interconnect_interface.h"
#include "../gpgpu-sim/icnt_wrapper.h"

#include "../gpgpu-sim/gpu-sim.h"
#include "../gpgpusim_entrypoint.h"


extern gpgpu_sim *g_the_gpu;
extern unsigned int gpu_stall_icnt2sh;
extern long long unsigned cpu_warmup_inst;
extern long long unsigned min_cpu_core_inst;

//CPU Stuff
int num_priority_levels = 8;
int arch = 2;

#define DBG_PROC -1
#define CPU_DEBUG 0

#define LOCALITY_AWARE_BANK_MAP 0
#define MAX_PAGES 400000
//virtual memory management
unsigned  physical_pages_touched[128];
double    page_profile[MAX_PAGES][128];
double    page_profile_sum[MAX_PAGES];
short     first_touch[MAX_PAGES];
unsigned  physical_pages_touched_profile;
unsigned  num_llc_bank_log2=4;
map<long long,long long> page_table;
FILE *pt_fp;

char UniqueApps[MAX_NODES][80];
int numApps = 0;
bool flc_transfer_overflow(int cpuid);

void processor_t::cpu_icnt_cycle()
{
    if( !cpu_response_fifo.empty() ) {
        mem_fetch *mf = cpu_response_fifo.front();

		// data response
        //FIXXX :  (Important) The transaction (in tr_pool) corresponding to this data has to be committed and,
        //the corresponding ROB entry has to be set to ready. Else, the core will be stalling.

        TRANSTYPE *tr;
        tr = map_transaction(mf->get_wid());
        tr->timestamp = cpu_global_clock+1;  //insert into cache next clock cycle
        tr->state = LLC_TO_CORE;

        cpu_response_fifo.pop_front();
	}

    if( cpu_response_fifo.size() < 8 ) {  //FIXXX : parameterize the number of packets that can be committed (to cpu ejection buffer)
        mem_fetch *mf = (mem_fetch*) ::icnt_pop(cpu2device(procID));
        if (!mf) {
            return;
        }

//        printf("CPU mf type enum value is: %d \n", mf->get_type()); // Type is 3. = WRITE_ACK
//        if(mf->get_type() == 1 || mf->get_type() == 3 || mf->get_type() == 6 || mf->get_type() || 7)
//        {
//        	printf("CPU size : %d \n", mf->size());
//            printf("CPU data size : %d \n", mf->get_data_size());
//            printf("CPU ctrl size : %d \n", mf->get_ctrl_size());
//        }


        // WRITE_ACK assignment is done using set_reply function. It is coming from dram.cc, void dram_t::cycle()
        // This means it was WRITE_REQUEST before.
        // It is assigned here:  m_type = m_access.is_write()?WRITE_REQUEST:READ_REQUEST;
        assert(mf->get_tpc() == cpu2device(procID));
        assert(mf->get_type() == CPU_READ_REPLY || mf->get_type() == CPU_WRITE_REPLY);
        mf->set_status(IN_ICNT_TO_SHADER,gpu_sim_cycle+gpu_tot_sim_cycle);
        cpu_response_fifo.push_back(mf);
    }
    else
    {
    	printf("\n\n\n\n\n\n\n\n\n\t\t  ONUR: WHAT IS THIS ??????????????  "
    			"NACHI: This is a computer."
    			"ONUR: WHAT? "
    			"NACHI: :D :P"
    			"ONUR: JOOMLAAA! **&#$&-JOOMLA!!"
    			"NACHI: WHAT?"
    			"ONUR: :D :P"
    			"\n\n\n\n\n\n\n\n\n\n\n");
    }
	return;
}


  void 
processor_t::initProcessor(char *fname, int proc_id, int size, int iwidth, int cwidth, int ewidth )
{

  procID          = proc_id;
  window_size     = size;
  issue_width     = iwidth;
  commit_width    = cwidth;
  execution_width = ewidth;

  if(trace_format != NOP_TRACE_FORMAT)
  {
    mp_trace_fp     = gzopen(fname, "rb");
    if(mp_trace_fp == NULL) 
    {
      printf("ERROR: Could not open trace file %s for processor %d!\n",fname,procID);
      fflush(stdout);
      exit(1);
    }
  }

  network_trace_mode = false;

  next         = 0;
  oldest       = 0;
  num_entrys   = 0;
  num_mem_entrys   = 0;
  num_unfinished_mem_entrys =0;
  instructions = 0;
  pass = 0;
  memory_instructions = 1;
  packets   = 1;
  packets_prior = 1;
  local_packets   = 1;

  memory_stall_cycles = 0;
  network_stall_cycles = 0;
  stall_alone = 0;

  memory_stall_cycles_prior = 0;
  network_stall_cycles_prior = 0;
  stall_alone_prior = 0;

  blocking_cycles = 0;
  zero_load_cycles = 0;
  packet_latency = 0;
  sumlatency = 0;
  buff_blocking_cycles = 0;

  ctrl_packet_latency=0;
  ctrl_packets=0;
  data_packet_latency=0;
  data_packets=0;

  last_gpu_sim_cycle = 0;

  l1misses = 0;
  l2misses = 0;
  prior_l1misses = 0;
  prior_l2misses = 0;
  //current_outstanding_packets =0;
  current_l1misses_in_rob =0;
  //max_concurrent_l1misses_in_rob =0;
  current_l2misses_in_rob =0;
  //max_concurrent_l2misses_in_rob =0;

  l1misses_phase_init = 0;
  l1misses_phase_end  = 0;
  network_phase_init  = 0;
  network_phase_end   = 0;
  network_phase       = 0;
  total_l1misses      = 0;
  computation_phase   = 0;
  total_network_length= 0;
  total_network_height= 0;
  total_computation_phase = 0;
  number_of_network_phase = 0;
  number_of_network_phase_this_phase = 0;
  total_avg_network_height_per_network_length = 0;
  running_avg_height_per_net_len = 0;
  phase_running_avg_height_per_net_len = 0;
 
  
  running_avg_episode_height = 0;
  phase_running_avg_episode_height = 0;
  episode_cumulative_height  = 0;
  episode_length = 0;
  
  comitted_mem_instr  = 0;
  comitted_l1misses   = 0;
  comitted_l2misses   = 0;
  
  clock_last_pass =0;

  mem_op_id    = 0;

  populateTraceBuffer();

  //stats
  sample_interval= 100050; //1000500; ASIT
  sum_rob_occupancy=0;
  sum_ot_occupancy=0;
  sum_slack=0;
  sum_sjf_values=0;
  
  sum_l1misses_in_rob  = 0;
  sum_l2misses_in_rob  = 0;

  physical_pages_touched[procID]=0;
  num_llc_bank_log2 = FloorLog2(num_llc_banks-1);
  for(int i=0; i< num_llc_banks; i++)
    per_flow_packets[i]=0;

  for(int i=0; i< MAX_PASS; i++)
  {
    IPC[i] = 0;
    finished_processors[i] = 0;
    nspi[i] = 0;
    nast[i] = 0;
    ast[i] = 0;
    ast_alone[i]=0;
    nast_alone[i]=0;
  }

  history_nast.reserve(window_size);
  readOracleStats();

  throttle_mode = false;

  //if(procID == numcpus - 1)
  printf("InitProcessor %d complete :) %s reorder:%d issue_width:%d commit_width:%d execution_width:%d bank bits:%d\n",
      procID,fname,window_size,issue_width,commit_width,execution_width,num_llc_bank_log2);
}

  bool
processor_t::isEmpty()
{
  return (num_entrys == 0); 
}

  bool
processor_t::isFull()
{
  return (num_entrys == window_size);
}

  bool
processor_t::isOldestReady()
{
  if(num_entrys == 0)
    return true;
  return (rob_array[oldest].ready);
}
  bool
processor_t::isOldestStallingNetwork()
{
  TRANSTYPE *tr;
  tr = map_transaction(rob_array[oldest].tr_id);
  if(tr->state == LLC_ICN_OVERFLOW || tr->state == LLC_ICN_TRANSFER
      || /*tr->state == MEM_ICN_OVERFLOW ||*/ tr->state == MEM_ICN_TRANSFER
      || (tr->type == NETWORK && tr->timestamp == BLOCKED)
      || (tr->type == SIMPLE && tr->timestamp == BLOCKED))
  {
    rob_array[oldest].network_stall++;
    return true;
  }

  return false;
}

  bool
processor_t::isPresent(long long address, long long id)
{
  for(int i=0; i<num_entrys; i++)
  {
    if(rob_array[(oldest+i) % window_size].address == address && 
        rob_array[(oldest+i) % window_size].tr_id  == id )
      return true;
  }
  return false;
}

  int
processor_t::get_entry_id(long long address, int id)
{
  for(int i=0; i<num_entrys; i++)
  {
    if(rob_array[(oldest+i) % window_size].address == address &&
        rob_array[(oldest+i) % window_size].tr_id  == id )
      return ((oldest+i) % window_size);
  }
  return -1;
}

  bool
processor_t::isOutStanding(long long address, int index)
{
  for(int i=0; i<num_entrys; i++)
  {
    if(rob_array[(oldest+i) % window_size].address == address && ((oldest+i) % window_size) != index) 
      return true;
  }
  return false;
}

  int
processor_t::returnSlackPriority(int entry, bool l2miss)
{
  //int slack = (int)((current_outstanding_packets - 1)* average_packet_latency);
  //printf("current_outstanding_packets%f\n",current_outstanding_packets);
  //return slack;
  return 0;
}
  void
processor_t::setReady(int entry, long long id)
{
  assert(entry < window_size && entry >=0);
  if(!(rob_array[entry].tr_id == id))
  {
    printf("ERR setReady() cannot find the reorder buffer entry for entry :%d rob id:%lld tr id:%lld proc:%d num_entrys:%d oldest:%d next:%d\n",
        entry,rob_array[entry].tr_id,id,procID,num_entrys,oldest,next);
    //print_tr(id);
    exit(1);
  }
/*  if(rob_array[entry].l1miss == 1)
    current_outstanding_packets--;*/
  rob_array[entry].ready = true;
  rob_array[entry].min_retire_timestamp = cpu_global_clock;
  ot_MemRefs.erase(rob_array[entry].address>>7);
  num_unfinished_mem_entrys--;
  return;
}

  void
processor_t::retire()
{
  long long trace_data[16]={0};
  for(int i=0; i<commit_width; i++)
  {
    if(isEmpty())
      break;

    if(rob_array[oldest].ready && cpu_global_clock >= rob_array[oldest].min_retire_timestamp)
    {
      retired_instructions++;
      if ( (cpu_global_clock > cpu_global_clock_start) && (cpu_global_clock_start != 0) )
    	  retired_instructions_after_warmup++;

//      if(CPU_DEBUG)
//   	  printf("Retiring instruction %d\n", retired_instructions);
//   	  printf("Curr_Clock=%lld Entertime=%d L1Miss=%d L2Miss=%d\n", cpu_global_clock,rob_array[oldest].enter_time,rob_array[oldest].l1miss,rob_array[oldest].l2miss);

      if(!(retired_instructions % cpu_instructions_to_simulate))
      {
        //PASS UPDATE
        finished_processors[pass]++;

        IPC[pass] = retired_instructions/(double)cpu_global_clock;
        nspi[pass] = network_stall_cycles/(double)retired_instructions;
        //not every l1 miss is a network transaction, and is a super set of l2
        //miss, every l2 miss also adds the stall time to the out standing l1
        //miss, so for calculating nast we use l1misses
        nast[pass] = network_stall_cycles/(double)l1misses;
        curr_nast[pass] = (network_stall_cycles-network_stall_cycles_last_pass)/(double)(l1misses-l1misses_last_pass);
        request_packets[pass] = (double)(l1misses-l1misses_last_pass);
        dram_request_packets[pass] = (double)(l2misses-l2misses_last_pass);
        ast[pass] = memory_stall_cycles/(double)l1misses;
        thpt[pass] = l1misses/(double)cpu_global_clock;
        curr_thpt[pass] = (l1misses-l1misses_last_pass)/(double)(cpu_global_clock-clock_last_pass);
        clock_last_pass = cpu_global_clock;
        ast_alone[pass] = stall_alone/(double)l1misses;
        nast_alone[pass] = network_stall_alone/(double)l1misses;
        network_stall_cycles_last_pass = network_stall_cycles;
        l1misses_last_pass = l1misses;
        l2misses_last_pass = l2misses;

        if(peak_packets < (l1misses+l2misses - cumulative_packets))
          peak_packets = l1misses+l2misses - cumulative_packets;
        cumulative_packets = l1misses+l2misses; 
        cumulative_nast = network_stall_cycles/cumulative_packets;
        cumulative_ncpi = network_stall_cycles/retired_instructions;
        cumulative_rate = cumulative_packets/retired_instructions;
        peak_rate = peak_packets/(double)cpu_instructions_to_simulate;


        pass = (pass + 1)%MAX_PASS;
        //Rewind logic
        if(pass % MAX_PASS_SUPPORTED == 0)
        {
          if(trace_format != NOP_TRACE_FORMAT)
          {
            gzseek(mp_trace_fp,FastForwardPoint,SEEK_SET);
            printf("Rewinding App to FastForwardPoint:%s pass:%d FastForwardPoint:%d\n",app_name,pass,FastForwardPoint);
          }
        }
        if(DEBUG_CPU)
          printf("finished procID:%d\n",procID);
      }

      num_entrys--;
      if(rob_array[oldest].type != CDLY_TYPE)
      {

    	  if(CPU_DEBUG)
        	printf("Retiring MEMORY instruction %d, of type : %d, Rob entry's TR_id : %d, oldest in rob = %d\n", comitted_mem_instr, rob_array[oldest].type, rob_array[oldest].tr_id, oldest);


    	num_mem_entrys--;
        //ASIT: added
        comitted_mem_instr++;
        if(rob_array[oldest].l1miss && !rob_array[oldest].l2miss)
           comitted_l1misses++;
        else if(rob_array[oldest].l1miss && rob_array[oldest].l2miss)
           comitted_l2misses++;
        
        updateStallEstimate(oldest);
        if(DEBUG_CPU)
          if(procID == 2)
            printf("retiring instruction:%lld type:%d next:%d old:%d num_entrys:%d trace_next:%d num_mem_entrys:%d\n",
                retired_instructions,rob_array[oldest].type,next,oldest,num_entrys,trace_next, num_mem_entrys);
        
        rob_array[oldest].l1miss=0;
        rob_array[oldest].l2miss=0;
        
        oldest = (oldest+1)%window_size;
        // insert here : ASIT check if not ready
        if(rob_array[oldest].type != CDLY_TYPE)
        {
          rob_array[oldest].measure_slack = (rob_array[oldest].ready)? 0 : cpu_global_clock - rob_array[oldest].enter_time;
          //rob_array[oldest].measure_slack = cpu_global_clock - rob_array[oldest].enter_time;
          //printf("procID:%d measure_slack%lld\n", procID, rob_array[oldest].measure_slack);
        }
        
        break; //because, only 1 mem instr can be commited
      }

      oldest = (oldest+1)%window_size;
       
      // insert here : ASIT check if not ready bit 
      if(rob_array[oldest].type != CDLY_TYPE)
      {
        rob_array[oldest].measure_slack = (rob_array[oldest].ready)? 0 : cpu_global_clock - rob_array[oldest].enter_time;
        //rob_array[oldest].measure_slack = cpu_global_clock - rob_array[oldest].enter_time;
        //printf("procID%d measure_slack%lld\n", procID, rob_array[oldest].measure_slack);
      }
    }
    else
    {
      rob_array[oldest].oldest++;
      //update network stall for the oldest instruction
      isOldestStallingNetwork();
      break;
    }
  }
  return;
}
  void
processor_t::printCumulativeStats(int pass)
{
  if(trace_format == NOP_TRACE_FORMAT)
    return;
  double l1mpki = 1000*(request_packets[pass])/(double)cpu_instructions_to_simulate;
  double l2mpki = 1000*(dram_request_packets[pass])/(double)cpu_instructions_to_simulate;
  printf("pass:%d:ncpi:%.4f:nst:%.4f:rate:%.4f:peak rate:%.4f:cpi:%.4f:l1mpki:%.2f:l2mpki:%.2f\n",pass,cumulative_ncpi,cumulative_nast,cumulative_rate,peak_rate,1/IPC[pass-1], l1mpki,l2mpki);
}

  void
processor_t::updateStallEstimate(int entry)
{
  //0. if was a l1 hit no stall, return
  if(rob_array[entry].hops == -1)
    return;

  int ot_instructions = rob_array[entry].ot_instructions; 
  int ot_packets = rob_array[entry].ot_packets; 
  long long trid = rob_array[entry].tr_id; 
  int hops = rob_array[entry].hops; 
  int l2miss = rob_array[entry].l2miss;


  int latency = rob_array[entry].enter_time;

  int slack =(int)(ot_instructions/(double)commit_width) + ot_packets;
  int stall = rob_array[entry].oldest; 
  int network_stall = rob_array[entry].network_stall; 
  int estimated_stall = 0;

  int network_latency = 0,instruction_latency = 0 ,cache_latency = 0, dram_latency=0; 

  //1. calculate latency alone
  if(rob_array[entry].hops != 0)
    network_latency = rob_array[entry].hops * (arch + 1) + (/*queueing*/2+ /*serialization*/8)*(1+ rob_array[entry].l2miss) +
      /*blocking latency*/ 
      rob_array[entry].hops * (ot_packets < 12 ? (ot_packets < 8 ? 0 : 2 ) : (ot_packets < 24 ? 4 : 8 ));
  cache_latency   = llc_lookup_time + mlc_lookup_time;  
  dram_latency    = rob_array[entry].l2miss * memory_access_time; 
  instruction_latency = network_latency + cache_latency + dram_latency;

  //printf("ot packets %d hist size %d\n",rob_array[entry].ot_packets,history_nast.size());
  //2. calculate slack alone
  //assert(ot_packets <= history_nast.size()); 
  if(ot_packets > history_nast.size())
    ot_packets = history_nast.size();
  for(int i=0,k=history_nast.size()-1; i<ot_packets; i++,k--)
    slack += history_nast[k];
  estimated_stall = instruction_latency - slack > 0 ? instruction_latency - slack : 0;
  history_nast.push_back(estimated_stall);
  //Asuming a maximum injection rate of 20% packets per instruction
  if(history_nast.size() > window_size)
    history_nast.erase(history_nast.begin());

  //3. update stall alone and stall shared
  stall_alone += estimated_stall + 1;
  memory_stall_cycles  += stall + 1;

  //4. update network stall alone and network stall shared
  // Lalone * (O-DL)/(L-DL) 
  if(hops > 0)
  {
    assert(latency > dram_latency);
    network_stall_alone += stall - dram_latency > 0 ? 
      network_latency * ((stall - dram_latency) / (double)(latency - dram_latency)) + 1
      : 1;
    sumlatency += latency - dram_latency;
    network_stall_cycles += network_stall + 1;
    zero_load_cycles += network_latency;
    /*if(procID == 0)
    {
      printf("Instruction:%lld Stall :%d trid:%lld hops:%d l2miss:%s\n",retired_instructions,network_stall+1,trid,hops,l2miss==1?"yes":"no");
      if(retired_instructions == 2)
        exit(1);
    }*/
  }
//if(estimated_stall > stall)
  /*printf("entry:%d stall:%d network stall:%d stall alone:%d latency(%d hops):%d slack:%d ot packets:%d actual latency:%d\n",entry,
    stall,network_stall,estimated_stall,
    rob_array[entry].hops,instruction_latency,slack,ot_packets,latency);
    */
  return;
}

  long long 
processor_t::memory_map(long long vaddr)
{
  //Page Management
  long long paddr, page_offset;
  long physical_page_number, virtual_page_number;

  virtual_page_number = vaddr>>12;
  page_offset = vaddr & ((long long)0xfff);

  if((physical_page_number = page_table[virtual_page_number]))
  {
    paddr = (physical_page_number << 12 ) | page_offset;
    int bankID = DECODE_BANK(paddr,procID);
    if(DEBUG_CPU)
      printf("hit on P%d --> C%d [%llx][%llx]\n",procID,bankID, vaddr,paddr);
  }
  else
  {
    for(int i=0; i< num_llc_banks; i++)
    {
      int bankID = bank_proximity[procID][i];
      if(physical_pages_touched[bankID] < max_physical_pages_per_bank)
      {
        physical_pages_touched[bankID]++;
        page_table[virtual_page_number] = (physical_pages_touched[bankID]<<num_llc_bank_log2) | bankID;
        paddr = (((physical_pages_touched[bankID]<<num_llc_bank_log2) | bankID) << 12) | page_offset;
        if(DEBUG_CPU)
          printf("assigning P%d --> C%d [%llx][%llx]\n",procID,bankID, vaddr,paddr);
        break;
      }
    }
  }
  if(DEBUG_CPU)
    //if(procID == 1 || procID == 2)
    printf("procID:%d vaddr:%llx paddr:%llx vpn:%lx ppn:%lx\n",procID,vaddr,paddr,virtual_page_number,physical_page_number);
  return paddr;
}
  void
processor_t::fetch()
{
  int i=0;
  long long trace_data[16]={0};
  //int tmpl1miss_entrys=0;
  
  while(i<issue_width)
  {
    if(isFull())
      break;

    if(mp_trace_buffer[trace_next].type == CDLY_TYPE)
    {
      instructions++;
      rob_array[next].type = CDLY_TYPE;
      rob_array[next].ready = false;
      rob_array[next].icount = instructions;
      rob_array[next].address = -1;
      rob_array[next].tr_id = -1;
      rob_array[next].oldest = 0;
      rob_array[next].hops = -1;
      rob_array[next].l1miss = 0;
      rob_array[next].l2miss = 0;
      rob_array[next].network_stall = 0;
      rob_array[next].ot_packets = num_unfinished_mem_entrys;
      rob_array[next].ot_instructions = num_entrys;
      rob_array[next].enter_time = cpu_global_clock;
      rob_array[next].min_retire_timestamp =  cpu_global_clock;

      if(DEBUG_CPU)
        if(procID == 2)
          printf("fetching instruction:%lld type:%d next:%d old:%d num_entrys:%d trace_next:%d\n",
              instructions,rob_array[next].type,next,oldest,num_entrys,trace_next);

      next = (next+1)%window_size;
      num_entrys++;
      i++;

      mp_trace_buffer[trace_next].cycles--;
      if(mp_trace_buffer[trace_next].cycles <= 0)
      {  
        trace_next++;
        if(trace_next == MAX_TRACE_SIZE)
          populateTraceBuffer();
      }
    }
    //Memory Instruction
    else
    {
      if(trace_format == NOP_TRACE_FORMAT)
      {
        printf("procID:%d traceformat:NOP shouldnt come here!!\n", procID);
        exit(1);
      }
      CACHE *cp; int cstate;
      cp = &(clevel[FLC].cache[procID]);
      // New Transaction but no place in the pool and L1 flow control
      if(tr_pool.size() > MAX_TR || flc_transfer_overflow(procID))//cp->queue_full())
        break;

      if(throttle_mode == true)
        break;

      instructions++;
      memory_instructions++;
      assert(mp_trace_buffer[trace_next].type > CDLY_TYPE && mp_trace_buffer[trace_next].type <= WRITE_TYPE);
      rob_array[next].type = mp_trace_buffer[trace_next].type;
      rob_array[next].ready = false;
      rob_array[next].icount = instructions;
      rob_array[next].ot_packets = num_unfinished_mem_entrys;
      rob_array[next].ot_instructions = num_entrys;
      rob_array[next].min_retire_timestamp = cpu_global_clock;

      if(LOCALITY_AWARE_BANK_MAP)
      {
        rob_array[next].address = memory_map(mp_trace_buffer[trace_next].address);
        mp_trace_buffer[trace_next].address = rob_array[next].address;
      }
      else
        rob_array[next].address = mp_trace_buffer[trace_next].address;

      rob_array[next].tr_id = -1;
      rob_array[next].oldest = 0;
      rob_array[next].network_stall = 0;
      rob_array[next].hops = -1;
      rob_array[next].l1miss = 0;
      rob_array[next].l2miss = 0;
      rob_array[next].enter_time = cpu_global_clock;
      rob_array[next].measure_slack = -1;

      num_entrys++;
      num_mem_entrys++;
      num_unfinished_mem_entrys++;

      if(DEBUG_CPU)
        if(procID == 2)
          printf("fetching instruction:%lld type:%d next:%d old:%d num_entrys:%d trace_next:%d num_mem_entrys:%d\n",
              instructions,rob_array[next].type,next,oldest,num_entrys,trace_next, num_mem_entrys);

      insertMemTransaction();
      next = (next+1)%window_size;
      i++;
      trace_next++;
      if(trace_next == MAX_TRACE_SIZE)
        populateTraceBuffer();
      break;
    }

  }
  return;
}

  void
processor_t::executeNonMemoryInstructions()
{
  for(int i=0; i<execution_width; i++)
  {
    if(rob_array[(oldest+i)%window_size].type == CDLY_TYPE)
      rob_array[(oldest+i)%window_size].ready = true;
    else
      break;
  }
  return;
}


  void
processor_t::doCycle()
{

	if ( (last_gpu_sim_cycle != gpu_sim_cycle) && (gpu_sim_cycle != 0) ) {
		if( !(gpu_sim_cycle % (g_the_gpu->get_config()).get_stat_sample_freq()) ) {
			last_gpu_sim_cycle = gpu_sim_cycle;
			if( ((g_the_gpu->get_config()).get_runtime_stat_flag() & GPU_RSTAT_ICNT_PUSH) && start_gpu_execution ) {
				printf("IPC%d:%f ", procID, (double)retired_instructions/(double)cpu_global_clock);
				if(procID == numcpus-1)
					printf("\n");
			}
		}
	}

//	if(cpu_global_clock%ipc_print_freq == 0 && cpu_global_clock!= 0)
//	{
//		printf("IPC%d:%f ", procID, (double)retired_instructions/(double)cpu_global_clock);
//		if(procID == numcpus-1)
//			printf("\n");
//	}

  //Then fetch new instructions for this cycle
  fetch();

  //execute non-memory instructions if any, memory instructions will be executed
  //by cache simulator
  executeNonMemoryInstructions();

  //retire completed instructions from last cycle
  retire();


  assert(num_mem_entrys >= 0 && num_mem_entrys <= window_size);
  assert(num_entrys >= 0 && num_entrys <= window_size);

  if(procID ==DBG_PROC)
    printf("%d %.0lf clock:%lld\n",num_unfinished_mem_entrys, sum_ot_occupancy,cpu_global_clock);

  sum_ot_occupancy  += num_unfinished_mem_entrys;
  sum_rob_occupancy += num_mem_entrys;
  sum_sjf_values    += num_unfinished_mem_entrys;
 
  int temp_count=0;
  long long temp_sum_slack=0;
  //printf("%d\n", num_entrys);
  for(int i=0; i<num_entrys; i++)
  {
    temp_sum_slack += (rob_array[i].measure_slack > 0) ? rob_array[i].measure_slack : 0;  //-1
    temp_count += (rob_array[i].measure_slack > 0) ? 1 : 0;
  }
  if(temp_count){
   sum_slack += temp_sum_slack/temp_count;
   //printf("sum_slack%lld\n",sum_slack);
  }
  
  //stats
  if(!(cpu_global_clock%sample_interval)) // Resetting at sample_interval length
  {
    sum_rob_occupancy = 0;
    sum_ot_occupancy = 0; 
    sum_slack=0;
    sum_l1misses_in_rob = 0;
    sum_l2misses_in_rob = 0;
    phase_running_avg_height_per_net_len = 0; //because this is a phase wise stat
    phase_running_avg_episode_height = 0; //because this is a phase wise stat
    number_of_network_phase_this_phase = 0; //because this is a phase wise stat
  }

  if(!(cpu_global_clock%(sample_interval)))
  {
    //if(procID == 1)
    //printf("************");
    // bank spread stats
    bool touch = false;
    for(int i= 0; i<num_llc_banks; i++)
    {
      if(bank_touch[i])
      {  
        bank_spread++;
        touch = true;
      }
      bank_touch[i]=0;
    }
    if(touch)
      bank_spread_counter++;

    if(DEBUG_CPU)
      if(procID == 1)
        printf("procID:%d instructions:%lld clock:%lld\n",procID,instructions,cpu_global_clock);
  }

  if(num_entrys>window_size || oldest > window_size || next > window_size)
  {
    printf("procID:%d instruction:%lld next:%d old:%d num_entrys:%d trace_next:%d\n",
        procID,instructions,next,oldest,num_entrys,trace_next);
    exit(1);
  }

  return;
}
  void
processor_t::getStats(int pass)
{
  //if(!strcmp(app_name,"nop"))
  //return;
  double tmpIPC = retired_instructions/(double)cpu_global_clock;
  unsigned physical_pages_touched_sum = 0;
  for(int i=0; i< num_llc_banks; i++)
  {
    physical_pages_touched_sum += physical_pages_touched[i];
  }
  //All these stats are per phase stats: ASIT
  //COP, COMP are per phase stats:ASIT
  //printf("procID(%s):%d phaseIPC:%8.4lf Systemipc:%8.4lf instruction_window:%5.2f l1mkpi:%5.2lf l2mkpi:%5.2lf l2mrate:%5.2lf bank:%5.2lf mspi:%5.4lf ast:%5.2lf pages:%u ot mem:%.2lf avg_slack:%lld COP:%.2lf COMP:%.2lf", app_name,procID,IPC[pass], tmpIPC, sum_rob_occupancy/(cpu_global_clock%(int)sample_interval), 1000*(l1misses/(double)instructions),1000*(l2misses/(double)instructions), l2misses/(double)l1misses,  bank_spread/bank_spread_counter,   memory_stall_cycles/(double)instructions, memory_stall_cycles/memory_instructions, physical_pages_touched[procID],       sum_ot_occupancy/(cpu_global_clock%(int)sample_interval),   sum_slack/(cpu_global_clock%(int)sample_interval),       sum_l1misses_in_rob/(cpu_global_clock%(int)sample_interval), sum_l2misses_in_rob/(cpu_global_clock%(int)sample_interval)); //avg. COMP: current outstanding memory packets
  
  //Modified by Nachi
  //printf("procID(%s):%d passIPC:%8.4lf Curripc:%8.4lf instr_wind:%5.2f l1mkpi:%5.2lf l2mkpi:%5.2lf l2mrate:%5.2lf bank:%5.2lf mspi:%5.4lf ast:%5.2lf pages:%u avg_slack:%lld",
//		  app_name,procID,IPC[pass], tmpIPC, sum_rob_occupancy/(cpu_global_clock%(int)sample_interval), 1000*(l1misses/(double)retired_instructions),1000*(l2misses/(double)retired_instructions), l2misses/(double)l1misses,  bank_spread/bank_spread_counter,   memory_stall_cycles/(double)retired_instructions,  sum_slack/(cpu_global_clock%(int)sample_interval));
  cout.precision(4);
  cout<<"procID("<<setw(12)<<app_name<<"):"<<setw(4)<<procID
//		  <<"\t AppCurrPassIPC:"<<IPC[pass]      // This is the IPC of the app when the "cpu_instructions_to_simulate" number of instructions were retired.
	      <<" FullAppIPC:"<<tmpIPC    			 //This is the IPC of the app when the slowest core has reached "cpu_instructions_to_simulate" number of instructions.
		  <<" instr_wind:"<<setw(5)<<sum_rob_occupancy/(cpu_global_clock%(int)sample_interval)
		  <<"\t l1mkpi:"<<setw(5)<<1000*(l1misses/(double)retired_instructions)
		  <<"\t l1missrate:"<<setw(5)<<(l1misses/(double)l1accesses)
		  <<"\t l2mkpi:"<<setw(5)<<1000*(l2misses/(double)retired_instructions)
		  <<"\t l2missrate:"<<setw(5)<<(l2misses/(double)l2accesses)
//		  << "\t FFPoint :" << FastForwardPoint
		  << "\t Instructions :" << instructions
//		  <<"l2mkpi:%5.2lf "
//		  <<"l2mrate:%5.2lf bank:%5.2lf "
//		  <<"mspi:%5.4lf "
//		  <<"ast:%5.2lf pages:%u avg_slack:%lld"
		  <<endl;
//,1000*(l2misses/(double)retired_instructions), l2misses/(double)l1misses,  bank_spread/bank_spread_counter,   memory_stall_cycles/(double)retired_instructions,  sum_slack/(cpu_global_clock%(int)sample_interval));

      //current_outstanding_packets);
      //current_l1misses_in_rob, /*max_concurrent_l1misses_in_rob,*/
      //current_l2misses_in_rob /*max_concurrent_l2misses_in_rob*/);  //num_unfinished_mem_entrys
  
  //These are aggregate stats:ASIT
  //i.e. Stats uptil now
  //Commented by Nachi
  /*printf(" num_net_ph:%lld tot_net_len:%lld tot_net_ht:%lld tot_com_ph:%lld tot_avg_ht_per_net_ln:%.5f", 
     number_of_network_phase, total_network_length, total_network_height, total_computation_phase, total_avg_network_height_per_network_length);
  printf(" num_network_ph_this_ph:%lld running_avg_ht_per_net_ln:%.5f phs_running_avg_ht_per_net_ln:%.5f",
      number_of_network_phase_this_phase, running_avg_height_per_net_len, phase_running_avg_height_per_net_len);
  printf(" total_episode_length:%lld running_avg_episode_height:%f", total_episode_length, running_avg_episode_height);
  printf(" @clock:%lld", cpu_global_clock);*/
  
  //Following - originally Commented by someone else - Not Nachi
  /*int tmpl1miss_entrys=0, tmpl2miss_entrys=0;
  for(int i=0; i<num_entrys; i++) {
   if(rob_array[i].l1miss == 1) {
     tmpl1miss_entrys++;
   }
   if(rob_array[i].l2miss == 1) {
     tmpl2miss_entrys++;
   }
  }
  //L1MR: L1 misses in ROB, L2MR: L2 misses in ROB right now: 
  //i.e. at the time this pass sample is taken
  printf(" num_entrys:%d L1MR:%d L2MR:%d\n", num_entrys, tmpl1miss_entrys, tmpl2miss_entrys);*/

}

void findUniqueApps()
{
  int flag = -1;
  for(int i=0; i<numcpus; i++)
  {
    flag = -1;
    for(int j=0; j<numApps; j++)
      if(!strcmp(processor[i].app_name,UniqueApps[j]))
        flag = 1;
    if(flag == -1)
    {
      strcpy(UniqueApps[numApps],processor[i].app_name);
      numApps++;
    }
  }
  for(int j=0; j<numApps; j++)
    printf("Unique App%d %s\n",j,UniqueApps[j]);
}

  void
getAppStats(int pass)
{
  int n;
  double slow, max=0, min=2000, sumIPC = 0, maxIPC=0, minIPC=2000;

  double wspeedup[64]={0};
  double hspeedup[64]={0};
  double nastslow[64]={0};
  double l1mpki[64]={0};
  double l2mpki[64]={0};
  int napps[64];
  double ctrl_latency, data_latency, packet_latency;

  printf("AppNames: ");
  for(int j=0; j<numApps; j++)
    printf("%s ",UniqueApps[j]);
  printf("\n");
  
  printf("AppNetLatency: ");
  for(int j=0; j<numApps; j++)
  {
    wspeedup[j] = 0;
    hspeedup[j] = 0;
    nastslow[j] = 0;
    napps[j] = 0;
    ctrl_latency = 0;
    data_latency = 0;
    packet_latency = 0;
    for(int n=0; n<numcpus; n++)
      if(!strcmp(processor[n].app_name,UniqueApps[j]))
      {
        slow = processor[n].oracleIPC[pass]/processor[n].IPC[pass];
        wspeedup[j] += 1/slow;
        hspeedup[j] += slow;
        slow = processor[n].nast[pass]/processor[n].oracleNast[pass];
        nastslow[j] += slow;
        max = max > slow ? max : slow ;
        min = min < slow ? min : slow ;
        napps[j]++;
        ctrl_latency += processor[n].ctrl_packet_latency/processor[n].ctrl_packets;
        data_latency += processor[n].data_packet_latency/processor[n].data_packets;
        packet_latency += processor[n].packet_latency/(processor[n].data_packets + processor[n].ctrl_packets);
        l1mpki[j] += 1000*(processor[n].request_packets[pass])/(double)cpu_instructions_to_simulate;
        l2mpki[j] += 1000*(processor[n].dram_request_packets[pass])/(double)cpu_instructions_to_simulate;
      }
    printf("%.2lf:%.2lf:%2lf:",ctrl_latency/napps[j],data_latency/napps[j],packet_latency/napps[j]);
  }
  printf("\n");
  
  printf("AppSlowDowns: ");
  for(int j=0; j<numApps; j++)
  {
    printf("%6.4f ",nastslow[j]/napps[j]);
  }
  printf("\n");
  double hs=0, ws=0;
  printf("AppIPCSlowDowns: ");
  for(int j=0; j<numApps; j++)
  {
    printf("%6.4f ",hspeedup[j]/napps[j]);
    hs += hspeedup[j];
    ws += wspeedup[j];
  }
  printf("\n");
  printf("AppMetrics:unfairness:%6.4f:wspeedup:%6.4f:hspeedup:%6.4f:max/min:%6.4f\n",max,ws/numcpus,numcpus/hs,max/min);
  
  printf("AppL1mpki:");
  for(int j=0; j<numApps; j++)
    printf("%5.2f:",l1mpki[j]/napps[j]);
  printf("\n");

  printf("AppL2mpki:");
  for(int j=0; j<numApps; j++)
    printf("%5.2f:",l2mpki[j]/napps[j]);
  printf("\n");
}

  void
processor_t::printOracleStats(int pass, int maxpass)
{
  if(!strcmp(app_name,"nop"))
    return;
  char name[40];
  if(pass == 0)
  {
    sprintf(name,"oracle/%s-%d-oracle",app_name,procID);
    oracle_fp = fopen(name,"w");
  }
  //FIXME hardcoded for 8x8 mesh
  int n1,n2,n3,n4;
  int x,y, xn, yn;
  n1 = procID; 
  y=n1/8; x=n1%8; yn = 4 + (3-y);  xn = 4 + (3-x);  
  n1=y*8 + x;  n2 = y*8 + xn; n3 = yn*8 + x; n4 = yn*8 + xn;
  fprintf(oracle_fp,"%s %d %d %.2lf %.2lf %.2lf %.2lf %.6lf %.6lf\n",app_name,n1,pass,IPC[pass],ast[pass],nast[pass], curr_nast[pass], thpt[pass], curr_thpt[pass]);
  fprintf(oracle_fp,"%s %d %d %.2lf %.2lf %.2lf %.2lf %.6lf %.6lf\n",app_name,n2,pass,IPC[pass],ast[pass],nast[pass], curr_nast[pass], thpt[pass], curr_thpt[pass]);
  fprintf(oracle_fp,"%s %d %d %.2lf %.2lf %.2lf %.2lf %.6lf %.6lf\n",app_name,n3,pass,IPC[pass],ast[pass],nast[pass], curr_nast[pass], thpt[pass], curr_thpt[pass]);
  fprintf(oracle_fp,"%s %d %d %.2lf %.2lf %.2lf %.2lf %.6lf %.6lf\n",app_name,n4,pass,IPC[pass],ast[pass],nast[pass], curr_nast[pass], thpt[pass], curr_thpt[pass]);

  fflush(oracle_fp);
  if(pass == maxpass)
    fclose(oracle_fp);
}

bool isFinite(double x)
{
  return(x <= DBL_MAX && x >= -DBL_MAX);

}

  void
processor_t::readOracleStats()
{
  if(!strcmp(app_name,"nop"))
    return;
  char name[40],app[20],buff[80];
  sprintf(name,"oracle/%s-oracle",app_name);
  FILE *fp = fopen(name,"r");

  if(fp == NULL)
  {
    printf("ERR in opening oracle file%s\n",name);
    return;
  }

  int n,p=0;
  double ipc,Ast,Nast,CurrNast,Thpt, CurrThpt;
  while (fgets(buff, 80, fp))
  {
    sscanf(buff,"%s %d %d %lf %lf %lf %lf %lf %lf\n",app,&n,&p,&ipc,&Ast,&Nast,&CurrNast, &Thpt, &CurrThpt);
    if(n == procID)
    {

      //FIXME comment this out after taking new oracles 
      //with synthetic
      if(strstr(app_name,"pattern"))
      {
        Nast /= 2;
        Ast /=  2;
        Thpt /= 2;
        CurrNast /= 2;
        CurrThpt /= 2;
      }
      //FIXME bug in oracle collection 
      //comment out when you collect new oracles
      Nast++; Ast++; CurrNast++;
      if(!isFinite(Nast))
        Nast = 1;
      oracleIPC[p] = ipc;
      oracleNast[p] = Nast;
      oracleAst[p] = Ast;
      oracleNastCurrent[p] = CurrNast;
      oracleThpt[p] = Thpt; 
      oracleThptCurrent[p] = CurrThpt;

      if(p == MAX_PASS_SUPPORTED)
        break;
    }
  }
  if(p<MAX_PASS)
  {
    for(p;p<=MAX_PASS_SUPPORTED;p++)
    {
      oracleIPC[p] = ipc;
      oracleNast[p] = Nast;
      oracleAst[p] = Ast;
      oracleNastCurrent[p] = CurrNast;
      oracleThpt[p] = Thpt; 
      oracleThptCurrent[p] = CurrThpt;
    }

    for(int i=p+1;i<MAX_PASS;i++)
    {
      oracleIPC[i] = oracleIPC[i%MAX_PASS_SUPPORTED];
      oracleNast[i] = oracleNast[i%MAX_PASS_SUPPORTED];
      oracleAst[i] = oracleAst[i%MAX_PASS_SUPPORTED];
      oracleNastCurrent[i] = oracleNastCurrent[i%MAX_PASS_SUPPORTED];
    }
  }

  fclose(fp);
}
  void
getAvgStats(int pass)
{
  double tmpInstructions=0,tmpIPC=0,tmprob=0,tmpl1misses=0,tmpl1missrate=0,tmpl2missrate=0,tmpl2misses=0,tmpMspi=0,tmpMast=0,tmpBankSpread=0,tmpl1mkpi=0,tmpl2mkpi=0,tmpmshrhits=0,tmpInstructions_warm=0;
  double tmpNast=0,tmpNspi=0,tmpHop=0,tmpLoc=0;
  double clock = (double)cpu_global_clock;
  double samples  = cpu_global_clock%(int)processor[0].sample_interval;
  double processors = (double)numcpus;
  double tmpprocessors = 0;
  double maxIPC = 0.0, minIPC = 3.0;
  double maxNspi = 0.0, minNspi = 10000.0;

  unsigned physical_pages_touched_sum = 0;
  for(int i=0; i< num_llc_banks; i++)
    physical_pages_touched_sum += physical_pages_touched[i];

  for(int i=0; i< numcpus; i++)
  {
    tmpInstructions      += processor[i].instructions;
    tmpIPC               += processor[i].IPC[pass];
    tmprob               += processor[i].sum_rob_occupancy/samples;
    tmpmshrhits			 += processor[i].mshr_hits;
    tmpl1misses          += processor[i].l1misses;
    tmpl1missrate		 += processor[i].l1misses / processor[i].l1accesses;
    tmpl2misses          += processor[i].l2misses;
    tmpl2missrate		 += processor[i].l2misses / processor[i].l2accesses;
    tmpMspi              += processor[i].memory_stall_cycles/(double)processor[i].instructions;
    tmpMast              += processor[i].memory_stall_cycles/(double)processor[i].memory_instructions;
    tmpBankSpread        += processor[i].bank_spread/processor[i].bank_spread_counter;
    tmpl1mkpi            += 1000*(processor[i].l1misses/(double)processor[i].instructions);
    tmpl2mkpi            += 1000*(processor[i].l2misses/(double)processor[i].instructions);
    tmpNspi              += processor[i].network_stall_cycles/(double)processor[i].instructions;
    tmpNast              += processor[i].network_stall_cycles/(double)processor[i].packets;
    tmpHop               += (processor[i].packets-processor[i].local_packets > 0) ? processor[i].hop_count/(double)(processor[i].packets-processor[i].local_packets) : 0;
    tmpprocessors        += (processor[i].packets-processor[i].local_packets > 0) ? 1 : 0;
    tmpLoc               += processor[i].local_packets/(double)processor[i].packets;
    minIPC               = minIPC > processor[i].IPC[pass] ? processor[i].IPC[pass] : minIPC;
    maxIPC               = maxIPC < processor[i].IPC[pass] ? processor[i].IPC[pass] : maxIPC;
    tmpNspi              = processor[i].nspi[pass];
    maxNspi              = maxNspi < tmpNspi ? tmpNspi : maxNspi;
    minNspi              = minNspi > tmpNspi ? tmpNspi : minNspi;
    tmpInstructions_warm += processor[i].retired_instructions_after_warmup;

  }

  //flora
  printf("Global_FUllIPC=%8.4lf Global_CurrPassIPC=%8.4lf cycles=%10.0lf ginstruction_window=%7.2f gl1mkpi=%7.2lf gl1missrate=%7.2lf gl2mpki=%7.2lf gl2missrate=%7.2lf gfp=%d\n",
	  tmpInstructions/clock, tmpIPC, clock, tmprob/processors, tmpl1mkpi/processors, tmpl1missrate/processors, tmpl2mkpi/processors, tmpl2missrate/processors,
      finished_processors[pass]);
  printf("Global_IPC_after_warmup = %f\n", (double)tmpInstructions_warm / (double)(clock - (double)cpu_global_clock_start));
//  printf("retired ins = %f, clock = %f, startclock = %f\n", (double)tmpInstructions_warm, (double)clock, (double)cpu_global_clock_start );

//  printf("l2misses=%f",tmpl2misses);
//  printf("\t mshrhits=%f\n",tmpmshrhits);
//  printf("cpu push=%d\n",g_the_gpu->cpu_icnt_push);
//  printf("tr size=%d\n",tr_pool.size());


 /* printf("Global_FUllIPC=%8.4lf Global_CurrPassIPC=%8.4lf ginstruction_window=%5.2f gl1mkpi=%5.2lf gl2mkpi=%5.2lf gl2mrate=%5.2lf gbank=%5.2lf gmspi=%5.4lf gast=%5.2lf gpages=%u gnspi=%5.4lf gnast=%5.4lf ghop=%4.2lf gloc=%.2lf gfp=%d\n",
	  tmpInstructions/clock, tmpIPC, tmprob/processors, tmpl1mkpi/processors, tmpl2mkpi/processors, tmpl2misses/tmpl1misses, tmpBankSpread/processors,
      tmpMspi/processors, tmpMast/processors,physical_pages_touched_sum,tmpNspi/processors,tmpNast/processors,tmpHop/tmpprocessors,tmpLoc/processors, finished_processors[pass]);
 */
}

  void
processor_t::getNetworkStats(int pass)
{
  //if(!strcmp(app_name,"nop"))
  //return;
  printf("app%s networkstats%d nspi:%5.4lf nast:%5.2lf nloc:%.2lf hopcount:%.2lf nast-alone:%5.2lf slowdown:%.2lf ipcratio:%.2lf zero-load:%.2lf buff-blocking:%.2lf blocking:%.2lf packet-lat:%.2lf packets:%.2lf%%\n",app_name,
      procID,nspi[pass], nast[pass], local_packets/packets, hop_count/(packets-local_packets),
      ast_alone[pass], ast[pass]/ast_alone[pass], nspi[pass]*IPC[pass],
      zero_load_cycles/packets, buff_blocking_cycles/packets, blocking_cycles/packets, packet_latency/packets, 100*(packets/(double)cpu_global_clock)
      );

}

  void
processor_t::getNetworkCommunicationMatrix()
{
  printf("locality%d ",procID);
  for(int i=0; i< num_llc_banks; i++)
  {
    printf("%4.0lf ",per_flow_packets[i]);
    per_flow_packets[i] = 0;
  }
  printf("mem:%4.0lf",l2misses-prior_l2misses);
  prior_l2misses = l2misses;
  printf("\n");
}

  void
processor_t::Reset()
{
  num_entrys           =0;
  num_mem_entrys       =0;
  num_unfinished_mem_entrys =0;
  next = oldest        =0;
  instructions         =0;
  retired_instructions =0;
  retired_instructions_after_warmup = 0;
  l1misses             =0;
  l2misses             =0;

  network_stall_cycles =0;
  memory_stall_cycles  =0;
  stall_alone          =0;

  network_stall_cycles_prior =0;
  memory_stall_cycles_prior =0;
  stall_alone_prior = 0;

  l1misses_phase_init = 0;
  l1misses_phase_end  = 0;
  network_phase_init  = 0;
  network_phase_end   = 0;
  network_phase       = 0;
  total_l1misses      = 0;
  computation_phase   = 0;
  total_network_length= 0;
  total_network_height= 0;
  total_computation_phase = 0;
  number_of_network_phase = 0;
  number_of_network_phase_this_phase = 0;
  total_avg_network_height_per_network_length = 0;
  running_avg_height_per_net_len = 0;
  phase_running_avg_height_per_net_len = 0;
  
  running_avg_episode_height = 0;
  phase_running_avg_episode_height = 0;
  episode_cumulative_height  = 0;
  episode_length = 0;
  
  comitted_mem_instr  = 0;
  comitted_l1misses   = 0;
  comitted_l2misses   = 0;
  
  current_l1misses_in_rob = 0;
  //max_concurrent_l1misses_in_rob = 0;
  current_l2misses_in_rob = 0;
  //max_concurrent_l2misses_in_rob = 0;
  sum_l1misses_in_rob  = 0;
  sum_l2misses_in_rob  = 0;
  
  memory_instructions  =0;
  sum_rob_occupancy    =0;
  sum_sjf_values       =0;
  sum_ot_occupancy     =0;
  sum_slack            =0;
  for(int i=0; i<=pass; i++)
  {
    finished_processors[i] = 0;
    IPC[i] = 0;
    nspi[i] = 0;
    nast[i] = 0;
    ast[i] = 0;
    ast_alone[i] = 0;
  }
  pass = 0;
  for(int i=0; i< num_llc_banks; i++)
    per_flow_packets[i]=0;
}
  void
processor_t::ResetStats()
{
  instructions         =0;
  retired_instructions =0;
  retired_instructions_after_warmup =0;
  l1misses             =0;
  l2misses             =0;


  network_stall_cycles =0;
  memory_stall_cycles  =0;
  stall_alone          =0;

  network_stall_cycles_prior =0;
  memory_stall_cycles_prior =0;
  stall_alone_prior = 0;

  current_l1misses_in_rob = 0;
  //max_concurrent_l1misses_in_rob = 0;
  current_l2misses_in_rob = 0;
  //max_concurrent_l2misses_in_rob = 0;
  sum_l1misses_in_rob  = 0;
  sum_l2misses_in_rob  = 0;
  
  memory_instructions  =0;
  sum_rob_occupancy    =0;
  sum_ot_occupancy     =0;
  sum_slack            =0;
  sum_sjf_values       =0;

  for(int i=0; i<=pass; i++)
  {
    finished_processors[i] = 0;
    IPC[i] = 0;
    nspi[i] = 0;
    nast[i] = 0;
    ast[i] = 0;
    ast_alone[i] = 0;
  }
  pass = 0;
  for(int i=0; i< num_llc_banks; i++)
    per_flow_packets[i]=0;
}

  void
processor_t::insertNetTransaction()
{
  double avg_ot_packets = sum_ot_occupancy / (cpu_global_clock % (int)sample_interval);
  int msg_priority =(int)(num_priority_levels*(num_unfinished_mem_entrys/avg_ot_packets)); 
  if(msg_priority >= num_priority_levels)
    msg_priority =  0; 
  else if(msg_priority < 0)
    msg_priority = num_priority_levels - 1; 
  else
    msg_priority = num_priority_levels - msg_priority ;
  if(procID == DBG_PROC)
    printf("p:%d u:%d avg%.0lf clock:%lld int:%d\n", msg_priority,num_unfinished_mem_entrys, avg_ot_packets,cpu_global_clock, cpu_global_clock%(int)sample_interval);

  TRANSTYPE *tr_temp;

  tr_pool.push_back(TRANSTYPE());
  tr_temp = &(tr_pool.at(tr_pool.size() - 1));

  rob_array[next].tr_id = transaction_count;
  tr_temp->rob_entry  = next;
  tr_temp->type       = NETWORK;
  tr_temp->address    = 0xDEAD;
  tr_temp->tid        = procID;
  tr_temp->cpuid      = procID;
  tr_temp->enter_time = cpu_global_clock;
  tr_temp->timestamp  = BLOCKED;
  tr_temp->pri        = tr_temp->cpuid % priority_levels;
  tr_temp->active_cp  = &(clevel[MLC].cache[tr_temp->cpuid]);
  tr_temp->tr_id      = transaction_count++;
  tr_temp->mem_op_id  = mem_op_id++;
  tr_temp->state      = START;
  tr_temp->src        = mp_trace_buffer[trace_next].src;
  tr_temp->dst        = mp_trace_buffer[trace_next].dst;
  tr_temp->msg_size   = rand()/(double)RAND_MAX > 0.5 ? 1 : 1024/FLIT_WIDTH;// 1;
  tr_temp->msg_inj_time    = cpu_global_clock;
  tr_temp->msg_priority = msg_priority; 
  insert_nic_queue( tr_temp->src, tr_temp->dst, tr_temp->msg_size,cpu_global_clock,tr_temp->tr_id);
}
  void
processor_t::insertMemNetTransaction()
{
  TRANSTYPE *tr_temp;

  tr_pool.push_back(TRANSTYPE());
  tr_temp = &(tr_pool.at(tr_pool.size() - 1));

  rob_array[next].tr_id = transaction_count;
  tr_temp->rob_entry  = next;
  tr_temp->type       = SIMPLE;
  tr_temp->address    = 0xDEAD;
  tr_temp->tid        = procID;
  tr_temp->cpuid      = procID;
  tr_temp->enter_time = cpu_global_clock;
  tr_temp->timestamp  = BLOCKED;
  tr_temp->pri        = tr_temp->cpuid % priority_levels;
  tr_temp->active_cp  = &(clevel[MLC].cache[tr_temp->cpuid]);
  tr_temp->tr_id      = transaction_count++;
  tr_temp->mem_op_id  = mem_op_id++;
  tr_temp->state      = L1_REQUEST;
  tr_temp->l1miss     = mp_trace_buffer[trace_next].l1miss;
  tr_temp->l2miss     = mp_trace_buffer[trace_next].l2miss;
  tr_temp->l1wbx      = mp_trace_buffer[trace_next].l1wbx;
  tr_temp->l2wbx      = mp_trace_buffer[trace_next].l2wbx;
  tr_temp->msg_size   = 1;
  insert_nic_queue( tr_temp->tid, tr_temp->l1miss, 1,cpu_global_clock,tr_temp->tr_id);
}
  void
processor_t::insertMemTransaction()
{

  TRANSTYPE *tr_temp;
  pair<set<unsigned long long>::iterator,bool> ret;
  bool insert = true;
  long long trace_data[16]={0};
  bool is_l1miss=true;
  bool is_l2miss=true;

  unsigned long long address = mp_trace_buffer[trace_next].address;
  int read_or_write = mp_trace_buffer[trace_next].type == READ_TYPE ? 1 : 0;

  ret = ot_MemRefs.insert(mp_trace_buffer[trace_next].address>>7);

  //1. if a read to an outstanding memory transaction no need to insert 
  if(ret.second == false && mp_trace_buffer[trace_next].type == READ_TYPE)
    insert = false;
  //2. if a new memory transaction and its a hit in L1 no need to insert
  //determine if it is a L1 miss and L2miss using the determinel1miss + determinel2miss function.
  //We have a private L1 and private L2.

  if(insert == true)
  {
	  l1accesses++;
	  rob_array[next].min_retire_timestamp = cpu_global_clock + flc_lookup_time;
	  is_l1miss = determinel1miss(address, procID, read_or_write);


	  if(is_l1miss == false)
		insert = false;
	  else //it was a L1 miss, so check L2 now!
	  {
		  l1misses++;
		  l2accesses++;
		  rob_array[next].min_retire_timestamp = rob_array[next].min_retire_timestamp + mlc_lookup_time;
		  rob_array[next].l1miss = 1;
		  is_l2miss = determinel2miss(address, procID, read_or_write);
		  if(is_l2miss == false) //3. if its a miss in L1 and hit in L2 no need to insert
				insert = false;
		  else
		  {
			  //l2 miss
			  l2misses++;
	//		  printf("Found l2 miss for address : %lx (proc %d) at time : %lld\n", address, procID, cpu_global_clock);
			  rob_array[next].l2miss = 1;
		  }
	  }
  }

  //FIXXX : hard coded to perfect L1
  //Perfect L1
  //insert = false;
  if(insert == false)   //L1 hit
  {
    num_unfinished_mem_entrys--;
    rob_array[next].ready = true;
    rob_array[next].hops = -1;
    return;
  }

  //The flow is : .. there is a L1 miss here.
  //This means, we need to insert into the transaction list maintained.
  //Meaning, next level of cache has to be polled now.

   tr_pool.push_back(TRANSTYPE());
   tr_temp = &(tr_pool.at(tr_pool.size() - 1));

   //Onur DEBUG
   if(CPU_DEBUG)
	   printf("Inserted transaction id: %d Address: %lx\n",transaction_count, mp_trace_buffer[trace_next].address);
   	   //printf("\n Inserting transaction %lld for address : %lx", tr_temp->tr_id, tr_temp->address);

   rob_array[next].tr_id = transaction_count;
   rob_array[next].hops = 0;
   tr_temp->rob_entry  = next;
   tr_temp->type       = mp_trace_buffer[trace_next].type == READ_TYPE ? CPU_READ : CPU_WRITE;
   tr_temp->address    = mp_trace_buffer[trace_next].address;
   tr_temp->tid        = procID;
   tr_temp->cpuid      = procID;
   tr_temp->enter_time = cpu_global_clock;
   tr_temp->timestamp  = cpu_global_clock;				// FIXXX parameterize MLC lookup time
   tr_temp->pri        = tr_temp->cpuid % priority_levels;
   tr_temp->active_cp  = &(clevel[MLC].cache[tr_temp->cpuid]);
   tr_temp->tr_id      = transaction_count++;
   tr_temp->mem_op_id  = mem_op_id++;
   tr_temp->state      = START;


}
  void
processor_t::NetworkTracer(long max_instructions, long warmup_instructions)
{
  trace_next=0;
  char buffer[80], ctype[20];
  long long paddr, vaddr;
  long long trace_data[16];
  int read_or_write;
  int offset,dummy;
  int i=0;
  int pass, prior_pass;
  long long prior_instructions = 0;

  int iter=0;
  char fname[100];

  if(network_trace_mode == true)
  {
    sprintf(fname,"/i3c/other/rdas/network-traces/%s.gz",app_name);
    record_trace_fp = gzopen(fname,"wb");
  }

  while(gzeof(gzFile(mp_trace_buffer)) != 1)
  {
    if(gzgets(mp_trace_fp, buffer, 80) == Z_NULL)
    {
      printf("ERROR: Reading the trace file %d %lld\n",procID,instructions);
      exit(1);
    }

    // -------------------------------------------------------------
    // 1. MSR Trace format.
    // -------------------------------------------------------------
    if(trace_format == MSR_TRACE_FORMAT)
    {
      //printf("instructions:%5lld %s",instructions, buffer);
      sscanf(buffer, "%d %s %llx", &offset, ctype, &vaddr); 
      instructions+=offset + 1;
      if(offset)
        if(network_trace_mode == true)
          gzprintf(record_trace_fp,"CDLY %d\n",offset);
      vaddr = vaddr | (((unsigned long long)(procID))<<48);
      if(!strcmp(ctype,"R"))
        read_or_write = 1;
      else
        read_or_write = 0;
      //update_state(vaddr,procID,read_or_write,trace_data); 
    }
    // -------------------------------------------------------------
    // 2. Intel or Pepsi Trace format.
    // -------------------------------------------------------------
    else
    {
      if(!strncmp(buffer, "CDLY",4))
      {
        sscanf(buffer, "%s %d", ctype, &offset); 
        instructions+=offset;
        if(network_trace_mode == true)
          gzprintf(record_trace_fp,"CDLY %d\n",offset);
      }
      else
      {
        sscanf(buffer, "%s %llx %llx", ctype, &paddr, &vaddr); 
        read_or_write = -1;
        if(!strncmp(buffer, "MRD",3))
          read_or_write = 1;
        if(!strncmp(buffer, "MWR", 3) || !strncmp(buffer, "MWBX",4))
          read_or_write = 0;
        //if(read_or_write != -1)
         // update_state(paddr,procID,read_or_write,trace_data); 
        instructions++;
      }
    }
    pass = (int)(instructions/(double)(MILLION/10));

    //if(!(instructions%(MILLION/10)))
    if(pass != prior_pass)
    {
      //printf("*************%.3lfM instructions**************\n",(double)instructions/MILLION);
      double l1mpki = (l1misses - prior_l1misses)/(double)((instructions-prior_instructions)/1000);
      double l2mpki = (l2misses - prior_l2misses)/(double)((instructions-prior_instructions)/1000);
      double avgl1mpki = l1misses/(double)(instructions/1000);
      double avgl2mpki = l2misses/(double)(instructions/1000);
      printf("%.4lf %.4lf %.4lf %.4lf %.4lf ",l1mpki,l2mpki,l1mpki+l2mpki,avgl1mpki,avgl2mpki);
      printf("%.4lf %.4lf %.4lf %.4lf\n",per_flow_packets[0],per_flow_packets[1],per_flow_packets[2],per_flow_packets[3]); 
      per_flow_packets[0] = 0;
      per_flow_packets[1] = 0;
      per_flow_packets[2] = 0;
      per_flow_packets[3] = 0;
      prior_pass = pass;
      prior_l1misses = l1misses;
      prior_l2misses = l2misses;
      prior_instructions = instructions;
    }

    if(instructions >= max_instructions)
    {
      if(network_trace_mode == true)
        gzclose(record_trace_fp);
      return;
    }
  }
  return;
}
  void
processor_t::fastforward(char* fast_forward_conf_file)
{
  char buffer[80], ctype[20];
  long long paddr, vaddr;
  long long trace_data[16];
  int read_or_write;
  int offset,dummy;

  int pass=0, prior_pass=0;
  long max_instructions;
  instructions = 0;

  if(!strcmp(app_name,"nop") || strstr(app_name,"pattern"))
    return;

  FILE *fp = fopen(fast_forward_conf_file,"r");
  if(fp == NULL)
  {
    printf("Could not read the fastforward points from fast forward configuration file: %s\n",fast_forward_conf_file);
    return;
  }

  float simpoint = 1;
  max_instructions = 1*MILLION;
  while (fgets(buffer, 80, fp))
  {
    if(strstr(buffer,app_name))
    {
      sscanf(buffer,"%s %f\n",ctype,&simpoint);
      max_instructions = (long)(simpoint * MILLION);

      //After reading simpoint fastforward, we further fasfoward add a number of instructions
      //so that all processors running this application does not run in a lock-step sort of fashion.
      //To create this randomness, we add procID*million to the simpoint's ff number.

      //If simpoint is 0, donot add any random instructions, because we usually use 0 as simpoint for
      //debugging purposes -- we dont add so that get to the execution faster.

      if(simpoint != 0)
    	  max_instructions = max_instructions + MILLION*procID;

      break;
    }
  }


  while(gzeof(gzFile(mp_trace_buffer)) != 1)
  {
    if(gzgets(mp_trace_fp, buffer, 80) == Z_NULL)
    {
      printf("ERROR: Reading the trace file %d %lld\n",procID,instructions);
      exit(1);
    }

    // -------------------------------------------------------------
    // 1. MSR Trace format.
    // -------------------------------------------------------------
    if(trace_format == MSR_TRACE_FORMAT)
    {
      //printf("instructions:%5lld %s",instructions, buffer);
      sscanf(buffer, "%d %s %llx", &offset, ctype, &vaddr); 
      instructions+=offset + 1;
      if(offset)
        if(network_trace_mode == true)
          gzprintf(record_trace_fp,"CDLY %d\n",offset);
      vaddr = vaddr | (((unsigned long long)(procID))<<48);
      if(!strcmp(ctype,"R"))
        read_or_write = 1;
      else
        read_or_write = 0;
      //update_state(vaddr,procID,read_or_write,trace_data); 
    }
    // -------------------------------------------------------------
    // 2. Intel or Pepsi Trace format.
    // -------------------------------------------------------------
    else
    {
      if(!strncmp(buffer, "CDLY",4))
      {
        sscanf(buffer, "%s %d", ctype, &offset); 
        instructions+=offset;
        if(network_trace_mode == true)
          gzprintf(record_trace_fp,"CDLY %d\n",offset);
      }
      else
      {
        sscanf(buffer, "%s %llx %llx", ctype, &paddr, &vaddr); 
        read_or_write = -1;
        if(!strncmp(buffer, "MRD",3))
          read_or_write = 1;
        if(!strncmp(buffer, "MWR", 3) || !strncmp(buffer, "MWBX",4))
          read_or_write = 0;

        instructions++;
      }
    }


    if(instructions >= max_instructions)
      break;
  }

  Reset();
  printf("Fast Forwarded %s to %.2lf Million Insructions\n",app_name,simpoint);

  FastForwardPoint = gztell(mp_trace_fp);
  populateTraceBuffer();

  return;
}

  bool
processor_t::profile()
{
  trace_next=0;
  char buffer[80], ctype[20];
  long long paddr, vaddr;
  int offset,dummy;
  int i=0;

  //Page Management
  long long page_offset;
  long physical_page_number, virtual_page_number;

  int iter=0;


  while(gzeof(gzFile(mp_trace_buffer)) != 1)
  {
    if(gzgets(mp_trace_fp, buffer, 80) == Z_NULL)
    {
      printf("ERROR: Reading the trace file %d %lld\n",procID,instructions);
      exit(1);
    }

    if(!strncmp(buffer, "CDLY",3))
    {
      sscanf(buffer, "%s %d", ctype, &offset); 
      instructions+=offset;
    }
    if(!strncmp(buffer, "MRD",3) || !strncmp(buffer, "MWR", 3) || !strncmp(buffer, "MWBX",4))
      instructions++;

    if(strncmp(buffer, "MRD",3))
      continue;

    //printf("procID:%d pages:%d %s",procID,physical_pages_touched_profile,buffer);
    if(iter++ == 1000000)
      return true;

    if(instructions >= 30000000)
      return false;

    sscanf(buffer, "%s %llx %llx", ctype, &paddr, &vaddr); 
    if(trace_format == INTEL_TRACE_FORMAT)
      virtual_page_number = paddr>>12;
    else
      virtual_page_number = vaddr>>12;

    if((physical_page_number = page_table[virtual_page_number]))
    {
      page_profile[physical_page_number][procID]++;
      page_profile_sum[physical_page_number]++;
    }
    else
    {
      physical_pages_touched_profile++;
      page_table[virtual_page_number] = physical_pages_touched_profile;
      page_profile[physical_pages_touched_profile][procID]++;
      first_touch[physical_pages_touched_profile] = procID;
      page_profile_sum[physical_pages_touched_profile]++;
    }
    if(physical_pages_touched_profile == MAX_PAGES)
    {
      profile_stats();
      printf("exceeding max pages !!!\n");
      exit(1);
    }
    return true;
  }
  return false;
}

typedef struct s_struct{
  double val;
  unsigned p1;
  unsigned p2;
}s_t;

bool sort_val_comm (s_t a, s_t b)
{
  if(a.val > b.val)
    return true;
  else
    return false;
}

vector<s_t> comm_array[128];
vector<s_t> comm_array_all;

  void
print_table()
{
  map<long long,long long>::iterator iter = page_table.begin();
  int i=0,j=0;
  double max_touches=0,sum_touches=0;
  int max_procID=-1;
  while(iter != page_table.end())
  {
    max_touches = 0;
    max_procID = -1;
    sum_touches = 0;
    for(j=0; j<numcpus; j++)
    {
      //printf("%.0lf ",page_profile[i][j]);//page_profile_sum[i]);
      if(max_touches < page_profile[iter->second][j])
      {
        max_touches = page_profile[iter->second][j];
        max_procID = j;
      }
      sum_touches += page_profile[iter->second][j];
    }
    printf("\n");
    fprintf(pt_fp,"%llx %llx %d %d %.0lf\n",iter->first,iter->second,max_procID,first_touch[iter->second],sum_touches);
    iter++;
    i++;
  }
}


  void
profile_stats()
{
  double sharer[128][128] = {0};
  double communication_matrix[128][128] = {0};
  for(int i=0; i<physical_pages_touched_profile; i++)
  {
    for(int j=0; j<numcpus; j++)
    {
      //printf("%.0lf ",page_profile[i][j]);//page_profile_sum[i]);
      for(int k=0; k<numcpus; k++)
        if(j!=k)
          communication_matrix[j][k] += page_profile[i][j] < page_profile[i][k] ? page_profile[i][j] : page_profile[i][k] ;
        else
          communication_matrix[j][k] = 0;
    }
    //printf("\n");
  }
  printf("total physical_pages_touched_profile :%d\n",physical_pages_touched_profile);
  for(int j=0; j<numcpus; j++)
  {
    for(int k=0; k<numcpus; k++)
    {
      s_t tmp;
      tmp.val = communication_matrix[j][k];
      tmp.p1 = j;
      tmp.p2 = k;
      comm_array[j].push_back(tmp);
      comm_array_all.push_back(tmp);
      printf("%.0lf ",communication_matrix[j][k]);
    }
    printf("\n");

    sort(comm_array[j].begin(), comm_array[j].end(), sort_val_comm);
    sort(comm_array_all.begin(), comm_array_all.end(), sort_val_comm);
  }

  printf("\n**************\n");
  for(int j=0; j<numcpus; j++)
  {
    for(int k=0; k<numcpus; k++)
      printf("%.0lf ",comm_array[j][k].val);
    printf("\n");
  }
  printf("\n**************\n");
  for(int j=0; j<numcpus; j++)
  {
    for(int k=0; k<numcpus; k++)
      printf("%d ",comm_array[j][k].p2);
    printf("\n");
  }
  printf("\n**************\n");
  for(int j=0; j<numcpus*numcpus; j++)
  {
    printf("%2d->%2d ",comm_array_all[j].p1,comm_array_all[j].p2);
    //comm_array[comm_array_all[j].p1][comm_array_all[j].p2].val =(double)j;
    if(j && !(j%numcpus))
      printf("\n");
  }
  printf("\n**************\n");
  for(int j=0; j<numcpus; j++)
  {
    printf("procID%d:",j);
    for(int k=0; k<numcpus; k++)
      printf("%2d ",comm_array[j][k].p2);//,comm_array[j][k].val);
    printf("\n");
    comm_array[j].clear();
  }
  comm_array_all.clear();


}
  void
processor_t::populateTraceBuffer()
{
  trace_next=0;
  char buffer[80], ctype[20];
  unsigned long long paddr, vaddr;
  int offset,dummy;
  int i=0;

  while(i<MAX_TRACE_SIZE)
  {
    if(trace_format == NOP_TRACE_FORMAT)
    {
      mp_trace_buffer[i].type = CDLY_TYPE;
      mp_trace_buffer[i].cycles = 5;
      i++;
      continue;
    }

    if(gzeof(mp_trace_fp) != 1)
    {
      if(gzgets(mp_trace_fp, buffer, 80) == Z_NULL)
      {
        printf("ERROR: Reading the trace file %d\n",procID);
        exit(1);
      }
      // -------------------------------------------------------------
      // 1. MSR Trace format.
      // -------------------------------------------------------------
      if(trace_format == MSR_TRACE_FORMAT)
      {
        sscanf(buffer, "%d %s %llx", &offset, ctype, &vaddr); 
        if(offset != 0)
        {
          mp_trace_buffer[i].type = CDLY_TYPE;
          mp_trace_buffer[i].cycles = offset;
          assert(offset>0);
          i++;
          if(i == MAX_TRACE_SIZE)
            break;
        }
        if(!strcmp(ctype,"R"))
          mp_trace_buffer[i].type = READ_TYPE;
        else if(!strcmp(ctype,"W"))
          mp_trace_buffer[i].type = WRITE_TYPE;
        else
        {printf("Invalid type %s\n",ctype); exit(1);}


        vaddr = vaddr | (((unsigned long long)(procID))<<48);
        mp_trace_buffer[i].address = vaddr;
        if(DEBUG_CPU)
          if(procID == 0 || procID == 2)
            printf("traceID:%d procID:%d type:%d vaddr:%llx offset:%d\n",i,procID,mp_trace_buffer[i].type,
                mp_trace_buffer[i].address,offset);
        i++;
        continue;
      }
      // -------------------------------------------------------------
      // 2. Intel or Pepsi Trace format.
      // -------------------------------------------------------------
      else
      {

        if(!strncmp(buffer, "CDLY",4))
        {
          sscanf(buffer, "%s %d", ctype, &offset); 
          if(offset != 0)
          {
            mp_trace_buffer[i].type = CDLY_TYPE;
            mp_trace_buffer[i].cycles = offset;
            if(DEBUG_CPU)
              if(procID == 1)
                printf("traceID:%d procID:%d type:%d cycles:%d\n",i,procID,mp_trace_buffer[i].type,offset);
            i++;
          }
        }
        else if(!strncmp(buffer, "MRD",3))
        {
          mp_trace_buffer[i].type = READ_TYPE;
          sscanf(buffer, "%s %llx %llx", ctype, &paddr, &vaddr); 
          //Only intel traces have more than 16 threads
          //if(trace_format != INTEL_TRACE_FORMAT)
          paddr = paddr  | (((unsigned long long)(procID))<<48);
          mp_trace_buffer[i].address = paddr;
          if(DEBUG_CPU)
            if(procID == 17)
              printf("traceID:%d procID:%d type:%d vaddr:%llx\n",i,procID,mp_trace_buffer[i].type,vaddr);
          i++;
        }
        else if(!strncmp(buffer, "MWR", 3) || !strncmp(buffer, "MWBX",4))
        {
          mp_trace_buffer[i].type = WRITE_TYPE;
          sscanf(buffer, "%s %llx %llx", ctype, &paddr, &vaddr); 
          //Only intel traces have more than 16 threads
          //if(trace_format != INTEL_TRACE_FORMAT)
          paddr = paddr  | (((unsigned long long)(procID))<<48);
          mp_trace_buffer[i].address = paddr;
          if(DEBUG_CPU)
            if(procID == 17)
              printf("traceID:%d procID:%d type:%d vaddr:%llx\n",i,procID,mp_trace_buffer[i].type,vaddr);
          i++;
        }
        // -------------------------------------------------------------
        // 3. Network Trace format.
        // -------------------------------------------------------------
        else if(!strncmp(buffer, "NT", 2))
        {
          int src=0, dst=0;
          sscanf(buffer, "%s %d %d", ctype, &src, &dst); 
          if(src == dst)
            continue;
          mp_trace_buffer[i].type = NET_TYPE;
          mp_trace_buffer[i].src = src;
          mp_trace_buffer[i].dst = dst;
          i++;
        }
        // -------------------------------------------------------------
        // 4. MemNet Trace format.
        // -------------------------------------------------------------
        else if(!strncmp(buffer, "MNET", 4))
        {
          int l1miss=-1, l2miss=-1, l1wbx=-1, l2wbx=-1 ;
          mp_trace_buffer[i].type = MNET_TYPE;
          sscanf(buffer, "%s %2d %2d %2d %2d", ctype, &l1miss, &l2miss, &l1wbx, &l2wbx); 
          mp_trace_buffer[i].l1miss = l1miss;
          mp_trace_buffer[i].l2miss = l2miss;
          mp_trace_buffer[i].l1wbx  = l1wbx;
          mp_trace_buffer[i].l2wbx  = l2wbx;
          i++;
        }
      }
    }
    else
    {
      gzrewind(mp_trace_fp);
      printf("recycling procID:%d at instructions:%lld\n",procID,instructions);
    }
  }


}
double std_dev(double values[64], int num_values)
{
  double mean =0, std_deviation = 0;
  for(int var=0; var<num_values; var++)
    mean += values[var];
  mean = mean/num_values;
  for(int var=0; var<num_values; var++)
    std_deviation += (values[var]-mean)*(values[var]-mean);
  std_deviation = std_deviation/num_values;
  std_deviation = sqrt(std_deviation);
  return std_deviation;
}
/* Intel RWT Trace Format
   TXNB 0 [F:1] 0 200 7200
   MWRC b6549d54 0
   CDLY 4 0
   MRDD 8a491ce8 0
   MWBX 8003f000 0
   MWRC 8a292034 0
   MRDC b9bc9ad1 0
   TXNE 0 [F:1] 1 1
   */


bool flc_transfer_overflow(int cpuid)
{
  int size = clevel[FLC].cache[cpuid].queue.size();
  // printf("size old %d for MLC%d\n", size, cpuid);
  if(size >= cache_rq_size[FLC])
    return true;

  std::vector<TRANSTYPE>::iterator tr;
  for(tr = tr_pool.begin(); tr != tr_pool.end() ; tr++)
    if(tr->cpuid == cpuid && tr->state == MLC_BUS_TRANSFER)
      size++;
  clevel[FLC].cache[cpuid].tot_rq_size += size;
  clevel[FLC].cache[cpuid].rq_size_samples++;
  //printf("size new %d\n", size);
  if(size < cache_rq_size[FLC])
    return false;
  else
    return true;
}
