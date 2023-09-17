#include "booksim.hpp"
#include <string>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <queue>

#include "routefunc.hpp"
#include "traffic.hpp"
#include "booksim_config.hpp"
#include "trafficmanager.hpp"
#include "random_utils.hpp"
#include "network.hpp"
#include "singlenet.hpp"
#include "kncube.hpp"
#include "fly.hpp"
#include "injection.hpp"
#include "interconnect_interface.h"
#include "../gpgpu-sim/mem_fetch.h"
#include <string.h>
#include <math.h>

#include "iq_router.hpp"

int _flit_size ;
int _seperate_network_ratio;

bool doub_net = false; //double networks disabled by default
// Onur added
bool quad_net = false; //quad networks disabled by default

BookSimConfig icnt_config; 
TrafficManager** traffic;
unsigned int g_num_vcs; //number of virtual channels
queue<Flit *> ** ejection_buf; 
vector<int> round_robin_turn; //keep track of boundary_buf last used in icnt_pop
unsigned int ejection_buffer_capacity ;  
unsigned int boundary_buf_capacity ;  

unsigned int input_buffer_capacity ;

class boundary_buf{
private:
   queue<void *> buf;
   queue<bool> tail_flag;
   int packet_n;
   unsigned capacity;
public:

   // Onur added
   int prev_flit_netnum;

   boundary_buf(){
      capacity = boundary_buf_capacity; //maximum flits the buffer can hold
      packet_n=0;
      prev_flit_netnum = -1;
   }
   bool is_full(void){
      return (buf.size()>=capacity);
   }
   bool is_empty(void){
      return (buf.size()==0);
   }
   bool has_packet() {
      return (packet_n);
   }
   //Onur added
   unsigned int get_capacity() {
	   return (capacity);
   }
   //Onur added
   unsigned int get_size() {
	   return (buf.size());
   }

   void * pop_packet(){
      assert (packet_n);
      void * data = NULL;
      void * temp_d = buf.front();

      while (data==NULL) {

         if (tail_flag.front()) {
            data = buf.front();
            packet_n--;
         }

         if (temp_d != buf.front()) {
        	 printf("Assert is coming!\n");
        	 printf(" temp_d : %s ", (char *)temp_d);
        	 printf(" buf_front : %s ", (char *) buf.front());
        	 printf(" packet_n : %d ", packet_n);
        	 printf(" data : %s \n\n", (char *)data);

        	  mem_fetch *mf = (mem_fetch*) data;
        	  mf->print(stdout);

        	  mem_fetch *mf1 = (mem_fetch*) temp_d;
        	  mf1->print(stdout);
         }

         assert(temp_d == buf.front()); //all flits must belong to the same packet
         buf.pop();
         tail_flag.pop();
      }
      return data; 
   }
   void * top_packet(){
      assert (packet_n);
      void * data = NULL;
      void * temp_d = buf.front();
      while (data==NULL) {
         if (tail_flag.front()) {
            data = buf.front();
         }
         assert(temp_d == buf.front()); //all flits must belong to the same packet
      }
      return data; 
   }
   bool last_flit_tail(){
	   return tail_flag.back();
   }
   void push_flit_data(void* data,bool is_tail) {
      buf.push(data);
      tail_flag.push(is_tail);

      if (is_tail) {
         packet_n++;
      }
   }  
};

boundary_buf** clock_boundary_buf; 

class mycomparison {
public:
   bool operator() (const void* lhs, const void* rhs) const
   {
      return( ((mem_fetch *)lhs)->get_icnt_receive_time() > ((mem_fetch *) rhs)->get_icnt_receive_time());
   }
};

bool perfect_icnt = 0;
int fixed_lat_icnt = 0;

priority_queue<void * , vector<void* >, mycomparison> * out_buf_fixedlat_buf; 


//perfect icnt stats:
unsigned int* max_fixedlat_buf_size;

static unsigned int net_c; //number of interconnection networks

static unsigned int _n_shader = 0;
static unsigned int _n_mem = 0;

// Onur added
static unsigned int _n_cpu = 0;

static int * node_map;  //deviceID to mesh location map
                        //deviceID : Starts from 0 for shaders and then continues until mem nodes 
                        // which starts at location n_shader and then continues to n_shader+n_mem (last device)   
static int * reverse_map; 

