/*****************************************************************************
 * File:        cpu_engine.cpp
 * Description: This file consists primarily of the cpu_main() function for the
 *              simulator. The function is basically a loop that reads a
 *              transaction and simulates its flow through the cache/memory
 *              hierarchy.
 *
*****************************************************************************/

#include <stdio.h>
//#include <iostream>
#include <stdlib.h>
#include <zlib.h>
#include <string.h>
#include <string>
#include <math.h>
#include <vector>
#include <exception>
#include <sys/time.h>
#include<assert.h>

using namespace std;

#include "cpu_engine.h"
#include "cache.h"
#include "transaction.h"
#include "processor.h"
#include "pepsi_init.h"
#include "pepsi_stats.h"
#include "sharedglobals.h"

//For Interconnect
#include "../intersim/interconnect_interface.h"
#include "../gpgpu-sim/icnt_wrapper.h"

#include "../gpgpu-sim/gpu-sim.h"
#include "../gpgpusim_entrypoint.h"


extern gpgpu_sim *g_the_gpu;
extern unsigned int cpu_stall_cpu2icnt;
extern unsigned int gpu_stall_dramfull_start;
extern unsigned int gpu_stall_icnt2sh_start;


#define MAX_POOL_SIZE 50
#define CPU_DEBUG 0

// Configs
bool gpgpu_hetero;
char* cpu_benchmark_config;
char* cpu_core_config;
char* cpu_fast_forward_config;
char* cpu_trace_config;
long long unsigned cpu_warmup_inst;
long long unsigned cpu_max_insn;
long long unsigned min_cpu_core_inst;

void cpu_reg_options(option_parser_t opp)
{

   // Onur added
   // Heterogeneous platform
   option_parser_register(opp, "-gpgpu_hetero", OPT_BOOL, &gpgpu_hetero,
                "heterogeneous system (0=Off, 1=On)",
                "0");
   // Onur added
   // CPU benchmark configuration
   option_parser_register(opp, "-cpu_benchmark_config", OPT_CSTR, &cpu_benchmark_config,
                               "CPU benchmark configuration file",
                                "mpmix-conf-1-a.file");
   // Onur added
   // CPU core configuration
   option_parser_register(opp, "-cpu_core_config", OPT_CSTR, &cpu_core_config,
                               "CPU core configuration file",
                                "cmp-conf-16.file");
   // Onur added
   // CPU fast forward configuration
   option_parser_register(opp, "-cpu_fast_forward_config", OPT_CSTR, &cpu_fast_forward_config,
                               "CPU fast forward configuration file",
                                "fastforward-conf.file");
   // Onur added
   // CPU trace configuration
   option_parser_register(opp, "-cpu_trace_config", OPT_CSTR, &cpu_trace_config,
                               "CPU trace configuration file",
                                "trace-conf.file");
   // Onur added
   // CPU warmup instructions
   option_parser_register(opp, "-cpu_warmup_inst", OPT_UINT64, &cpu_warmup_inst,
                               "Number of instructions for warmup",
                                "100000");
   // Onur added
   // CPU warmup instructions
   option_parser_register(opp, "-cpu_max_insn", OPT_UINT64, &cpu_max_insn,
                               "Number of instructions to simulate",
                                "10000000");
   // Onur added
   // CPU stat display frequency
   option_parser_register(opp, "-cpu_instructions_to_simulate", OPT_UINT64, &cpu_instructions_to_simulate,
                               "Number of instructions to display CPU stats",
                                "10000");

   return;
}




