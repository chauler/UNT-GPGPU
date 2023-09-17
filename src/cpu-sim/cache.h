/*****************************************************************************
 * CASPER: Cache Analysis, Simulation and Performance Exploration w/ Refstreams
 * 
 * Author: Ravi Iyer
 *
 * File:        cacheheader.h
 *
 * Description: This file consists of state definitions, component definitions
 *              and data structures for the cache components.
 *
 *****************************************************************************/
#ifndef __CACHE_H__
#define __CACHE_H__

#include <stdio.h>
#include <zlib.h>
#include <vector>
#include <map>

#define CPU_MODIFIED  0
#define EXCLUSIVE 1
#define SHARED    2
#define CPU_INVALID   3
#define NOT_FOUND 4
#define STALE_ENTRY 5
#define SHARED_BY_BOTH 6

#define CPUTYPE   -1
#define CACHETYPE -2
#define BUSTYPE   -3

#define ICACHE 0
#define DCACHE 1

#define MAX_SECTORS 8

#define CACHE_LINE_INTERLEAVING 0
#define MEM_PAGE_INTERLEAVING 1


extern char cache_state[4][14];

/* Forward declaration to avaoid circular header problem */
class TRANSTYPE;

TRANSTYPE* map_transaction( long long tr_id);
int FloorLog2(unsigned x);
long long Log2Mask(unsigned x);

class CLINE {
 public:
  long long address;
  short state;
  long long timetouched;  /* For LRU repl policy */
 // int pri;
 // long long data[16];
  bool cstatus;
  short csize;

  
  CLINE(){
  timetouched = 0;
  address = 0;
  state = CPU_INVALID;
  //pri = 0;
  csize = 16;
  cstatus = 0;
  // data[0] = data[1] = data[2] = data[3] = 0x0;
  // data[4] = data[5] = data[6] = data[7] = 0x0;
  // data[8] = data[9] = data[10] = data[11] = 0x0;
  // data[12] = data[13] = data[14] = data[15] = 0x0;

  }

void print()
{
  //printf("\n ADDR %llx STATE %s TIME %lld CSTATUS %d CSIZE %d",address,cache_state[state],timetouched,cstatus,csize);
  // printf("DATA ");
  // for(int i=0;i<16;i++)
    // printf("%llx ", data[i]);
//  printf("\n");
}


};

class CSET {
 public:	 
  int assoc;
  int numlines;
  std::vector<CLINE> clines;     /* List of lines in each set */

  CSET(int _assoc) 
  : assoc(_assoc), numlines(0)
  {
 
    for (int i = 0; i < assoc; ++i) 
    {
      clines.push_back(CLINE());
    }

  }
void kill()
    {
      clines.clear();
    }
void print()
  {
    for (int i = 0; i < clines.size(); ++i) 
      clines[i].print();
  }
 
};

struct bq_node {
  long long tr_id;
  std::vector<long long> mshr;
};

struct MSHR_entry {
	std::vector<long long> tr_id;
  long long line_address;
};

class CACHE {
 public:

  //Basic Parameters
  long cachesize;
  int linesize;
  int assoc;
  int numsectors;
  int is_snoop_filter;
  int numsets;
  char clabel[16];
  int level;
  long long  numrefs_for_repl;
    
  //Self Bookkeeping
  int numprevcaches;  /* lower level caches */
  int numnextcaches;  /* higher level caches */
  
  //helper variables (these should be private, but not all relevant cachesim
  //functions are methods of this class yet)
  int sector_shift;  //bits to shift to have sector be LSB
  int set_shift;  //bits to shift to have set be LSB
  int tag_shift;  //bits to shift to have tag be LSB
  int set_mask;  //after shifting, this mask yields the set
  int line_mask;  //after shifting, this mask yields the line
  int sector_mask;  //after shifting, this mask yields the sector
  int sets_used;

  std::vector<CSET> sets;
  std::vector<long long> queue;
  std::vector<bq_node> blocked_queue;

  std::vector<MSHR_entry> MSHR;
  std::vector<long long> MSHR_trid;
  std::vector<long long> MSHR_address;

/* Stats */
  long long refs;
  long long hits;
  long long shared_hits;
  long long misses;
  long long snoops;
  long long hitm;
  long long writebacks;
  long long invalidate_snoops;
  long long misses_per_core[128];
  long long refs_per_core[128];
  long long misses_per_pri[128];
  long long refs_per_pri[128];
  int bq_size;
  int rq_size;
  long long tot_bq_size;
  long long tot_rq_size;
  long long bq_size_samples;
  long long rq_size_samples;

  long long reads;
  long long writes;

  int shared;
  int modified;
  int invalid;
  int exclusive;

  void func_change_state(long long address, int nstate, long long *vaddress, int *vstate, long long data[16]);
  int  func_look_up(long long address, int update);

  int look_up(long long address, int update, TRANSTYPE *tr);
  void change_state(long long address, int newstate, TRANSTYPE *tr);
  long long int victim_lookup(long long address , int *pos);
  void replace_victim(long long address, long long vaddress, int state, int pos);
  
 
  int lookup_pri(long long address);
 
  void insert_queue(long long tr_id);
  void insert_queue_after_head(long long tr_id);
  long long delete_queue();
  bool queue_empty();
  long long queue_head();
  long long  queue_top_4(int trid);
  bool insert_blocked_queue(long long tr_id, long long tr_address);
  bool  delete_blocked_queue(long long tr_id, long long tr_address);
  long long  delete_queue_tr(long long tr_id);
  TRANSTYPE* blocked_queue_lookup(long long address);
  bool address_in_blocked_queue(long long address, long long tr_id);
 
  bool queue_full();
  bool blocked_queue_full();
  void reset_stats();
  void print();

  void func_change_state_on_hit(long long address, int nstate);
  CACHE(long size, int lsize, int assoc, int numsectors, int cpu, int cl, 
	char type);

  void kill()
  {
   for(int i =0; i < numsets; i++)
     sets[i].kill();
   sets.clear();
  }
  
};

class BTYPE {
  int caches_to_snoop;  /* Not used currently */
  CACHE *cachelist;
  int snoopresult;
};

  class CLEVEL {
  public:
    std::vector<CACHE> cache;
  int csize;
  int lsize;
  int assoc;
  int sectors;

  void kill()
  {
   for(int i =0; i < cache.size(); i++)
    cache[i].kill();
   cache.clear();
  }
  
  };

struct nic_entry_t {
  int src;
  int dst;
  int msg_size;
  long long clock;
  long long trid;
};

void 
insert_nic_queue(int src, int dst, int msg_size, long long clock, long long trid);
bool 
nic_queue_full(int node);
#endif