void map_gen(int dim,int  memcount, int memnodes[])
{
   int k = 0;
   int i=0 ;
   int j=0 ;
   int memfound=0;
   for (i = 0; i < dim*dim ; i++) {
      memfound=0;
      for (j = 0; j<memcount ; j++) {
         if (memnodes[j]==i) {
            memfound=1;
         }
      }   
      if (!memfound) {
         node_map[k]=i;
         k++;
      }
   }
   for (int j = 0; j<memcount ; j++) {
      node_map[k]=memnodes[j];
      k++;
   }
   assert(k==dim*dim);
}

void display_map(int dim,int count)
{
    printf("GPGPU-Sim uArch: ");
   int i=0;
   for (i=0;i<count;i++) {
      printf("%3d ",node_map[i]);
      if (i%dim ==0) 
         printf("\nGPGPU-Sim uArch: ");
   }
}

void display_reverse_map(int dim,int count)
{
    printf("GPGPU-Sim uArch: ");
   int i=0;
   for (i=0;i<count;i++) {
      printf("%3d ",reverse_map[i]);
      if (i%dim ==0)
         printf("\nGPGPU-Sim uArch: ");
   }
}

void create_node_map(int n_shader, int n_mem, int n_cpu, int size, int use_map)
{
   node_map = (int*)malloc((size)*sizeof(int));   
   if (use_map) {
      switch (size) {
      case 4 :
         {     int newmap[]  = {
               2, 3, // MC
               0, 	// GPU
               1   // CPU
            };
            memcpy (node_map, newmap,4*sizeof(int));
            break;
         }
      case 16 :
         { // good for 8 shaders and 8 memory cores
            int newmap[]  = {  
               0, 2, 5, 7,
               8,10,13,15,
               1, 3, 4, 6, //memory nodes
               9,11,12,14   //memory nodes
            }; 
            memcpy (node_map, newmap,16*sizeof(int));
            break;
         }

      case 36 :
         { // for CPU+GPU mesh topology 8 MC, 14 CPU, 14*2 GPU
            int newmap[]  = {
            		1, 3, 8, 10, 12, 14, 16, 19, 21, 23, 25, 27, 32, 34, // GPU
            		2, 4, 7, 9, 13, 15, 17, 18, 20, 22, 26, 28, 31, 33, // CPU
            		0, 5, 6, 11, 24, 29, 30, 35 // MC
            };



            memcpy (node_map, newmap,36*sizeof(int));
            break;
         }
      case 64:
         { // good for 56 shaders and 8 memory cores
            int newmap[]  = {  
               0,  1,  2,  4,  5,  6,  7,  8,
               9, 10, 11, 12, 13, 14, 16, 18,
               19, 20, 21, 22, 23, 24, 25, 26,
               27, 28, 30, 31, 32, 33, 34, 35,
               37, 38, 39, 40, 41, 42, 43, 44,                    
               45, 46, 48, 50, 51, 52, 53, 54,
               55, 56, 57, 58, 59, 60, 62, 63, 
               3, 15, 17, 29, 36, 47, 49, 61  //memory nodes are in this line
            }; 
            memcpy (node_map, newmap,64*sizeof(int));
            break;
         }
      case 121:
         { // good for 110 shaders and 11 memory cores
            int newmap[]  = {  
               0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
               11, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23,
               24, 26, 27, 29, 30, 31, 32, 33, 34, 35, 36,
               37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
               48, 49, 50, 51, 52, 53, 54, 55, 56, 58, 59,
               61, 62, 64, 65, 66, 67, 68, 69, 70, 71, 72,
               73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83,
               84, 85, 86, 87, 88, 89, 90, 91, 93, 94, 96,
               97, 98, 99,101,102,103,104,105,106,107,109,
               110,111,112,113,114,115,116,117,118,119,120,  
               12, 20, 25, 28, 57, 60, 63, 92, 95,100,108 //memory nodes are in this line
            }; 
            memcpy (node_map, newmap,121*sizeof(int));
            break;
         }
//      case 36:
//         {
//            int memnodes[8]={3,7,10,12,23,25,28,32};
//            map_gen(6/*dim*/,8/*memcount*/,memnodes);
//            break;
//         }
      default: 
         {
            cout<<"WARNING !!! NO MAPPING IMPLEMENTED YET FOR THIS CONFIG"<<endl;
            for (int i=0;i<size;i++) {
               node_map[i]=i;
            }
         }
      }
   } else { // !use_map
      for (int i=0;i<size;i++) {
         node_map[i]=i;
      }
   }
   reverse_map = (int*)malloc((size)*sizeof(int));   
   for (int i = 0; i < size ; i++) {
      for (int j = 0; j<size ; j++) {
         if (node_map[j]==i) {
            reverse_map[i]=j;
            break;
         }
      }
   }
   printf("GPGPU-Sim uArch: interconnect nodemap\n");
   display_map((int) sqrt(size),size);
   display_reverse_map((int) sqrt(size),size);

}

