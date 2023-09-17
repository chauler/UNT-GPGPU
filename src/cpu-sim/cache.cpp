/*****************************************************************************
 * File:        cachesim.c
 * Description: This file consists of several functions that comprise the
 *              cache simulation engine and core
 *
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <vector>
#include "transaction.h"
#include "pepsi_stats.h"
#include "cache.h"
#include "globals.h"
#include "defines.h"

// CACHE_DEBUG CHECK FLAGS
int CHECK_DUP = 0;
int NUMLINES = 0;
int cache_dbg = 0;
long long addr1_mask=0, addr2_mask=0;
int addr1_shift=0, addr2_shift=0;

int CACHE_DEBUG = cache_dbg;
//#define CACHE_DEBUG (cpu_global_clock>300)




#if !SHORT_CIRCUIT 
extern unsigned int num_ejt_msg, num_inj_msg;
#endif

char cache_state[4][14] = {"CPU_MODIFIED", "EXCLUSIVE", "SHARED", "CPU_INVALID"};

/* Local Functions */
TRANSTYPE* map_transaction( long long tr_id);
void check_set_occ();

// This function is looking address in the cache
// ,seeing whether block is there or not and 
// returning its state

  int
CACHE::func_look_up(long long address, int update)
{
  int setnumber = DECODE_SET(address);
  int numlines = assoc; 
  int setindex;

  std::vector<CLINE> &cacheset = sets[setnumber].clines;
  //Checking for duplicates
  if(CHECK_DUP)
  {
    for (int i = 0; i < numlines; i++)
    {
      if(cacheset[i].state == CPU_INVALID)
        continue;

      for (int j = 0; j< numlines ; j++)
      {
        if(cacheset[j].state == CPU_INVALID)
          continue;
        if((cacheset[i].address >> tag_shift == cacheset[j].address >> tag_shift) && i!=j && cacheset[i].address != 0)
        {
          printf("Err duplicate found in %s in set%d \n",clabel,setnumber);
          printf("Err dup i: LINE %d ADDR %llx TIME %lld STATE %s\n",
              i,cacheset[i].address,cacheset[i].timetouched, cache_state[cacheset[i].state]);
          printf("Err dup j: LINE %d ADDR %llx TIME %lld STATE %s\n",
              j,cacheset[j].address,cacheset[j].timetouched, cache_state[cacheset[j].state]);
          DEBUG_CPU=1;
          PRINT=1;
        } 

      }
    }
  }//Checking for duplicates ends here


  for (setindex = 0; setindex < numlines; setindex++)
  {
    if(CACHE_DEBUG)
      cacheset[setindex].print();

    if (((cacheset[setindex].address >> tag_shift) == (address >> tag_shift)) && cacheset[setindex].state != CPU_INVALID)
    {
      if (CACHE_DEBUG) printf("\n %s CACHE:func_look_up Found address %lx in ",clabel, address);
      if (CACHE_DEBUG) fflush(stdout);
      if (CACHE_DEBUG)  printf("state %s", cache_state[cacheset[setindex].state]);
      if (CACHE_DEBUG) fflush(stdout);
      if(update)
      {
        cacheset[setindex].timetouched = numrefs_for_repl;
      }

      return cacheset[setindex].state;
    }
  }
  return CPU_INVALID;

}

  void
CACHE::func_change_state_on_hit(long long address, int nstate)
{
  int setnumber = DECODE_SET(address);// (address >> set_shift) & set_mask;
  int numlines = assoc;
  int setindex;
  std::vector<CLINE> &cacheset = sets[setnumber].clines;

  for (setindex = 0; setindex < numlines; setindex++)
  {
    if(CACHE_DEBUG)
      cacheset[setindex].print();
    if (((cacheset[setindex].address >> tag_shift) == (address >> tag_shift)) && cacheset[setindex].state != CPU_INVALID)
    {
      if (CACHE_DEBUG) printf("CACHE:change state Found address %lx in ",address);
      if (CACHE_DEBUG) fflush(stdout);
      if (CACHE_DEBUG)  printf("state %s\n", cache_state[cacheset[setindex].state]);
      if (CACHE_DEBUG) fflush(stdout);
      cacheset[setindex].state = nstate;
      return;
    }
  } 

}
  void
