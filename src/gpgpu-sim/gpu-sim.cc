// Copyright (c) 2009-2011, Tor M. Aamodt, Wilson W.L. Fung, George L. Yuan,
// Ali Bakhoda, Andrew Turner, Ivan Sham
// The University of British Columbia
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
// Redistributions in binary form must reproduce the above copyright notice, this
// list of conditions and the following disclaimer in the documentation and/or
// other materials provided with the distribution.
// Neither the name of The University of British Columbia nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#include "gpu-sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "zlib.h"


#include "shader.h"
#include "dram.h"
#include "mem_fetch.h"

#include <time.h>
#include "gpu-cache.h"
#include "gpu-misc.h"
#include "delayqueue.h"
#include "shader.h"
#include "icnt_wrapper.h"
#include "dram.h"
#include "addrdec.h"
#include "stat-tool.h"
#include "l2cache.h"

#include "../cuda-sim/ptx-stats.h"
#include "../intersim/statwraper.h"
#include "../intersim/interconnect_interface.h"
#include "../abstract_hardware_model.h"
#include "../debug.h"
#include "../gpgpusim_entrypoint.h"
#include "../cuda-sim/cuda-sim.h"
#include "mem_latency_stat.h"
#include "power_stat.h"
#include "visualizer.h"
#include "stats.h"
//Onur added ; CPU headers;
#include "../cpu-sim/cpu_engine.h"
#include "../cpu-sim/globals.h"
#include "../cpu-sim/processor.h"
#include "../cpu-sim/pepsi_stats.h"

#ifdef GPGPUSIM_POWER_MODEL
#include "power_interface.h"
#else
class  gpgpu_sim_wrapper {};
#endif

#include <stdio.h>
#include <string.h>

#define MAX(a,b) (((a)>(b))?(a):(b))


bool g_interactive_debugger_enabled=false;

unsigned long long  gpu_sim_cycle = 0;
unsigned long long  gpu_tot_sim_cycle = 0;

// Onur added for average number of warps
unsigned long long gpu_warp_limit_sum = 0;
unsigned long long gpu_warp_limit_num = 0;
unsigned long long gpu_warp_limit_sum_last = 0;

// Onur added for correct stats printing
bool gpu_first_launch = false;

// performance counter for stalls due to congestion.
unsigned int gpu_stall_dramfull = 0; 
unsigned int gpu_stall_icnt2sh = 0;
unsigned int cpu_stall_cpu2icnt = 0;

// Onur added
unsigned int gpu_stall_dramfull_start = 0;
unsigned int gpu_stall_icnt2sh_start = 0;

// Onur added to switch between schemes
bool enable_scheme2 = false;

// Onur added
int gpgpu_w0mem_th_1;
int gpgpu_w0mem_th_2;

/* Clock Domains */

#define  CORE  0x01
#define  L2    0x02
#define  DRAM  0x04
#define  ICNT  0x08  

// Onur added
#define	 CPU   0x10


// Onur added
extern int instruction_passes;

#define MEM_LATENCY_STAT_IMPL




#include "mem_latency_stat.h"

void power_config::reg_options(class OptionParser * opp)
{


	  option_parser_register(opp, "-gpuwattch_xml_file", OPT_CSTR,
			  	  	  	  	 &g_power_config_name,"GPUWattch XML file",
	                   "gpuwattch.xml");

	   option_parser_register(opp, "-power_simulation_enabled", OPT_BOOL,
	                          &g_power_simulation_enabled, "Turn on power simulator (1=On, 0=Off)",
	                          "0");

	   option_parser_register(opp, "-power_per_cycle_dump", OPT_BOOL,
	                          &g_power_per_cycle_dump, "Dump detailed power output each cycle",
	                          "0");

	   // Output Data Formats
	   option_parser_register(opp, "-power_trace_enabled", OPT_BOOL,
	                          &g_power_trace_enabled, "produce a file for the power trace (1=On, 0=Off)",
	                          "0");

	   option_parser_register(opp, "-power_trace_zlevel", OPT_INT32,
	                          &g_power_trace_zlevel, "Compression level of the power trace output log (0=no comp, 9=highest)",
	                          "6");

	   option_parser_register(opp, "-steady_power_levels_enabled", OPT_BOOL,
	                          &g_steady_power_levels_enabled, "produce a file for the steady power levels (1=On, 0=Off)",
	                          "0");

	   option_parser_register(opp, "-steady_state_definition", OPT_CSTR,
			   	  &gpu_steady_state_definition, "allowed deviation:number of samples",
	                 	  "8:4");

}

void memory_config::reg_options(class OptionParser * opp)
{
    option_parser_register(opp, "-gpgpu_dram_scheduler", OPT_INT32, &scheduler_type, 
                                "0 = fifo, 1 = FR-FCFS (defaul)", "1");
    option_parser_register(opp, "-gpgpu_dram_partition_queues", OPT_CSTR, &gpgpu_L2_queue_config, 
                           "i2$:$2d:d2$:$2i",
                           "8:8:8:8");

    option_parser_register(opp, "-l2_ideal", OPT_BOOL, &l2_ideal, 
                           "Use a ideal L2 cache that always hit",
                           "0");
    option_parser_register(opp, "-gpgpu_cache:dl2", OPT_CSTR, &m_L2_config.m_config_string, 
                   "unified banked L2 data cache config "
                   " {<nsets>:<bsize>:<assoc>,<rep>:<wr>:<alloc>:<wr_alloc>,<mshr>:<N>:<merge>,<mq>}",
                   "64:128:8,L:B:m:N,A:16:4,4");
    option_parser_register(opp, "-gpgpu_cache:dl2_texture_only", OPT_BOOL, &m_L2_texure_only, 
                           "L2 cache used for texture only",
                           "1");
    option_parser_register(opp, "-gpgpu_n_mem", OPT_UINT32, &m_n_mem, 
                 "number of memory modules (e.g. memory controllers) in gpu",
                 "8");
    option_parser_register(opp, "-gpgpu_n_mem_per_ctrlr", OPT_UINT32, &gpu_n_mem_per_ctrlr, 
                 "number of memory chips per memory controller",
                 "1");
    option_parser_register(opp, "-gpgpu_memlatency_stat", OPT_INT32, &gpgpu_memlatency_stat, 
                "track and display latency statistics 0x2 enables MC, 0x4 enables queue logs",
                "0");
    option_parser_register(opp, "-gpgpu_dram_sched_queue_size", OPT_INT32, &gpgpu_dram_sched_queue_size, 
                "0 = unlimited (default); # entries per chip",
                "0");
    option_parser_register(opp, "-gpgpu_dram_buswidth", OPT_UINT32, &busW, 
                 "default = 4 bytes (8 bytes per cycle at DDR)",
                 "4");
    option_parser_register(opp, "-gpgpu_dram_burst_length", OPT_UINT32, &BL, 
                 "Burst length of each DRAM request (default = 4 data bus cycle)",
                 "4");
    option_parser_register(opp, "-dram_data_command_freq_ratio", OPT_UINT32, &data_command_freq_ratio, 
                 "Frequency ratio between DRAM data bus and command bus (default = 2 times, i.e. DDR)",
                 "2");
    option_parser_register(opp, "-gpgpu_dram_timing_opt", OPT_CSTR, &gpgpu_dram_timing_opt, 
                "DRAM timing parameters = {nbk:tCCD:tRRD:tRCD:tRAS:tRP:tRC:CL:WL:tCDLR:tWR:nbkgrp:tCCDL:tRTPL}",
                "4:2:8:12:21:13:34:9:4:5:13:1:0:0");
    option_parser_register(opp, "-rop_latency", OPT_UINT32, &rop_latency,
                     "ROP queue latency (default 85)",
                     "85");
    option_parser_register(opp, "-dram_latency", OPT_UINT32, &dram_latency,
                     "DRAM latency (default 30)",
                     "30");

    m_address_mapping.addrdec_setoption(opp);
}

