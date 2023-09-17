#include "booksim.hpp"

#include <map>
#include <stdlib.h>
#include <assert.h>

#include "routefunc.hpp"
#include "kncube.hpp"
#include "random_utils.hpp"

//Energy
#include "../cpu-sim/globals.h"

map<string, tRoutingFunction> gRoutingFunctionMap;

/* Global information used by routing functions */

int gNumVCS;

int gNumVCS_CPU;
int gNumVCS_GPU;

/* Add routing functions here */

//=============================================================

int dor_next_mesh( int cur, int dest, int FlitsSta )
{


   //printf("************f->src:%d, f->dest:%d, ", cur, dest);
   int dim_left;
   int out_port;

   //Energy
   if (coRun) {
      p_routing += 0.087;
   }

   //UNT Modified
   if ( (FlitsSta == 0) || (FlitsSta == 3) ) {
      for ( dim_left = 0; dim_left < gN; ++dim_left ) {
         if ( ( cur % gK ) != ( dest % gK ) ) {
            break;
         }
         cur /= gK; dest /= gK;
      }

      if ( dim_left < gN ) {
         cur %= gK; dest %= gK;

         if ( cur < dest ) {
            out_port = 2*dim_left;     // Right
         } else {
            out_port = 2*dim_left + 1; // Left
         }
      } else {
         out_port = 2*gN;  // Eject
      }
   } else if ( ( FlitsSta == 2 ) || (FlitsSta == 1) ) {
      if ( cur == dest) {
         dim_left = gN;
      } else if ( ( cur / gK ) != ( dest / gK ) ) {
         cur /= gK; dest /= gK;
         dim_left = 1;
      } else {
         dim_left = 0;
      }

      if ( dim_left < gN ) {
         cur %= gK; dest %= gK;

         if ( cur < dest ) {
            out_port = 2*dim_left;     // Right
         } else {
            out_port = 2*dim_left + 1; // Left
         }
      } else {
         out_port = 2*gN;  // Eject
      }
   }

   //printf("out_port:%d**********\n", out_port);

   return out_port;
}

//=============================================================

void dim_order_mesh( const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject )
{
   int out_port;

   outputs->Clear( );

   if(f->net_num / 2) { // if CPU
	   gNumVCS = gNumVCS_CPU;
   }
   else { // if GPU
	   gNumVCS = gNumVCS_GPU;
   }

   if ( inject ) { // use any VC for injection
      outputs->AddRange( 0, 0, gNumVCS - 1 );
   } else {
      out_port = dor_next_mesh( r->GetID( ), f->dest, f->net_s );

      if ( f->watch ) {
         cout << "flit " << f->id << " (" << f << ") at " << r->GetID( ) << " destined to " 
         << f->dest << " using channel " << out_port << ", vc range = [" 
         << 0 << "," << gNumVCS - 1 << "] (in_channel is " << in_channel << ")" << endl;
      }

      outputs->AddRange( out_port, 0, gNumVCS - 1 );
   }
}

//=============================================================

void InitializeRoutingMap( )
{
   /* Register routing functions here */
   gRoutingFunctionMap["dim_order_mesh"]  = &dim_order_mesh;
}

tRoutingFunction GetRoutingFunction( const Configuration& config, int seperate_network_ratio, int net_num   )
{

   map<string, tRoutingFunction>::const_iterator match;
   tRoutingFunction rf;

   string fn, topo, fn_topo;

   gNumVCS = config.GetInt( "num_vcs" );

   // Onur added
   if(seperate_network_ratio) {
      if(net_num / 2) { // if CPU network
         gNumVCS_CPU = (gNumVCS * seperate_network_ratio) / 4;
      } else { // if GPU network
         gNumVCS_GPU = gNumVCS - (gNumVCS * seperate_network_ratio) / 4;
      }
   } else {
      gNumVCS_CPU = gNumVCS;
      gNumVCS_GPU = gNumVCS;
   }
   cout << "vcs = " << gNumVCS << "; CPU = " << gNumVCS_CPU << "; GPU = " << gNumVCS_GPU << endl;

   config.GetStr( "topology", topo );

   config.GetStr( "routing_function", fn, "none" );
   fn_topo = fn + "_" + topo;
   match = gRoutingFunctionMap.find( fn_topo );

   if ( match != gRoutingFunctionMap.end( ) ) {
      rf = match->second;
   } else {
      if ( fn == "none" ) {
         cout << "Error: No routing function specified in configuration." << endl;
      } else {
         cout << "Error: Undefined routing function '" << fn << "' for the topology '" 
         << topo << "'." << endl;
      }
      exit(-1);
   }

   return rf;
}
