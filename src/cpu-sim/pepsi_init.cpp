/*****************************************************************************
 * File:        pepsi_init.c
 * Description: This file consists primarily all the functions for
 *              initializing the simulation parameters.
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

/* FUNCTIONS for system configuration SETUP */
inline char *get_next(char *token, char *config_file)
{
  if (!(token = strtok(NULL, ":"))) {
    printf("something wrong with config file %s!\n", config_file);
    exit(1);
  }
  return token;
}


void sim_setup(char *config_file)
{
  int def_llc_bank = 0;

  int mem_controllers = 0;

  if (config_file != NULL) {
    printf("setting up from config file %s\n", config_file);
    char buffer[100];
    char *token;
    FILE *fp = fopen(config_file, "r");
    if (!fp)
    { printf("Error opening file %s\n", config_file); exit(1); }

    /* Parse all options */
    while (fgets(buffer, 100, fp)) {
      if(strstr(buffer,"#"))
      {
        printf("Ignoring comment : %s",buffer);
        strcpy(buffer,"");
        continue;
      }
      if (!(token = strtok(buffer, ":"))) {
        printf("something wrong with config file %s!\n", config_file);
        exit(1);
      }

      if (!strcmp(buffer, "llc_shared")) {
        token = get_next(token, config_file);
        llc_shared = atoi(token);
      }

      if (!strcmp(buffer, "network_lat")) {
        token = get_next(token, config_file);
        short_circuit_icn_latency = atoi(token);
      }
      if (!strcmp(buffer, "ipc_print_freq")) {
        token = get_next(token, config_file);
        ipc_print_freq = atoi(token);
      }
//      if (!strcmp(buffer, "numcpus")) {
//        token = get_next(token, config_file);
//        numcpus = atoi(token);
//      }
      
      if (!strcmp(buffer, "page_bank_map")) {
        token = get_next(token, config_file);
        bank_map = MEM_PAGE_INTERLEAVING;
      }


      if (!strcmp(buffer, "llc_banks")) {
        token = get_next(token, config_file);
        num_llc_banks = atoi(token);
        def_llc_bank = 1;
      }
	  
      if (!strcmp(buffer, "mem_controllers")) {
        token = get_next(token, config_file);
        mem_controllers = atoi(token);
      }

      if (!strcmp(buffer, "core_transfer_time")) {
        token = get_next(token, config_file);
        core_transfer_time = atoi(token);
      }

      if (!strcmp(buffer, "mlc_lookup_time")) {
        token = get_next(token, config_file);
        mlc_lookup_time = atoi(token);
      }
      if (!strcmp(buffer, "flc_lookup_time")) {
        token = get_next(token, config_file);
        flc_lookup_time = atoi(token);
      }
      if (!strcmp(buffer, "hop_time")) {
        token = get_next(token, config_file);
        hop_time = atoi(token);
      }


      if (!strcmp(buffer, "llc_transfer_time")) {
        token = get_next(token, config_file);
        llc_transfer_time = atoi(token);
      }

      if (!strcmp(buffer, "mem_transfer_time")) {
        token = get_next(token, config_file);
        mem_transfer_time = atoi(token);
      }

      if (!strcmp(buffer, "llc_lookup_time")) {
        token = get_next(token, config_file);
        llc_lookup_time = atoi(token);
      }

      if (!strcmp(buffer, "llc_victim_lookup_time")) {
        token = get_next(token, config_file);
        llc_victim_lookup_time = atoi(token);
      }

      if (!strcmp(buffer, "mlc_victim_lookup_time")) {
        token = get_next(token, config_file);
        mlc_victim_lookup_time = atoi(token);
      }

      if (!strcmp(buffer, "DEBUG")) {
        token = get_next(token, config_file);
        DEBUG_CPU = atoi(token);
      }
      if (!strcmp(buffer, "RECORD")) {
        token = get_next(token, config_file);
        RECORD = atoi(token);
      } 

      if (!strcmp(buffer, "CACHE_QOS")) {
        token = get_next(token, config_file);
        CACHE_QOS = atoi(token);
      }
      if (!strcmp(buffer, "core_raw_cpi")) {
        token = get_next(token, config_file);
        core_raw_cpi = atof(token);
      }

      if (!strcmp(buffer, "priority_levels")) {
        token = get_next(token, config_file);
        priority_levels = atoi(token);
      }
      if (!strcmp(buffer, "priority_occ_limit")) {
        token = get_next(token, config_file);
        int index = atoi(token);
        token = get_next(token, config_file);
        priority_occ_limit[index]=atoi(token);
      }

      if (!strcmp(buffer, "RECORD_NOC")) {
        token = get_next(token, config_file);
        RECORD_NOC = atoi(token);
      }

      if (!strcmp(buffer, "EP_RECORD_NOC")) {
        token = get_next(token, config_file);
        EP_RECORD_NOC = atoi(token);
      }



      if (!strcmp(buffer, "PRINT")) {
        token = get_next(token, config_file);
        PRINT = atoi(token);
      }

      if (!strcmp(buffer, "INTERACTIVE")) {
        token = get_next(token, config_file);
        INTERACTIVE = atoi(token);
      }
      if (!strcmp(buffer, "STAT")) {
        token = get_next(token, config_file);
        STAT = atoi(token);
      }

      if (!strcmp(buffer, "stat_interval")) {
        token = get_next(token, config_file);
        stat_interval = atol(token);
      }



      if (!strcmp(buffer, "threads_per_core")) {
        token = get_next(token, config_file);
        threads_per_proc = atoi(token);
      }


      if (!strcmp(buffer, "mlc_blocked_queue_size")) {
        token = get_next(token, config_file);
        cache_bq_size[MLC] = atoi(token);
      }
      
      if (!strcmp(buffer, "flc_blocked_queue_size")) {
            token = get_next(token, config_file);
            cache_bq_size[FLC] = atoi(token);
      }
      if (!strcmp(buffer, "mem_blocked_queue_size")) {
        token = get_next(token, config_file);
        cache_bq_size[MEM] = atoi(token);
      }

      if (!strcmp(buffer, "mlc_request_queue_size")) {
        token = get_next(token, config_file);
        cache_rq_size[MLC] = atoi(token);
      }

      if (!strcmp(buffer, "flc_request_queue_size")) {
        token = get_next(token, config_file);
        cache_rq_size[FLC] = atoi(token);
      }
      if (!strcmp(buffer, "mem_request_queue_size")) {
        token = get_next(token, config_file);
        cache_rq_size[2] = atoi(token);
      }


      if (!strcmp(buffer, "llc_blocked_queue_size")) {
        token = get_next(token, config_file);
        cache_bq_size[LLC] = atoi(token);
      }

      if (!strcmp(buffer, "llc_request_queue_size")) {
        token = get_next(token, config_file);
        cache_rq_size[LLC] = atoi(token);
      }


      if (!strcmp(buffer, "numclevels")) {
        token = get_next(token, config_file);
        numclevels = atoi(token);

        for (int i = 0; i < numclevels; ++i) {
          clevel.push_back(CLEVEL());
        }
      }
      if (!strcmp(buffer, "clevel")) {
        token = get_next(token, config_file);
        int index;
        if (!strcmp(token, "flc")) {
          index = FLC;
        }
        if (!strcmp(token, "mlc")) {
          index = MLC;
        } 
        if (!strcmp(token, "llc")) {
          index = LLC;
        }
        token = get_next(token, config_file);
        if (!strcmp(token, "csize")) {	
          token = get_next(token, config_file);
          clevel[index].csize = atoi(token);
        }
        if (!strcmp(token, "lsize")) {	
          token = get_next(token, config_file);
          clevel[index].lsize = atoi(token);
//          llc_linesize = clevel[index].lsize;
        }
        if (!strcmp(token, "assoc")) {	
          token = get_next(token, config_file);
          clevel[index].assoc = atoi(token);
        }
        if (!strcmp(token, "sectors")) {	
          token = get_next(token, config_file);
          clevel[index].sectors = atoi(token);
        }
      }

      if (!strcmp(token, "mem_access_time")) {	
        token = get_next(token, config_file);
        memory_access_time = atoi(token);
      }
      
      if (!strcmp(token, "mem_short_circuit")) {	
        token = get_next(token, config_file);
        MEM_SHORT_CIRCUIT  = atoi(token);
      }

      if (!strcmp(buffer, "tracefile")) {
        token = get_next(token, config_file);	
        std::string str = std::string(token);
        trace_files.push_back(std::string(str, 0, str.length()-1));
        printf("%s\n", trace_files[trace_files.size()-1].c_str());
      }

      if (!strcmp(buffer, "nocfile")) {
        token = get_next(token, config_file);	
        noc_file = std::string(token);
        printf("%s\n", noc_file.c_str());
      }

      if (!strcmp(buffer, "outputfile")) {
        strcpy(output_file_name,"");
        token = get_next(token, config_file);	
        sscanf(token,"%s\n",output_file_name);
        if(freopen(output_file_name, "w", stdout) == NULL)
          exit(-1);
        output_file_defined = 1;
      }

      if (!strcmp(buffer, "benchmark")) {
        token = get_next(token, config_file);	
        sscanf(token,"%s\n",benchmark);
      }


      if (!strcmp(buffer, "compress")) {
        token = get_next(token, config_file);	
        if(!strncmp(token, "cc", 2))
        {COMPRESS_CSIM=1; COMPRESS_NOX=0;}
        else if(!strncmp(token, "nc", 1))
        {COMPRESS_CSIM=0; COMPRESS_NOX=1;}
        else
        {COMPRESS_CSIM=0; COMPRESS_NOX=0;}
      }
    }

    if (!feof(fp))
    { printf("Error reading file %s\n", config_file); exit(1); }

    fclose(fp);
  }

  cmp_network_nodes = numcpus + num_llc_banks;
 

  //FIXME hardcoded for now
  for(int i = 0; i< numcpus; i++)
  {
    llc_bank_map[i]=num_llc_banks;
    llc_bank_start[i]=0;
  }

  // Assuming maximum of 1024 banks
  int ci=0;
  for(ci=0; ci < 10; ci++)
    if(num_llc_banks < pow(2,ci))
      break;
  llc_bank_power_2 = (int)pow(2,ci);


  /* Initialize FLC caches */
  int index = FLC;
  for(int i = 0; i< numcpus; i++)
    clevel[index].cache.push_back(
        CACHE((long)clevel[index].csize,
          clevel[index].lsize,
          clevel[index].assoc,
          clevel[index].sectors,
          i,
          index,
          'U'
          ));

  /* Initialize MLC caches */
  index = MLC;
  for(int i = 0; i< numcpus; i++)
    clevel[index].cache.push_back(
        CACHE((long)clevel[index].csize,
          clevel[index].lsize,
          clevel[index].assoc,
          clevel[index].sectors,
          i,
          index,
          'U'
          ));

  /* Initialize LLC caches */
  index = LLC;
  if(!def_llc_bank)
    num_llc_banks = numcpus;


  // Overhead of Compression to the cache lookup time
  if(COMPRESS_CSIM)
    llc_lookup_time+=2;

  //lower order interleaving
  if(bank_map == CACHE_LINE_INTERLEAVING)
  {
    bank_shift = FloorLog2(clevel[LLC].lsize-1);
    bank_mask = Log2Mask(FloorLog2(num_llc_banks-1));
  }
  if(bank_map == MEM_PAGE_INTERLEAVING)
  {
    //physical page order interleaving
    bank_shift = 12; //4KB Pages
    bank_mask = Log2Mask(FloorLog2(num_llc_banks-1));
    max_physical_pages_per_bank = 1*MEGA;//4GB of memory
  }
 
  num_mem_controllers = mem_controllers == 0 ? num_llc_banks/4 : mem_controllers;
  //higher order interleaving for memory controllers
  //mem_controller_shift = 14; //256MB DRAM banks interleaving
  //lower order interleaving for memory controllers
  mem_controller_shift = 12; //page level interleaving for DRAM banks
  mem_controller_mask = Log2Mask(FloorLog2(num_mem_controllers-1));

  // Default Layout
  for(int i = 0; i< numcpus; i++)
    cpu_network_layout[i] = i;
  for(int i = 0; i< num_llc_banks; i++)
    cache_network_layout[i] = i+numcpus;
  for(int i = 0; i< num_mem_controllers/2; i++)
  {
    mem_network_layout[i] = i;
    mem_network_layout[num_mem_controllers - i - 1] = numcpus + num_llc_banks - i - 1;
  }


  index = LLC;
  for(int i = 0; i< num_llc_banks; i++)
    clevel[index].cache.push_back(
        CACHE((long)clevel[index].csize,
          clevel[index].lsize,
          clevel[index].assoc,
          clevel[index].sectors,
          i,
          index,
          'U'
          ));                  



  MEMORY = new CACHE(1024,64,8,1,1,2,'M');
  printf("CONFIGURATION:\n");
  printf("DEBUG = %d\n", DEBUG_CPU);
  printf("numcpus = %d\n", numcpus);
  printf("num llc banks = %d\n", num_llc_banks);
  printf("numclevels = %d\n", numclevels);
  printf("Last Level Cache shared : %c\n", llc_shared?'y':'n');
  printf("Cache QoS Enabled : %c\n", CACHE_QOS?'y':'n');
  if(CACHE_QOS)
  {
    printf("--------------------------------------\n");
    printf("QoS priority levels : %d\n",priority_levels);
    printf("Priority distribution :");
    for(int i = 0; i<priority_levels; i++)
      printf("%d :", priority_occ_limit[i]);
    printf("\n");
    printf("Priorty occ limits in cache lines :");
    for(int i = 0; i<priority_levels; i++)
      printf("%d :", pri_occ_limit[i]);
    printf("\n");

  }
  printf("NoC Trace Recording Enabled : %c\n", RECORD_NOC?'y':'n');
  printf("Number of threads sharing an MLC %d\n", threads_per_proc);

  int end = clevel.size();
  for (int i = 0; i < end; ++i) {
    printf("CACHE LEVEL %d:\n", i+1);
    printf("\tline size = %d\n", clevel[i].lsize);
    printf("\tsectors = %d\n", clevel[i].sectors);
    printf("\tcsize = %ld\n", clevel[i].csize);
    printf("\tassoc = %d\n", clevel[i].assoc);
  }
  printf("Bank Decoding Logic -- bank_mask:%llx bank_shift:%d mem_controller_mask:%llx mem_controller_shift:%d\n",
      bank_mask,bank_shift,mem_controller_mask,mem_controller_shift);

  /* OUTPUT File setup */
  if(RECORD)
  {
    out_fp = gzopen("out.gz", "wb");
    if (out_fp == NULL) 
    {
      printf("ERROR: Could not open output file!\n");
      fflush(stdout);
      exit(1);
    }
  }
  if(output_file_defined)
    if(freopen(output_file_name, "a", stdout) == NULL)
      exit(-1);

}

