/*****************************************************************************
 * CASPER: Cache Analysis, Simulation and Performance Exploration w/ Refstreams
 *
 * This file contains the data structure declaration for the transaction.
 *
 *****************************************************************************/
#ifndef __TRANSACTION_H__
#define __TRANSACTION_H__
#include <zlib.h>
#include <vector>
#include <map>
#include "defines.h"
#include "cache.h"

//For MemFetch
#include "../gpgpu-sim/mem_fetch.h"

  /*States of TRANSACTION */
  
  enum tr_type  { CPU_READ, CPU_WRITE, SNOOP , WRITEBACK, NETWORK, SIMPLE};
  enum tr_state  { START, 
                    EXIT,
                    CORE_TO_LLC,
                    LLC_TO_CORE,
                    MLC_BUS_TRANSFER,
                    MLC_QUEUED,
                    MLC_LOOK_UP,
                    LLC_BUS_TRANSFER,
                    LLC_ICN_OVERFLOW,
                    LLC_ICN_QUEUED,
                    LLC_ICN_TRANSFER,
                    LLC_QUEUED,
                    LLC_LOOK_UP,
                    MEM_QUEUED,
                    MEM_BUS_TRANSFER,
                    SNOOP_BLOCKED,
                    LLC_BLOCKED_QUEUE_HEAD,
                    LLC_REPLACE_SM,
                    MLC_RESPONSE_TRANSFER,
                    MLC_BLOCKED_HEAD_QUEUE,
                    MLC_REPLACE_SM,
                    MLC_RSM_LLC_TRANSFER,
                    MLC_RSM_LLC_LOOKUP,
                    LLC_RSM_LLC_LOOKUP,
                    LLC_MSHR_PENDING,
                    MLC_MSHR_PENDING,
                    LLC_RSM_LOOKUP_COMPLETE,
                    MLC_BQ_OVERFLOW,
                    LLC_BQ_OVERFLOW,
                    MEM_ICN_TRANSFER,
                    MEM_ICN_OVERFLOW,
                    REQUEST,
                    RESPONSE,
                    L1_REQUEST,
                    L2_REQUEST,
                    L1_RESPONSE,
                    L2_RESPONSE
  };

typedef struct { 
                tr_state state;
                long long timestamp;
} history_entry_t;
                    

class TRANSTYPE {
 public:
   tr_type accesstype;
   tr_type type;
   tr_state state;
   //std::vector<tr_state> history;
   std::vector<history_entry_t> history;
   tr_state prev_state;
   int cache;
   CACHE *active_cp;
   long long timestamp;
   long long enter_time;
   long long tr_id;
   int mem_op_id;
   int cstate, mlc_state, llc_state;
   long long address;
   int cpuid;
   int pri;
   int tid;
   int pid;

   int rob_entry;

   mem_fetch *ptr_to_mf;

   // long long data[16];
   // long long snoop_data[16];
   // long long vdata[16];
   // long long trace_data[16];
   
/* compression flags */
   bool fat_write;
   bool mlc_wbx_fat_write;
   short linesize; 
   long long compaction_delay; 
   short noxlinesize; 
/* for replacement */
   int llc_first;
   int vstate;
   long long vaddress;
   int rep_pos;
   int vpri;
/* for lockup free cache */
   bool mshr_pending[2];
/* case when a mshr blocked tr is serviced, it needs to be deleted if this flag
** is set */
   bool bq_delete;
   bool from_blocked_queue;

/* for snoops */
   long long parent;
   int snoop_done[MAX_CORES];
   int snoop_state[MAX_CORES];
   int snoop_count; 
/* flag to diffrentiate between invalidate snoops from replacement and block lookup
** snoops */   
   int snoop_invalidate;

   /* stats */
   bool llc_memq_miss;
   long long mlc_queue_wait;
   long long llc_queue_wait;
   long long mem_queue_wait;
   long long snoop_wait;

   long long mlcq_enter;
   long long snoopq_enter;
   long long llcq_enter;
   long long memq_enter;

   /* OVERFLOW */
   bool mlc_bq_overflow;
   bool llc_bq_overflow;
   bool mlc_rq_overflow;
   bool llc_rq_overflow;

   /* for ref to simple core */
   int ref_inst;
   int ref_data;

   /* for ICN */
   tr_state llc_transfer_state;
   tr_state mem_transfer_state;
   int src;
   int dst;
   bool icn_trans_comp;
   long long msg_inj_time;
   int msg_size;
   int msg_priority;
   int slack;