int fixed_latency(int input, int output)
{
   int latency;
   if (perfect_icnt) {
      latency = 1; 
   } else {
      int dim = icnt_config.GetInt( "k" ); 
      int xhops = abs ( input%dim - output%dim);
      int yhops = abs ( input/dim - output/dim);
      latency = ( (xhops+yhops)*fixed_lat_icnt ); 
   }
   return latency;
}

//This function gets a mapped node number and tells if it is a shader node or a memory node
static inline bool is_shd(int node)
{
   if (reverse_map[node] < (int) _n_shader)
      return true;
   else
      return false;
}

// Onur modified
static inline bool is_mem(int node)
{
   if (reverse_map[node] < (int) _n_shader + (int) _n_cpu)
	  return false;
   else
	  return true;
}

// Onur modified
static inline bool is_cpu(int node)
{
   if (!is_shd(node) && !is_mem(node))
	  return true;
   else
	  return false;
}

////////////////////
void interconnect_stats()
{
   if (!fixed_lat_icnt) {
      for (unsigned i=0; i<net_c;i++) {
         cout <<"Traffic "<<i<< " Stat" << endl;
         traffic[i]->ShowStats();
         if (icnt_config.GetInt("enable_link_stats")) {
            cout << "%=================================" << endl;
            cout <<"Traffic "<<i<< "Link utilizations:" << endl;
            traffic[i]->_net->Display();
         }
      }
   } else {
      //show max queue sizes
      cout<<"Max Fixed Latency ICNT queue size for"<<endl;
      // Onur modified: _n_cpu
      for (unsigned i=0;i<(_n_mem + _n_shader + _n_cpu);i++) {
         cout<<" node[" << i <<"] is "<<max_fixedlat_buf_size[i];
      }
      cout<<endl;
   }
}

void icnt_overal_stat() //should be called upon simulation exit to give an overall stat
{
   if (!fixed_lat_icnt) {
      for (unsigned i=0; i<net_c;i++) {
         traffic[i]->ShowOveralStat();
      }
   }
}

void icnt_init_grid (){
   for (unsigned i=0; i<net_c;i++) {
      traffic[i]->IcntInitPerGrid(0/*_time*/); //initialization before gpu grid start
   }
}

bool interconnect_has_buffer(unsigned int input_node, unsigned int output_node, unsigned int tot_req_size)
{
   unsigned int input = node_map[input_node];
   unsigned int output = node_map[output_node];
   bool has_buffer = false;
   unsigned int n_flits = tot_req_size / _flit_size + ((tot_req_size % _flit_size)? 1:0);
//   cout << "n_flits = " << n_flits << " tot_req_size = " << tot_req_size << " flit size = " << _flit_size << endl;
   if (!(fixed_lat_icnt || perfect_icnt)) {
	  if (net_c <= 2)
		 has_buffer = (traffic[0]->_partial_packets[input][0].size() + n_flits) <=  input_buffer_capacity;
	  if ((net_c == 2) && is_mem(input))
         has_buffer = (traffic[1]->_partial_packets[input][0].size() + n_flits) <=  input_buffer_capacity;
      else if (net_c == 4) {
    	  int _flit_size_cpu = (_flit_size * _seperate_network_ratio) / 4;
    	  int _flit_size_gpu = _flit_size - _flit_size_cpu;
//    	  cout << "cpu flit sizze = " << _flit_size_cpu << " gpu flit size= " << _flit_size_gpu << endl;
    	  unsigned int n_flits_gpu = tot_req_size / _flit_size_gpu + ((tot_req_size % _flit_size_gpu)? 1:0);
    	  unsigned int n_flits_cpu = tot_req_size / _flit_size_cpu + ((tot_req_size % _flit_size_cpu)? 1:0);
//    	  cout << "n_flits cpu = " << n_flits_cpu << " n_flits_gpu = " << n_flits_gpu << endl;
    	  if (is_shd(input))
    		 has_buffer = (traffic[0]->_partial_packets[input][0].size() + n_flits_gpu) <=  input_buffer_capacity;
    	  else if (is_cpu(input))
    		 has_buffer = (traffic[2]->_partial_packets[input][0].size() + n_flits_cpu) <=  input_buffer_capacity;
    	  else if (is_mem(input) && is_shd(output))
    		 has_buffer = (traffic[1]->_partial_packets[input][0].size() + n_flits_gpu) <=  input_buffer_capacity;
    	  else if (is_mem(input) && is_cpu(output))
    		 has_buffer = (traffic[3]->_partial_packets[input][0].size() + n_flits_cpu) <=  input_buffer_capacity;
    	  else {
    		  printf("Impossible!\n");
    		  abort();
    	  }
      }
   } else {
      has_buffer = true; 
   }
   return has_buffer;
}