void shader_core_config::reg_options(class OptionParser * opp)
{
    option_parser_register(opp, "-gpgpu_simd_model", OPT_INT32, &model, 
                   "1 = post-dominator", "1");
    option_parser_register(opp, "-gpgpu_shader_core_pipeline", OPT_CSTR, &gpgpu_shader_core_pipeline_opt, 
                   "shader core pipeline config, i.e., {<nthread>:<warpsize>}",
                   "1024:32");
    option_parser_register(opp, "-gpgpu_tex_cache:l1", OPT_CSTR, &m_L1T_config.m_config_string, 
                   "per-shader L1 texture cache  (READ-ONLY) config "
                   " {<nsets>:<bsize>:<assoc>,<rep>:<wr>:<alloc>:<wr_alloc>,<mshr>:<N>:<merge>,<mq>:<rf>}",
                   "8:128:5,L:R:m:N,F:128:4,128:2");
    option_parser_register(opp, "-gpgpu_const_cache:l1", OPT_CSTR, &m_L1C_config.m_config_string, 
                   "per-shader L1 constant memory cache  (READ-ONLY) config "
                   " {<nsets>:<bsize>:<assoc>,<rep>:<wr>:<alloc>:<wr_alloc>,<mshr>:<N>:<merge>,<mq>} ",
                   "64:64:2,L:R:f:N,A:2:32,4" );
    option_parser_register(opp, "-gpgpu_cache:il1", OPT_CSTR, &m_L1I_config.m_config_string, 
                   "shader L1 instruction cache config "
                   " {<nsets>:<bsize>:<assoc>,<rep>:<wr>:<alloc>:<wr_alloc>,<mshr>:<N>:<merge>,<mq>} ",
                   "4:256:4,L:R:f:N,A:2:32,4" );
    option_parser_register(opp, "-gpgpu_cache:dl1", OPT_CSTR, &m_L1D_config.m_config_string, 
                   "per-shader L1 data cache config "
                   " {<nsets>:<bsize>:<assoc>,<rep>:<wr>:<alloc>:<wr_alloc>,<mshr>:<N>:<merge>,<mq> | none}",
                   "none" );
    option_parser_register(opp, "-gpgpu_perfect_mem", OPT_BOOL, &gpgpu_perfect_mem, 
                 "enable perfect memory mode (no cache miss)",
                 "0");
    option_parser_register(opp, "-n_regfile_gating_group", OPT_UINT32, &n_regfile_gating_group,
                 "group of lanes that should be read/written together)",
                 "4");
    option_parser_register(opp, "-gpgpu_clock_gated_reg_file", OPT_BOOL, &gpgpu_clock_gated_reg_file,
                 "enable clock gated reg file for power calculations",
                 "0");
    option_parser_register(opp, "-gpgpu_clock_gated_lanes", OPT_BOOL, &gpgpu_clock_gated_lanes,
                 "enable clock gated lanes for power calculations",
                 "0");
    option_parser_register(opp, "-gpgpu_shader_registers", OPT_UINT32, &gpgpu_shader_registers, 
                 "Number of registers per shader core. Limits number of concurrent CTAs. (default 8192)",
                 "8192");
    option_parser_register(opp, "-gpgpu_shader_cta", OPT_UINT32, &max_cta_per_core, 
                 "Maximum number of concurrent CTAs in shader (default 8)",
                 "8");
    option_parser_register(opp, "-gpgpu_n_clusters", OPT_UINT32, &n_simt_clusters, 
                 "number of processing clusters",
                 "10");
    option_parser_register(opp, "-gpgpu_n_cores_per_cluster", OPT_UINT32, &n_simt_cores_per_cluster, 
                 "number of simd cores per cluster",
                 "3");
    option_parser_register(opp, "-gpgpu_n_cluster_ejection_buffer_size", OPT_UINT32, &n_simt_ejection_buffer_size, 
                 "number of packets in ejection buffer",
                 "8");
    option_parser_register(opp, "-gpgpu_n_ldst_response_buffer_size", OPT_UINT32, &ldst_unit_response_queue_size, 
                 "number of response packets in ld/st unit ejection buffer",
                 "2");
    option_parser_register(opp, "-gpgpu_shmem_size", OPT_UINT32, &gpgpu_shmem_size, 
                 "Size of shared memory per shader core (default 16kB)",
                 "16384");
    option_parser_register(opp, "-gpgpu_shmem_num_banks", OPT_UINT32, &num_shmem_bank, 
                 "Number of banks in the shared memory in each shader core (default 16)",
                 "16");
    option_parser_register(opp, "-gpgpu_shmem_limited_broadcast", OPT_BOOL, &shmem_limited_broadcast, 
                 "Limit shared memory to do one broadcast per cycle (default on)",
                 "1");
    option_parser_register(opp, "-gpgpu_shmem_warp_parts", OPT_INT32, &mem_warp_parts,  
                 "Number of portions a warp is divided into for shared memory bank conflict check ",
                 "2");
    option_parser_register(opp, "-gpgpu_warpdistro_shader", OPT_INT32, &gpgpu_warpdistro_shader, 
                "Specify which shader core to collect the warp size distribution from", 
                "-1");
    option_parser_register(opp, "-gpgpu_local_mem_map", OPT_BOOL, &gpgpu_local_mem_map, 
                "Mapping from local memory space address to simulated GPU physical address space (default = enabled)", 
                "1");
    option_parser_register(opp, "-gpgpu_num_reg_banks", OPT_INT32, &gpgpu_num_reg_banks, 
                "Number of register banks (default = 8)", 
                "8");
    option_parser_register(opp, "-gpgpu_reg_bank_use_warp_id", OPT_BOOL, &gpgpu_reg_bank_use_warp_id,
             "Use warp ID in mapping registers to banks (default = off)",
             "0");
    option_parser_register(opp, "-gpgpu_operand_collector_num_units_sp", OPT_INT32, &gpgpu_operand_collector_num_units_sp,
                "number of collector units (default = 4)", 
                "4");
    option_parser_register(opp, "-gpgpu_operand_collector_num_units_sfu", OPT_INT32, &gpgpu_operand_collector_num_units_sfu,
                "number of collector units (default = 4)", 
                "4");
    option_parser_register(opp, "-gpgpu_operand_collector_num_units_mem", OPT_INT32, &gpgpu_operand_collector_num_units_mem,
                "number of collector units (default = 2)", 
                "2");
    option_parser_register(opp, "-gpgpu_operand_collector_num_units_gen", OPT_INT32, &gpgpu_operand_collector_num_units_gen,
                "number of collector units (default = 0)", 
                "0");
    option_parser_register(opp, "-gpgpu_operand_collector_num_in_ports_sp", OPT_INT32, &gpgpu_operand_collector_num_in_ports_sp,
                           "number of collector unit in ports (default = 1)", 
                           "1");
    option_parser_register(opp, "-gpgpu_operand_collector_num_in_ports_sfu", OPT_INT32, &gpgpu_operand_collector_num_in_ports_sfu,
                           "number of collector unit in ports (default = 1)", 
                           "1");
    option_parser_register(opp, "-gpgpu_operand_collector_num_in_ports_mem", OPT_INT32, &gpgpu_operand_collector_num_in_ports_mem,
                           "number of collector unit in ports (default = 1)", 
                           "1");
    option_parser_register(opp, "-gpgpu_operand_collector_num_in_ports_gen", OPT_INT32, &gpgpu_operand_collector_num_in_ports_gen,
                           "number of collector unit in ports (default = 0)", 
                           "0");
    option_parser_register(opp, "-gpgpu_operand_collector_num_out_ports_sp", OPT_INT32, &gpgpu_operand_collector_num_out_ports_sp,
                           "number of collector unit in ports (default = 1)", 
                           "1");
    option_parser_register(opp, "-gpgpu_operand_collector_num_out_ports_sfu", OPT_INT32, &gpgpu_operand_collector_num_out_ports_sfu,
                           "number of collector unit in ports (default = 1)", 
                           "1");
    option_parser_register(opp, "-gpgpu_operand_collector_num_out_ports_mem", OPT_INT32, &gpgpu_operand_collector_num_out_ports_mem,
                           "number of collector unit in ports (default = 1)", 
                           "1");
    option_parser_register(opp, "-gpgpu_operand_collector_num_out_ports_gen", OPT_INT32, &gpgpu_operand_collector_num_out_ports_gen,
                           "number of collector unit in ports (default = 0)", 
                           "0");
    option_parser_register(opp, "-gpgpu_coalesce_arch", OPT_INT32, &gpgpu_coalesce_arch, 
                            "Coalescing arch (default = 13, anything else is off for now)", 
                            "13");
    option_parser_register(opp, "-gpgpu_num_sched_per_core", OPT_INT32, &gpgpu_num_sched_per_core, 
                            "Number of warp schedulers per core", 
                            "1");
    option_parser_register(opp, "-gpgpu_max_insn_issue_per_warp", OPT_INT32, &gpgpu_max_insn_issue_per_warp,
                            "Max number of instructions that can be issued per warp in one cycle by scheduler",
                            "2");
    option_parser_register(opp, "-gpgpu_simt_core_sim_order", OPT_INT32, &simt_core_sim_order,
                            "Select the simulation order of cores in a cluster (0=Fix, 1=Round-Robin)",
                            "1");
    option_parser_register(opp, "-gpgpu_pipeline_widths", OPT_CSTR, &pipeline_widths_string,
                            "Pipeline widths "
                            "ID_OC_SP,ID_OC_SFU,ID_OC_MEM,OC_EX_SP,OC_EX_SFU,OC_EX_MEM,EX_WB",
                            "1,1,1,1,1,1,1" );
    option_parser_register(opp, "-gpgpu_num_sp_units", OPT_INT32, &gpgpu_num_sp_units,
                            "Number of SP units (default=1)",
                            "1");
    option_parser_register(opp, "-gpgpu_num_sfu_units", OPT_INT32, &gpgpu_num_sfu_units,
                            "Number of SF units (default=1)",
                            "1");
    option_parser_register(opp, "-gpgpu_num_mem_units", OPT_INT32, &gpgpu_num_mem_units,
                            "Number if ldst units (default=1) WARNING: not hooked up to anything",
                             "1");
    option_parser_register(opp, "-gpgpu_scheduler", OPT_CSTR, &gpgpu_scheduler_string,
                                "Scheduler configuration: < lrr | gto | two_level_active > "
                                "If two_level_active:<num_active_warps>:<inner_prioritization>:<outer_prioritization>"
                                "For complete list of prioritization values see shader.h enum scheduler_prioritization_type"
                                "Default: gto",
                                 "gto");
    // Onur added
    option_parser_register(opp, "-gpgpu_enable_dynamic_throttling", OPT_INT32, &gpgpu_enable_dyn_throttling,
                 "Use the default warp scheduler (0); dynamically limit the number of warps based on DRAM-full (1);"
    			 "dynamically limit the number of warps based on all three congestion metrics (2)",
                 "0");

    // Onur added
    // Number of CPUs
    option_parser_register(opp, "-gpgpu_n_cpu", OPT_INT32,
                           &gpgpu_n_cpu, "Number of CPUs",
                           "8");
}