void trace_setup(char *config_file, char *benchname, int StartProc, int EndProc)
{
	char buff[200],fname[200];
	FILE *fp = fopen(config_file, "r");
	if(!fp)
	{ printf("Error opening file %s\n", config_file); exit(1); }

	printf("setting up %s from procID:%d to procID:%d\n",benchname,StartProc,EndProc);



	while (fgets(buff, 200, fp))
	{
		if(strstr(buff,benchname))
		{
			sscanf(buff,"%s\n",fname);
			break;
		}
	}

	for(int i =StartProc, k=0; i<EndProc; i++,k++)
	{
    strcpy(processor[i].app_name,benchname);

    if(!strcmp(benchname,"nop"))
			processor[i].trace_format = NOP_TRACE_FORMAT;
    //1. Multiprogrammed
    else if(strstr(buff,"multiprogram"))
    {
      printf("trace setup for multiprogram");
      if(strstr(buff,"commercial")) 
      {
        processor[i].trace_format = INTEL_TRACE_FORMAT;
        sprintf(mp_trace_file_name[i],"%s.gz",fname);
        printf(" format is INTEL_TRACE_FORMAT\n");
      }
      else if(strstr(buff,"desktop")) 
      {
        processor[i].trace_format = MSR_TRACE_FORMAT;
        sprintf(mp_trace_file_name[i],"%s.gz",fname);
        printf(" format is MSR_TRACE_FORMAT\n");
      }
      else
      {
        processor[i].trace_format = PEPSI_TRACE_FORMAT;
        sprintf(mp_trace_file_name[i],"%s.gz",fname);
        printf(" format is PEPSI_TRACE_FORMAT\n");
      }
    }
    //2. Parallel
    else
    {
      printf("trace setup for parallel");
      if(strstr(buff,"commercial")) //"intel")) 
      {
        processor[i].trace_format = INTEL_TRACE_FORMAT;
        sprintf(mp_trace_file_name[i],"%s.gz",fname,k);//"%s%d.gz",fname,k);
        printf(" format is commercial::INTEL_TRACE_FORMAT\n");
      }
      else if(strstr(buff,"scientific"))//"64p")) 
      {
        processor[i].trace_format = PEPSI_TRACE_FORMAT;
        sprintf(mp_trace_file_name[i],"%s.gz",fname,k);//"%s%d.gz",fname,k);
        printf(" format is scientific::PEPSI_TRACE_FORMAT\n");
      }
      else if(strstr(buff,"desktop")) 
      {
        processor[i].trace_format = MSR_TRACE_FORMAT;
        sprintf(mp_trace_file_name[i],"%s.gz",fname,k);
        printf(" format is desktop::MSR_TRACE_FORMAT\n");
      }
      else if(strstr(buff,"synthetic")) 
      {
        processor[i].trace_format = PEPSI_TRACE_FORMAT;
        sprintf(mp_trace_file_name[i],"%s%d.gz",fname,i);
        printf(" format is SYNTHETIC:PEPSI_TRACE_FORMAR\n");
      }
      else if(strstr(buff,"network")) 
      {
        processor[i].trace_format = PEPSI_TRACE_FORMAT;
        sprintf(mp_trace_file_name[i],"%s.gz",fname,i);
        printf(" format is NETWORK:PEPSI_TRACE_FORMAR\n");
      }
      else 
      {
        processor[i].trace_format = PEPSI_TRACE_FORMAT;
        sprintf(mp_trace_file_name[i],"%s%d.gz",fname,(k%16));
        printf(" format is plain PEPSI_TRACE_FORMAT\n");
      }
    }
	}

	fclose(fp);
}