   /* simple transaction for network*/
   short l1miss;
   short l1wbx;
   short l2miss;
   short l2wbx;
   

   /*CONSTRUCTOR*/
   TRANSTYPE() {
   accesstype = CPU_READ;
   type = CPU_READ;
   state = START;
   prev_state = START;
   cache =  MLC;
   active_cp = NULL;
   timestamp = 0;
   enter_time = 0;
   tr_id = 0;
   cstate = 0;
   mlc_state = 0;
   llc_state = 0;
   address = 0;
   cpuid = 0;
   tid = 0;
   pid = 0;
   pri = 0;
   l1miss = -1;
   l2miss = -1;
   l1wbx = -1;
   l2wbx = -1;
   //Initializing mem_fetch pointer to null
   ptr_to_mf = NULL;

  // for(int i = 0; i<16; i++)
  // {
    // data[i]= trace_data[i] = snoop_data[i] = vdata[i] =0;
  // }

   
  fat_write = false;
  mlc_wbx_fat_write = false;
  compaction_delay = 0;
  
/* for replacement */
   llc_first = 0;
   vstate = 0;
   vaddress = 0;
   vpri = 0;

   mshr_pending[MLC] = false;
   mshr_pending[LLC] = false;
   bq_delete = false;
   from_blocked_queue = false;

/* for snoops */
   parent = -1;
   for(int i =0; i< MAX_CORES ; i++)
   {
    snoop_done[i]=0;
    snoop_state[i]=0;
   }
   snoop_count = 0;
   snoop_invalidate = 0;
/* stats */
   llc_memq_miss = false;
   mlc_queue_wait = 0;
   llc_queue_wait = 0;
   mem_queue_wait = 0;
   snoop_wait = 0;

   mlc_bq_overflow = false;
   llc_bq_overflow = false;
   mlc_rq_overflow = false;
   llc_rq_overflow = false;
   
   /* for ref to simple core */
   ref_inst = 0;
   ref_data = 0;

   src = 0;
   icn_trans_comp = false;
   dst = 0;
   msg_size = 4;
   llc_transfer_state = MLC_RESPONSE_TRANSFER;
   mem_transfer_state = MEM_BUS_TRANSFER;
   //FIXME try and remove the hardcoding, this is bad
   linesize = 16;//clevel[0].cache[0].linesize/4;
   noxlinesize = 16;//clevel[0].cache[0].linesize/4;

   /* simics */
   mem_op_id = -1;
  }
/*COPY*/
   void copy(TRANSTYPE tr) {
   accesstype  = tr.accesstype;
   type        = tr.type;
   state       = tr.state;
   prev_state  = tr.prev_state;
   cache       = tr.cache;
   active_cp   = tr.active_cp;
   timestamp   = tr.timestamp;
   enter_time  = tr.enter_time;
   tr_id       = tr.tr_id;
   cstate      = tr.cstate;
   mlc_state   = tr.mlc_state;
   llc_state   = tr.llc_state;
   address     = tr.address;
   cpuid       = tr.cpuid;
   tid         = tr.tid;
   pid         = tr.pid;
   pri         = tr.pri;

  // for(int i = 0; i<16; i++)
  // {
    // data[i]= tr.data[i];
    // trace_data[i]= tr.trace_data[i];
  // }
  

/* for replacement */
   llc_first   = tr.llc_first;
   vstate      = tr.vstate;
   vaddress    = tr.vaddress;
   vpri        = tr.vpri;

/* for snoops */
   parent      = tr.parent;
   for(int i =0; i< MAX_CORES ; i++)
   {
    snoop_done[i]  = tr.snoop_done[i];
    snoop_state[i] = tr.snoop_state[i];
   }
   snoop_count = tr.snoop_count;
   snoop_invalidate = tr.snoop_invalidate;

   // ref_data = tr.ref_data;
   ref_inst = tr.ref_inst;
   src = tr.src;
   dst = tr.dst;
   msg_size = tr.msg_size;
   llc_transfer_state = tr.llc_transfer_state;

  }
void hist();
void print();
};


/* STATE transition function */
bool determinel1miss(long long address, int cpuid, int read_or_write);
bool determinel2miss(long long address, int cpuid, int read_or_write);
void transaction_commit(TRANSTYPE *tr);

#endif //__TRANSACTION_H__