extern unsigned long long  gpu_sim_cycle;
extern unsigned long long  gpu_tot_sim_cycle;

void interconnect_push ( unsigned int input_node, unsigned int output_node, 
                         void* data, unsigned int size) 
{ 

   int input = node_map[input_node];
   int output;
   if ( is_shd(input) ) {
      if( (input % 6) < 3 && (input / 6) < 3 ) {
         output = 6;
      } else if ((input % 6) > 2 && (input / 6) < 3) {
         output = 11;
      } else if ((input % 6) < 3 && (input / 6) > 2) {
         output = 24;
      } else if ((input % 6) > 2 && (input / 6) > 2) {
         output = 29;
      }
   } else if ( is_cpu(input) ) {
      if( (input % 6) < 3 && (input / 6) < 3 ) {
         output = 0;
      } else if ((input % 6) > 2 && (input / 6) < 3) {
         output = 5;
      } else if ((input % 6) < 3 && (input / 6) > 2) {
         output = 30;
      } else if ((input % 6) > 2 && (input / 6) > 2) {
         output = 35;
      }
   } else {

      output = node_map[output_node];
   }

   if (fixed_lat_icnt) {
      ((mem_fetch *) data)->set_icnt_receive_time( gpu_sim_cycle + fixed_latency(input,output) );  
      out_buf_fixedlat_buf[output].push(data); //deliver the whole packet to destination in zero cycles
      if (out_buf_fixedlat_buf[output].size()  > max_fixedlat_buf_size[output]) {
         max_fixedlat_buf_size[output]= out_buf_fixedlat_buf[output].size();
      }
   } else {

      int nc;

      unsigned int n_flits = size / _flit_size + ((size % _flit_size)? 1:0);

      // Onur modified a lot
      if (!doub_net && !quad_net) {
         nc=0;
         traffic[nc]->_GeneratePacket( input, n_flits, 0 /*class*/, traffic[nc]->_time, data, output, 0);
      } else if (doub_net) { // doub_net enabled
         if (!is_mem(input) ) {
        	 nc=0;
         } else {
        	 nc=1;
         }

         //UNT added
         int _nets;
         if ( is_shd(input) ) {
            _nets = 0;
         } else if ( is_cpu(input) ) {
            _nets = 2;
         } else if ( is_mem(input) && is_shd(output) ) {
            _nets = 1;
         } else if ( is_mem(input) && is_cpu(output) ) {
            _nets = 3;
         }

         traffic[nc]->_GeneratePacket( input, n_flits, 0 /*class*/, traffic[nc]->_time, data, output, _nets);
      }
      else if (quad_net) { // quad net enabled
    	  int _flit_size_cpu = (_flit_size * _seperate_network_ratio) / 4;
    	  int _flit_size_gpu = _flit_size - _flit_size_cpu;
    	  unsigned int n_flits_gpu = size / _flit_size_gpu + ((size % _flit_size_gpu)? 1:0);
    	  unsigned int n_flits_cpu = size / _flit_size_cpu + ((size % _flit_size_cpu)? 1:0);
          if (is_shd(input) ) {
         	nc=0;
         	traffic[nc]->_GeneratePacket( input, n_flits_gpu, 0 /*class*/, traffic[nc]->_time, data, output, nc);
//         	printf("Generate packet GPU to mem: from %d to %d; %d flits\n", input, output, n_flits_gpu);
          } else if (is_cpu(input)) {
         	nc=2;
         	traffic[nc]->_GeneratePacket( input, n_flits_cpu, 0 /*class*/, traffic[nc]->_time, data, output, nc);
//         	printf("Generate packet CPU to mem: from %d to %d; %d flits\n", input, output, n_flits_cpu);
          } else if (is_mem(input) && is_shd(output)) {
        	nc=1;
        	traffic[nc]->_GeneratePacket( input, n_flits_gpu, 0 /*class*/, traffic[nc]->_time, data, output, nc);
//        	 printf("Generate packet mem to GPU: from %d to %d; %d flits\n", input, output, n_flits_gpu);
          } else if (is_mem(input) && is_cpu(output)) {
        	nc=3;
        	traffic[nc]->_GeneratePacket( input, n_flits_cpu, 0 /*class*/, traffic[nc]->_time, data, output, nc);
//        	 printf("Generate packet mem to CPU: from %d to %d; %d flits\n", input, output, n_flits_cpu);
          } else {
        	 printf("Don't know into which network to inject\n");
        	 abort();
          }

      }
      else {
    	  printf("Don't know how many networks there are\n");
    	  abort();
      }

//      traffic[nc]->_GeneratePacket( input, n_flits, 0 /*class*/, traffic[nc]->_time, data, output);
//

#if DOUB
      cout <<"Traffic[" << nc << "] (mapped) sending form "<< input << " to " << output <<endl;
#endif
   }

}

