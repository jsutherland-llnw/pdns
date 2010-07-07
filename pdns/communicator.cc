/*
    PowerDNS Versatile Database Driven Nameserver
    Copyright (C) 2002-2009  PowerDNS.COM BV

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as 
    published by the Free Software Foundation; 

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "packetcache.hh"
#include "utility.hh"
#include <errno.h>
#include "communicator.hh"
#include <set>
#include <boost/utility.hpp>
#include "dnsbackend.hh"
#include "ueberbackend.hh"
#include "packethandler.hh"
#include "resolver.hh"
#include "logger.hh"
#include "dns.hh"
#include "arguments.hh"
#include "session.hh"
#include "packetcache.hh"
#include <boost/lexical_cast.hpp>

// #include "namespaces.hh"


void CommunicatorClass::mainloop(void)
{
  try {
#ifndef WIN32
    signal(SIGPIPE,SIG_IGN);
#endif // WIN32
    L<<Logger::Error<<"Master/slave communicator launching"<<endl;
    PacketHandler P;
    d_tickinterval=::arg().asNum("slave-cycle-interval");
    makeNotifySocket();

    int rc;
    time_t next;

    time_t tick;

    for(;;) {
      slaveRefresh(&P);
      masterUpdateCheck(&P);
      tick=doNotifications();
      
      tick = min (tick, d_tickinterval); 

      //      L<<Logger::Error<<"tick = "<<tick<<", d_tickinterval = "<<d_tickinterval<<endl;
      next=time(0)+tick;

      while(time(0) < next) {
        rc=d_any_sem.tryWait();

        if(rc)
          Utility::sleep(1);
        else { 
          if(!d_suck_sem.tryWait()) {
            SuckRequest sr;
            {
              Lock l(&d_lock);
              if(d_suckdomains.empty()) 
                continue;
                
              sr=d_suckdomains.front();
              d_suckdomains.pop_front();
            }
            suck(sr.domain,sr.master);
          }
        }
        // this gets executed at least once every second
        doNotifications();
      }
    }
  }
  catch(AhuException &ae) {
    L<<Logger::Error<<"Communicator thread died because of error: "<<ae.reason<<endl;
    Utility::sleep(1);
    exit(0);
  }
  catch(std::exception &e) {
    L<<Logger::Error<<"Communicator thread died because of STL error: "<<e.what()<<endl;
    exit(0);
  }
  catch( ... )
  {
    L << Logger::Error << "Communicator caught unknown exception." << endl;
    exit( 0 );
  }
}

