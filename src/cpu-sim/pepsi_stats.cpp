/*****************************************************************************
 * File:        pepsi_stats.c
 * Description: This file consists primarily all the functions for the
 *              printing the statistics of the simulator.
 *
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include <string.h>
#include <string>
#include <math.h>
#include <vector>
#include <exception>
#include <sys/time.h>


#include "cache.h"
#include "cpu_engine.h"
#include "transaction.h"
#include "globals.h"
#include "pepsi_stats.h"


void print_stats_cpu()
{

  long long writebacks=0, snoops=0, inv_snoops=0, hitm =0;
  int mlcbqsize=0, mlcrqsize =0;
  int llcbqsize=0, llcrqsize =0;
  long long compactions=0, fat_writes=0, mlc_wbx_fat_writes=0; 
  long long refs_per_core[64];
  for(int i = 0; i< numcpus; i++)
  {
    misses_per_core[i] = 0;
    refs_per_core[i]=0;
  }


  mlc_hits=0; mlc_misses=0; 
  llc_hits=0; llc_misses=0; 
  reads =0; writes=0;
  long long mem_queue_tot_wait = 0;
  for(int i = 0; i< numcpus; i++)
  {
    clevel[MLC].cache[i].hits = clevel[MLC].cache[i].refs - clevel[MLC].cache[i].misses;
    long long  hit = clevel[MLC].cache[i].hits;
    long long  miss = clevel[MLC].cache[i].misses;
    if(STAT)
    {
      printf("******Statistics FOR Middle Level Cache (MLC%d)\n",i);
      printf("\tHits                 : \t%lld\n ",clevel[MLC].cache[i].hits); fflush(stdout);
      printf("\tMisses               : \t%lld\n", clevel[MLC].cache[i].misses); fflush(stdout);
      printf("\tMiss ratio           : \t%f\n\tTotal References     : \t%lld\n", miss/(float)(hit + miss), hit+miss); fflush(stdout);
      printf("\tWrite Backs          : \t%lld\n", clevel[MLC].cache[i].writebacks); fflush(stdout);
      printf("\tSnoops               : \t%lld\n", clevel[MLC].cache[i].snoops); fflush(stdout);
      printf("\tHit to M blocks      : \t%lld\n", clevel[MLC].cache[i].hitm); fflush(stdout);
      printf("\tInvalidate Snoops    : \t%lld\n", clevel[MLC].cache[i].invalidate_snoops); fflush(stdout);
      if(clevel[MLC].cache[i].rq_size_samples)
        printf("\tAvg. Queue Length    :\t%d\n",clevel[MLC].cache[i].tot_rq_size/clevel[MLC].cache[i].rq_size_samples);
      if(clevel[MLC].cache[i].bq_size_samples)
        printf("\tAvg Blocked Queue Length :\t%d\n",clevel[MLC].cache[i].tot_bq_size/clevel[MLC].cache[i].bq_size_samples);
      printf("\tAccess Reads         :\t%lld\n\tAccess Writes        :\t%lld\n",clevel[MLC].cache[i].reads,clevel[MLC].cache[i].writes);
      printf("\n");
    }       
    reads += clevel[MLC].cache[i].reads;
    writes += clevel[MLC].cache[i].writes;
    if(clevel[MLC].cache[i].rq_size_samples)
      mlcrqsize += clevel[MLC].cache[i].tot_rq_size/clevel[MLC].cache[i].rq_size_samples;
    if(clevel[MLC].cache[i].bq_size_samples)
      mlcbqsize += clevel[MLC].cache[i].tot_bq_size/clevel[MLC].cache[i].bq_size_samples;
    mlc_hits += clevel[MLC].cache[i].hits;
    mlc_misses += clevel[MLC].cache[i].misses;
    writebacks += clevel[MLC].cache[i].writebacks;
    snoops += clevel[MLC].cache[i].snoops;
    hitm += clevel[MLC].cache[i].hitm;
    inv_snoops += clevel[MLC].cache[i].invalidate_snoops;


  }
  printf("\n");
  long long shared_hits = 0;
  for(int i = 0; i< num_llc_banks; i++)
  {
    clevel[LLC].cache[i].hits = clevel[LLC].cache[i].refs - clevel[LLC].cache[i].misses;
    long long hit = clevel[LLC].cache[i].hits;
    long long miss = clevel[LLC].cache[i].misses;
    if(STAT)
    {
      printf("*******Statistics FOR Middle Level Cache (LLC%d)\n",i);
      printf("\tHits                 : \t%lld\n ",clevel[LLC].cache[i].hits); fflush(stdout);
      printf("\tMisses               : \t%lld\n", clevel[LLC].cache[i].misses); fflush(stdout);
      printf("\tMiss ratio           : \t%f\n\tTotal References     : \t%lld\n", miss/(float)(hit + miss), hit+miss); fflush(stdout);
      printf("\tWrite Backs          : \t%lld\n", clevel[LLC].cache[i].writebacks); fflush(stdout);
      printf("\tSnoops               : \t%lld\n", clevel[LLC].cache[i].snoops); fflush(stdout);
      printf("\tHit to M blocks      : \t%lld\n", clevel[LLC].cache[i].hitm); fflush(stdout);
      printf("\tInvalidate Snoops    : \t%lld\n", clevel[LLC].cache[i].invalidate_snoops); fflush(stdout);
      if(clevel[LLC].cache[i].rq_size_samples)
        printf("\tAvg. Queue Length    : \t%d\n",clevel[LLC].cache[i].tot_rq_size/clevel[LLC].cache[i].rq_size_samples);
      if(clevel[LLC].cache[i].bq_size_samples)
        printf("\tAvg Blocked Queue Length : \t%d\n",clevel[LLC].cache[i].tot_bq_size/clevel[LLC].cache[i].bq_size_samples);
      printf("\tAccess Reads         : \t%lld\n\tAccess Writes        : \t%lld\n",clevel[LLC].cache[i].reads,clevel[LLC].cache[i].writes);
      printf("\n");
    }       

    mem_queue_tot_wait += mem_queue_wait[i];
    reads += clevel[LLC].cache[i].reads;
    writes += clevel[LLC].cache[i].writes;
    llc_hits += clevel[LLC].cache[i].hits;
    shared_hits += clevel[LLC].cache[i].shared_hits;
    llc_misses += clevel[LLC].cache[i].misses;
    writebacks += clevel[LLC].cache[i].writebacks;
    if(clevel[LLC].cache[i].rq_size_samples)
      llcrqsize += clevel[LLC].cache[i].tot_rq_size/clevel[LLC].cache[i].rq_size_samples;
    if(clevel[LLC].cache[i].bq_size_samples)
      llcbqsize += clevel[LLC].cache[i].tot_bq_size/clevel[LLC].cache[i].bq_size_samples;

    for(int j = 0; j< numcpus; j++)
    {
      misses_per_core[j] += clevel[LLC].cache[i].misses_per_core[j]; 
      refs_per_core[j] += clevel[LLC].cache[i].refs_per_core[j]; 
    }
  }
  if(CACHE_QOS)
  {
    printf("Miss rates per core\n");
    for(int i = 0; i< numcpus; i++)
      printf("Core %d ***References: %lld ***Misses: %lld ***Miss Rate: %.2f ***Memory Response Time %d\n",i,refs_per_core[i],misses_per_core[i],misses_per_core[i]/(float)refs_per_core[i],mem_queue_wait[i]/misses_per_core[i]);
  }
  printf("====================CSim Summary Statistics==========================\n");
  printf(" - Global Clock                  : \t%lld \n",cpu_global_clock);
  printf(" - Total commited References     : \t%lld\n",tr_commited);
  printf(" - Avg. Response time            : \t%.2f\n", total_resp_time/(float)tr_commited); 
  printf(" - Ratio. Network Response time  : \t%.2f\n", total_icn_latency/(float)tr_commited); 
  printf(" - Ratio. Memory Response time   : \t%.2f\n", mem_queue_tot_wait/(float)tr_commited); 
  printf(" - Ratio.[%] of time  in Network : \t%.2f\%\n", 100*((total_icn_latency)/(float)total_resp_time)); 
  printf(" - Ratio.[%] of time  in Memory  : \t%.2f\%\n", 100*((mem_queue_tot_wait)/(float)total_resp_time)); 
  printf(" - Avg. MLC miss ratio           : \t%.2f\%\n",100*(mlc_misses/(float)(mlc_hits+mlc_misses))); fflush(stdout);
  printf(" - Avg. LLC miss ratio           : \t%.2f\%\n",(llc_misses + llc_hits ) ? 100*(llc_misses/(float)(llc_hits+llc_misses)) : 0); fflush(stdout);
  printf(" - MLC Misses                    : \t%lld\n", mlc_misses); fflush(stdout);
  printf(" - LLC Misses                    : \t%lld\n", llc_misses); fflush(stdout);
  printf(" - Avg. MLC Misses               : \t%lld\n",  mlc_misses/numcpus); fflush(stdout);
  printf(" - Avg. MLC References           : \t%lld\n", (mlc_hits + mlc_misses)/numcpus); fflush(stdout);
  printf(" - Avg. LLC Misses               : \t%lld\n",  llc_misses/numcpus); fflush(stdout);
  printf(" - Avg. LLC References           : \t%lld\n", (llc_hits + llc_misses)/numcpus); fflush(stdout);
  printf(" - Total Reads                   : \t%lld\n",reads);
  printf(" - Total Writes                  : \t%lld\n",writes);
  printf(" - LLC Shared Hits               : \t%lld\n", shared_hits); fflush(stdout);
  printf(" - LLC References                : \t%lld\n", llc_hits + llc_misses); fflush(stdout);
  printf(" - MLC References                : \t%lld\n", mlc_hits + mlc_misses); fflush(stdout);
  printf(" - Writebacks                    : \t%lld\n", writebacks); fflush(stdout);
  printf(" - Snoops                        : \t%lld\n", snoops); fflush(stdout);
  printf(" - Hit on M Blocks               : \t%lld\n", hitm); fflush(stdout);
  printf(" - Invalidate Snoops             : \t%lld\n", inv_snoops); fflush(stdout);
  printf(" - Avg. MLC Queue Length         : \t%d\n", mlcrqsize/numcpus);
  printf(" - Avg. Blocked MLC Queue Length : \t%d\n", mlcbqsize/numcpus);
  printf(" - Avg. LLC Queue Length         : \t%d\n", llcrqsize/numcpus);
  printf(" - Avg. Blocked LLC Queue Length : \t%d\n", llcbqsize/numcpus);
  if(MEMORY->rq_size_samples)
    printf(" - Avg. Memory Queue Length      : \t%d\n",MEMORY->tot_rq_size/MEMORY->rq_size_samples);
  printf(" - Avg. MLC Queued time          : \t%lld\n", tr_commited ? mlc_queue_wait/tr_commited : 0); 
  printf(" - Avg. Memory Response time     : \t%lld\n", llc_misses ? mem_queue_tot_wait/llc_misses : 0); 
  printf(" - Avg. LLC Queued Time          : \t%lld\n", tr_commited ? llc_queue_wait/tr_commited : 0); 
  printf(" - Avg. Snoop wait time          : \t%lld\n", tr_commited ? snoop_wait/tr_commited : 0); 
  printf(" - Conflict priority misses      : \t%lld\n",conflict_pri_misses);fflush(stdout); 
  printf(" - MLC Busy time ratio           : \t%.6f\n",mlc_busy_time/(float)cpu_global_clock); fflush(stdout);
  printf(" - LLC Busy time ratio           : \t%.6f\n",llc_busy_time/(float)cpu_global_clock); fflush(stdout);
  printf(" - Avg. Event Gap in cycles      : \t%.2lf\n",jump/countj);

  float tot_msgs = addr_msgs + snoop_msgs + wb_msgs + data_msgs ;
  if(tot_msgs)
  {
    printf(" - Data Msgs ratio               : \t%2.4f\%\n",(data_msgs/tot_msgs)*100); fflush(stdout);
    printf(" - Addr Msgs ratio               : \t%2.4f\%\n",(addr_msgs/tot_msgs)*100); fflush(stdout);
    printf(" - Snoop Msgs ratio              : \t%2.4f\%\n",(snoop_msgs/tot_msgs)*100); fflush(stdout);
    printf(" - Writeback msgs ratio          : \t%2.4f\%\n",(wb_msgs/tot_msgs)*100); fflush(stdout);
  }

  // printf("\n====================Compression Statistics==============================\n");
  // printf(" - Achieved Compression Ratio    : \t%.2f\n", 1 - (compress_linesize * SET_SEG_SIZE)/(float)(clevel[LLC].cache[0].linesize * compress_count));
  // printf(" - Compactions                   : \t%lld\n", compactions); 
  // printf(" - Compactions per set Avg. dead time : \t%.2f\n", set_tot_dead_times/(float)set_compacts); 
  // printf(" - Compactions per Bank Avg. compact time : \t%.2f\n", bank_tot_compact_delay/(float)bank_compacts); 
  // printf(" - Fat Writes to LLC             : \t%lld\n", fat_writes); 
  // printf(" - Fat Write Backs from to MLC   : \t%lld\n", mlc_wbx_fat_writes); 

  // float total_patterns = pattern_zero + pattern_4b_se + pattern_8b_se + pattern_8b_rep + pattern_16b_se +
    // pattern_16b_se_half + pattern_16b_padded + pattern_32b ;   

  // printf(" - Ratio of Zero Patterns         : \t%.2f\n", pattern_zero/(float)total_patterns); 
  // printf(" - Ratio of 4 Bit Patterns SE     : \t%.2f\n", pattern_4b_se/(float)total_patterns); 
  // printf(" - Ratio of 8 Bit Patterns SE     : \t%.2f\n", pattern_8b_se/(float)total_patterns); 
  // printf(" - Ratio of 16 Bit Patterns SE    : \t%.2f\n", pattern_16b_se/(float)total_patterns); 
  // printf(" - Ratio of 16 Bit Padded         : \t%.2f\n", pattern_16b_padded/(float)total_patterns); 
  // printf(" - Ratio of 16 Bit Half SE        : \t%.2f\n", pattern_16b_se_half/(float)total_patterns); 
  // printf(" - Ratio of 8 Bit Repeated        : \t%.2f\n", pattern_8b_rep/(float)total_patterns); 
  // printf(" - Ratio of Uncompressed          : \t%.2f\n", pattern_32b/(float)total_patterns); 

   
 printf("\n\n***Transaction Pool Size      : \t%d\n",tr_pool.size()); 

  return;
}

float print_data_sharing()
{
  long long shared_blocks;
  long long modified_blocks;
  long long exclusive_blocks;
  long long invalid_blocks;


  /* MLC */
  shared_blocks = 0;
  modified_blocks= 0;
  exclusive_blocks= 0;
  invalid_blocks= 0;
  printf("\n====================Cache Sharing Statistics========================\n");

  for(int cindex=0; cindex < numcpus ; cindex++)
  {
    CACHE *cp = &(clevel[MLC].cache[cindex]);
    cp->modified = 0;
    cp->shared = 0;
    cp->exclusive = 0;
    cp->invalid= 0;

    for(int i=0; i<cp->numsets;i++)
    {
      CSET *setp = &(cp->sets[i]);
      for(int j =0; j < setp->assoc; j++)
      {
        switch(setp->clines[j].state)
        {
          case CPU_MODIFIED : cp->modified++;
                          break;
          case SHARED : cp->shared++;
                        break;
          case EXCLUSIVE : cp->exclusive++;
                           break;
          case CPU_INVALID : cp->invalid++;
                         break;
        }
      }
    }

    shared_blocks+=cp->shared;
    modified_blocks+= cp->modified;
    exclusive_blocks+=cp->exclusive;
    invalid_blocks+=cp->invalid;

  } 
  int tot_blocks = numcpus*clevel[MLC].cache[0].numsets*clevel[MLC].cache[0].assoc;
  mlc_occ = 1 - invalid_blocks/(float)tot_blocks;
  printf("***Data Occupancy in MLC       : \t%.2f\n",1 - invalid_blocks/(float)tot_blocks);
  printf("***Exclusive Data ratio in MLC : \t%.2f\n",exclusive_blocks/(float)tot_blocks);
  printf("***Dirty Data ratio in MLC     : \t%.2f\n",modified_blocks/(float)tot_blocks);

  /*LLC */

  shared_blocks = 0;
  modified_blocks= 0;
  exclusive_blocks= 0;
  invalid_blocks= 0;


  long long tot_pri_occ[64];
  int sumocc=0;
  long long nsegs=0, totsegs=0;
  for(int i = 0; i< priority_levels; i++)
    tot_pri_occ[i] = 0;

  //tot_blocks = numcpus*clevel[LLC].cache[0].numsets*clevel[LLC].cache[0].assoc;
  for(int cindex=0; cindex < num_llc_banks ; cindex++)
  {
    CACHE *cp = &(clevel[LLC].cache[cindex]);
    cp->modified = 0;
    cp->shared = 0;
    cp->exclusive = 0;
    cp->invalid= 0;

    cp->sets_used = 0;

    for(int i=0; i<cp->numsets;i++)
    {
      CSET *setp = &(cp->sets[i]);

      if(setp->numlines > 0)
        cp->sets_used++;

      for(int j =0; j < setp->clines.size(); j++)
      {
        tot_blocks++;
        switch(setp->clines[j].state)
        {
          case CPU_MODIFIED : cp->modified++;
                          break;
          case SHARED : cp->shared++;
                        break;
          case EXCLUSIVE : cp->exclusive++;
                           break;
          case CPU_INVALID : cp->invalid++;
                         break;
        }
      }
    }

    shared_blocks+=cp->shared;
    modified_blocks+= cp->modified;
    exclusive_blocks+=cp->exclusive;
    invalid_blocks+=cp->invalid;
    printf("%s sets used %d set utilzation %.4f\n",cp->clabel,cp->sets_used, cp->sets_used/(float)cp->numsets);

  }

  //if(CACHE_QOS)
  for(int i = 0; i< priority_levels; i++)
  {
    tot_pri_occ[i] += /*clevel[LLC].cache[cindex].*/pri_occ[i];
  }
  if(CACHE_QOS)
    for(int i = 0; i< priority_levels; i++)
    {
      printf("PRIORITY %d OCC %.2f\n",i,tot_pri_occ[i]/(float)tot_blocks);
    }

  //llc_occ = 1 - invalid_blocks/(float)tot_blocks;
  llc_occ = (float)totsegs;
  printf("***Data Occupancy in LLC       : \t%.2f\n",llc_occ);
  printf("***Data Sharing in LLC         : \t%.2f\n",shared_blocks/(float)tot_blocks);
  printf("***Exclusive Data Ratio in LLC : \t%.2f\n",exclusive_blocks/(float)tot_blocks);
  printf("***Dirty data Ratio in LLC     : \t%.2f\n",modified_blocks/(float)tot_blocks);
  printf("***Invalid data Ratio in LLC     : \t%.2f\n",invalid_blocks/(float)tot_blocks);

  return shared_blocks/(float)tot_blocks;
}