void gpgpu_sim_config::reg_options(option_parser_t opp)
{
    gpgpu_functional_sim_config::reg_options(opp);
    m_shader_config.reg_options(opp);
    m_memory_config.reg_options(opp);
    power_config::reg_options(opp);

    // Onur added
    option_parser_register(opp, "-gpgpu_enable_gpu", OPT_BOOL, &gpgpu_enable_gpu,
                 "Execute only CPU (0) or enable GPU execution (1)",
                 "1");

   option_parser_register(opp, "-gpgpu_max_cycle", OPT_INT32, &gpu_max_cycle_opt, 
               "terminates gpu simulation early (0 = no limit)",
               "0");
   option_parser_register(opp, "-gpgpu_max_insn", OPT_INT32, &gpu_max_insn_opt, 
               "terminates gpu simulation early (0 = no limit)",
               "0");
   option_parser_register(opp, "-gpgpu_max_cta", OPT_INT32, &gpu_max_cta_opt, 
               "terminates gpu simulation early (0 = no limit)",
               "0");
   option_parser_register(opp, "-gpgpu_runtime_stat", OPT_CSTR, &gpgpu_runtime_stat, 
                  "display runtime statistics such as dram utilization {<freq>:<flag>}",
                  "10000:0");
   option_parser_register(opp, "-gpgpu_flush_l1_cache", OPT_BOOL, &gpgpu_flush_l1_cache,
                "Flush L1 cache at the end of each kernel call",
                "0");
   option_parser_register(opp, "-gpgpu_flush_l2_cache", OPT_BOOL, &gpgpu_flush_l2_cache,
                   "Flush L2 cache at the end of each kernel call",
                   "0");

   option_parser_register(opp, "-gpgpu_deadlock_detect", OPT_BOOL, &gpu_deadlock_detect, 
                "Stop the simulation at deadlock (1=on (default), 0=off)", 
                "1");
   option_parser_register(opp, "-gpgpu_ptx_instruction_classification", OPT_INT32, 
               &gpgpu_ptx_instruction_classification, 
               "if enabled will classify ptx instruction types per kernel (Max 255 kernels now)", 
               "0");
   option_parser_register(opp, "-gpgpu_ptx_sim_mode", OPT_INT32, &g_ptx_sim_mode, 
               "Select between Performance (default) or Functional simulation (1)", 
               "0");

   // Onur added
   option_parser_register(opp, "-gpgpu_warplimiting_sample_freq", OPT_INT32, &gpu_warplimiting_sample_freq,
                  "Number of shader cycles to make a decision on the number of warps",
                  "2048");
   option_parser_register(opp, "-gpgpu_w0mem_th_1", OPT_INT32, &gpgpu_w0mem_th_1,
                "The first threshold for the W0mem scheme",
                "125");
   option_parser_register(opp, "-gpgpu_w0mem_th_2", OPT_INT32, &gpgpu_w0mem_th_2,
                "The first threshold for the W0mem scheme",
                "200");
   option_parser_register(opp, "-gpgpu_throttling_threshold", OPT_INT32, &gpu_throttling_threshold,
                  "Threshold to use in scheme 8, trade-off between GPU vs CPU performance",
                  "1024");

   // Onur modified: added cpu clock domain
   option_parser_register(opp, "-gpgpu_clock_domains", OPT_CSTR, &gpgpu_clock_domains, 
                  "Clock Domain Frequencies in MhZ {<Core Clock>:<ICNT Clock>:<L2 Clock>:<DRAM Clock>:<CPU Clock>}",
                  "500.0:2000.0:2000.0:2000.0:2000.0");

   option_parser_register(opp, "-gpgpu_max_concurrent_kernel", OPT_INT32, &max_concurrent_kernel,
                          "maximum kernels that can run concurrently on GPU", "8" );
   option_parser_register(opp, "-gpgpu_cflog_interval", OPT_INT32, &gpgpu_cflog_interval, 
               "Interval between each snapshot in control flow logger", 
               "0");
   option_parser_register(opp, "-visualizer_enabled", OPT_BOOL,
                          &g_visualizer_enabled, "Turn on visualizer output (1=On, 0=Off)",
                          "1");
   option_parser_register(opp, "-visualizer_outputfile", OPT_CSTR, 
                          &g_visualizer_filename, "Specifies the output log file for visualizer",
                          NULL);
   option_parser_register(opp, "-visualizer_zlevel", OPT_INT32,
                          &g_visualizer_zlevel, "Compression level of the visualizer output log (0=no comp, 9=highest)",
                          "6");
   ptx_file_line_stats_options(opp);

}

/////////////////////////////////////////////////////////////////////////////

void increment_x_then_y_then_z( dim3 &i, const dim3 &bound)
{
   i.x++;
   if ( i.x >= bound.x ) {
      i.x = 0;
      i.y++;
      if ( i.y >= bound.y ) {
         i.y = 0;
         if( i.z < bound.z ) 
            i.z++;
      }
   }
}

void gpgpu_sim::launch( kernel_info_t *kinfo )
{
   unsigned cta_size = kinfo->threads_per_cta();
   if ( cta_size > m_shader_config->n_thread_per_shader ) {
      printf("Execution error: Shader kernel CTA (block) size is too large for microarch config.\n");
      printf("                 CTA size (x*y*z) = %u, max supported = %u\n", cta_size, 
             m_shader_config->n_thread_per_shader );
      printf("                 => either change -gpgpu_shader argument in gpgpusim.config file or\n");
      printf("                 modify the CUDA source to decrease the kernel block size.\n");
      abort();
   }
   unsigned n=0;
   for(n=0; n < m_running_kernels.size(); n++ ) {
       if( (NULL==m_running_kernels[n]) || m_running_kernels[n]->done() ) {
           m_running_kernels[n] = kinfo;
           break;
       }
   }
   assert(n < m_running_kernels.size());
}