void killcachesim()
{
	for(int cindex=0; cindex < numcpus ; cindex++)
	{
	CACHE *cp = &(clevel[FLC].cache[cindex]);
	for(int i=0; i<cp->numsets;i++)
	{
	  CSET *setp = &(cp->sets[i]);
	  for(int j =0; j < setp->clines.size(); j++)
		setp->clines.clear();
	}
	cp->sets.clear();
	}
	clevel[FLC].cache.clear();

	printf("cleaned FLC !!\n");

  for(int cindex=0; cindex < numcpus ; cindex++)
  {
    CACHE *cp = &(clevel[MLC].cache[cindex]);
    for(int i=0; i<cp->numsets;i++)
    {
      CSET *setp = &(cp->sets[i]);
      for(int j =0; j < setp->clines.size(); j++)
        setp->clines.clear();
    }
    cp->sets.clear(); 
  }
  clevel[MLC].cache.clear();

  printf("cleaned MLC !!\n");

  for(int cindex=0; cindex < numcpus ; cindex++)
  {
    CACHE *cp = &(clevel[LLC].cache[cindex]);
    for(int i=0; i<cp->numsets;i++)
    {
      CSET *setp = &(cp->sets[i]);
      for(int j =0; j < setp->clines.size(); j++)
        setp->clines.clear();
    }
    cp->sets.clear(); 
  }
  clevel[LLC].cache.clear();

  printf("cleaned LLC !!\n");

  clevel.clear();
  tr_pool.clear();
  delete MEMORY;

  cpu_global_clock=0;

  flag = true;

  transaction_count = 0 ;
  insts_executed = 0;
  tr_commited = 0;

  countj=0; jump = 0;

  mlc_busy_time=0;
  llc_busy_time=0;
  reads = 0;
  writes = 0;
  total_resp_time = 0;
  mlc_queue_wait = 0;
  llc_queue_wait = 0;
  snoop_wait = 0;
  llc_hits=0; mlc_misses=0; 
  llc_hits=0; llc_misses=0;
  mlc_occ=0; llc_occ=0;

}