void* interconnect_pop(unsigned int output_node) 
{ 
   int output = node_map[output_node];

#if DEBUG
   cout<<"Call interconnect POP  " << output<<endl;
#endif
   void* data = NULL;
   if (fixed_lat_icnt) {
      if (!out_buf_fixedlat_buf[output].empty()) {
         if (((mem_fetch *)out_buf_fixedlat_buf[output].top())->get_icnt_receive_time() <= gpu_sim_cycle) {
            data = out_buf_fixedlat_buf[output].top();
            out_buf_fixedlat_buf[output].pop();
            assert (((mem_fetch *)data)->get_icnt_receive_time());
         }
      }
   } else {
      unsigned vc;
      unsigned turn = round_robin_turn[output];

      // Onur added
      unsigned g_num_ejection_bufs = quad_net ? 2*g_num_vcs: g_num_vcs;

      // Onur modified
      for (vc=0;(vc<g_num_ejection_bufs) && (data==NULL);vc++) {
         if (clock_boundary_buf[output][turn].has_packet()) {
        	 data = clock_boundary_buf[output][turn].pop_packet();
         }
         turn++;
         if (turn == g_num_ejection_bufs) turn = 0;
      }
      if (data) {
         round_robin_turn[output] = turn;
      }
   }
   return data; 
}

extern int MATLAB_OUTPUT        ; 
extern int DISPLAY_LAT_DIST     ; 
extern int DISPLAY_HOP_DIST     ; 
extern int DISPLAY_PAIR_LATENCY ; 