void print_mlc()
{
  for(int i = 0; i< numcpus; i++)
  {
    if(!clevel[MLC].cache[i].queue.empty())
    {  
      printf(" MLC%d QUEUE:",i);
      for(std::vector<long long>::iterator qindex = clevel[MLC].cache[i].queue.begin(); 
          qindex != clevel[MLC].cache[i].queue.end(); qindex++)
        printf(" %lld", *qindex);
      printf("\n");
    }
    if(!clevel[MLC].cache[i].blocked_queue.empty())
    {  
      printf(" MLC%d BLOCKED QUEUE:",i);
      for(std::vector<bq_node>::iterator qindex = clevel[MLC].cache[i].blocked_queue.begin(); 
          qindex != clevel[MLC].cache[i].blocked_queue.end(); qindex++)
      {
        printf(" %lld", qindex->tr_id);
        if(!qindex->mshr.empty())
        {
          printf("MSHR[");
          for(std::vector<long long>::iterator temp = qindex->mshr.begin(); temp != qindex->mshr.end(); temp++)
            printf("%lld ",*temp);
          printf("]");
        }
      }
      printf("\n");
    }
  }
  return;
}

void print_llc()
{
  for(int i = 0; i< num_llc_banks; i++)
  {
    if(!clevel[LLC].cache[i].queue.empty())
    {  
      printf(" LLC%d QUEUE:",i);
      for(std::vector<long long>::iterator qindex = clevel[LLC].cache[i].queue.begin(); 
          qindex != clevel[LLC].cache[i].queue.end(); qindex++)
        printf(" %lld", *qindex);
      printf("\n");
    }
    if(!clevel[LLC].cache[i].blocked_queue.empty())
    {  
      printf(" LLC%d BLOCKED QUEUE:",i);
      for(std::vector<bq_node>::iterator qindex = clevel[LLC].cache[i].blocked_queue.begin(); 
          qindex != clevel[LLC].cache[i].blocked_queue.end(); qindex++)
      {
        printf(" %lld", qindex->tr_id);
        if(!qindex->mshr.empty())
        {
          printf("MSHR[");
          for(std::vector<long long>::iterator temp = qindex->mshr.begin(); temp != qindex->mshr.end(); temp++)
            printf("%lld ",*temp);
          printf("]");
        }
      }
      printf("\n");
    }
  }

  return;
}