CACHE::func_change_state(long long address, int nstate, long long *vaddress, int *vstate, long long data[16])
{
  int setnumber = DECODE_SET(address);// (address >> set_shift) & set_mask;
  int numlines = assoc;
  int setindex;
  
  std::vector<CLINE> &cacheset = sets[setnumber].clines;
  int tmp_invalid_cline = -1, tmp_victim_cline =0;
  long long tmp_min_timetouched = -1;

  if(numlines>=1)
    tmp_min_timetouched = cacheset[0].timetouched;
  else
    tmp_invalid_cline = 0;

  *vaddress = -1;
  *vstate = CPU_INVALID;
  //Checking for duplicates
  if(CHECK_DUP)
  {
    for (int i = 0; i < numlines; i++)
    {
      if(cacheset[i].state == CPU_INVALID)
        continue;

      for (int j = 0; j< numlines ; j++)
      {
        if(cacheset[j].state == CPU_INVALID)
          continue;
        if((cacheset[i].address >> tag_shift == cacheset[j].address >> tag_shift) && i!=j && cacheset[i].address != 0)
        {
          printf("Err duplicate found in %s in set%d \n",clabel,setnumber);
          printf("Err dup i: LINE %d ADDR %llx TIME %lld STATE %s\n",
              i,cacheset[i].address,cacheset[i].timetouched, cache_state[cacheset[i].state]);
          printf("Err dup j: LINE %d ADDR %llx TIME %lld STATE %s\n",
              j,cacheset[j].address,cacheset[j].timetouched, cache_state[cacheset[j].state]);
          DEBUG_CPU=1;
          PRINT=1;
        } 

      }
    }
  }//Checking for duplicates ends here


  std::vector<CLINE>::iterator iter= cacheset.begin();

  for (setindex = 0; setindex < numlines; setindex++)
  {
    if(CACHE_DEBUG)
      cacheset[setindex].print();
    if(cacheset[setindex].state != CPU_INVALID && tmp_min_timetouched > cacheset[setindex].timetouched)
    {
      tmp_victim_cline = setindex;
      tmp_min_timetouched = cacheset[setindex].timetouched;
    }
    if(cacheset[setindex].state == CPU_INVALID)
      tmp_invalid_cline = setindex;

    if (((cacheset[setindex].address >> tag_shift) == (address >> tag_shift)) && cacheset[setindex].state != CPU_INVALID)
    {
      if (CACHE_DEBUG) printf("CACHE:change state Found address %lx in ",address);
      if (CACHE_DEBUG) fflush(stdout);
      if (CACHE_DEBUG)  printf("state %s\n", cache_state[cacheset[setindex].state]);
      if (CACHE_DEBUG) fflush(stdout);
      cacheset[setindex].state = nstate;
      if(nstate == CPU_INVALID)
      {
        sets[setnumber].numlines--; // VALID LINES
 
      }
      return;
    }
    iter++;
  } // end of for
  if(nstate != CPU_INVALID)
  {
    if(sets[setnumber].numlines < assoc)
    {
      if(tmp_invalid_cline == -1)
      {
        printf("cache %s at set%d\n",clabel,setnumber);
        printf("numlines :%d (not full) but no invalid line found\n",sets[setnumber].numlines);
        sets[setnumber].print();
        exit(1);
      }
      sets[setnumber].numlines++;
   
      cacheset[tmp_invalid_cline].address = address;
      cacheset[tmp_invalid_cline].timetouched = numrefs_for_repl;
      cacheset[tmp_invalid_cline].state = nstate;
    }
    else
    {
      if(tmp_victim_cline >= assoc)
      {
        printf("cache %s at set%d\n",clabel,setnumber);
        printf("numlines :%d (not full) but no victim line found:%d\n",sets[setnumber].numlines, tmp_victim_cline);
        sets[setnumber].print();
        exit(1);
      }

      *vaddress = cacheset[tmp_victim_cline].address;
      *vstate = cacheset[tmp_victim_cline].state;
      cacheset[tmp_victim_cline].address = address;
      cacheset[tmp_victim_cline].state = nstate;
      cacheset[tmp_victim_cline].timetouched = numrefs_for_repl;
    }
  }

  //Checking for duplicates
  if(CHECK_DUP)
  {
    for (int i = 0; i < numlines; i++)
    {
      if(cacheset[i].state == CPU_INVALID)
        continue;

      for (int j = 0; j< numlines ; j++)
      {
        if(cacheset[j].state == CPU_INVALID)
          continue;
        if((cacheset[i].address >> tag_shift == cacheset[j].address >> tag_shift) && i!=j && cacheset[i].address != 0)
        {
          printf("Err duplicate found in %s in set%d \n",clabel,setnumber);
          printf("Err dup i: LINE %d ADDR %llx TIME %lld STATE %s\n",
              i,cacheset[i].address,cacheset[i].timetouched, cache_state[cacheset[i].state]);
          printf("Err dup j: LINE %d ADDR %llx TIME %lld STATE %s\n",
              j,cacheset[j].address,cacheset[j].timetouched, cache_state[cacheset[j].state]);
          DEBUG_CPU=1;
          PRINT=1;
        } 

      }
    }
  }//Checking for duplicates ends here

  return;

}


  int
