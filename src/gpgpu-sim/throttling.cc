/*
 * throttling.cc
 *
 *  Created on: Apr 10, 2014
 *      Author: oik5019
 */

#include "gpu-sim.h"
#include "../cpu-sim/globals.h"

#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))

// Onur added for monitoring congestion metrics
unsigned int stall_dramfull_last = 0;
unsigned int stall_dramfull_window = 0;
unsigned int stall_icnt2sh_last = 0;
unsigned int stall_icnt2sh_window = 0;
unsigned int memory_waiting_last = 0;
unsigned int memory_waiting_window = 0;
unsigned int memory_waiting_window_last = 0;
unsigned int warp_limit = 0;
unsigned int warp_limit_last = 0;
unsigned int memory_waiting_avg_cur = 0;
unsigned int memory_waiting_avg_past = 0;
unsigned int stall_dramfull_avg_cur = 0;
unsigned int stall_dramfull_avg_past = 0;
unsigned int stall_icnt2sh_avg_cur = 0;
unsigned int stall_icnt2sh_avg_past = 0;
unsigned int stall_avg_cur = 0;
unsigned int stall_avg_past = 0;
unsigned int warp_limit_avg_past = 0;
unsigned int warp_limit_avg_cur = 0;
double com_to_mem = 0;
double com_to_mem_last = 0;
unsigned int inactive_cyc = 0;
unsigned int inactive_cyc_last = 0;
unsigned int inactive_cyc_window = 0;
unsigned int inactive_cyc_window_last = 0;
unsigned iter = 0;
int step = 2;

std::map<unsigned/*warp_limit*/, unsigned/*moving_average*/> moving_avg;

