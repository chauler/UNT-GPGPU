#ifndef __PEPSI_INIT_H__
#define __PEPSI_INIT_H__
/* Function Declarations */

void sim_setup(char *config_file);
//void nox_setup(char *config_file);
void trace_setup(char *config_file,char *benchname, int StartProc, int EndProc);
void killcachesim();

#endif