bool gpgpu_sim::can_start_kernel()
{
   for(unsigned n=0; n < m_running_kernels.size(); n++ ) {
       if( (NULL==m_running_kernels[n]) || m_running_kernels[n]->done() ) 
           return true;
   }
   return false;
}

bool gpgpu_sim::get_more_cta_left() const
{ 
   if (m_config.gpu_max_cta_opt != 0) {
      if( m_total_cta_launched >= m_config.gpu_max_cta_opt )
          return false;
   }
   for(unsigned n=0; n < m_running_kernels.size(); n++ ) {
       if( m_running_kernels[n] && !m_running_kernels[n]->no_more_ctas_to_run() ) 
           return true;
   }
   return false;
}

kernel_info_t *gpgpu_sim::select_kernel()
{
    for(unsigned n=0; n < m_running_kernels.size(); n++ ) {
        unsigned idx = (n+m_last_issued_kernel+1)%m_config.max_concurrent_kernel;
        if( m_running_kernels[idx] && !m_running_kernels[idx]->no_more_ctas_to_run() ) {
            m_last_issued_kernel=idx;
            return m_running_kernels[idx];
        }
    }
    return NULL;
}

unsigned gpgpu_sim::finished_kernel()
{
    if( m_finished_kernel.empty() ) 
        return 0;
    unsigned result = m_finished_kernel.front();
    m_finished_kernel.pop_front();
    return result;
}

void gpgpu_sim::set_kernel_done( kernel_info_t *kernel ) 
{ 
    unsigned uid = kernel->get_uid();
    m_finished_kernel.push_back(uid);
    std::vector<kernel_info_t*>::iterator k;
    for( k=m_running_kernels.begin(); k!=m_running_kernels.end(); k++ ) {
        if( *k == kernel ) {
            *k = NULL;
            break;
        }
    }
    assert( k != m_running_kernels.end() ); 
}

void set_ptx_warp_size(const struct core_config * warp_size);


//Onur added
unsigned int gpgpu_sim::get_gpgpu_n_cpu()
{
   return m_shader_config->gpgpu_n_cpu;
}


gpgpu_sim::gpgpu_sim( const gpgpu_sim_config &config ) 
    : gpgpu_t(config), m_config(config)
{ 
    m_shader_config = &m_config.m_shader_config;
    m_memory_config = &m_config.m_memory_config;
    set_ptx_warp_size(m_shader_config);
    ptx_file_line_stats_create_exposed_latency_tracker(m_config.num_shader());

#ifdef GPGPUSIM_POWER_MODEL
        m_gpgpusim_wrapper = new gpgpu_sim_wrapper(config.g_power_simulation_enabled,config.g_power_config_name);
#endif

    m_shader_stats = new shader_core_stats(m_shader_config);
    m_memory_stats = new memory_stats_t(m_config.num_shader(),m_shader_config,m_memory_config);
    average_pipeline_duty_cycle = (float *)malloc(sizeof(float));
    active_sms=(float *)malloc(sizeof(float));
    m_power_stats = new power_stat_t(m_shader_config,average_pipeline_duty_cycle,active_sms,m_shader_stats,m_memory_config,m_memory_stats);

    gpu_sim_insn = 0;
    gpu_tot_sim_insn = 0;
    gpu_tot_issued_cta = 0;
    gpu_deadlock = false;

    // Onur added
    gpu_max_insn_struck = false;


    m_cluster = new simt_core_cluster*[m_shader_config->n_simt_clusters];
    for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) 
        m_cluster[i] = new simt_core_cluster(this,i,m_shader_config,m_memory_config,m_shader_stats,m_memory_stats);

    m_memory_partition_unit = new memory_partition_unit*[m_memory_config->m_n_mem];
    for (unsigned i=0;i<m_memory_config->m_n_mem;i++) 
        m_memory_partition_unit[i] = new memory_partition_unit(i, m_memory_config, m_memory_stats);


    // Onur modified: n_cpu
    icnt_init(m_shader_config->n_simt_clusters,m_memory_config->m_n_mem,m_shader_config->gpgpu_n_cpu);
    cout << "icnt init called..." << endl;

    time_vector_create(NUM_MEM_REQ_STAT);
    fprintf(stdout, "GPGPU-Sim uArch: performance model initialization complete.\n");

    m_running_kernels.resize( config.max_concurrent_kernel, NULL );
    m_last_issued_kernel = 0;
    m_last_cluster_issue = 0;
    *average_pipeline_duty_cycle=0;
    *active_sms=0;
}

int gpgpu_sim::shared_mem_size() const
{
   return m_shader_config->gpgpu_shmem_size;
}

int gpgpu_sim::num_registers_per_core() const
{
   return m_shader_config->gpgpu_shader_registers;
}

int gpgpu_sim::wrp_size() const
{
   return m_shader_config->warp_size;
}

int gpgpu_sim::shader_clock() const
{
   return m_config.core_freq/1000;
}

void gpgpu_sim::set_prop( cudaDeviceProp *prop )
{
   m_cuda_properties = prop;
}

const struct cudaDeviceProp *gpgpu_sim::get_prop() const
{
   return m_cuda_properties;
}

enum divergence_support_t gpgpu_sim::simd_model() const
{
   return m_shader_config->model;
}

void gpgpu_sim_config::init_clock_domains(void ) 
{
	// Onur modified: added cpu_freq
   sscanf(gpgpu_clock_domains,"%lf:%lf:%lf:%lf:%lf",
          &core_freq, &icnt_freq, &l2_freq, &dram_freq, &cpu_freq);

   core_freq = core_freq MhZ;
   icnt_freq = icnt_freq MhZ;
   l2_freq = l2_freq MhZ;
   dram_freq = dram_freq MhZ;        
   core_period = 1/core_freq;
   icnt_period = 1/icnt_freq;
   dram_period = 1/dram_freq;
   l2_period = 1/l2_freq;

   // Onur added:
   cpu_freq = cpu_freq MhZ;
   cpu_period = 1/cpu_freq;

   printf("GPGPU-Sim uArch: clock freqs: %lf:%lf:%lf:%lf:%lf\n",core_freq,icnt_freq,l2_freq,dram_freq,cpu_freq);
   printf("GPGPU-Sim uArch: clock periods: %.20lf:%.20lf:%.20lf:%.20lf:%.20lf\n",core_period,icnt_period,l2_period,dram_period,cpu_period);
}

void gpgpu_sim::reinit_clock_domains(void)
{
   core_time = 0;
   dram_time = 0;
   icnt_time = 0;
   l2_time = 0;

   // Onur added
   cpu_time = 0;
}

bool gpgpu_sim::active()
{
    if (m_config.gpu_max_cycle_opt && (gpu_tot_sim_cycle + gpu_sim_cycle) >= m_config.gpu_max_cycle_opt) 
       return false;
//    if (m_config.gpu_max_insn_opt && (gpu_tot_sim_insn + gpu_sim_insn) >= m_config.gpu_max_insn_opt)
//       return false;
    if (m_config.gpu_max_cta_opt && (gpu_tot_issued_cta >= m_config.gpu_max_cta_opt) )
       return false;
    if (m_config.gpu_deadlock_detect && gpu_deadlock) 
       return false;
    for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) 
       if( m_cluster[i]->get_not_completed()>0 ) 
           return true;;

    // Onur added: If there CPU cores, do not check for icnt and mem to start the next kernel
    // FIXXX: if we can make the simulator run without any CPU cores, then the following part should
    // not be commented out. Put it inside an if.
//    for (unsigned i=0;i<m_memory_config->m_n_mem;i++)
//       if( m_memory_partition_unit[i]->busy()>0 )
//           return true;;
//    if( icnt_busy() )
//        return true;

    if( get_more_cta_left() )
        return true;
    return false;
}