// void print_llc_transactions()
// {
  // for(int i = 0; i< num_llc_banks; i++)
  // {
    // if(!clevel[LLC].cache[i].queue.empty())
    // {  
      // printf(" LLC%d QUEUE:",i);
      // for(std::vector<long long>::iterator qindex = clevel[LLC].cache[i].queue.begin(); 
          // qindex != clevel[LLC].cache[i].queue.end(); qindex++)
      // { 
        // printf("*****%lld\n", *qindex);
        // print_tr(*qindex);
        // printf("*****\n", *qindex);
      // }
      // printf("\n");
    // }
    // if(!clevel[LLC].cache[i].blocked_queue.empty())
    // {  
      // printf(" LLC%d BLOCKED QUEUE:",i);
      // for(std::vector<bq_node>::iterator qindex = clevel[LLC].cache[i].blocked_queue.begin(); 
          // qindex != clevel[LLC].cache[i].blocked_queue.end(); qindex++)
      // {
        // printf("*****%lld\n", qindex->tr_id);
        // print_tr(qindex->tr_id);
        // printf("*****\n");
        // if(!qindex->mshr.empty())
        // {
          // printf("MSHR[");
          // for(std::vector<long long>::iterator temp = qindex->mshr.begin(); temp != qindex->mshr.end(); temp++)
            // printf("%lld ",*temp);
          // printf("]");
        // }
      // }
      // printf("\n");
    // }
  // }

  // return;
// }

// void print_mem()
// {
  // if(!MEMORY->queue.empty())
    // for(std::vector<long long>::iterator qindex = MEMORY->queue.begin(); 
        // qindex != MEMORY->queue.end(); qindex++)
      // printf(" %d", *qindex);


  // return;
// }

// void print_tr(int id)
// {
  // std::vector<TRANSTYPE>::iterator print_tr;
  // for(print_tr = tr_pool.begin(); print_tr != tr_pool.end() ; print_tr++)
  // {
    // if(print_tr->tr_id == id)
    // {
      // print_tr->print();
      // print_tr->hist();
      // return;
    // }
  // }
// }

// void print_pool()
// {
  // std::vector<TRANSTYPE>::iterator print_tr;
  // for(print_tr = tr_pool.begin(); print_tr != tr_pool.end() ; print_tr++)
    // print_tr->print();
// }

// void print_hist()
// {
  // std::vector<TRANSTYPE>::iterator print_tr;
  // for(print_tr = tr_pool.begin(); print_tr != tr_pool.end() ; print_tr++)
  // {print_tr->print(); print_tr->hist();}
// }
