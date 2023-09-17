/*****************************************************************************
 * This file contains functions for transaction state machine.
 *
*****************************************************************************/
#include <vector>
#include <math.h>

#include <sys/time.h>
#include <string.h>
#include <cassert>
#include "cache.h"
#include "transaction.h"
#include "globals.h"
#include "cpu_engine.h"
#include "pepsi_stats.h"
#include "pepsi_init.h"

//#include "interface.h"

#define CORES numcpus 
#define NODES (num_llc_banks - 1)
#define DIRECTORY 1

#if SIMICS_INTEG
extern long long instr_count[32];
#endif

#define INTR 100000

bool determinel1miss(long long address, int cpuid, int read_or_write)
{
  CACHE *cp_flc;
  int flc_cstate;
  bool miss = false;



  cp_flc = &(clevel[FLC].cache[cpuid]);
  flc_cstate = cp_flc->func_look_up(address,FALSE);
  cp_flc->numrefs_for_repl++;

  //DEBUG
  //printf("\n FLC: Going to lookup %lx", address);
  //printf("\n FLC: Found cache line in state %d ;; read_or_write = %d", flc_cstate, read_or_write);

  if(read_or_write)  //for read
  {
    if(flc_cstate == CPU_INVALID)
      miss = true;
  }
  else	//for write
  {
    if(flc_cstate == CPU_INVALID)
      miss = true;
  }

  if(miss)
	  cp_flc->numrefs_for_repl--;
  else
    cp_flc->func_look_up(address,TRUE);

  return miss;
}


bool determinel2miss(long long address, int cpuid, int read_or_write)
{
  CACHE *cp_mlc;
  int mlc_cstate;
  bool miss = false;



  cp_mlc = &(clevel[MLC].cache[cpuid]);
  cp_mlc->numrefs_for_repl++;
  mlc_cstate = cp_mlc->func_look_up(address,FALSE);

  //DEBUG
  //printf("\n MLC : Going to lookup %lx", address);
  //printf("\n MLC: Found cache line in state %d ;; read_or_write = %d", mlc_cstate, read_or_write);

  if(read_or_write)  //for read
  {
    if(mlc_cstate == CPU_INVALID)
      miss = true;
  }
  else	//for write
  {
    if(mlc_cstate == CPU_INVALID)
      miss = true;
  }

  if(miss)
	  cp_mlc->numrefs_for_repl--;
  else
    cp_mlc->func_look_up(address,TRUE);

  return miss;
}

void transaction_commit(TRANSTYPE *tr)
{
  char types[20];
  char buffer[100];
  long long diff;

  tr_commited++;

  if(!(tr_commited %10000000))
    transaction_count =0;


  if(STAT)
  {
    if(!(tr_commited %100000))
    {
      print_data_sharing();
      print_stats_cpu();
#if !SHORT_CIRCUIT
      //sim_result();
      printf("Total Ejected  msgs     : %lld\nAverage Network Latency : %.2f\nAvg. queue delay        : %.2f \n",total_icn_msgs, total_icn_latency/(float)total_icn_msgs, total_csim_nic_qdelay/(float)total_icn_msgs);
#endif
      printf("Executed References     : %lld\n",tr_commited);
    }
  }

  diff =  (tr->timestamp - tr->enter_time);
  total_resp_time += diff;
  llc_queue_wait += tr->llc_queue_wait;
  mlc_queue_wait += tr->mlc_queue_wait;
  mem_queue_wait[tr->cpuid] += tr->mem_queue_wait;
  snoop_wait += tr->snoop_wait;


  if(DEBUG_CPU)
    printf("TRANSACTION:committing Transaction :%lld, at time %lld\n ",tr->tr_id, tr->timestamp);

  switch(tr->type)
  {
    case CPU_READ: strcpy(types,"CPU_READ"); break;
    case CPU_WRITE: strcpy(types,"CPU_WRITE"); break;
    case SNOOP: strcpy(types,"SNOOP"); break;
    case WRITEBACK: strcpy(types,"WRITEBACK"); break;
	//case PREFETCH: strcpy(types,"PREFETCH"); break;
    case NETWORK: strcpy(types,"NETWORK"); break;
    case SIMPLE: strcpy(types,"SIMPLE"); break;
    default : strcpy(types,"@@ERR");                           
  }
  if(tr->timestamp < cpu_global_clock - 100)
  {
    printf("ERR: in committing clock :%lld timestamp:%lld for transaction:%lld\n",cpu_global_clock, tr->timestamp,tr->tr_id);
    //tr->hist();
    //tr->print();
    if(DEBUG_CPU)
      getchar();
  }

  if(RECORD)
  {
    gzprintf(out_fp, "%8s %8llx %2d %8ld ", types, tr->address, tr->cpuid, diff);
    gzflush(out_fp, 0);
    gzprintf(out_fp,"%8d ",   tr->mlc_queue_wait);
    gzflush(out_fp, 0);
    gzprintf(out_fp,"%8d ",   tr->llc_queue_wait);
    gzflush(out_fp, 0);
    gzprintf(out_fp,"%8d\n",  tr->snoop_wait);
    gzflush(out_fp, 0);
  }

}
