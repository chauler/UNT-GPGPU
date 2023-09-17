#include "booksim.hpp"
#include <iostream>
#include <stdlib.h>
#include <assert.h>

#include "buffer_state.hpp"
#include "random_utils.hpp"

//UNT added
#include "../cpu-sim/globals.h"

void BufferState::init( const Configuration& config, int seperate_network_ratio, int net_num )
{
   _Init( config, seperate_network_ratio, net_num );
}

BufferState::BufferState( const Configuration& config, 
                          Module *parent, const string& name, int seperate_network_ratio, int uid ) :
Module( parent, name )
{
   _Init( config, seperate_network_ratio, uid );
}

void BufferState::_Init( const Configuration& config, int seperate_network_ratio, int uid )
{
   _buf_size     = config.GetInt( "vc_buf_size" );
   _vcs          = config.GetInt( "num_vcs" );

   // Onur added
   if(seperate_network_ratio) {
	   if(uid / 2) { // if CPU network
		   _vcs = (_vcs * seperate_network_ratio) / 4;
	   }
	   else { // if GPU network
		   _vcs = _vcs - (_vcs * seperate_network_ratio) / 4;
	   }
   }

   _wait_for_tail_credit = config.GetInt( "wait_for_tail_credit" );

   _in_use       = new bool [_vcs];
   _tail_sent    = new bool [_vcs];
   _cur_occupied = new int  [_vcs];

   _last_avail   = 0;

   for ( int v = 0; v < _vcs; ++v ) {
      _in_use[v]       = false;
      _tail_sent[v]    = false;
      _cur_occupied[v] = 0;
   }
}

BufferState::~BufferState( )
{
   delete [] _in_use;
   delete [] _tail_sent;
   delete [] _cur_occupied;
}

void BufferState::ProcessCredit( Credit *c )
{
   assert( c );

   for ( int v = 0; v < c->vc_cnt; ++v ) {
      assert( ( c->vc[v] >= 0 ) && ( c->vc[v] < _vcs ) );

      if ( ( _wait_for_tail_credit ) && 
           ( !_in_use[c->vc[v]] ) ) {
         Error( "Received credit for idle buffer" );
      }

      if ( _cur_occupied[c->vc[v]] > 0 ) {
         --_cur_occupied[c->vc[v]];

         if ( ( _cur_occupied[c->vc[v]] == 0 ) && 
              ( _tail_sent[c->vc[v]] ) ) {
            _in_use[c->vc[v]] = false;
         }
      } else {
         cout << "VC = " << c->vc[v] << endl;
         Error( "Buffer occupancy fell below zero" );
      }
   }
}

void BufferState::SendingFlit( Flit *f )
{
   assert( f && ( f->vc >= 0 ) && ( f->vc < _vcs ) );

   if ( _cur_occupied[f->vc] < _buf_size ) {
      ++_cur_occupied[f->vc];

      if ( f->tail ) {
         _tail_sent[f->vc] = true;

         if ( !_wait_for_tail_credit ) {
            _in_use[f->vc] = false;
         }
      }
   } else {
      Error( "Flit sent to full buffer" );
   }
}

void BufferState::TakeBuffer( int vc )
{
   assert( ( vc >= 0 ) && ( vc < _vcs ) );

   if ( _in_use[vc] ) {
      Error( "Buffer taken while in use" );
   }

   _in_use[vc]    = true;
   _tail_sent[vc] = false;
}

bool BufferState::IsFullFor( int vc  ) const
{
   assert( ( vc >= 0 ) && ( vc < _vcs ) );
   return( _cur_occupied[vc] == _buf_size ) ? true : false;
}

bool BufferState::IsAvailableFor( int vc ) const
{
	// FIXXX debug
//	cout << "vc = " << vc << " ; _vcs = " << _vcs << endl;
   assert( ( vc >= 0 ) && ( vc < _vcs ) );
   return !_in_use[vc];
}

int BufferState::FindAvailable( Flit *f )
{
   int available_vc = -1;
   int vc;

   _last_avail = RandomInt( _vcs - 1 );

   //UNT added
   //Let the CPU and GPU Flits inject from different VC
   if ( f->net_s / 2 ) {
      for ( int v = 0; v < _cpu_vcs; ++v ) {
         vc = ( v + _last_avail + 1 ) % _cpu_vcs; // Round-robin

         if ( IsAvailableFor( vc ) ) {
            available_vc = vc;
            _last_avail  = vc;
            break;
         }
      }
   } else {
      for ( int v = _cpu_vcs; v < _vcs; ++v ) {
         vc = ( v + _last_avail + 1 ) % (_vcs - _cpu_vcs) + _cpu_vcs; // Round-robin

         if ( IsAvailableFor( vc ) ) {
            available_vc = vc;
            _last_avail  = vc;
            break;
         }
      }
   }

   return available_vc;
}

void BufferState::Display( ) const
{
   cout << _fullname << " :" << endl;
   for ( int v = 0; v < _vcs; ++v ) {
      cout << "  buffer class " << v << endl;
      cout << "    in_use = " << _in_use[v] << " tail_sent = " << _tail_sent[v] << endl;
      cout << "    occupied = " << _cur_occupied[v] << endl;
   }
}
