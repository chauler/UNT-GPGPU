#include <string>
#include <sstream>
#include <iostream>
#include <stdlib.h>
#include <assert.h>

#include "iq_router.hpp"

//Energy
#include "../cpu-sim/globals.h"

IQRouter::IQRouter( const Configuration& config,
                    Module *parent, string name, int id,
                    int inputs, int outputs, int seperate_network_ratio, int net_num  )
: Router( config,
          parent, name,
          id,
          inputs, outputs )
{
   string alloc_type;
   ostringstream vc_name;

   _vcs         = config.GetInt( "num_vcs" );
   _vc_size     = config.GetInt( "vc_buf_size" );

   // Onur added
   if(seperate_network_ratio) {
	   if(net_num / 2) { // if CPU network
		   _vcs = (_vcs * seperate_network_ratio) / 4;
	   }
	   else { // if GPU network
		   _vcs = _vcs - (_vcs * seperate_network_ratio) / 4;
	   }
   }

   _iq_time = 0;

   _output_extra_latency = config.GetInt( "output_extra_latency" );

   // Routing

   _rf = GetRoutingFunction( config, seperate_network_ratio, net_num  );

   // Alloc VC's

   _vc = new VC * [_inputs];

   for ( int i = 0; i < _inputs; ++i ) {
      _vc[i] = new VC [_vcs];
      for( int j=0; j < _vcs; j++ )
         _vc[i][j].init( config, _outputs );

      for ( int v = 0; v < _vcs; ++v ) { // Name the vc modules
         vc_name << "vc_i" << i << "_v" << v;
         _vc[i][v].SetName( this, vc_name.str( ) );
         vc_name.seekp( 0, ios::beg );
      }
   }

   // Alloc next VCs' buffer state

   _next_vcs = new BufferState [_outputs];
   for( int j=0; j < _outputs; j++ ) {
      _next_vcs[j].init( config, seperate_network_ratio, net_num );
   }

   for ( int o = 0; o < _outputs; ++o ) {
      vc_name << "next_vc_o" << o;
      _next_vcs[o].SetName( this, vc_name.str( ) );
      vc_name.seekp( 0, ios::beg );
   }

   // Alloc allocators

   config.GetStr( "vc_allocator", alloc_type );
   _vc_allocator = Allocator::NewAllocator( config, 
                                            this, "vc_allocator",
                                            alloc_type, 
                                            _vcs*_inputs, 1,
                                            _vcs*_outputs, 1 );

   if ( !_vc_allocator ) {
      cout << "ERROR: Unknown vc_allocator type " << alloc_type << endl;
      exit(-1);
   }

   config.GetStr( "sw_allocator", alloc_type );
   _sw_allocator = Allocator::NewAllocator( config,
                                            this, "sw_allocator",
                                            alloc_type, 
                                            _inputs*_input_speedup, _input_speedup, 
                                            _outputs*_output_speedup, _output_speedup );

   if ( !_sw_allocator ) {
      cout << "ERROR: Unknown sw_allocator type " << alloc_type << endl;
      exit(-1);
   }

   _sw_rr_offset = new int [_inputs*_input_speedup];
   for ( int i = 0; i < _inputs*_input_speedup; ++i ) {
      _sw_rr_offset[i] = 0;
   }

   // Alloc pipelines (to simulate processing/transmission delays)

   _crossbar_pipe = 
   new PipelineFIFO<Flit>( this, "crossbar_pipeline", _outputs*_output_speedup, 
                           _st_prepare_delay + _st_final_delay );

   _credit_pipe =
   new PipelineFIFO<Credit>( this, "credit_pipeline", _inputs,
                             _credit_delay );

   // Input and output queues

   _input_buffer  = new queue<Flit *> [_inputs]; 
   _output_buffer = new queue<pair<Flit *, int> > [_outputs]; 

   _in_cred_buffer  = new queue<Credit *> [_inputs]; 
   _out_cred_buffer = new queue<Credit *> [_outputs];

   // Switch configuration (when held for multiple cycles)

   _hold_switch_for_packet = config.GetInt( "hold_switch_for_packet" );
   _switch_hold_in  = new int [_inputs*_input_speedup];
   _switch_hold_out = new int [_outputs*_output_speedup];
   _switch_hold_vc  = new int [_inputs*_input_speedup];

   for ( int i = 0; i < _inputs*_input_speedup; ++i ) {
      _switch_hold_in[i] = -1;
      _switch_hold_vc[i] = -1;
   }

   for ( int i = 0; i < _outputs*_output_speedup; ++i ) {
      _switch_hold_out[i] = -1;
   }
}

