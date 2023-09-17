#ifndef _SINGLENET_HPP_
#define _SINGLENET_HPP_

#include "network.hpp"

class SingleNet : public Network {

   void _ComputeSize( const Configuration &config );
   void _BuildNet( const Configuration &config, int seperate_network_ratio, int net_num );

public:
   SingleNet( const Configuration &config, int seperate_network_ratio, int net_num );

   void Display( ) const;
};

#endif