CACHE::look_up(long long address, int update, TRANSTYPE *tr)
{
  int setnumber = DECODE_SET(address);// (address >> set_shift) & set_mask;
  int numlines = assoc;

  std::vector<CLINE> &cacheset = sets[setnumber].clines;

  if (CACHE_DEBUG) printf("CACHE:look_up in cache %s at set%d\n",clabel,setnumber);
  if (CACHE_DEBUG) fflush(stdout);
  int setindex;

  //Checking for duplicates
  if(CHECK_DUP)
  {
    for (int i = 0; i < numlines; i++)
    {
      if(cacheset[i].state == CPU_INVALID)
        continue;

      for (int j = 0; j< numlines ; j++)
      {
        if(cacheset[j].state == CPU_INVALID)
          continue;
        if((cacheset[i].address >> tag_shift == cacheset[j].address >> tag_shift) && i!=j && cacheset[i].address != 0)
        {
          printf("Err duplicate found in %s in set%d \n",clabel,setnumber);
          printf("Err dup i: LINE %d ADDR %llx TIME %lld STATE %s\n",
              i,cacheset[i].address,cacheset[i].timetouched, cache_state[cacheset[i].state]);
          printf("Err dup j: LINE %d ADDR %llx TIME %lld STATE %s\n",
              j,cacheset[j].address,cacheset[j].timetouched, cache_state[cacheset[j].state]);
          DEBUG_CPU=1;
          PRINT=1;
        } 

      }
    }
  }//Checking for duplicates ends here

  //Line search begins here
  for (setindex = 0; setindex < numlines; setindex++)
  {
    if(CACHE_DEBUG)
      cacheset[setindex].print();

    if (((cacheset[setindex].address >> tag_shift) == (address >> tag_shift)) && cacheset[setindex].state != CPU_INVALID)
    {
      if (CACHE_DEBUG) printf("\n CACHE:look_up Found address %lx in ",address);
      if (CACHE_DEBUG) fflush(stdout);
      if (CACHE_DEBUG)  printf("state %s", cache_state[cacheset[setindex].state]);
      if (CACHE_DEBUG) fflush(stdout);
      if(update)
      {
        cacheset[setindex].timetouched = numrefs_for_repl;
       
         /* if(tr->type == WRITE && (cacheset[setindex].state == CPU_MODIFIED || cacheset[setindex].state == EXCLUSIVE))
          {
            for(int j=0; j<linesize/SET_SEG_SIZE; j++)
              cacheset[setindex].data[j] = tr->data[j];
          }
          else
          {
            if(tr->type == SNOOP && cacheset[setindex].state == CPU_MODIFIED)
              for(int j=0; j<linesize/SET_SEG_SIZE; j++)
              {
                tr->data[j] = cacheset[setindex].data[j];
                tr->linesize = fetch_csize(tr->data); 
                //if(tr->data[j] != tr->trace_data[j])
                //printf("Trace data not matching stored cache data!!!!\n");
              }

          } */
      }
      break;
    }
  }
  if (setindex >= numlines)
  {
    if (CACHE_DEBUG) printf("\n CACHE:look_up Did not find %lx",
        address);
    if (CACHE_DEBUG) fflush(stdout);
    return(CPU_INVALID);
  }

  switch (cacheset[setindex].state) 
  {
    case SHARED: return(SHARED); printf("Err!!\n"); break;
    case EXCLUSIVE: return(EXCLUSIVE); break;
    case CPU_MODIFIED: return(CPU_MODIFIED); break;
    case CPU_INVALID: return(CPU_INVALID); break;
    default:
                  if (CACHE_DEBUG) printf("CACHE:look_up Cannot come here\n");
  }

}

  void
