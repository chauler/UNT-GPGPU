#!/usr/bin/perl -w 

use English;

@benchmarks = ("AES", "BFS", "BLK", "CP", "JPEG", "KM", "LIB", "MUM", "NN", "NQU", "RAY", "SCP", "STO", "WP", "BP", "HOT", 
"NW", "LUD", "LKC", "PFF", "PFN", "SD1", "SD2", "PATH", "MYO", "LAV", "GSS", "BTR",
"HIST", "MM", "SAD", "SPMV",
"IIX", "PVC", "PVR", "SSC",
"BFS-S", "DMEM", "MFLP", "MD", "QTC", "RED", "SCAN", "SPMV-S", "ST2D", "TRD");

@file_list = ("CUDA/AES/output_AES.txt", "CUDA/BFS/output_BFS.txt", "CUDA/BlackScholes/output_BlackScholes.txt", "CUDA/CP/output_CP.txt",  
"CUDA/JPEG/output_JPEG.txt", "CUDA/kmeans/output_kmeans.txt", "CUDA/LIB/output_LIB.txt", "CUDA/MUM/output_MUM.txt", 
"CUDA/NN/output_NN.txt", "CUDA/NQU/output_NQU.txt", "CUDA/RAY/output_RAY.txt", "CUDA/SCP/output_SCP.txt", "CUDA/STO/output_STO.txt", 
"CUDA/WP/output_WP.txt", "rodinia/backprop/output_backprop.txt", 
"rodinia/hotspot/output_hotspot.txt", "rodinia/nw/output_nw.txt", 
"rodinia/lud/output_lud.txt", "rodinia/leukocyte/output_leukocyte.txt", "rodinia/pf_float/output_pf_float.txt", 
"rodinia/pf_naive/output_pf_naive.txt", "rodinia/srad_v1/output_srad_v1.txt", "rodinia/srad_v2/output_srad_v2.txt",
"rodinia/pathfinder/output_pathfinder.txt", "rodinia/myocyte/output_myocyte.txt", "rodinia/lavaMD/output_lavaMD.txt",
"rodinia/gaussian/output_gaussian.txt", "rodinia/b+tree/output_b+tree.txt",
"parboil/histo/output_histo.txt", "parboil/mm/output_mm.txt", "parboil/sad/output_sad.txt", 
"parboil/spmv/output_spmv.txt",
"Mars/InvertedIndex/output_IIX.txt", "Mars/PageViewCount/output_PVC.txt", "Mars/PageViewRank/output_PVR.txt", "Mars/SimilarityScore/output_SSC.txt",
"shoc/BFS/output_BFS.txt", "shoc/DeviceMemory/output_DeviceMemory.txt", "shoc/MaxFlops/output_MaxFlops.txt", "shoc/MD/output_MD.txt",
"shoc/QTC/output_QTC.txt", "shoc/Reduction/output_Reduction.txt", "shoc/Scan/output_Scan.txt", "shoc/Spmv/output_Spmv.txt",
"shoc/Stencil2D/output_Stencil2D.txt", "shoc/Triad/output_Triad.txt");



$result_file = "all_results.txt";

open(OUTPUT, ">$result_file") || die "Cannot open file $result_file for writing\n";