void gpgpu_sim::init()
{
    // run a CUDA grid on the GPU microarchitecture simulator
    gpu_sim_cycle = 0;
    gpu_sim_insn = 0;
    last_gpu_sim_insn = 0;
    m_total_cta_launched=0;

    // Onur added
    // number of packets injected into the network from gpu, cpu and memory
    gpu_icnt_push = 0;
    cpu_icnt_push = 0;
    mem_icnt_push = 0;
    // CPU cache warmup
//    gpu_sim_cycle_start = 0;



    reinit_clock_domains();
    set_param_gpgpu_num_shaders(m_config.num_shader());
    for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) 
       m_cluster[i]->reinit();
    m_shader_stats->new_grid();
    // initialize the control-flow, memory access, memory latency logger
    if (m_config.g_visualizer_enabled) {
        create_thread_CFlogger( m_config.num_shader(), m_shader_config->n_thread_per_shader, 0, m_config.gpgpu_cflog_interval );
    }
    shader_CTA_count_create( m_config.num_shader(), m_config.gpgpu_cflog_interval);
    if (m_config.gpgpu_cflog_interval != 0) {
       insn_warp_occ_create( m_config.num_shader(), m_shader_config->warp_size );
       shader_warp_occ_create( m_config.num_shader(), m_shader_config->warp_size, m_config.gpgpu_cflog_interval);
       shader_mem_acc_create( m_config.num_shader(), m_memory_config->m_n_mem, 4, m_config.gpgpu_cflog_interval);
       shader_mem_lat_create( m_config.num_shader(), m_config.gpgpu_cflog_interval);
       shader_cache_access_create( m_config.num_shader(), 3, m_config.gpgpu_cflog_interval);
       set_spill_interval (m_config.gpgpu_cflog_interval * 40);
    }

    if (g_network_mode) 
       icnt_init_grid(); 

    // McPAT initialization function. Called on first launch of GPU
#ifdef GPGPUSIM_POWER_MODEL
    if(m_config.g_power_simulation_enabled){
        init_mcpat(m_config, m_gpgpusim_wrapper, m_config.gpu_stat_sample_freq,  gpu_tot_sim_insn, gpu_sim_insn);
    }
#endif
}

void gpgpu_sim::update_stats() {
    m_memory_stats->memlatstat_lat_pw();
    gpu_tot_sim_cycle += gpu_sim_cycle;
    totsimcycle = gpu_tot_sim_cycle;
    gpu_tot_sim_insn += gpu_sim_insn;
}

void gpgpu_sim::print_stats()
{

    ptx_file_line_stats_write_file();
    gpu_print_stat();

    if (g_network_mode) {
       interconnect_stats();
       printf("----------------------------Interconnect-DETAILS---------------------------------" );
       icnt_overal_stat();
       printf("----------------------------END-of-Interconnect-DETAILS-------------------------" );
    }
}

void gpgpu_sim::deadlock_check()
{
   if (m_config.gpu_deadlock_detect && gpu_deadlock) {
      fflush(stdout);
      printf("\n\nGPGPU-Sim uArch: ERROR ** deadlock detected: last writeback core %u @ gpu_sim_cycle %u (+ gpu_tot_sim_cycle %u) (%u cycles ago)\n", 
             gpu_sim_insn_last_update_sid,
             (unsigned) gpu_sim_insn_last_update, (unsigned) (gpu_tot_sim_cycle-gpu_sim_cycle),
             (unsigned) (gpu_sim_cycle - gpu_sim_insn_last_update )); 
      unsigned num_cores=0;
      for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) {
         unsigned not_completed = m_cluster[i]->get_not_completed();
         if( not_completed ) {
             if ( !num_cores )  {
                 printf("GPGPU-Sim uArch: DEADLOCK  shader cores no longer committing instructions [core(# threads)]:\n" );
                 printf("GPGPU-Sim uArch: DEADLOCK  ");
                 m_cluster[i]->print_not_completed(stdout);
             } else if (num_cores < 8 ) {
                 m_cluster[i]->print_not_completed(stdout);
             } else if (num_cores >= 8 ) {
                 printf(" + others ... ");
             }
             num_cores+=m_shader_config->n_simt_cores_per_cluster;
         }
      }
      printf("\n");
      for (unsigned i=0;i<m_memory_config->m_n_mem;i++) {
         bool busy = m_memory_partition_unit[i]->busy();
         if( busy ) 
             printf("GPGPU-Sim uArch DEADLOCK:  memory partition %u busy\n", i );
      }
      if( icnt_busy() ) {
         printf("GPGPU-Sim uArch DEADLOCK:  interconnect contains traffic\n");
         display_icnt_state( stdout );
      }
      printf("\nRe-run the simulator in gdb and use debug routines in .gdbinit to debug this\n");
      fflush(stdout);
      abort();
   }

//   // Onur added: terminate once reaching n GPU instructions
//   if (m_config.gpu_max_insn_opt && gpu_max_insn_struck) {
//		print_stats();
//		printf("GPGPU-Sim uArch GPU MAX INSTRUCTIONS STRUCK\n");
//		abort();
//   }
//
//   // Onur added: terminate once reaching n CPU instructions
//   if (cpu_max_insn_struck) {
//		print_stats();
//		printf("GPGPU-Sim uArch CPU MAX INSTRUCTIONS STRUCK\n");
//		abort();
//   }

   	  if (m_config.gpu_max_insn_opt) {
   		  if (gpu_max_insn_struck && cpu_max_insn_struck) {
   	   		print_stats();
   	   		printf("GPGPU-Sim uArch CPU and GPU MAX INSTRUCTIONS STRUCK\n");
   	   		abort();
   		  }
   	  }
   	  else {
   		  if (cpu_max_insn_struck) {
   	   		print_stats();
   	   		printf("GPGPU-Sim uArch CPU MAX INSTRUCTIONS STRUCK\n");
   	   		abort();
   		  }
      }
}

void gpgpu_sim::gpu_print_stat() 
{  
   // Onur added
   printf("gpu_sim_cycle_start = %lld\n", gpu_sim_cycle_start);

   printf("gpu_sim_cycle = %lld\n", gpu_sim_cycle);
   printf("gpu_sim_insn = %lld\n", gpu_sim_insn);

   // Onur modified
   if(!gpu_first_launch)
	   printf("gpu_ipc = %12.4f\n", (float)gpu_sim_insn / (gpu_sim_cycle - gpu_sim_cycle_start)); // FIXXX: wrong after first relaunch
   else
	   printf("gpu_ipc = %12.4f\n", (float)gpu_sim_insn / gpu_sim_cycle);
   gpu_first_launch = true;

   printf("gpu_tot_sim_cycle = %lld\n", gpu_tot_sim_cycle+gpu_sim_cycle);
   printf("gpu_tot_sim_insn = %lld\n", gpu_tot_sim_insn+gpu_sim_insn);

   // Onur modified
   printf("gpu_tot_ipc = %12.4f\n", (float)(gpu_tot_sim_insn+gpu_sim_insn) / (gpu_tot_sim_cycle+gpu_sim_cycle-gpu_sim_cycle_start));

   //UNT add
   printf("gputotipc = %12.4f\n", (float) (gpu_tot_sim_insn+gpu_sim_insn) / (totsimcycle+simcycle-gpu_sim_cycle_start));
   printf("gpu_tot_issued_cta = %lld\n", gpu_tot_issued_cta);



   // performance counter for stalls due to congestion.
   printf("gpu_stall_dramfull = %d\n", gpu_stall_dramfull);
   printf("gpu_stall_icnt2sh    = %d\n", gpu_stall_icnt2sh );
   printf("cpu_stall_cpu2icnt    = %d\n", cpu_stall_cpu2icnt );

   // Onur added
   printf("gpu_stall_dramfull_start = %d\n", gpu_stall_dramfull_start );
   printf("gpu_stall_icnt2sh_start    = %d\n", gpu_stall_icnt2sh_start );

   // Onur added
   printf("gpu_icnt_push = %u\n", gpu_icnt_push);
   printf("cpu_icnt_push = %u\n", cpu_icnt_push);
   printf("mem_icnt_push = %u\n", mem_icnt_push);

   printf("average_number_of_warps = %f\n", (double)gpu_warp_limit_sum / (double)gpu_warp_limit_num );

   time_t curr_time;
   time(&curr_time);
   unsigned long long elapsed_time = MAX( curr_time - g_simulation_starttime, 1 );
   printf( "gpu_total_sim_rate=%u\n", (unsigned)( ( gpu_tot_sim_insn + gpu_sim_insn ) / elapsed_time ) );

   shader_print_l1_miss_stat( stdout );

   m_shader_stats->print(stdout);
#ifdef GPGPUSIM_POWER_MODEL
   if(m_config.g_power_simulation_enabled){
       m_gpgpusim_wrapper->print_power_kernel_stats(gpu_sim_cycle,gpu_tot_sim_cycle,gpu_tot_sim_insn + gpu_sim_insn );
   }
#endif

   // performance counter that are not local to one shader
   m_memory_stats->memlatstat_print(m_memory_config->m_n_mem,m_memory_config->nbk);
   m_memory_stats->print(stdout);
   for (unsigned i=0;i<m_memory_config->m_n_mem;i++) 
      m_memory_partition_unit[i]->print(stdout);
   if (!m_memory_config->m_L2_config.disabled() && m_memory_config->m_L2_config.get_num_lines())
      L2c_print_cache_stat();
   if (m_config.gpgpu_cflog_interval != 0) {
      spill_log_to_file (stdout, 1, gpu_sim_cycle);
      insn_warp_occ_print(stdout);
   }
   if ( gpgpu_ptx_instruction_classification ) {
      StatDisp( g_inst_classification_stat[g_ptx_kernel_count]);
      StatDisp( g_inst_op_classification_stat[g_ptx_kernel_count]);
   }

#ifdef GPGPUSIM_POWER_MODEL
   if(m_config.g_power_simulation_enabled){
       m_gpgpusim_wrapper->detect_print_steady_state(1,gpu_tot_sim_insn+gpu_sim_insn);
   }
#endif


   // Interconnect power stat print
   unsigned total_mem_to_simt=0;
   unsigned total_simt_to_mem=0;
   for (unsigned i=0;i<m_memory_config->m_n_mem;i++){
      unsigned temp=0;
      m_memory_partition_unit[i]->set_icnt_power_stats(temp);
      total_mem_to_simt += temp;
   }
   for(unsigned i=0; i<m_config.num_cluster(); i++){
	   unsigned temp=0;
	   m_cluster[i]->set_icnt_stats(temp);
	   total_simt_to_mem += temp;
   }
   printf("\nicnt_total_pkts_mem_to_simt=%u\n", total_mem_to_simt);
   printf("icnt_total_pkts_simt_to_mem=%u\n\n", total_simt_to_mem);

   time_vector_print();
   fflush(stdout);

   print_cpu_stats();


}