CACHE::change_state(long long address, int nstate, TRANSTYPE *tr)
{
  int setnumber = DECODE_SET(address);// (address >> set_shift) & set_mask;
  int numlines = assoc;//sets[setnumber].numlines;
  std::vector<CLINE> &cacheset = sets[setnumber].clines;
  std::vector<CLINE>::iterator iter= cacheset.begin();

  int setindex;
  if (CACHE_DEBUG) printf("CACHE:change_state in cache %s at set%d\n",clabel,setnumber);
  if (CACHE_DEBUG) fflush(stdout);

  for (setindex = 0; setindex < numlines; setindex++)
  {
    if(CACHE_DEBUG)
      cacheset[setnumber].print();

    if (((cacheset[setindex].address >> tag_shift) == (address >> tag_shift)) && cacheset[setindex].state != CPU_INVALID)
    {
      if(CACHE_DEBUG) printf("CACHE:change_state Found address %lx ", address); if (DEBUG_CPU) fflush(stdout);
      if(CACHE_DEBUG) printf("in state %s changing to state ",cache_state[cacheset[setindex].state] ); if (DEBUG_CPU) fflush(stdout);
      if(CACHE_DEBUG) printf("%s\n", cache_state[nstate]); if (DEBUG_CPU) fflush(stdout);

      cacheset[setindex].state = nstate;
      if(nstate == CPU_INVALID)
        sets[setnumber].numlines--;

      break;
    } // End of cache hit for set
    iter++;
  }// End of line search

  if (setindex >= numlines)
  {
    if (CACHE_DEBUG) printf("CACHE:change_state Did not find %lx \n", address);
    if (CACHE_DEBUG) fflush(stdout);
  }

  //Checking for duplicates
  if(CHECK_DUP)
  {
    for (int i = 0; i < numlines; i++)
    {
      if(cacheset[i].state == CPU_INVALID)
        continue;

      for (int j = 0; j< numlines ; j++)
      {
        if(cacheset[j].state == CPU_INVALID)
          continue;
        if((cacheset[i].address >> tag_shift == cacheset[j].address >> tag_shift) && i!=j && cacheset[i].address != 0)
        {
          printf("Err duplicate found in %s in set%d %s\n",clabel,setnumber, __func__);
          printf("Err dup i: LINE %d ADDR %llx TIME %lld STATE %s\n",
              i,cacheset[i].address,cacheset[i].timetouched, cache_state[cacheset[i].state]);
          printf("Err dup j: LINE %d ADDR %llx TIME %lld STATE %s\n",
              j,cacheset[j].address,cacheset[j].timetouched, cache_state[cacheset[j].state]);
          DEBUG_CPU=1;
          PRINT=1;
          getchar();
        } 

      }
    }
  }
  //Checking for numlines
  if(NUMLINES)
  {
    int inv = 0;
    for (int i = 0; i < assoc; i++)
      if(cacheset[i].state == CPU_INVALID)
        inv++;
    int occ = assoc - inv;        
    if(abs(sets[setnumber].numlines - occ) > 0) 
    { 
      printf("ERR in NUMLINES!! numlines %d occ %d", sets[setnumber].numlines , assoc - inv);
      getchar();
      print();
    }
  }
  return; 
}
/*********
  END
*********/
  long long 
CACHE::victim_lookup(long long int address , int *pos)
{
  int i,j;
  int minpos = 0;
  int setnumber =  DECODE_SET(address);//(address >> set_shift) & set_mask;
  long long mintime = cpu_global_clock;
  unsigned long long  vic_addr = 0;
  int numlines = sets[setnumber].numlines;
  std::vector<CLINE> &cacheset = sets[setnumber].clines;
  if (CACHE_DEBUG) printf("CACHE:victim_lookup in cache %s at set%d\n",clabel,setnumber);
  if (CACHE_DEBUG) fflush(stdout);
  
    // Empty lines , no need to replace
    if(sets[setnumber].numlines < sets[setnumber].assoc)
    {
      for (i = 0;i <= assoc; i++)
      {
        if(CACHE_DEBUG )
          printf("\n LINE %d ADDR %llx TIME %lld STATE %s",
              i,cacheset[i].address,cacheset[i].timetouched, cache_state[cacheset[i].state]);

        if (cacheset[i].state == CPU_INVALID)
        {
          cacheset[i].address = address;

          cacheset[i].timetouched = numrefs_for_repl ;
          if (CACHE_DEBUG) printf("\n %s CACHE:victim_lookup found victim in CPU_INVALID , numlines %d\n",clabel, i);
          if (CACHE_DEBUG) fflush(stdout);
          *pos = i;
          return address;
        }
      }

    }
    // Replace needed LRU
    else
    {
      mintime = cacheset[0].timetouched;
      for (i = 0;i < assoc; i++)
      {
        if(CACHE_DEBUG )
          printf("\n LINE %d ADDR %llx TIME %lld STATE %s",
              i,cacheset[i].address,cacheset[i].timetouched, cache_state[cacheset[i].state]);

        if (mintime > cacheset[i].timetouched && cacheset[i].state != CPU_INVALID)
        {
          mintime = cacheset[i].timetouched; 
          minpos = i;
        }
      }
      if (CACHE_DEBUG) printf("\n CACHE:victim_lookup found victim in state %s at pos %d", cache_state[cacheset[minpos].state],minpos);
      if (CACHE_DEBUG) fflush(stdout);
	
      *pos = minpos;
      return(cacheset[minpos].address);
    }
}

/*********
  END
*********/

  void