void gpgpu_sim::throttle() {

//	printf("throttling option = %d\n", m_shader_config->gpgpu_enable_dyn_throttling);

	if( gpu_sim_cycle == 1 ) {
		unsigned initial_warp_limit = m_cluster[0]->get_initial_warp_limit();
		for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) {
			m_cluster[i]->set_num_warps(initial_warp_limit);
		}
		moving_avg.clear();
	}

	if (!(gpu_sim_cycle % m_config.gpu_warplimiting_sample_freq)) {
		inactive_cyc = m_shader_stats->shader_cycle_distro[0] + m_shader_stats->shader_cycle_distro[1] + m_shader_stats->shader_cycle_distro[2];
		inactive_cyc_window = inactive_cyc - inactive_cyc_last;

		if ( start_gpu_execution ) {

			enum congestion_metric_level dramfull_level;
			enum congestion_metric_level icnt2sh_level;
			enum congestion_metric_level com_mem_level;

			bool enable_decrement = false;
			bool enable_increment = false;

			warp_limit = m_cluster[0]->get_warp_limit();

			stall_dramfull_window = gpu_stall_dramfull - stall_dramfull_last;
			stall_icnt2sh_window = gpu_stall_icnt2sh - stall_icnt2sh_last;

			stall_dramfull_avg_cur = stall_dramfull_window / 4 + (3 * stall_dramfull_avg_past) / 4;
			stall_icnt2sh_avg_cur = stall_icnt2sh_window / 4 + (3 * stall_icnt2sh_avg_past) / 4;
			stall_avg_cur = MAX(stall_dramfull_avg_cur,stall_icnt2sh_avg_cur);

			memory_waiting_window = m_shader_stats->memory_waiting - memory_waiting_last;
			memory_waiting_avg_cur = memory_waiting_window / 4 + (3 * memory_waiting_avg_past) / 4;

			warp_limit_avg_cur = warp_limit / 4 + (3 * warp_limit_avg_past) / 4;

			unsigned long long ScoreboardSize = m_shader_stats->ScoreboardSize;
			unsigned long long numMemWarps = m_shader_stats->numMemWarps;
			unsigned long long numPendingWrites = m_shader_stats->numPendingWrites;
			unsigned long long ScoreboardSize_last = m_shader_stats->ScoreboardSize_last_scheme;
			unsigned long long numMemWarps_last = m_shader_stats->numMemWarps_last_scheme;
			unsigned long long numPendingWrites_last = m_shader_stats->numPendingWrites_last_scheme;
			m_shader_stats->ScoreboardSize_last_scheme = ScoreboardSize;
			m_shader_stats->numMemWarps_last_scheme = numMemWarps;
			m_shader_stats->numPendingWrites_last_scheme = numPendingWrites;
			unsigned long long ScoreboardSize_window = ScoreboardSize - ScoreboardSize_last;
			unsigned long long numMemWarps_window = numMemWarps - numMemWarps_last;
			unsigned long long numPendingWrites_window = numPendingWrites - numPendingWrites_last;
			unsigned long long Compute_window = ScoreboardSize_window - numMemWarps_window;
			com_to_mem = (double) Compute_window / (double) numMemWarps_window;

			if(start_gpu_execution) {
			  gpu_warp_limit_sum_last = gpu_warp_limit_sum;
			  gpu_warp_limit_sum += warp_limit;
			  gpu_warp_limit_num++;
			}

			if ( (double) stall_dramfull_window >= m_config.gpu_warplimiting_sample_freq * (double)1 )
			  dramfull_level = HIGH;
			else if ( (double) stall_dramfull_window < m_config.gpu_warplimiting_sample_freq * (double)0.25 )
			  dramfull_level = LOW;
			else
			  dramfull_level = MED;

			if ( (double) stall_icnt2sh_window >= m_config.gpu_warplimiting_sample_freq * (double)1 )
			  icnt2sh_level = HIGH;
			else if ( (double) stall_icnt2sh_window < m_config.gpu_warplimiting_sample_freq * (double)0.25 )
			  icnt2sh_level = LOW;
			else
			  icnt2sh_level = MED;

			if ( (double) Compute_window >= (double)numPendingWrites_window * (double)1.25 )
				com_mem_level = HIGH;
			else if ( (double) Compute_window < (double)numPendingWrites_window * (double)0.75 )
				com_mem_level = LOW;
			else
				com_mem_level = MED;

		//	printf("m_shader_stats->memory_waiting = %u\n", m_shader_stats->memory_waiting);

			if (m_shader_config->gpgpu_enable_dyn_throttling == 1) { // DRAM-full
			  if ( dramfull_level == HIGH )
				  enable_decrement = true;
			  else if ( dramfull_level == LOW )
				  enable_increment = true;
			}
			else if (m_shader_config->gpgpu_enable_dyn_throttling == 2) { // two metrics
			  if ( (dramfull_level == HIGH) || (icnt2sh_level == HIGH) )
				  enable_decrement = true;
			  else if ( (dramfull_level == LOW) && (icnt2sh_level == LOW) )
				  enable_increment = true;
			}

			else if (m_shader_config->gpgpu_enable_dyn_throttling == 3) { // W0_mem + two metrics

			  if ( (dramfull_level == HIGH) || (icnt2sh_level == HIGH) )
				  enable_decrement = true;
			  else if ( (dramfull_level == LOW) && (icnt2sh_level == LOW) )
				  enable_increment = true;

		//	printf("enable decrement = %d, enable increment = %d\n", enable_decrement, enable_increment);
		//	printf("warp limit = %u, warp limit last = %u\n", warp_limit, warp_limit_last);
		//	printf("memory waiting window = %u, memory waiting moving avg. = %u\n", memory_waiting_window, memory_waiting_avg_cur);
		//
		//	printf("first = %f\n", memory_waiting_window_last * ( gpgpu_w0mem_th_1 / (double) 100 ));
		//	printf("second = %f\n", memory_waiting_window * ( gpgpu_w0mem_th_2 / (double) 100 ));

			  if ( warp_limit && warp_limit_last ) {
					  if ( warp_limit < warp_limit_last ) {
							  if ( memory_waiting_window * warp_limit > memory_waiting_window_last * warp_limit_last * (double) 1.25 ) {
									  enable_decrement = false;
									  enable_increment = true;
							  }
					  }
					  else if ( warp_limit > warp_limit_last ) {
							  if ( memory_waiting_window * warp_limit * (double) 2 < memory_waiting_window_last * warp_limit_last ) {
									  enable_decrement = false;
									  enable_increment = true;
							  }
					  }
			  }

			//				  printf("enable decrement = %d, enable increment = %d\n", enable_decrement, enable_increment);
			}
		//	else if (m_shader_config->gpgpu_enable_dyn_throttling == 4) { // W0_mem + two metrics - epoch based switching
		//
		//	  if ( (dramfull_level == HIGH) || (icnt2sh_level == HIGH) )
		//		  enable_decrement = true;
		//	  else if ( (dramfull_level == LOW) && (icnt2sh_level == LOW) )
		//		  enable_increment = true;
		//
		//	  if ( ( stall_avg_cur > stall_avg_past ) && ( memory_waiting_avg_cur > memory_waiting_avg_past ) )
		//		  enable_scheme2 = ~enable_scheme2;
		//	  else if ( ( stall_avg_cur > stall_avg_past ) && ( memory_waiting_avg_cur < memory_waiting_avg_past ) )
		//		  enable_scheme2 = false;
		//	  else if ( ( stall_avg_cur < stall_avg_past ) && ( memory_waiting_avg_cur > memory_waiting_avg_past ) )
		//		  enable_scheme2 = true;
		//
		//
		//	  printf("enable decrement = %d, enable increment = %d\n", enable_decrement, enable_increment);
		//	  printf("warp limit = %u, warp limit last = %u\n", warp_limit, warp_limit_last);
		//	  printf("memory waiting window = %u, memory waiting moving avg. = %u\n", memory_waiting_window, memory_waiting_avg_cur);
		//
		//	  if (enable_scheme2) {
		//		  if ( warp_limit && warp_limit_last ) {
		//			  if ( warp_limit < warp_limit_last ) {
		//				  //if ( memory_waiting_window * warp_limit > memory_waiting_window_last * warp_limit_last * (double) 1.25 ) {
		//				  if ( memory_waiting_avg_cur > memory_waiting_avg_past * ( (double) gpgpu_w0mem_th_1 / (double) 100 ) ) { // 1.25
		//					  enable_decrement = false;
		//					  enable_increment = true;
		//				  }
		//			  }
		//			  else if ( warp_limit > warp_limit_last ) {
		//				  //if ( memory_waiting_window * warp_limit * (double) 2 < memory_waiting_window_last * warp_limit_last ) {
		//				  if ( memory_waiting_avg_cur * ( (double) gpgpu_w0mem_th_2 / (double) 100 ) < memory_waiting_avg_past ) { // 2
		//					  enable_decrement = false;
		//					  enable_increment = true;
		//				  }
		//			  }
		//		  }
		//	  }
		//	}
			else if (m_shader_config->gpgpu_enable_dyn_throttling == 5) { // DYNCTA-like

			  // apply dyncta locally on each core
			  for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) {
				  m_cluster[i]->dyncta(); //
			  }

			  // set enables to false so that another decision is not done below
			  enable_decrement = false;
			  enable_increment = false;
			//				  printf("enable decrement = %d, enable increment = %d\n", enable_decrement, enable_increment);
			//				  printf("warp limit = %u, warp limit last = %u\n", warp_limit, warp_limit_last);
			//				  printf("idle window = %u, ", idle_window);
			//				  printf("memory waiting window = %u\n", memory_waiting_window);
			}
			else if (m_shader_config->gpgpu_enable_dyn_throttling == 6) { // if high lat. tolerance, throttle; o.w. DYNCTA-like
				bool is_dyncta = false;

			//	printf("enable decrement = %d, enable increment = %d\n", enable_decrement, enable_increment);
			//	printf("warp limit = %u, warp limit last = %u\n", warp_limit, warp_limit_last);
			//	printf("memory waiting window = %u, memory waiting moving avg. = %u\n", memory_waiting_window, memory_waiting_avg_cur);
			//
			//	printf("first = %f\n", memory_waiting_window_last * ( gpgpu_w0mem_th_1 / (double) 100 ));
			//	printf("second = %f\n", memory_waiting_window * ( gpgpu_w0mem_th_2 / (double) 100 ));

				  if ( warp_limit && warp_limit_last ) {
					  if ( warp_limit < warp_limit_last ) {
						  if ( memory_waiting_window * warp_limit > memory_waiting_window_last * warp_limit_last * (double) 1.25 ) {
							  is_dyncta = true;
						  }
					  }
					  else if ( warp_limit > warp_limit_last ) {
						  if ( memory_waiting_window * warp_limit * (double) 2 < memory_waiting_window_last * warp_limit_last ) {
							  is_dyncta = true;
						  }
					  }
				  }
				  if(is_dyncta) {
					  enable_decrement = false;
					  enable_increment = false;

					  // apply dyncta locally on each core
					  for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) {
						  m_cluster[i]->dyncta(); //
					  }
				  }
				  else {
					  if ( (dramfull_level == HIGH) || (icnt2sh_level == HIGH) ) {
						  enable_decrement = true;
						  enable_increment = false;
					  }
					  else if ( (dramfull_level == LOW) && (icnt2sh_level == LOW) ) {
						  enable_increment = true;
						  enable_decrement = false;
					  }
				  }
			}
			else if (m_shader_config->gpgpu_enable_dyn_throttling == 7) {
				if ( com_mem_level == LOW )
				  enable_decrement = true;
				else if ( com_mem_level == HIGH )
				  enable_increment = true;
			}
			else if (m_shader_config->gpgpu_enable_dyn_throttling == 8) {

				unsigned level_up = findWarpLevel(warp_limit, true);
				assert(level_up != 4294967295);
				unsigned level_down = findWarpLevel(warp_limit, false);
				assert(level_down != 4294967295);
				unsigned max_inactive_cyc = m_config.gpu_warplimiting_sample_freq * m_shader_config->gpgpu_num_sched_per_core * m_shader_config->n_simt_clusters * m_shader_config->n_simt_cores_per_cluster;
				unsigned throttling_threshold = m_config.gpu_throttling_threshold;

				if ( (dramfull_level == HIGH) || (icnt2sh_level == HIGH) ) {
					enable_decrement = true;
					enable_increment = false;
				}
				else if ( (dramfull_level == LOW) && (icnt2sh_level == LOW) ) {
					enable_increment = true;
					enable_decrement = false;
				}

				if( moving_avg.find(warp_limit) != moving_avg.end() ) {
					moving_avg[warp_limit] = moving_avg[warp_limit] * 0.25 + inactive_cyc_window * 0.75;
				}
				else {
					moving_avg[warp_limit] = inactive_cyc_window;
				}

				if( moving_avg.find(level_up) != moving_avg.end() ) {
					if ( moving_avg[warp_limit] > moving_avg[level_up] + throttling_threshold ) {
						enable_increment = true;
						enable_decrement = false;
					}
					if ( moving_avg[level_up] + throttling_threshold >= max_inactive_cyc ) {
						enable_increment = true;
						enable_decrement = false;
					}
//					if ( max_inactive_cyc - moving_avg[warp_limit] < 2048 ) {
//						enable_increment = true;
//						enable_decrement = false;
//					}
				}
				if( moving_avg.find(level_down) != moving_avg.end() ) {
					if ( moving_avg[warp_limit] + throttling_threshold < moving_avg[level_down] ) {
						enable_decrement = false;
					}
				}

//				if(enable_decrement || enable_increment) {
//					iter = 0;
//				}
//
//				if(iter == 3) {
//					enable_increment = true;
//					enable_decrement = false;
//				}
//				iter = (iter + 1) % 4;

				if(enable_increment) {
					for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) {
						m_cluster[i]->set_num_warps(level_up);
					}
				}
				else if (enable_decrement) {
					for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) {
						m_cluster[i]->set_num_warps(level_down);
					}
				}

				if( warp_limit == m_cluster[0]->get_warp_limit() ) {
					iter = (iter + 1) % 5;
				}
				else {
					iter = 0;
				}
				if ( iter == 4 ) {
					if ( warp_limit < 6) {
						for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) {
							m_cluster[i]->set_num_warps(level_up);
						}
					}
					else {
						for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) {
							m_cluster[i]->set_num_warps(level_down);
						}
					}
				}