// Onur modified: n_cpu
void init_interconnect (char* config_file,
                        unsigned int n_shader, 
                        unsigned int n_mem,
                        unsigned int n_cpu)
{
   _n_shader = n_shader;
   _n_mem = n_mem;

   // Onur added
   _n_cpu = n_cpu;

   if (! config_file ) {
      cout << "Interconnect Requires a configfile" << endl;
      exit (-1);
   }
   icnt_config.Parse( config_file );

   net_c = icnt_config.GetInt( "network_count" );
   _seperate_network_ratio = icnt_config.GetInt( "seperate_network_ratio" );
   if (net_c==2) {
      doub_net = true;    
   } else if (net_c==4) {
	   quad_net = true;
   } else if (net_c<1 || net_c==3 || net_c>4) {
      cout <<net_c<<" Network_count less than 1 or 3 or more than 4 not supported."<<endl;
      abort();
   }

   g_num_vcs = icnt_config.GetInt( "num_vcs" );

   InitializeRoutingMap( );
   InitializeTrafficMap( );
   InitializeInjectionMap( );

   RandomSeed( icnt_config.GetInt("seed") );

   Network **net;

   traffic = new TrafficManager *[net_c];
   net = new Network *[net_c];

   // Onur added
   for (unsigned i=0;i<net_c;i++) {
	  string topo;

	  icnt_config.GetStr( "topology", topo );

	  if(!quad_net) { // do not change anything
		  if ( topo == "torus" ) {
			 net[i] = new KNCube( icnt_config, true, 0, i );
		  } else if (   topo =="mesh"  ) {
			 net[i] = new KNCube( icnt_config, false, 0, i );
		  } else if ( topo == "fly" ) {
			 net[i] = new KNFly( icnt_config, 0, i );
		  } else if ( topo == "single" ) {
			 net[i] = new SingleNet( icnt_config, 0, i );
		  } else {
			 cerr << "Unknown topology " << topo << endl;
			 exit(-1);
		  }

		  if ( icnt_config.GetInt( "link_failures" ) ) {
			 net[i]->InsertRandomFaults( icnt_config );
		  }

		  traffic[i] = new TrafficManager ( icnt_config, net[i], i/*id*/, 0 );
	  }
	  else { // if partitioning resources, create the network and traffic manager differently
		  if(i/2) { // if CPU network
			  if ( topo == "torus" ) {
				 net[i] = new KNCube( icnt_config, true, _seperate_network_ratio, i );
			  } else if (   topo =="mesh"  ) {
				 net[i] = new KNCube( icnt_config, false, _seperate_network_ratio, i );
			  } else if ( topo == "fly" ) {
				 net[i] = new KNFly( icnt_config, _seperate_network_ratio, i );
			  } else if ( topo == "single" ) {
				 net[i] = new SingleNet( icnt_config, _seperate_network_ratio, i );
			  } else {
				 cerr << "Unknown topology " << topo << endl;
				 exit(-1);
			  }

			  if ( icnt_config.GetInt( "link_failures" ) ) {
				 net[i]->InsertRandomFaults( icnt_config );
			  }

			  traffic[i] = new TrafficManager ( icnt_config, net[i], i/*id*/, _seperate_network_ratio );
		  }
		  else { // if GPU network
			  if ( topo == "torus" ) {
				 net[i] = new KNCube( icnt_config, true, _seperate_network_ratio, i );
			  } else if (   topo =="mesh"  ) {
				 net[i] = new KNCube( icnt_config, false, _seperate_network_ratio, i );
			  } else if ( topo == "fly" ) {
				 net[i] = new KNFly( icnt_config, _seperate_network_ratio, i );
			  } else if ( topo == "single" ) {
				 net[i] = new SingleNet( icnt_config, _seperate_network_ratio, i );
			  } else {
				 cerr << "Unknown topology " << topo << endl;
				 exit(-1);
			  }

			  if ( icnt_config.GetInt( "link_failures" ) ) {
				 net[i]->InsertRandomFaults( icnt_config );
			  }

			  traffic[i] = new TrafficManager ( icnt_config, net[i], i/*id*/, _seperate_network_ratio );
		  }
	  }
   }

   // Onur added, partition buffer in terms of VC depth
//   if(quad_net) {
//	   int VC_size = icnt_config.GetInt( "vc_buf_size" );
//	   int VC_size_;
//	   _seperate_network_ratio = icnt_config.GetInt( "seperate_network_ratio" );
//	   for(unsigned i=0; i<net_c; ++i) {
//		   IQRouter** a = (IQRouter**) net[i]->GetRouters();
//		   if(i/2) // it is CPU traffic
//			   VC_size_ = (_seperate_network_ratio * VC_size) / 4;
//		   else
//			   VC_size_ = VC_size - (_seperate_network_ratio * VC_size) / 4;
//		   for(unsigned j=0; j<traffic[0]->_dests; ++j) {
//			   a[j]->SetVCSize(VC_size_);
//		   }
//	   }
//   }

   fixed_lat_icnt = icnt_config.GetInt( "fixed_lat_per_hop" );

   if (icnt_config.GetInt( "perfect_icnt" )) {
      perfect_icnt = true;
      fixed_lat_icnt = 1;  
   }
   _flit_size = icnt_config.GetInt( "flit_size" );
   if (icnt_config.GetInt("ejection_buf_size")) {
      ejection_buffer_capacity = icnt_config.GetInt( "ejection_buf_size" ) ;
   } else {
      ejection_buffer_capacity = icnt_config.GetInt( "vc_buf_size" );
   }
   boundary_buf_capacity = icnt_config.GetInt( "boundary_buf_size" ) ;

   if (icnt_config.GetInt("input_buf_size")) {
      input_buffer_capacity = icnt_config.GetInt("input_buf_size");
   } else {
      input_buffer_capacity = 9;
   }

   // FIXXX edit vcs
   create_buf(traffic[0]->_dests,input_buffer_capacity,icnt_config.GetInt( "num_vcs" )); 
   MATLAB_OUTPUT        = icnt_config.GetInt("MATLAB_OUTPUT");
   DISPLAY_LAT_DIST     = icnt_config.GetInt("DISPLAY_LAT_DIST");
   DISPLAY_HOP_DIST     = icnt_config.GetInt("DISPLAY_HOP_DIST");
   DISPLAY_PAIR_LATENCY = icnt_config.GetInt("DISPLAY_PAIR_LATENCY");
   create_node_map(n_shader,n_mem,n_cpu,traffic[0]->_dests, icnt_config.GetInt("use_map"));
   for (unsigned i=0;i<net_c;i++) {
      traffic[i]->_FirstStep();
   }
}