CACHE::replace_victim(long long vaddress, long long address, int state, int pos)
{
  int setnumber = DECODE_SET(address);// (address >> set_shift) & set_mask;
  int numlines = assoc;//sets[setnumber].numlines;
  std::vector<CLINE> &cacheset = sets[setnumber].clines;

  if (CACHE_DEBUG) printf("\n CACHE:replace_victim in cache %s at set%d",clabel,setnumber);
  if (CACHE_DEBUG) fflush(stdout);
  int setindex, p;
  if( address != vaddress)
  {
    for (setindex = 0; setindex < numlines; setindex++)
    {
      if(CACHE_DEBUG) cacheset[setindex].print();

      if (((cacheset[setindex].address >> tag_shift) == (vaddress >> tag_shift)) && cacheset[setindex].state != CPU_INVALID)
      {
        p = setindex;
        if(p != pos)
        { 
          printf("Err in replacement, set full case p:%d pos:%d in cache %s \n",p,pos,clabel);
          printf("Err vaddress: %llx, address: %llx\n",vaddress, address);
          printf("Err line found in replace_victim: \n");
          cacheset[setindex].print();
          printf("Err line found in victim_lookup: \n");
          cacheset[pos].print();
          getchar();
        }

        cacheset[setindex].address = address;
        cacheset[setindex].timetouched = numrefs_for_repl;
        cacheset[setindex].state = state;

        /*for(int i = 0; i<linesize/SET_SEG_SIZE; i++)
        {
          cacheset[setindex].data[i] = tr->data[i];
          if(CACHE_DEBUG)
            if(tr->type == READ && tr->data[i] != tr->trace_data[i])
              printf("Trace data not matching stored cache data!!!! %s\n",__func__);
        }*/
        break;
      } // End of line hit
    } // End of line search
  } // End of replacement for empty set case
  else
  {
    sets[setnumber].numlines++;
    cacheset[pos].state = state;
    /*for(int i = 0; i<linesize/SET_SEG_SIZE; i++)
    {
      cacheset[pos].data[i] = tr->data[i];
      if(CACHE_DEBUG)
        if(tr->type == READ && tr->data[i] != tr->trace_data[i])
          printf("Trace data not matching stored cache data!!!! %s\n",__func__);
    }*/
    p = pos;
  }

  if (CACHE_DEBUG) printf("\n %s CACHE:replace_victim victim address %lx target address ", clabel, vaddress);
  if (CACHE_DEBUG) fflush(stdout);
  if (CACHE_DEBUG) printf("%llx ",address); 
  if (CACHE_DEBUG) fflush(stdout);
  if (CACHE_DEBUG) printf("at setindex %d ",p); 
  if (CACHE_DEBUG) fflush(stdout);
  if (CACHE_DEBUG) printf("to state %s", cache_state[state]);
  if (CACHE_DEBUG) fflush(stdout);


    if(CHECK_DUP)
  {
    //Checking for duplicates
    for (int i = 0; i < numlines; i++)
    {
      if(cacheset[i].state == CPU_INVALID)
        continue;

      for (int j = 0; j< numlines ; j++)
      {
        if(cacheset[j].state == CPU_INVALID)
          continue;
        if((cacheset[i].address >> tag_shift == cacheset[j].address >> tag_shift) && i!=j && cacheset[i].address != 0)
        {
          printf("Err duplicate found in %s in set%d in %s\n",clabel,setnumber, __func__);
          printf("Err dup i: LINE %d ADDR %llx TIME %lld STATE %s\n",
              i,cacheset[i].address,cacheset[i].timetouched, cache_state[cacheset[i].state]);
          printf("Err dup j: LINE %d ADDR %llx TIME %lld STATE %s\n",
              j,cacheset[j].address,cacheset[j].timetouched, cache_state[cacheset[j].state]);
          DEBUG_CPU=1;
          PRINT=1;
          getchar();
        } 

      }
    }
  }

  //Checking for numlines
  if(NUMLINES)
  {
    int inv = 0;
    for (int i = 0; i < assoc; i++)
      if(cacheset[i].state == CPU_INVALID)
        inv++;
    int occ = assoc - inv;        
    if(abs(sets[setnumber].numlines - occ) > 0) 
    { 
      printf("ERR in NUMLINES!! numlines %d occ%d\n", sets[setnumber].numlines , assoc - inv);
      getchar();
      print();
    }
  }
  return; 
}
/*********
  END
*********/


/* QUEUE MANAGEMENT */ 


  void 
CACHE::insert_queue(long long tr_id)
{
  rq_size++;
  if(level)
  {
    tot_rq_size += rq_size;
    rq_size_samples++;
  }
  queue.push_back(tr_id);
}

  void 
CACHE::insert_queue_after_head(long long tr_id)
{
  rq_size++;
  if(level)
  {
    tot_rq_size += rq_size;
    rq_size_samples++;
  }
  if(queue.size() >= 1)
  {
    std::vector<long long>::iterator start = queue.begin();
    TRANSTYPE *tr = map_transaction(*start);
    if(tr->state != MLC_BQ_OVERFLOW && tr->state != LLC_BQ_OVERFLOW)
      start++;
    queue.insert(start, tr_id);
  }
  else
    queue.push_back(tr_id);
  return;
}


  long long 