void cpu_init(unsigned int num_cpus)
{
  char cmp_conf_file[200];
  char trace_conf_file[200];
  char fast_forward_conf_file[200];

  strcpy(benchmark, cpu_benchmark_config);

  //Global variable that initializes the number of CPUs.
  numcpus = num_cpus;

  strcpy(cmp_conf_file,cpu_core_config);
  strcpy(trace_conf_file,cpu_trace_config);
  strcpy(fast_forward_conf_file,cpu_fast_forward_config);

  //Check
  printf("CPU bm config : %s\n", benchmark);
  printf("CPU core config : %s\n", cmp_conf_file);
  fflush(stdout);

  //CMP Configuration is read and set up
  sim_setup(cmp_conf_file);

  if(strstr(benchmark,"mpmix"))
  {
    char buff[80];
    FILE *fp = fopen(benchmark, "r");
    int numProcs = 0;
    int StartProc = 0, EndProc = 0;

    printf("Setting up for mpmix\n");

    if(!fp || feof(fp))
    {
    	printf("\n File cannot be opened! Aborting");
    	fflush(stdout);
    	exit(0);
    }

    while (fgets(buff, 80, fp))
    {
	if(strstr(buff,"#"))
        continue;
      sscanf(buff,"%s %d\n",benchmark,&numProcs);
      EndProc = StartProc + numProcs;
      printf(" \n\n\n\n %s-- %s-- %d-- %d\n\n\n\n",trace_conf_file,benchmark,StartProc,EndProc);
      trace_setup(trace_conf_file,benchmark,StartProc,EndProc);
      strcpy(buff,"");
      StartProc = EndProc;
    }
  }
  else{
	  printf("\n Invalid filename. Should be named as 'mpmix-XYZ.file'!\n");
	  return;
  }

  //tag_shift = clevel[LLC].cache[0].tag_shift;
  //tr_pool.reserve(MAX_TR);

  /*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
  //Initialize processor here and fastforward to simpoint
  /*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
  for(int i = 0; i < numcpus; i++)
    //#proc number, trace file name, reorder buffer size, issue width, commit
    //width, execution width
  {
    processor[i].initProcessor(mp_trace_file_name[i],i,128,3,3,3);
    processor[i].fastforward(fast_forward_conf_file);
  }
  findUniqueApps();

  /*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
  //statistics interval
  /*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

  cpu_global_clock=0;

}

void cpu_cycle() {

	/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
	//1. Simulate Processors
	/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
	  for(int i = 0; i < numcpus; i++)
	   processor[i].doCycle();
	/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
	//2. Simulate Caches and Memory
	/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
	simulate_memory();    //Here, we are completing a cache transaction and marking the ROB entry ready.
	//L1 cache simulated in the "doCycle function itself.


	/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
	//3. Simulate Interconnect
	/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

	/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
	//4. Update system clock
	/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

	/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

	//if(!(cpu_global_clock%50000))
	//getInstProgress();

//	printf("finished_processors[instr_passes] = %d ;; %d\n",finished_processors[instruction_passes], numcpus);

//	// Onur added: Start running GPU
	min_cpu_core_inst = processor[0].instructions;
	for(int i=0; i< numcpus; i++) {
		//calculate the least number of insts executed by a core
		min_cpu_core_inst = (processor[i].instructions < min_cpu_core_inst)? processor[i].instructions : min_cpu_core_inst ;
	}

	if (!start_gpu_execution) {
		if (min_cpu_core_inst >= cpu_warmup_inst ) {
		  start_gpu_execution = true;

                  //Energy
                  coRun = true;

		  gpu_sim_cycle_start = gpu_sim_cycle;
		  cpu_global_clock_start = cpu_global_clock;
		  gpu_stall_dramfull_start = gpu_stall_dramfull;
		  gpu_stall_icnt2sh_start = gpu_stall_icnt2sh;

		  //Nachi adds for Reset stats after warmup
//		  for(int k=0; k< numcpus; k++)
//		  {
//			  processor[k].Reset();
//			  processor[k].ResetStats();
//			  clevel[0].cache.at(k).reset_stats();
//			  cpu_global_clock=0;
//		  }
		}
		//printf(" Min Inst\t\t%ld \n",min_cpu_core_inst);
	}
//	else {
//
//	}

//	if(min_cpu_core_inst%20000 == 0) {
//		g_the_gpu->gpu_print_stat();
//	}

	if(finished_processors[instruction_passes] == numcpus)
	{
	  /*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
	  //5. Statistics per million instructions of each processor
	  /*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
	  //ASIT:Uncomment below
	  //for(int i = 0; i < numcpus; i++)
	  //  processor[i].printOracleStats(instruction_passes%max_passes, max_passes);
	  //ASIT: end comment

	  //////Single App Profile Starts
	  //for(int i = 0; i < numcpus; i++)
	  //  processor[i].printCumulativeStats(instruction_passes%MAX_PASS);
	  //////Single App Profile Ends

	  //getIPCMatrix(instruction_passes%MAX_PASS);
	  //getStallMatrix(instruction_passes%MAX_PASS);
	  //getNetworkStatsMatrix(instruction_passes%MAX_PASS);
	  //getAppStats(instruction_passes%MAX_PASS);

	  printf("\n************Summary for Pass %.3lfMillion instructions **************\n",(double)((instruction_passes+1)*cpu_instructions_to_simulate)/1000000.0);

	  for(int i = 0; i < numcpus; i++)
		  processor[i].getStats(instruction_passes);

	  if(!SHORT_CIRCUIT)
		{
		//Commented by Nachi
		  //for(int i = 0; i < numcpus; i++)
		   // processor[i].getNetworkStats(instruction_passes);
		}
	  else
		 printf("injection rate:%.2lf\n",100*(csim_inj_msgs/((double)(numcpus + num_llc_banks)*cpu_global_clock)));

	  //Network stats
	 // if(!SHORT_CIRCUIT)
		 //sim_result();
	  //Cache Stats
//		 print_stats_cpu();

	  getAvgStats(instruction_passes);

          icnt_overal_stat();
	  printf("************Summary for Pass %.3lfM instructions **************\n\n",(double)((instruction_passes+1)*cpu_instructions_to_simulate)/1000000.0);

		if(min_cpu_core_inst >= cpu_max_insn + cpu_warmup_inst) {
			cpu_max_insn_struck = true;
			printf("CPU max instructions reached\n");
		}

	  instruction_passes++;

	}

	return;
}

void simulate_memory()
{

	std::vector<TRANSTYPE>::iterator temp;
	TRANSTYPE *tr_temp, *tr;
	int rob_entry_id;
	int tr_index;
	for(tr_index = 0; tr_index < tr_pool.size() ; tr_index++)
	{
		try
		{
			tr = &(tr_pool.at(tr_index));
		}
		catch (std::exception e)
		{
			if (CPU_DEBUG) printf("The location at %d is empty ion tr_pool, Exception is : %d",tr_index, e.what());
		}

		//FIXXX make sure the transaction is in EXIT state also. Right now, we are not making that check.

//		switch(tr->type)
		{

//		case CPU_READ:
//		case CPU_WRITE:
			if(tr->timestamp <= cpu_global_clock && tr->timestamp != -1)
			{
				switch(tr->state)
				{
					case START:
					{
						tr->timestamp = cpu_global_clock+flc_lookup_time+mlc_lookup_time;  // inject into NoC after 6 cycles  // FIXXX parameterize "4" " MLC lookup time
						tr->prev_state = tr->state;
						tr->state = CORE_TO_LLC;
						tr->active_cp = &(clevel[MLC].cache[tr->cpuid]);
						break;
					}

					case CORE_TO_LLC:
					{

						CACHE *cp_mlc = &(clevel[MLC].cache[tr->cpuid]);
						CACHE *cp_flc = &(clevel[FLC].cache[tr->cpuid]);
						int mlc_cstate = 0;
						int flc_cstate = 0;
						flc_cstate = cp_flc->func_look_up(tr->address,FALSE);
						mlc_cstate = cp_mlc->func_look_up(tr->address,FALSE);

						//Just checking if the line lookup is still invalid

						if(mlc_cstate == EXCLUSIVE || mlc_cstate == SHARED || mlc_cstate == CPU_MODIFIED)
 						{
							processor[tr->cpuid].mshr_hits++;
//							printf("Found l2 PRESENCE for address : %lx (proc %d) at time : %lld\n", tr->address, tr->cpuid, cpu_global_clock);
							tr->state = EXIT;
							break;
 						}

						//Here, we need to make a GPU packet of req type and send it in the network to the LLC-cache.
						mem_access_t acc(GLOBAL_ACC_R,tr->address,CLINE_REQUEST_SIZE,((tr->type == CPU_WRITE)? true : false));
						int ctrlsize = ((tr->type == CPU_WRITE)? CLINE_REQUEST_SIZE : CLINE_CTRL_SIZE);
						// If its a write type, in memcontroller, it responds with a data size equal to mf's "ctrl_size"

						unsigned int gpgpu_src_id = cpu2device(tr->cpuid);

						//FIXXX enum of define the second parameter in icnt_has_buffer : CPU_TO_MEM = 2
						if ( ::icnt_has_buffer( gpgpu_src_id, 2, (ctrlsize) ) )
						{

							if(cp_mlc->insert_blocked_queue(tr->tr_id, tr->address))
			                {
			                    tr->prev_state = tr->state;
			                    tr->llcq_enter = cpu_global_clock;
			                    tr->timestamp = BLOCKED;
			                    tr->state = LLC_MSHR_PENDING;
								if(CPU_DEBUG)
									printf("\n JOOMLA! There was a MSHR HITT for %lx with trid %lld!!!", tr->address, tr->tr_id);
//								printf("Found MSHR HIT for address : %lx (proc %d) at time : %lld\n", tr->address, tr->cpuid, cpu_global_clock);
								processor[tr->cpuid].mshr_hits++;
			                    break;
			                }

							mem_fetch *mf = new mem_fetch(acc,
												   NULL, //we don't have an instruction yet FIXXX ,
												   ctrlsize,
												   tr->tr_id,
												   cpu2core(tr->cpuid),
												   cpu2device(tr->cpuid),    //CPU device id. Returns (procID+simt_core_cluster)
												   g_the_gpu->getMemoryConfig());  //   FIXXX: Correct in the mem_fetch constructor to handle memory_config to be NULL.


							unsigned int gpgpu_dst_id = g_the_gpu->getShaderCoreConfig()->mem2device(mf->get_tlx_addr().chip);

							mf->set_status(IN_ICNT_TO_MEM,gpu_sim_cycle+gpu_tot_sim_cycle);   // FIXXX : CPU_TO_ICNT for timestamp.
							tr->timestamp = -1;

							if(CPU_DEBUG)
							printf("\n Tr %d request into network.\n Address %lx", tr->tr_id, tr->address);

//							printf("NoC Inject for address : %lx (proc %d) at time : %lld\n", tr->address, tr->cpuid, cpu_global_clock);
							::icnt_push(gpgpu_src_id , gpgpu_dst_id, (void*)mf, ctrlsize);
							
							// Onur added
							if (start_gpu_execution)
								g_the_gpu->cpu_icnt_push++;

							tr->prev_state = tr->state;
							tr->ptr_to_mf = mf;
							//tr->state = LLC_TO_CORE;
						}
						else {
							// FIXXX: cpu stall
							cpu_stall_cpu2icnt++;
//							printf("waiting for icnt buffer: %lx (proc %d) at time : %lld\n", tr->address, tr->cpuid, cpu_global_clock);
						}
						break;
					}
					case LLC_TO_CORE:
					{

						CACHE *cp_mlc = &(clevel[MLC].cache[tr->cpuid]);
						CACHE *cp_flc = &(clevel[FLC].cache[tr->cpuid]);
						int mlc_cstate = 0;
						int flc_cstate = 0;
						flc_cstate = cp_flc->func_look_up(tr->address,FALSE);
						mlc_cstate = cp_mlc->func_look_up(tr->address,FALSE);
						
						//Just checking if the line lookup is still invalid

						if(mlc_cstate == CPU_INVALID )
						{
							//Inserting data into MLC
							//Data is not found in the cache
							int victim_pos;
							long long int victim_address;
							victim_address = cp_mlc->victim_lookup(tr->address , &victim_pos);
							cp_mlc->replace_victim(victim_address, tr->address, EXCLUSIVE, victim_pos);
							tr->timestamp = cpu_global_clock;   // FIXXX : parameterize : cycles to replace victim
							mlc_cstate = cp_mlc->func_look_up(tr->address,FALSE);
							if(mlc_cstate == CPU_INVALID)
								printf("\n Data was   NOTT  inserted into MLC cache!");
							//Unblocking other transactions dependent on this transaction
							cp_mlc->delete_blocked_queue(tr->tr_id, tr->address);

							if(CPU_DEBUG)
								printf("\n Tr %d : Response from network.\n For address %lx", tr->tr_id, tr->address);
							if(CPU_DEBUG)
								printf("\n Victim Address %lx \n Position: %d", victim_address, victim_pos);

							tr->state = EXIT;
						}
						else
						{
							//Should never happen
							printf("\n Received data from NoC for insertion. But magically data was already inserted into L2 cache!");
							//CNN_DEBUG
							printf("\n When Tr %d : Response from network.\n For address %lx. Found in state : %d\n", tr->tr_id, tr->address,mlc_cstate);
//							abort();
						}

						if(flc_cstate == CPU_INVALID)
						{
							//Next, inserting data into FLC
							//Data is not found in the cache
							int flc_victim_pos;
							long long int flc_victim_address;
							flc_victim_address = cp_flc->victim_lookup(tr->address , &flc_victim_pos);
							cp_flc->replace_victim(flc_victim_address, tr->address, EXCLUSIVE, flc_victim_pos);
							tr->timestamp = cpu_global_clock;   // FIXXX : parameterize : cycles to replace victim
							flc_cstate = cp_flc->func_look_up(tr->address,FALSE);
							if(flc_cstate == CPU_INVALID)
								printf("\n Data was   NOTT  inserted into FLC cache!");
							//Unblocking other transactions dependent on this transaction
							cp_flc->delete_blocked_queue(tr->tr_id, tr->address);

							if(CPU_DEBUG)
								printf("\n FLC: Tr %d : Response from network.\n For address %lx", tr->tr_id, tr->address);
							if(CPU_DEBUG)
								printf("\n FLC: Victim Address %lx \n Position: %d", flc_victim_address, flc_victim_pos);

 							tr->state = EXIT;
						}
						else
						{
							//Should never happen
							printf("\n Received data from NoC for insertion. But magically data was already inserted into L1 cache!");
							//CNN_DEBUG
							printf("\n When Tr %d : Response from network.\n For address %lx. Found in state : %d\n", tr->tr_id, tr->address,mlc_cstate);
//							abort();
						}

//						printf("PUT INTO CACHES: %lx (proc %d) at time : %lld\n", tr->address, tr->cpuid, cpu_global_clock);

					}
					break;

					case LLC_MSHR_PENDING:
						break;
						
					case EXIT:
					{
						if((tr->timestamp != -1) && (tr->state == EXIT))
						{
							bool trans_removed = false;
							assert(processor[tr->cpuid].isPresent(tr->address, tr->tr_id));
							rob_entry_id = processor[tr->cpuid].get_entry_id(tr->address, tr->tr_id);
							if(CPU_DEBUG)
								printf("\n In proc %d ROB_entry id of transaction %d is : %d", tr->cpuid, tr->tr_id, rob_entry_id);

							if(rob_entry_id != -1 && tr->mem_op_id != -1)
							{
								processor[tr->cpuid].setReady(rob_entry_id,tr->tr_id);

								if(CPU_DEBUG)
									printf("\n Erasing transaction %d with id %d \n",tr_index, tr->tr_id);

								for(temp = tr_pool.begin(); temp != tr_pool.end(); temp++)
									if(temp->tr_id == tr->tr_id)
									{
										if(temp->ptr_to_mf)
											delete temp->ptr_to_mf;
										tr_pool.erase(temp);
										trans_removed = true;
										break;
									}
							}
							if(trans_removed == false)
								printf("\n\n\n\n\n  DID NOT REMOVE A TRANSACTION THOUGH AN INST WAS COMMITTED!!\n\n\n\n");
						}
						break;
					}
				}


			}
		}
	}
}

// FIXXX: print stats
// interconnect -- msg injection
// L1 and L2 private caches.
//

unsigned cpu2device(unsigned cpuid) { return g_the_gpu->getShaderCoreConfig()->n_simt_clusters + cpuid; }
unsigned cpu2core(unsigned cpuid) { return (g_the_gpu->getShaderCoreConfig()->n_simt_clusters*g_the_gpu->getShaderCoreConfig()->n_simt_cores_per_cluster) + cpuid; }