void advance_interconnect () 
{
   if (!fixed_lat_icnt) {
      for (unsigned i=0;i<net_c;i++) {
         traffic[i]->_Step( );
      }
   }
}

unsigned interconnect_busy()
{
   unsigned i,j;
   for(i=0; i<net_c;i++) {
      if (traffic[i]->_measured_in_flight) {
         return 1;
      }
   }
   for ( i=0 ;i<(_n_shader + _n_mem + _n_cpu);i++ ) {
		if ( !traffic[0]->_partial_packets[i] [0].empty() ) {
			return 1;
		}
		if ( doub_net && !traffic[1]->_partial_packets[i] [0].empty() ) {
			return 1;
		}

		// Onur added
		if ( quad_net && ( !traffic[1]->_partial_packets[i] [0].empty() ||
				!traffic[2]->_partial_packets[i] [0].empty() ||
				!traffic[3]->_partial_packets[i] [0].empty()) ) {
			return 1;
		}

	     // Onur added
	     unsigned g_num_ejection_bufs = quad_net ? 2*g_num_vcs: g_num_vcs;

		for ( j=0;j<g_num_ejection_bufs;j++ ) {
			if ( !ejection_buf[i][j].empty() || clock_boundary_buf[i][j].has_packet() ) {
				return 1;
			}
		}
	}
	return 0;
}

void display_icnt_state( FILE *fp )
{
   fprintf(fp,"GPGPU-Sim uArch: interconnect busy state\n");
   for (unsigned i=0; i<net_c;i++) {
      if (traffic[i]->_measured_in_flight) 
         fprintf(fp,"   Network %u has %u _measured_in_flight\n", i, traffic[i]->_measured_in_flight );
   }
   
   for (unsigned i=0 ;i<(_n_shader + _n_mem + _n_cpu);i++ ) {
      if( !traffic[0]->_partial_packets[i] [0].empty() ) 
         fprintf(fp,"   Network 0 has nonempty _partial_packets[%u][0]\n", i);
		if ( doub_net && !traffic[1]->_partial_packets[i] [0].empty() ) 
         fprintf(fp,"   Network 1 has nonempty _partial_packets[%u][0]\n", i);

		// Onur added
		if ( quad_net && !traffic[1]->_partial_packets[i] [0].empty() )
         fprintf(fp,"   Network 1 has nonempty _partial_packets[%u][0]\n", i);
		if ( quad_net && !traffic[2]->_partial_packets[i] [0].empty() )
         fprintf(fp,"   Network 2 has nonempty _partial_packets[%u][0]\n", i);
		if ( quad_net && !traffic[3]->_partial_packets[i] [0].empty() )
         fprintf(fp,"   Network 3 has nonempty _partial_packets[%u][0]\n", i);

	     // Onur added
	     unsigned g_num_ejection_bufs = quad_net ? 2*g_num_vcs: g_num_vcs;

	     for (unsigned j=0;j<g_num_ejection_bufs;j++ ) {
			if( !ejection_buf[i][j].empty() )
            fprintf(fp,"   ejection_buf[%u][%u] is non-empty\n", i, j);
         if( clock_boundary_buf[i][j].has_packet() )
            fprintf(fp,"   clock_boundary_buf[%u][%u] has packet\n", i, j );
		}
	}
}