CACHE::delete_queue()
{
  if(queue.empty())
    return -1;
  else
  {
    rq_size--;
    if(level)
    {
      tot_rq_size += rq_size;
      rq_size_samples++;
    }
    std::vector<long long>::iterator start = queue.begin();
    long long tr_id = *start;
    queue.erase(start);
    return tr_id;
  }
}

  bool 
CACHE::queue_empty()
{
//  if(BYPASS_MEM_BUS && level > LLC)
//    return true;

  return queue.empty();
}

  bool 
CACHE::blocked_queue_full()
{
  bq_size = blocked_queue.size();
  for(int i  = 0; i < blocked_queue.size(); i++)
  {
    if(!blocked_queue[i].mshr.empty())
      bq_size += blocked_queue[i].mshr.size();  
  }
  tot_bq_size += bq_size;
  bq_size_samples++;
  if (bq_size < cache_bq_size[level])
    return false;
  else
    return true;      
}

  bool 
CACHE::queue_full()
{
  int size = queue.size();

  if(BYPASS_MEM_BUS && level > LLC)
    return false;
  //if(level == MLC)
  //return false;

  if(CACHE_DEBUG)
    if(rq_size != size)
      printf("rq_size calc err rq%d curr%d\n", rq_size, size);
  if(level)
  {
    tot_rq_size += size;
    rq_size_samples++;
  }
  if (size < cache_rq_size[level])
    return false;
  else
    return true;      
}


  long long 
CACHE::queue_head()
{
  if(queue.size() == 0)
    return -1;
  std::vector<long long>::iterator start = queue.begin();
  long long tr_id = *start;
  return tr_id;
}



  long long
CACHE::queue_top_4(int trid)
{
//	  printf("\n Queue Size is : %d",queue.size());
//	  printf("\n Searching for %d till size is : %d",trid, queue.size()<4?queue.size():4);
  if(queue.size() == 0)
    return -1;

  int i = 0;

  for(i=0; i<queue.size()<4?queue.size():4;i++)
	  if( queue.at(i) == trid)
		 return trid;

  return -1;

//
//
//  std::vector<long long>::iterator start = queue.begin();
//  long long tr_id = *start;
//  return tr_id;
}



  bool 
CACHE::address_in_blocked_queue(long long address, long long tr_id)
{
  TRANSTYPE *tr;

  for(int i  = 0; i < blocked_queue.size(); i++)
  {
    tr = map_transaction(blocked_queue[i].tr_id);
    if((tr->address & ~line_mask) == (address & ~line_mask) && tr->tr_id != tr_id)
      return true;
  }

  return false;
}

bool
CACHE::insert_blocked_queue(long long tr_id, long long tr_address)
{
	//if(MSHR.size() > 16)    //FIXXX  == mshr parameterize
	//{
	//	printf("\n MSHR size exceeded!\n");
	//	abort();
	//}

	for(int i = 0; i < MSHR.size(); i++)
	{
	    if((tr_address & ~line_mask) == (MSHR.at(i).line_address & ~line_mask))
	    {
	    	//match for an cache line masked address  //add the tr_id at the end of MSHR entry'e tr vector.
	    	MSHR.at(i).tr_id.push_back(tr_id);
	    	return true;
	    }
	}

	//New MSHR entry inserted.
	MSHR_entry temp_mshr;
	temp_mshr.line_address = tr_address;
	temp_mshr.tr_id.push_back(tr_id);

	MSHR.push_back(temp_mshr);

	return false;
}


bool
CACHE::delete_blocked_queue(long long tr_id, long long tr_address)
{
	//MSHR_address.erase(std::remove(MSHR_address.begin(), MSHR_address.end(), tr_address), MSHR_address.end());
	//MSHR_trid.erase(std::remove(MSHR_trid.begin(), MSHR_trid.end(), tr_address), MSHR_trid.end());
	for(int i = 0; i < MSHR.size(); i++)
	{
	    if((tr_address & ~line_mask) == (MSHR.at(i).line_address & ~line_mask))
	    {
	    	for(int j = 0; j< MSHR.at(i).tr_id.size(); j++)
	    	{
	            //if(tr_id == MSHR.at(i).tr_id.at(j))
	            //{
	            	//This tr_id is myself. I should actually unblock only others dependent on me.
	            //}
	            //else
	    		//{
	    		//if(tr_id == 96)
	    			//printf("\n Dep tr id on %lld are : %lld", tr_id,  MSHR.at(i).tr_id.at(j));

	            	TRANSTYPE *tr_blocked;
					tr_blocked = map_transaction(MSHR.at(i).tr_id.at(j));
					tr_blocked->timestamp = cpu_global_clock+1;  //insert into cache next clock cycle
					tr_blocked->state = EXIT;
	    		//}
	    	}
	    	MSHR.erase (MSHR.begin()+i);
	    }
	}
	return true;
}


