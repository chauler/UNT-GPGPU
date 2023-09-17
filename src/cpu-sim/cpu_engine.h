/*
 * cpu_engine.h
 *
 *  Created on: Feb 6, 2013
 *      Author: oik5019
 */

#ifndef CPU_ENGINE_H_
#define CPU_ENGINE_H_

#include "../option_parser.h"

void cpu_init(unsigned int num_cpus);
void cpu_cycle();
void cpu_reg_options(option_parser_t opp);

//long long compress_fvc(TRANSTYPE *tr);
//short fetch_csize(long long data[16]);

void simulate_memory();
void dump_state();
bool mlc_transfer_overflow(int cpuid);
unsigned cpu2device(unsigned cpuid);
unsigned cpu2core(unsigned cpuid);

extern long long cpu_global_clock;

#endif /* CPU_ENGINE_H_ */