IQRouter::~IQRouter( )
{
   for ( int i = 0; i < _inputs; ++i ) {
      delete [] _vc[i];
   }

   delete [] _vc;
   delete [] _next_vcs;

   delete _vc_allocator;
   delete _sw_allocator;

   delete [] _sw_rr_offset;

   delete _crossbar_pipe;
   delete _credit_pipe;

   delete [] _input_buffer;
   delete [] _output_buffer;

   delete [] _in_cred_buffer;
   delete [] _out_cred_buffer;

   delete [] _switch_hold_in;
   delete [] _switch_hold_vc;
   delete [] _switch_hold_out;
}

void IQRouter::ReadInputs( )
{
   _ReceiveFlits( );
   _ReceiveCredits( );
}

void IQRouter::InternalStep( )
{
   _InputQueuing( );
   _Route( );
   _VCAlloc( );
   _SWAlloc( );

   for ( int input = 0; input < _inputs; ++input ) {
      for ( int vc = 0; vc < _vcs; ++vc ) {
         _vc[input][vc].AdvanceTime( );
      }
   }

   _crossbar_pipe->Advance( );
   _credit_pipe->Advance( );
   ++_iq_time;

   _OutputQueuing( );

}

#include "interconnect_interface.h"
void IQRouter::WriteOutputs( )
{

   _SendFlits( );
   _SendCredits( );

}

void IQRouter::_ReceiveFlits( )
{
   Flit *f;

   for ( int input = 0; input < _inputs; ++input ) {
      f = *((*_input_channels)[input]);

      if ( f ) {

         //Energy
         if (coRun) {
            p_buffer += 6.8167;
         }

         _input_buffer[input].push( f );
      }
   }
}

void IQRouter::_ReceiveCredits( )
{
   Credit *c;

   for ( int output = 0; output < _outputs; ++output ) {
      c = *((*_output_credits)[output]);

      if ( c ) {
         _out_cred_buffer[output].push( c );
      }
   }
}

void IQRouter::_InputQueuing( )
{
   Flit   *f;
   Credit *c;
   VC     *cur_vc;

   for ( int input = 0; input < _inputs; ++input ) {
      if ( !_input_buffer[input].empty( ) ) {
         f = _input_buffer[input].front( );
         _input_buffer[input].pop( );

         cur_vc = &_vc[input][f->vc];

         if ( !cur_vc->AddFlit( f ) ) {
            Error( "VC buffer overflow" );
         }

         if ( f->watch ) {
            cout << "Received flit at " << _fullname << endl;
            cout << *f;
         }
      }
   }

   for ( int input = 0; input < _inputs; ++input ) {
      for ( int vc = 0; vc < _vcs; ++vc ) {

         cur_vc = &_vc[input][vc];

         if ( cur_vc->GetState( ) == VC::idle ) {
            f = cur_vc->FrontFlit( );

            if ( f ) {
               if ( !f->head ) {
                  Error( "Received non-head flit at idle VC" );
               }

               cur_vc->Route( _rf, this, f, input );
               cur_vc->SetState( VC::routing );
            }
         }
      }
   }  

   for ( int output = 0; output < _outputs; ++output ) {
      if ( !_out_cred_buffer[output].empty( ) ) {
         c = _out_cred_buffer[output].front( );
         _out_cred_buffer[output].pop( );

         _next_vcs[output].ProcessCredit( c );
         delete c;
      }
   }
}