for ($j = 0; $j <= $#file_list; $j++) {  
	
	my $ipc = `grep gpu_tot_ipc $file_list[$j] | tail -n 1 | awk '\{print \$3\}'`;
	chomp($ipc);
	push @globalipc, $ipc;
		
	my $ins = `grep gpu_tot_sim_insn $file_list[$j] | tail -n 1 | awk '\{print \$3\}'`;
	chomp($ins);
	push @globalins, $ins;	
	
	my $cycle = `grep gpu_tot_sim_cycle $file_list[$j] | tail -n 1 | awk '\{print \$3\}'`;
	chomp($cycle);
	push @globalcycles, $cycle;
	
	my $averagemflatency = `grep averagemflatency $file_list[$j] | tail -n 1 | awk '\{print \$3\}'`;
	chomp($averagemflatency);
	push @globalaveragemflatency, $averagemflatency;
	
	my $L1_data_miss_rate = `grep total_dl1_miss_rate $file_list[$j] | tail -n 1 | awk '\{print \$2\}'`;
	chomp($L1_data_miss_rate);
	push @global_dl1_miss_rate, $L1_data_miss_rate;
	
	my $L2_miss_rate = `grep L2 $file_list[$j] | tail -n 1 | awk '\{print \$7\}'`;
	chomp($L2_miss_rate);
	push @global_l2_miss_rate, $L2_miss_rate;	
	
	my $i_cache_hits = `grep gpgpu_n_icache_hits $file_list[$j] | tail -n 1 | awk '\{print \$3\}'`;
	chomp($i_cache_hits);
	push @global_i_cache_hits, $i_cache_hits;
	
	my $i_cache_misses = `grep gpgpu_n_icache_misses $file_list[$j] | tail -n 1 | awk '\{print \$3\}'`;
	chomp($i_cache_misses);
	push @global_i_cache_misses, $i_cache_misses;
	
	my $gpu_stall_dramfull = `grep gpu_stall_dramfull $file_list[$j] | tail -n 1 | awk '\{print \$3\}'`;
	chomp($gpu_stall_dramfull);
	push @global_gpu_stall_dramfull, $gpu_stall_dramfull;
	
	my $gpu_stall_icnt2sh = `grep gpu_stall_icnt2sh $file_list[$j] | tail -n 1 | awk '\{print \$3\}'`;
	chomp($gpu_stall_icnt2sh);
	push @global_gpu_stall_icnt2sh, $gpu_stall_icnt2sh;
		
	my $Active = `grep "Active" $file_list[$j] | tail -n 1 |  awk -F ':' '\{print \$2\}' |  awk '\{print \$1\}'`;
	chomp($Active);
	push @globalActive, $Active;
	
	my $Inactive = `grep "Active" $file_list[$j] | tail -n 1 |  awk -F ':' '\{print \$3\}' |  awk '\{print \$1\}'`;
	chomp($Inactive);
	push @globalInactive, $Inactive;
	
	my $RACT = `grep "Active" $file_list[$j] | tail -n 1 |  awk -F ':' '\{print \$4\}' |  awk '\{print \$1\}'`;
	chomp($RACT);
	push @globalRACT, $RACT;
	
	my $lat_G_M = `grep -F 'Traffic[0]class0Overall average latency' $file_list[$j] | tail -n 1 |  awk -F '=' '\{print \$2\}' |  awk '\{print \$1\}'`;
	chomp($lat_G_M);
	push @globallat_G_M, $lat_G_M;
	
	my $lat_M_G = `grep -F 'Traffic[1]class0Overall average latency' $file_list[$j] | tail -n 1 |  awk -F '=' '\{print \$2\}' |  awk '\{print \$1\}'`;
	chomp($lat_M_G);
	push @globallat_M_G, $lat_M_G;
	
	my $lat_C_M = `grep -F 'Traffic[2]class0Overall average latency' $file_list[$j] | tail -n 1 |  awk -F '=' '\{print \$2\}' |  awk '\{print \$1\}'`;
	chomp($lat_C_M);
	push @globallat_C_M, $lat_C_M;
	
	my $lat_M_C = `grep -F 'Traffic[3]class0Overall average latency' $file_list[$j] | tail -n 1 |  awk -F '=' '\{print \$2\}' |  awk '\{print \$1\}'`;
	chomp($lat_M_C);
	push @globallat_M_C, $lat_M_C;
	
 } 

	print OUTPUT "Benchmarks \t";
	print OUTPUT "IPC\t";
	print OUTPUT "Insn\t";
	print OUTPUT "Cycle\t";
	print OUTPUT "averagemflatency\t";
	print OUTPUT "DL1 Miss Rate \t";
	print OUTPUT "L2 Miss Rate \t";
	print OUTPUT "i1 hits \t";
	print OUTPUT "i1 misses \t";
	print OUTPUT "DRAM-full \t";
	print OUTPUT "icnt2sh \t";
	print OUTPUT "Active \t";
	print OUTPUT "Inactive \t";
	print OUTPUT "RACT \t";
	print OUTPUT "lat_G_M \t";	
	print OUTPUT "lat_M_G \t";	
	print OUTPUT "lat_C_M \t";	
	print OUTPUT "lat_M_C \t";	
	
	print OUTPUT "\n";


for ($k = 0;$k <= $#file_list;$k++) 
{
	print OUTPUT "$benchmarks[$k] \t";
		
	print OUTPUT "$globalipc[$k] \t";
	print OUTPUT "$globalins[$k] \t";	
	print OUTPUT "$globalcycles[$k] \t";
	print OUTPUT "$globalaveragemflatency[$k] \t";
	print OUTPUT "$global_dl1_miss_rate[$k] \t";
	print OUTPUT "$global_l2_miss_rate[$k] \t";	
	print OUTPUT "$global_i_cache_hits[$k] \t";
	print OUTPUT "$global_i_cache_misses[$k] \t";
	print OUTPUT "$global_gpu_stall_dramfull[$k] \t";
	print OUTPUT "$global_gpu_stall_icnt2sh[$k] \t";	
	print OUTPUT "$globalActive[$k] \t";
	print OUTPUT "$globalInactive[$k] \t";
	print OUTPUT "$globalRACT[$k] \t";
	print OUTPUT "$globallat_G_M[$k] \t";
	print OUTPUT "$globallat_M_G[$k] \t";
	print OUTPUT "$globallat_C_M[$k] \t";
	print OUTPUT "$globallat_M_C[$k] \t";
			
	print OUTPUT "\n";
}


sub avg {
	@_ == 1 or die ('Sub usage: $avg = avg(\@array);');
	my ($array_ref) = @_;
	my $sum;
	my $count = scalar @$array_ref;
        if ($count == 0) {
                return 0;
        }
	foreach (@$array_ref) { $sum += $_; }
	return $sum / $count;
}

sub max {
	@_ == 1 or die ('Sub usage: $max = max(\@array);');
	my ($array_ref) = @_;
	my $maxval = -999999999999999999;
	foreach (@$array_ref)
	{
		if ( $maxval < $_ )
		{
			$maxval = $_;
		}
	}
	return $maxval;
}

sub sum {
	@_ == 1 or die ('Sub usage: $sum = sum(\@array);');
	my ($array_ref) = @_;
	my $sum;
	foreach (@$array_ref) { $sum += $_; }
	return $sum;
}