//create buffers for src_n nodes   
void create_buf(int src_n,int warp_n,int vc_n)
{
   int i;
   ejection_buf   = new queue<Flit *>* [src_n];
   clock_boundary_buf = new boundary_buf* [src_n];

   round_robin_turn.resize( src_n );
   for (i=0;i<src_n;i++){
	   //Onur added  (but actually nachi added it :-)
       if(quad_net) {
  	   	 ejection_buf[i]= new queue<Flit *>[2*vc_n];
         clock_boundary_buf[i]= new boundary_buf[2*vc_n];
         round_robin_turn[vc_n-1];
       }
       else {
	   	 ejection_buf[i]= new queue<Flit *>[vc_n];
         clock_boundary_buf[i]= new boundary_buf[vc_n];
         round_robin_turn[vc_n-1];
       }
   } 
   if (fixed_lat_icnt) {
      out_buf_fixedlat_buf  = new priority_queue<void * , vector<void* >, mycomparison> [src_n];  
      max_fixedlat_buf_size    = new unsigned int [src_n];
      for (i=0;i<src_n;i++) {
         max_fixedlat_buf_size[i]=0;
      }
   }

}

void write_out_buf(int output, Flit *flit) {
	int vc = flit->vc;
	assert (ejection_buf[output][vc].size() < ejection_buffer_capacity);
	//FIXXX
	//Onur added
	//if((ejection_buf[output][vc].back()->net_num == flit->net_num) && (quad_net))
	//{
	if(quad_net) {
		if(flit->net_num == 0)  //net_num 0 = GPU to mem
			ejection_buf[output][2*vc].push(flit);
		else if(flit->net_num == 2)  //net_num 2 = CPU to mem
			ejection_buf[output][(2*vc)+1].push(flit);
		else if(flit->net_num == 1)  //net_num 1 = mem to GPU
			ejection_buf[output][2*vc].push(flit);
		else if(flit->net_num == 3)  //net_num 3 = mem to CPU
			ejection_buf[output][(2*vc)+1].push(flit);
		else {
			printf("Should not happen!\n");
			abort();
		}
	}
	else {
		ejection_buf[output][vc].push(flit);
	}

	//}

}

void transfer2boundary_buf(int output) {
	Flit* flit;
   unsigned vc;

   if(quad_net) {
	   for (vc=0; vc<2*g_num_vcs;vc++) {
		  if ( !ejection_buf[output][vc].empty() && !clock_boundary_buf[output][vc].is_full() ) {
			 flit = ejection_buf[output][vc].front();
			 ejection_buf[output][vc].pop();
			 clock_boundary_buf[output][vc].push_flit_data( flit->data, flit->tail);
			 traffic[flit->net_num]->credit_return_queue[output].push(flit); //will send back credit
			 if ( flit->head ) {
				assert (flit->dest == output);
			 }
			#if DOUB
					 cout <<"Traffic " <<nc<<" push out flit to (mapped)" << output <<endl;
			#endif
		  }
	   }
   }
   else {  // 1-network or 2-networks
	   for (vc=0; vc<g_num_vcs;vc++) {
		  if ( !ejection_buf[output][vc].empty() && !clock_boundary_buf[output][vc].is_full() ) {
			 flit = ejection_buf[output][vc].front();
			 ejection_buf[output][vc].pop();
			 clock_boundary_buf[output][vc].push_flit_data( flit->data, flit->tail);
			 traffic[flit->net_num]->credit_return_queue[output].push(flit); //will send back credit
			 if ( flit->head ) {
				assert (flit->dest == output);
			 }
			#if DOUB
					 cout <<"Traffic " <<nc<<" push out flit to (mapped)" << output <<endl;
			#endif
		  }
	   }
   }
}

void time_vector_update(unsigned int uid, int slot , long int cycle, int type);

void time_vector_update_icnt_injected(void* data, int input) 
{
    /*
    mem_fetch* mf = (mem_fetch*) data;
    if( mf->get_mshr() && !mf->get_mshr()->isinst() ) {
        unsigned uid=mf->get_request_uid();
        long int cycle = gpu_sim_cycle + gpu_tot_sim_cycle;
        int req_type = mf->get_is_write()? WT_REQ : RD_REQ;
        if (is_mem(input)) {
           time_vector_update( uid, MR_2SH_ICNT_INJECTED, cycle, req_type );      
        } else { 
           time_vector_update( uid, MR_ICNT_INJECTED, cycle,req_type );
        }
    }
    */
}

/// Returns size of flit
unsigned interconnect_get_flit_size(){
	return _flit_size;
}