void IQRouter::_Route( )
{
   VC *cur_vc;

   for ( int input = 0; input < _inputs; ++input ) {
      for ( int vc = 0; vc < _vcs; ++vc ) {

         cur_vc = &_vc[input][vc];

         if ( ( cur_vc->GetState( ) == VC::routing ) &&
              ( cur_vc->GetStateTime( ) >= _routing_delay ) ) {

            cur_vc->SetState( VC::vc_alloc );
         }
      }
   }
}

void IQRouter::_AddVCRequests( VC* cur_vc, int input_index, int _nets, bool watch )
{
   const OutputSet *route_set;
   BufferState *dest_vc;
   int vc_cnt, out_vc;
   int in_priority, out_priority;

   //UNT added
   //Virtual channel partitioned

   int _vc_s, _vc_e;
   int node = this->GetID();
   if ( coRun && (node == 0 || node == 5 || node == 30 || node == 35 ) ) {
      if ( _nets / 2 ) {
         _vc_s = 0; _vc_e = 1;
      } else {
         _vc_s = 3; _vc_e = 0;
      }
   } else if ( coRun && (node == 6 || node == 11 || node == 24 || node == 29) ) {
      if ( _nets / 2 ) {
          _vc_s = 0; _vc_e = 3;
      } else {
         _vc_s = 1; _vc_e = 0;
      }
   } else {
      _vc_s = 0; _vc_e = 0;
   }

   route_set    = cur_vc->GetRouteSet( );
   out_priority = cur_vc->GetPriority( );

   for ( int output = 0; output < _outputs; ++output ) {
      vc_cnt = route_set->NumVCs( output );
      dest_vc = &_next_vcs[output];

      //for ( int vc_index = 0; vc_index < vc_cnt; ++vc_index ) {
      for ( int vc_index = _vc_s; vc_index < (vc_cnt - _vc_e); ++vc_index ) {
         out_vc = route_set->GetVC( output, vc_index, &in_priority );

         if ( watch ) {
            cout << "  trying vc " << out_vc << " (out = " << output << ") ... ";
         }

         // On the input input side, a VC might request several output 
         // VCs.  These VCs can be prioritized by the routing function
         // and this is reflected in "in_priority".  On the output,
         // if multiple VCs are requesting the same output VC, the priority
         // of VCs is based on the actual packet priorities, which is
         // reflected in "out_priority".

         if ( dest_vc->IsAvailableFor( out_vc ) ) {
            _vc_allocator->AddRequest( input_index, output*_vcs + out_vc, 1, 
                                       in_priority, out_priority );
            if ( watch ) {
               cout << "available" << endl;
            }
         } else if ( watch ) {
            cout << "busy" << endl;
         }
      }
   }
}

void IQRouter::_VCAlloc( )
{
   VC          *cur_vc;
   BufferState *dest_vc;
   int         input_and_vc;
   int         match_input;
   int         match_vc;

   Flit        *f;
   bool        watched;

   //Energy
   if (coRun) {
      p_vc_arb += 0.3005;
   }

   _vc_allocator->Clear( );
   watched = false;

   for ( int input = 0; input < _inputs; ++input ) {
      for ( int vc = 0; vc < _vcs; ++vc ) {

         cur_vc = &_vc[input][vc];

         if ( ( cur_vc->GetState( ) == VC::vc_alloc ) &&
              ( cur_vc->GetStateTime( ) >= _vc_alloc_delay ) ) {

            f = cur_vc->FrontFlit( );
            if ( f->watch ) {
               cout << "VC requesting allocation at " << _fullname << endl;
               cout << "  input_index = " << input*_vcs + vc << endl;
               cout << *f;
               watched = true;
            }

            _AddVCRequests( cur_vc, input*_vcs + vc, f->net_s, f->watch );
         }
      }
   }

   _vc_allocator->Allocate( );

   // Winning flits get a VC

   for ( int output = 0; output < _outputs; ++output ) {
      for ( int vc = 0; vc < _vcs; ++vc ) {
         input_and_vc = _vc_allocator->InputAssigned( output*_vcs + vc );

         if ( input_and_vc != -1 ) {
            match_input = input_and_vc / _vcs;
            match_vc    = input_and_vc - match_input*_vcs;

            cur_vc  = &_vc[match_input][match_vc];
            dest_vc = &_next_vcs[output];

            cur_vc->SetState( VC::active );
            cur_vc->SetOutput( output, vc );
            dest_vc->TakeBuffer( vc );

            f = cur_vc->FrontFlit( );

            if ( f->watch ) {
               cout << "Granted VC allocation at " << _fullname 
               << " (input index " << input_and_vc << " )" << endl;
               cout << *f;
            }
         }
      }
   }
}

