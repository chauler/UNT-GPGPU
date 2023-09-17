#ifndef _KNCUBE_HPP_
#define _KNCUBE_HPP_

#include "network.hpp"

class KNCube : public Network {

   bool _mesh;

   int _k;
   int _n;

   void _ComputeSize( const Configuration &config );
   void _BuildNet( const Configuration &config, int seperate_network_ratio, int net_num );

   int _LeftChannel( int node, int dim );
   int _RightChannel( int node, int dim );

   int _LeftNode( int node, int dim );
   int _RightNode( int node, int dim );

public:
   KNCube( const Configuration &config, bool mesh, int seperate_network_ratio, int net_num );

   int GetN( ) const;
   int GetK( ) const;

   double Capacity( ) const;

   void InsertRandomFaults( const Configuration &config );
};

#endif