/*  bool
CACHE::insert_blocked_queue(long long tr_id)
{
  // if(blocked_queue_full())
  //   printf("ERR! BLOCKED QUEUE FULL \n");

  bool insert = true, mshr = false ;
  TRANSTYPE *tr, *tr_ins;
  int i,j;
  std::vector<bq_node>::iterator temp;
  bq_node ins_node;
  bq_node *ins = &ins_node;
  ins->tr_id = tr_id;

  tr_ins = map_transaction(tr_id);
  if(blocked_queue.empty())
  {
    blocked_queue.push_back(*ins);
    return true;
  }

  for(i  = 0; i < blocked_queue.size(); i++)
  {
    if( blocked_queue[i].tr_id == tr_id)
    {
      insert = false;
      break;
    }
    tr = map_transaction(blocked_queue[i].tr_id);
    if((tr->address & ~line_mask) == (tr_ins->address & ~line_mask))
    {
      mshr = true;
      break;
    }
  }
  if(!insert)
    return true;
  if(!mshr)
  {
    blocked_queue.push_back(*ins);
    return true;
  }
  else
  {
    insert = true;
    for(j = 0; j< blocked_queue[i].mshr.size(); j++)
      if(blocked_queue[i].mshr[j] == tr_id)
      {
        insert = false;
        break;
      }
    if(insert)
    {
      blocked_queue[i].mshr.push_back(tr_id);
      return false;
    }
    else
      return true;
  }

}
*/

/*  long long
CACHE::delete_blocked_queue(long long tr_id)
{
  int i,j;
  bool del = false;
  TRANSTYPE *tr;
  std::vector<bq_node>::iterator temp;
  std::vector<long long>::iterator temp_mshr;

  if(blocked_queue.empty())
    return -1;

  for(i  = 0, temp = blocked_queue.begin(); i < blocked_queue.size(); i++,temp++)
    if( blocked_queue[i].tr_id == tr_id)
    {
      del = true;
      break;
    }
    else
    {
      for(j = 0,temp_mshr=blocked_queue[i].mshr.begin(); j< blocked_queue[i].mshr.size(); j++,temp_mshr++)
        if(blocked_queue[i].mshr[j] == tr_id)
        {
          blocked_queue[i].mshr.erase(temp_mshr);
          return tr_id;
        }
    }


  if(!del)
    return -1;

  if(blocked_queue[i].mshr.empty())
    blocked_queue.erase(temp);
  else
  {
    blocked_queue[i].tr_id = blocked_queue[i].mshr[0];
    tr = map_transaction(blocked_queue[i].tr_id);
    tr->mshr_pending[level] = true;
    blocked_queue[i].mshr.erase(blocked_queue[i].mshr.begin());

  }
  return tr_id;

}*/

  long long 
CACHE::delete_queue_tr(long long tr_id)
{
  int i;
  bool del = false;
  TRANSTYPE *tr;
  std::vector<long long>::iterator temp;

  if(queue.empty())
    return -1;

  for(i  = 0, temp = queue.begin(); i < queue.size(); i++,temp++)
    if( queue[i] == tr_id)
    {
      del = true;
      break;
    }

  if(!del)
    return -1;

  rq_size--;
  if(level)
  {
    tot_rq_size += rq_size;
    rq_size_samples++;
  }
  queue.erase(temp);

  return tr_id;

}


/*********************************************************************
 * Constructor for the Cache object.  Used to be cacheinit().
*********************************************************************/
CACHE::CACHE(long size, int lsize, int assoc, int numsectors, 
    int cpu, int cl, char type)