void IQRouter::_SWAlloc( )
{
   Flit        *f;
   Credit      *c;

   VC          *cur_vc;
   BufferState *dest_vc;

   int input;
   int output;
   int vc;

   int expanded_input;
   int expanded_output;

   //Energy
   if (coRun) {
      p_xbar += 14.8967;
      p_sw_arb += 0.442;
   }

   _sw_allocator->Clear( );

   for ( input = 0; input < _inputs; ++input ) {
      for ( int s = 0; s < _input_speedup; ++s ) {
         expanded_input  = s*_inputs + input;

         // Arbitrate (round-robin) between multiple 
         // requesting VCs at the same input (handles 
         // the case when multiple VC's are requesting
         // the same output port)
         vc = _sw_rr_offset[ expanded_input ];

         for ( int v = 0; v < _vcs; ++v ) {

            // This continue acounts for the interleaving of 
            // VCs when input speedup is used
            if ( ( vc % _input_speedup ) != s ) {
               vc = ( vc + 1 ) % _vcs;
               continue;
            }

            cur_vc = &_vc[input][vc];

            if ( ( cur_vc->GetState( ) == VC::active ) && 
                 ( !cur_vc->Empty( ) ) ) {

               dest_vc = &_next_vcs[cur_vc->GetOutputPort( )];

               if ( !dest_vc->IsFullFor( cur_vc->GetOutputVC( ) ) ) {

                  // When input_speedup > 1, the virtual channel buffers
                  // are interleaved to create multiple input ports to
                  // the switch.  Similarily, the output ports are
                  // interleaved based on their originating input when
                  // output_speedup > 1.

                  assert( expanded_input == (vc%_input_speedup)*_inputs + input );
                  expanded_output = (input%_output_speedup)*_outputs + cur_vc->GetOutputPort( );

                  if ( ( _switch_hold_in[expanded_input] == -1 ) && 
                       ( _switch_hold_out[expanded_output] == -1 ) ) {

                     // We could have requested this same input-output pair in a previous
                     // iteration, only replace the previous request if the current
                     // request has a higher priority (this is default behavior of the
                     // allocators).  Switch allocation priorities are strictly 
                     // determined by the packet priorities.

                     _sw_allocator->AddRequest( expanded_input, expanded_output, vc, 
                                                cur_vc->GetPriority( ), 
                                                cur_vc->GetPriority( ) );
                  }
               }
            }

            vc = ( vc + 1 ) % _vcs;
         }
      }
   }

   _sw_allocator->Allocate( );

   // Winning flits cross the switch

   _crossbar_pipe->WriteAll( 0 );

   for ( int input = 0; input < _inputs; ++input ) {
      c = 0;

      for ( int s = 0; s < _input_speedup; ++s ) {

         expanded_input  = s*_inputs + input;

         if ( _switch_hold_in[expanded_input] != -1 ) {
            expanded_output = _switch_hold_in[expanded_input];
            vc = _switch_hold_vc[expanded_input];
            cur_vc = &_vc[input][vc];

            if ( cur_vc->Empty( ) ) { // Cancel held match if VC is empty
               expanded_output = -1;
            }
         } else {
            expanded_output = _sw_allocator->OutputAssigned( expanded_input );
         }

         if ( expanded_output >= 0 ) {
            output = expanded_output % _outputs;

            if ( _switch_hold_in[expanded_input] == -1 ) {
               vc = _sw_allocator->ReadRequest( expanded_input, expanded_output );
               cur_vc = &_vc[input][vc];
            }

            if ( _hold_switch_for_packet ) {
               _switch_hold_in[expanded_input] = expanded_output;
               _switch_hold_vc[expanded_input] = vc;
               _switch_hold_out[expanded_output] = expanded_input;
            }

            assert( ( cur_vc->GetState( ) == VC::active ) && 
                    ( !cur_vc->Empty( ) ) && 
                    ( cur_vc->GetOutputPort( ) == ( expanded_output % _outputs ) ) );

            dest_vc = &_next_vcs[cur_vc->GetOutputPort( )];

            assert( !dest_vc->IsFullFor( cur_vc->GetOutputVC( ) ) );

            // Forward flit to crossbar and send credit back
            f = cur_vc->RemoveFlit( );

            f->hops++;

            if ( f->watch ) {
               cout << "Forwarding flit through crossbar at " << _fullname << ":" << endl;
               cout << *f;
            }

            if ( !c ) {
               c = _NewCredit( _vcs );
            }

            c->vc[c->vc_cnt] = f->vc;
            c->vc_cnt++;

            f->vc = cur_vc->GetOutputVC( );
            dest_vc->SendingFlit( f );

            _crossbar_pipe->Write( f, expanded_output );

            if ( f->tail ) {
               cur_vc->SetState( VC::idle );

               _switch_hold_in[expanded_input] = -1;
               _switch_hold_vc[expanded_input] = -1;
               _switch_hold_out[expanded_output] = -1;
            }

            _sw_rr_offset[expanded_input] = ( f->vc + 1 ) % _vcs;
         }
      }

      _credit_pipe->Write( c, input );
   }
}