// performance counter that are not local to one shader
unsigned gpgpu_sim::threads_per_core() const 
{ 
   return m_shader_config->n_thread_per_shader; 
}

void shader_core_ctx::mem_instruction_stats(const warp_inst_t &inst)
{
    unsigned active_count = inst.active_count(); 
    //this breaks some encapsulation: the is_[space] functions, if you change those, change this.
    switch (inst.space.get_type()) {
    case undefined_space:
    case reg_space:
        break;
    case shared_space:
        m_stats->gpgpu_n_shmem_insn += active_count; 
        break;
    case const_space:
        m_stats->gpgpu_n_const_insn += active_count;
        break;
    case param_space_kernel:
    case param_space_local:
        m_stats->gpgpu_n_param_insn += active_count;
        break;
    case tex_space:
        m_stats->gpgpu_n_tex_insn += active_count;
        break;
    case global_space:
    case local_space:
        if( inst.is_store() )
            m_stats->gpgpu_n_store_insn += active_count;
        else 
            m_stats->gpgpu_n_load_insn += active_count;
        break;
    default:
        abort();
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Launches a cooperative thread array (CTA). 
 *  
 * @param kernel 
 *    object that tells us which kernel to ask for a CTA from 
 */

void shader_core_ctx::issue_block2core( kernel_info_t &kernel ) 
{
    set_max_cta(kernel);

    // find a free CTA context 
    unsigned free_cta_hw_id=(unsigned)-1;
    for (unsigned i=0;i<kernel_max_cta_per_shader;i++ ) {
      if( m_cta_status[i]==0 ) {
         free_cta_hw_id=i;
         break;
      }
    }
    assert( free_cta_hw_id!=(unsigned)-1 );

    // determine hardware threads and warps that will be used for this CTA
    int cta_size = kernel.threads_per_cta();

    // hw warp id = hw thread id mod warp size, so we need to find a range 
    // of hardware thread ids corresponding to an integral number of hardware
    // thread ids
    int padded_cta_size = cta_size; 
    if (cta_size%m_config->warp_size)
      padded_cta_size = ((cta_size/m_config->warp_size)+1)*(m_config->warp_size);
    unsigned start_thread = free_cta_hw_id * padded_cta_size;
    unsigned end_thread  = start_thread +  cta_size;

    // reset the microarchitecture state of the selected hardware thread and warp contexts
    reinit(start_thread, end_thread,false);
     
    // initalize scalar threads and determine which hardware warps they are allocated to
    // bind functional simulation state of threads to hardware resources (simulation) 
    warp_set_t warps;
    unsigned nthreads_in_block= 0;
    for (unsigned i = start_thread; i<end_thread; i++) {
        m_threadState[i].m_cta_id = free_cta_hw_id;
        unsigned warp_id = i/m_config->warp_size;
        nthreads_in_block += ptx_sim_init_thread(kernel,&m_thread[i],m_sid,i,cta_size-(i-start_thread),m_config->n_thread_per_shader,this,free_cta_hw_id,warp_id,m_cluster->get_gpu());
        m_threadState[i].m_active = true; 
        warps.set( warp_id );
    }
    assert( nthreads_in_block > 0 && nthreads_in_block <= m_config->n_thread_per_shader); // should be at least one, but less than max
    m_cta_status[free_cta_hw_id]=nthreads_in_block;

    // now that we know which warps are used in this CTA, we can allocate
    // resources for use in CTA-wide barrier operations
    m_barriers.allocate_barrier(free_cta_hw_id,warps);

    // initialize the SIMT stacks and fetch hardware
    init_warps( free_cta_hw_id, start_thread, end_thread);
    m_n_active_cta++;

    shader_CTA_count_log(m_sid, 1);
    printf("GPGPU-Sim uArch: core:%3d, cta:%2u initialized @(%lld,%lld)\n", m_sid, free_cta_hw_id, gpu_sim_cycle, gpu_tot_sim_cycle );
}

///////////////////////////////////////////////////////////////////////////////////////////

void dram_t::dram_log( int task ) 
{
   if (task == SAMPLELOG) {
      StatAddSample(mrqq_Dist, que_length());   
   } else if (task == DUMPLOG) {
      printf ("Queue Length DRAM[%d] ",id);StatDisp(mrqq_Dist);
   }
}

//Find next clock domain and increment its time
int gpgpu_sim::next_clock_domain(void) 
{
	// Onur modified: added cpu_time
   double smallest = min4(core_time,icnt_time,dram_time,cpu_time);

   int mask = 0x00;
   if ( l2_time <= smallest ) {
      smallest = l2_time;
      mask |= L2 ;
      l2_time += m_config.l2_period;
   }
   if ( icnt_time <= smallest ) {
      mask |= ICNT;
      icnt_time += m_config.icnt_period;
   }
   if ( dram_time <= smallest ) {
      mask |= DRAM;
      dram_time += m_config.dram_period;
   }
   if ( core_time <= smallest ) {
      mask |= CORE;
      core_time += m_config.core_period;
   }

   // Onur added: CPU clock domain
   if ( cpu_time <= smallest ) {
      mask |= CPU;
      cpu_time += m_config.cpu_period;
   }

   return mask;
}

void gpgpu_sim::issue_block2core()
{
    unsigned last_issued = m_last_cluster_issue; 
    for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) {
        unsigned idx = (i + last_issued + 1) % m_shader_config->n_simt_clusters;
        unsigned num = m_cluster[idx]->issue_block2core();
        if( num ) {
            m_last_cluster_issue=idx;
            m_total_cta_launched += num;
        }
    }
}

unsigned long long g_single_step=0; // set this in gdb to single step the pipeline

void gpgpu_sim::cycle()
{
   int clock_mask = next_clock_domain();

	if (clock_mask & CORE ) {
	   // shader core loading (pop from ICNT into core) follows CORE clock
	  for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) {
		 m_cluster[i]->icnt_cycle();
	  }
	}

   // Onur added: CPU
   if (clock_mask & CPU) {
       // shader core loading (pop from ICNT into CPU) follows CPU clock
	   //unsigned int cpu_id;
      for (unsigned i=m_shader_config->n_simt_clusters,cpu_id=0;i<m_shader_config->n_simt_clusters+m_shader_config->gpgpu_n_cpu;i++, cpu_id++)
      {
    	  processor[cpu_id].cpu_icnt_cycle();
      }
   }

    if (clock_mask & ICNT) {
        // pop from memory controller to interconnect
        for (unsigned i=0;i<m_memory_config->m_n_mem;i++) {
            mem_fetch* mf = m_memory_partition_unit[i]->top();
            if (mf) {
                unsigned response_size = mf->get_is_write()?mf->get_ctrl_size():mf->size();

                // Onur added
                int cpu_or_gpu;
                if(mf->get_tpc() < m_shader_config->n_simt_clusters)
                	cpu_or_gpu = 1;   //memory to gpu  //FIXXX enum oR define the number : MEM_TO_GPU = 1
                else {
                	cpu_or_gpu = 3;   //memory to cpu  //FIXXX enum oR define the number : MEM_TO_CPU = 3
                	response_size = mf->get_is_write() ? 8 : 128; //FIXXX this is a quick fix. fix it.
                }
                //FIXXX enum oR define the second parameter in icnt_has_buffer : cpu_or_gpu
                if ( ::icnt_has_buffer( m_shader_config->mem2device(i), cpu_or_gpu, response_size ) ) {
                    if (!mf->get_is_write()) 
                       mf->set_return_timestamp(gpu_sim_cycle+gpu_tot_sim_cycle);
                    mf->set_status(IN_ICNT_TO_SHADER,gpu_sim_cycle+gpu_tot_sim_cycle);

                    //Connect to intersim/interconnect_interference.cpp
                    ::icnt_push( m_shader_config->mem2device(i), mf->get_tpc(), mf, response_size );

  	  	  // Onur added
                    if (start_gpu_execution)
                    	mem_icnt_push++;

                    m_memory_partition_unit[i]->pop();
                } else {
                    gpu_stall_icnt2sh++;
                }
            } else {
               m_memory_partition_unit[i]->pop();
            }
        }
    }

   if (clock_mask & DRAM) {
      for (unsigned i=0;i<m_memory_config->m_n_mem;i++){
         m_memory_partition_unit[i]->dram_cycle(); // Issue the dram command (scheduler + delay model)
         // Update performance counters for DRAM
         m_memory_partition_unit[i]->set_dram_power_stats(m_power_stats->pwr_mem_stat->n_cmd[0][i], m_power_stats->pwr_mem_stat->n_activity[0][i],
                        m_power_stats->pwr_mem_stat->n_nop[0][i], m_power_stats->pwr_mem_stat->n_act[0][i], m_power_stats->pwr_mem_stat->n_pre[0][i],
                        m_power_stats->pwr_mem_stat->n_rd[0][i], m_power_stats->pwr_mem_stat->n_wr[0][i], m_power_stats->pwr_mem_stat->n_req[0][i]);
      }
   }

   // L2 operations follow L2 clock domain
   if (clock_mask & L2) {
      for (unsigned i=0;i<m_memory_config->m_n_mem;i++) {
          //move memory request from interconnect into memory partition (if not backed up)
          //Note:This needs to be called in DRAM clock domain if there is no L2 cache in the system
          if ( m_memory_partition_unit[i]->full() ) {
             gpu_stall_dramfull++;
          } else {
              mem_fetch* mf = (mem_fetch*) icnt_pop( m_shader_config->mem2device(i) );
           	  m_memory_partition_unit[i]->push( mf, gpu_sim_cycle + gpu_tot_sim_cycle );
          }

          m_memory_partition_unit[i]->cache_cycle(gpu_sim_cycle+gpu_tot_sim_cycle);
          m_memory_partition_unit[i]->set_L2cache_power_stats(m_power_stats->pwr_mem_stat->n_l2_read_access[0][i], m_power_stats->pwr_mem_stat->n_l2_read_miss[0][i],
          m_power_stats->pwr_mem_stat->n_l2_write_access[0][i], m_power_stats->pwr_mem_stat->n_l2_write_miss[0][i]);

          m_memory_partition_unit[i]->set_icnt_power_stats(m_power_stats->pwr_mem_stat->n_mem_to_simt[0][i]);
       }
   }

   if (clock_mask & ICNT) {
      icnt_transfer();
   }


   if (clock_mask & CORE) {
	  // L1 cache + shader core pipeline stages
	  for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) {
		 if (m_cluster[i]->get_not_completed() || get_more_cta_left() ) {
			   m_cluster[i]->core_cycle();
			   *active_sms+=m_cluster[i]->get_n_active_sms();

			   // Interconnect power stats: SIMT->MEM
			   m_cluster[i]->set_icnt_stats(m_power_stats->pwr_mem_stat->n_simt_to_mem[0][i]);
		 }
	  }
	  float temp=0;
	  for (unsigned i=0;i<m_shader_config->num_shader();i++){
		temp+=m_shader_stats->m_pipeline_duty_cycle[i];
	  }
	  temp=temp/m_shader_config->num_shader();
	  *average_pipeline_duty_cycle=((*average_pipeline_duty_cycle)+temp);
		//cout<<"Average pipeline duty cycle: "<<*average_pipeline_duty_cycle<<endl;


	  if( g_single_step && ((gpu_sim_cycle+gpu_tot_sim_cycle) >= g_single_step) ) {
		  asm("int $03");
	  }
          simcycle++;
	  gpu_sim_cycle++;
	  if( g_interactive_debugger_enabled )
		  gpgpu_debug();

	  // McPAT main cycle (interface with McPAT)
#ifdef GPGPUSIM_POWER_MODEL
	  if(m_config.g_power_simulation_enabled){
		  mcpat_cycle(m_config, getShaderCoreConfig(), m_gpgpusim_wrapper, m_power_stats, m_config.gpu_stat_sample_freq, gpu_tot_sim_cycle, gpu_sim_cycle, gpu_tot_sim_insn, gpu_sim_insn);
	  }
#endif

	  // Onur added
	  if (start_gpu_execution && m_config.gpgpu_enable_gpu)
		  issue_block2core();

	  // Depending on configuration, flush the caches once all of threads are completed.
	  int all_threads_complete = 1;
	  if (m_config.gpgpu_flush_l1_cache) {
		 for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) {
			if (m_cluster[i]->get_not_completed() == 0)
				m_cluster[i]->cache_flush();
			else
			   all_threads_complete = 0 ;
		 }
	  }

	  if(m_config.gpgpu_flush_l2_cache){
		  if(!m_config.gpgpu_flush_l1_cache){
			  for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) {
				  if (m_cluster[i]->get_not_completed() != 0){
					  all_threads_complete = 0 ;
					  break;
				  }
			  }
		  }

		 if (all_threads_complete && !m_memory_config->m_L2_config.disabled() ) {
			printf("Flushed L2 caches...\n");
			if (m_memory_config->m_L2_config.get_num_lines()) {
			   int dlc = 0;
			   for (unsigned i=0;i<m_memory_config->m_n_mem;i++) {
				  dlc = m_memory_partition_unit[i]->flushL2();
				  assert (dlc == 0); // need to model actual writes to DRAM here
				  printf("Dirty lines flushed from L2 %d is %d\n", i, dlc  );
			   }
			}
		 }
	  }

	  if (m_shader_config->gpgpu_enable_dyn_throttling) {
			  throttle();
	  }

	  if (!(gpu_sim_cycle % m_config.gpu_stat_sample_freq)) {
		 time_t days, hrs, minutes, sec;
		 time_t curr_time;
		 time(&curr_time);
		 unsigned long long  elapsed_time = MAX(curr_time - g_simulation_starttime, 1);
		 days    = elapsed_time/(3600*24);
		 hrs     = elapsed_time/3600 - 24*days;
		 minutes = elapsed_time/60 - 60*(hrs + 24*days);
		 sec = elapsed_time - 60*(minutes + 60*(hrs + 24*days));
		 printf("GPGPU-Sim uArch: cycles simulated: %lld  inst.: %lld (ipc=%4.1f) sim_rate=%u (inst/sec) elapsed = %u:%u:%02u:%02u / %s",
				gpu_tot_sim_cycle + gpu_sim_cycle, gpu_tot_sim_insn + gpu_sim_insn,
				(double)gpu_sim_insn/(double)gpu_sim_cycle,
				(unsigned)((gpu_tot_sim_insn+gpu_sim_insn) / elapsed_time),
				(unsigned)days,(unsigned)hrs,(unsigned)minutes,(unsigned)sec,
				ctime(&curr_time));

		 fflush(stdout);
		 visualizer_printstat();
		 m_memory_stats->memlatstat_lat_pw();
		 if (m_config.gpgpu_runtime_stat && (m_config.gpu_runtime_stat_flag != 0) ) {
			if (m_config.gpu_runtime_stat_flag & GPU_RSTAT_BW_STAT) {
			   for (unsigned i=0;i<m_memory_config->m_n_mem;i++)
				  m_memory_partition_unit[i]->print_stat(stdout);
			   printf("maxmrqlatency = %d \n", m_memory_stats->max_mrq_latency);
			   printf("maxmflatency = %d \n", m_memory_stats->max_mf_latency);
			}
			if (m_config.gpu_runtime_stat_flag & GPU_RSTAT_SHD_INFO)
			   shader_print_runtime_stat( stdout );
			if (m_config.gpu_runtime_stat_flag & GPU_RSTAT_L1MISS)
			   shader_print_l1_miss_stat( stdout );

            // Onur added
            if (m_config.gpu_runtime_stat_flag & GPU_RSTAT_LDST)
               shader_print_ldst_stat( stdout );
            // Onur added
            if ((m_config.gpu_runtime_stat_flag & GPU_RSTAT_ICNT_PUSH) && start_gpu_execution) {
               printf("cpu_icnt_push = %u\n", cpu_icnt_push);
               printf("gpu_icnt_push = %u\n", gpu_icnt_push);
               printf("mem_icnt_push = %u\n", mem_icnt_push);

               // performance counter for stalls due to congestion.
               printf("gpu_stall_dramfull = %d\n", gpu_stall_dramfull);
               printf("gpu_stall_icnt2sh    = %d\n", gpu_stall_icnt2sh );
               printf("cpu_stall_cpu2icnt    = %d\n", cpu_stall_cpu2icnt );

               printf("average_number_of_warps = %f\n", (double)gpu_warp_limit_sum / (double)gpu_warp_limit_num );
               printf("average_number_of_warps_window = %llu\n", gpu_warp_limit_sum - gpu_warp_limit_sum_last);
            }
            // Onur added
            if (m_config.gpu_runtime_stat_flag & GPU_RSTAT_MP_INFO)
            for (unsigned i=0;i<m_memory_config->m_n_mem;i++)
            	m_memory_partition_unit[i]->print(stdout);

            // Onur added
            if ( (m_config.gpu_runtime_stat_flag & GPU_RSTAT_LATTOL) && start_gpu_execution ) {
            	unsigned long long numPendingWrites_window = m_shader_stats->numPendingWrites - m_shader_stats->numPendingWrites_last_print;
            	unsigned long long ScoreboardSize_window = m_shader_stats->ScoreboardSize - m_shader_stats->ScoreboardSize_last_print;
            	unsigned long long numMemWarps_window = m_shader_stats->numMemWarps - m_shader_stats->numMemWarps_last_print;
            	m_shader_stats->numPendingWrites_last_print = m_shader_stats->numPendingWrites;
            	m_shader_stats->ScoreboardSize_last_print = m_shader_stats->ScoreboardSize;
            	m_shader_stats->numMemWarps_last_print = m_shader_stats->numMemWarps;
//            	printf("num_pending_writes_window = %llu\n", numPendingWrites_window);
            	printf("scoreboard_size_window = %llu\n", ScoreboardSize_window);
            	printf("num_mem_warps_window = %llu\n", numMemWarps_window);
            	printf("compute_to_memory_window = %f\n", (double)(ScoreboardSize_window - numMemWarps_window) / (double)numMemWarps_window );
//            	printf("Valid_notissued = %llu\n", m_shader_stats->valid_notissued);
            	printf("W0_idle = %u\n", m_shader_stats->shader_cycle_distro[0]);
            	printf("W0_scoreboard = %u\n", m_shader_stats->shader_cycle_distro[1]);
            	printf("Pipe_stall = %u\n", m_shader_stats->shader_cycle_distro[2]);
            }

		 }
	  }

	  // Onur modified: WAS: if (!(gpu_sim_cycle % 20000)) {
	  if (!(gpu_sim_cycle % 20000) && start_gpu_execution && m_config.gpgpu_enable_gpu) {
		 // deadlock detection
		 if (m_config.gpu_deadlock_detect && gpu_sim_insn == last_gpu_sim_insn) {
			gpu_deadlock = true;
		 } else {
			last_gpu_sim_insn = gpu_sim_insn;
		 }
	  }

	  // Onur added: terminate once reaching n GPU instructions
		if (!(gpu_sim_cycle % 200)) {
			if (m_config.gpu_max_insn_opt && (gpu_tot_sim_insn + gpu_sim_insn) >= m_config.gpu_max_insn_opt) {
				gpu_max_insn_struck = true;
			}
		}

	  try_snap_shot(gpu_sim_cycle);
	  spill_log_to_file (stdout, 0, gpu_sim_cycle);
   }


   // Run CPU cycle
   // Check how CORE is simulated, and see how packets are injected
   // Using push, in gpu-cache
   // Look into CORE L1 cache
   // Print stats every stat sampling period
   // Onur added: CPU
   if (clock_mask & CPU) {
		/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
		//START SIMULATION ENGINE
		/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
	   //printf("Entering CPU cycle %d.\n",cpu_global_clock);
			cpu_cycle();
			cpu_global_clock++;
   }
}