: cachesize(size), linesize(lsize), assoc(assoc), numsectors(numsectors)
{
  int i,j,k;

  numsets = size/(lsize*assoc);

  //sprintf(clabel, "P%d_C%d_%c",cpu, cl, type);
  if(cl == FLC)
      sprintf(clabel, "FLC%d", cpu);
  else if(cl == MLC)
    sprintf(clabel, "MLC%d", cpu);
  else if(cl == LLC)
    sprintf(clabel, "LLC%d", cpu);
  else
    sprintf(clabel, "MEMORY");
  if(STAT)
  {
    printf("Initializing Cache %s:\n", clabel);
    printf("\tCache Size: %d bytes\n", cachesize);
    printf("\tLine Size: %d bytes\n", linesize);
    printf("\tAssoc: %d-way\n", assoc);
    printf("\tNum Sets: %d\n", numsets);
  }
  level = cl;
  numrefs_for_repl = 0;
  numprevcaches = 0;
  numnextcaches = 0;


  /* initializing the private variables */

  if(cl == LLC)
  {
    set_shift = FloorLog2(lsize-1);
    set_mask = (unsigned)Log2Mask(FloorLog2(numsets - 1));
    //This removes the bits which decode the bank from the address
    //addr1 is the bits in the original address before that bank bits start
    //addr2 is the bits in the original address after the bank bits end
    addr1_shift = 0;
    addr1_mask  = Log2Mask(bank_shift);
    addr2_shift = FloorLog2(num_llc_banks-1);
    addr2_mask  = ~(Log2Mask(bank_shift + FloorLog2(num_llc_banks-1)));
  }
  else
  {
    set_shift = FloorLog2(lsize-1);
    set_mask = (unsigned)Log2Mask(FloorLog2(numsets - 1));

    //printf("\n JOOMLA : set_shift = %d ;; set_mask = %d\n\n\n", set_shift, set_mask);

  }

  tag_shift = set_shift + FloorLog2(numsets-1);
  line_mask = (unsigned)Log2Mask(FloorLog2(linesize - 1));
  //printf("\n JOOMLA : tag_shift = %d ;; line_mask = %d\n\n\n", tag_shift, line_mask);

  if(DEBUG_CPU || 1)
  {
	printf("\nJOOMLA\n");
    printf("%s: numsets :%d set_shift:%d set_mask :%d\n",clabel,numsets,set_shift,set_mask);
    printf("%s: tag_shift:%d\n",clabel,tag_shift);
    printf("%s: line_mask :%d\n",clabel,line_mask);
  }

  for(int i = 0; i< numcpus; i++)
  {
    misses_per_core[i] = 0;
    refs_per_core[i] = 0;
  }

  /* Initializing the sets */

  for (i = 0;i < numsets;i++) {
    sets.push_back(CSET(assoc));
  }
   nic_queue_size = 32;//cache_bq_size[MLC]*2;

  /* Stats */
  refs = 0;
  hits = 0;
  misses = 0;
  shared_hits=0; 
  writebacks = 0;
  snoops = 0;
  hitm = 0;
  invalidate_snoops = 0;
  tot_bq_size=0;
  tot_rq_size=0;
  rq_size_samples=0;
  bq_size_samples=0;
  reads=0;
  writes=0;
  bq_size=0;
  rq_size=0;
  
}

  void
CACHE::reset_stats()
{
  /* Stats */
  refs =0;
  hits = 0;
  misses = 0;
  writebacks = 0;
  snoops = 0;
  hitm = 0;
  invalidate_snoops = 0;
  tot_bq_size=0;
  tot_rq_size=0;
  rq_size_samples=0;
  bq_size_samples=0;
  reads=0;
  writes=0;
}

  void
CACHE::print()
{
  for(int i = 0; i<sets.size(); i++)
  {
    printf("\nSET %d\n",i);
    sets[i].print();
  }

}

  int
FloorLog2(unsigned x)
{
  int y = 0;
  unsigned r = x;
  while(r)
  {
    y++;
    r=x>>y;
  }
  return y;
}
  long long
Log2Mask(unsigned x)
{
  long long mask=0;
  int i = 0;
  while(i<x)
  {
    mask = mask | (0x1 << i);
    i++;
  }
  return mask;
}



TRANSTYPE* map_transaction( long long tr_id)
{
  std::vector<TRANSTYPE>::iterator temp;
  for( temp = tr_pool.begin() ; temp != tr_pool.end() ; temp++)
  {
    if(temp->tr_id == tr_id)
    {
      return &(*temp);
    }
  }
  //if(CACHE_DEBUG)
  printf("ERROR :: map transaction returning NULL!!! id:%lld\n",tr_id);
  //print_pool();
  print_mlc();
  print_llc();
  //getchar();
  return NULL;
}

  void  insert_nic_queue(int src , int dst, int size, long long clock, long long trid)
{

  nic_entry_t n;
  char c;

	//dst = cmp_network_nodes - 1;

  if(src < 0 || src > cmp_network_nodes || dst < 0 || dst >cmp_network_nodes || size <= 0)
  {
    printf("ERR NoX NiC Inject error!! src %d dst %d size:%d\n",src,dst,size);
    printf("Changing to CACHE_DEBUG mode (y/n) ?\n");
    c = getchar();
    if(c=='n')
      exit(1);
    return; 
  }
  if(DEBUG_CPU)
    printf("NoX NiC Inject transaction:%lld ---> src %d dst %d clock:%lld\n",trid,src,dst,cpu_global_clock);
  csim_inj_msgs++;
  n.src = src;
  n.dst = dst;
  n.msg_size = size;
  n.clock = clock;
  n.trid = trid;
  nic_queue[src][dst].push_back(n);
}


  bool 
nic_queue_full(int dir)
{
  int size = 0;
  for(int n=0; n < numcpus; n++)
    size +=  nic_queue[dir][n].size();

  if (size < nic_queue_size)
    return false;
  else
    return true;      
}