void IQRouter::_OutputQueuing( )
{
   Flit   *f;
   Credit *c;
   int expanded_output;

   //Energy
   if (coRun) {
      p_link += 27.1488;
   }

   for ( int output = 0; output < _outputs; ++output ) {
      for ( int t = 0; t < _output_speedup; ++t ) {
         expanded_output = _outputs*t + output;
         f = _crossbar_pipe->Read( expanded_output );

         if ( f ) {

            _output_buffer[output].push( make_pair(f,_iq_time) );
         }
      }
   }  

   for ( int input = 0; input < _inputs; ++input ) {
      c = _credit_pipe->Read( input );

      if ( c ) {
         _in_cred_buffer[input].push( c );
      }
   }
}

void IQRouter::_SendFlits( )
{
   Flit *f;

   for ( int output = 0; output < _outputs; ++output ) {

      f = NULL;

      if ( !_output_buffer[output].empty( ) ) {
         if ((_iq_time - _output_buffer[output].front().second) >= _output_extra_latency) {
            f = _output_buffer[output].front( ).first;
            _output_buffer[output].pop( );
         }
      }

      *(*_output_channels)[output] = f;
   }
}

void IQRouter::_SendCredits( )
{
   Credit *c;

   for ( int input = 0; input < _inputs; ++input ) {
      if ( !_in_cred_buffer[input].empty( ) ) {
         c = _in_cred_buffer[input].front( );
         _in_cred_buffer[input].pop( );
      } else {
         c = 0;
      }

      *(*_input_credits)[input] = c;
   }
}

void IQRouter::Display( ) const
{
   for ( int input = 0; input < _inputs; ++input ) {
      for ( int v = 0; v < _vcs; ++v ) {
         _vc[input][v].Display( );
      }
   }
}