void shader_core_ctx::dump_warp_state( FILE *fout ) const
{
   fprintf(fout, "\n");
   fprintf(fout, "per warp functional simulation status:\n");
   for (unsigned w=0; w < m_config->max_warps_per_shader; w++ ) 
       m_warp[w].print(fout);
}

void gpgpu_sim::dump_pipeline( int mask, int s, int m ) const
{
/*
   You may want to use this function while running GPGPU-Sim in gdb.
   One way to do that is add the following to your .gdbinit file:
 
      define dp
         call g_the_gpu.dump_pipeline_impl((0x40|0x4|0x1),$arg0,0)
      end
 
   Then, typing "dp 3" will show the contents of the pipeline for shader core 3.
*/

   printf("Dumping pipeline state...\n");
   if(!mask) mask = 0xFFFFFFFF;
   for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) {
      if(s != -1) {
         i = s;
      }
      if(mask&1) m_cluster[m_shader_config->sid_to_cluster(i)]->display_pipeline(i,stdout,1,mask & 0x2E);
      if(s != -1) {
         break;
      }
   }
   if(mask&0x10000) {
      for (unsigned i=0;i<m_memory_config->m_n_mem;i++) {
         if(m != -1) {
            i=m;
         }
         printf("DRAM / memory controller %u:\n", i);
         if(mask&0x100000) m_memory_partition_unit[i]->print_stat(stdout);
         if(mask&0x1000000)   m_memory_partition_unit[i]->visualize();
         if(mask&0x10000000)   m_memory_partition_unit[i]->print(stdout);
         if(m != -1) {
            break;
         }
      }
   }
   fflush(stdout);
}

const struct shader_core_config * gpgpu_sim::getShaderCoreConfig()
{
   return m_shader_config;
}

const struct memory_config * gpgpu_sim::getMemoryConfig()
{
   return m_memory_config;
}

simt_core_cluster * gpgpu_sim::getSIMTCluster()
{
   return *m_cluster;
}

void memory_partition_unit::visualizer_print( gzFile visualizer_file )
{
   m_dram->visualizer_print(visualizer_file);
}

void gpgpu_sim::print_cpu_stats()
{
	printf("\n************Summary for End of Kernel **************\n");

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
//	 print_stats_cpu();

	getAvgStats(instruction_passes);

        //Energy
        p_tot = p_buffer + p_routing + p_vc_arb + p_sw_arb + p_xbar + p_link + p_retrans + p_static * 64*0.5 + p_photonic;
        printf(" Total Energy Consumption: %16.7f nJ(10^-9)\n", p_tot/1000);

	printf("************Summary for End of Kernel **************\n\n");

}