//			    Print all the generated addresses
				std::ofstream modulation_file;
			    modulation_file.open("modulation_file.txt", std::ofstream::out | std::ofstream::app);
			    modulation_file << warp_limit << ", ";
			    modulation_file << moving_avg[warp_limit] << ", ";

			    if( moving_avg.find(level_down) != moving_avg.end() )
			    	modulation_file << (int) moving_avg[level_down] - (int) moving_avg[warp_limit] << ", ";
			    else
			    	modulation_file << "-1, ";

			    if( moving_avg.find(level_up) != moving_avg.end() )
			    	modulation_file << (int) moving_avg[warp_limit] - (int) moving_avg[level_up] << endl;
			    else
			    	modulation_file << "-1, " << endl;

				modulation_file.close();

//				printf("warp_limit = %u\n", warp_limit);
//				printf("warp_limit_last = %u\n", warp_limit_last);
//				if( moving_avg.find(level_down) != moving_avg.end() )
//					printf("moving_avg[%u] = %u\n", level_down, moving_avg[level_down]);
//				printf("moving_avg[%u] = %u\n", warp_limit, moving_avg[warp_limit]);
//				if( moving_avg.find(level_up) != moving_avg.end() )
//					printf("moving_avg[%u] = %u\n", level_up, moving_avg[level_up]);
//				if(enable_decrement)
//					printf("decrement...\n");
//				else if(enable_increment)
//					printf("increment...\n");

				enable_increment = false;
				enable_decrement = false;
			}

			if (enable_decrement) {
			  // decrement the limit by 2 on each core
			  for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) {
				  m_cluster[i]->dec_num_warps(2, 1); // FIXXX: parameterize the step and min
			  }
			}
			else if (enable_increment) {
			  // increment the limit by 2 on each core
			  for (unsigned i=0;i<m_shader_config->n_simt_clusters;i++) {
				  m_cluster[i]->inc_num_warps(step, m_shader_config->max_warps_per_shader); // FIXXX: parameterize the step
			  }
			}

			stall_dramfull_last = gpu_stall_dramfull;
			stall_icnt2sh_last = gpu_stall_icnt2sh;
			stall_dramfull_avg_past = stall_dramfull_avg_cur;
			stall_icnt2sh_avg_past = stall_icnt2sh_avg_cur;
			stall_avg_past = stall_avg_cur;

			memory_waiting_last = m_shader_stats->memory_waiting;
			memory_waiting_avg_past = memory_waiting_avg_cur;

			warp_limit_avg_past = warp_limit_avg_cur;


			//			  memory_waiting_window_last = memory_waiting_window;
			//			  warp_limit_last = warp_limit;
			// update these stats only if the limit has changed
			if (warp_limit != m_cluster[0]->get_warp_limit()) {
				memory_waiting_window_last = memory_waiting_window;
				warp_limit_last = warp_limit;
				com_to_mem_last = com_to_mem;

			}

		}
		inactive_cyc_last = inactive_cyc;
		inactive_cyc_window_last = inactive_cyc_window;
	}
}

unsigned gpgpu_sim::findWarpLevel(unsigned curr_limit, bool is_up) {
	if(is_up) {
		switch (curr_limit){
			case 1: return 2;
			case 2: return 3;
			case 3: return 4;
			case 4: return 6;
			case 6: return 8;
			case 8: return 16;
			case 16: return 24;
			case 24: return 48;
			case 48: return 48;
			default: return -1;
		}
	}
	else {
		switch (curr_limit){
			case 1: return 1;
			case 2: return 1;
			case 3: return 2;
			case 4: return 3;
			case 6: return 4;
			case 8: return 6;
			case 16: return 8;
			case 24: return 16;
			case 48: return 24;
			default: return -1;
		}
	}
}

