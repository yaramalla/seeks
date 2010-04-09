/**
 * This is the p2p messaging component of the Seeks project,
 * a collaborative websearch overlay network.
 *
 * Copyright (C) 2006, 2010  Emmanuel Benazera, juban@free.fr
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Stabilizer.h"
#include "Random.h"
#include <math.h>
#include <iostream>
#include <time.h>
#include <assert.h>

using lsh::Random;

namespace dht
{
   /**
    * For now we're using the same parameters and values as in Chord.
    * We'll change these as needed.
    */
   const int Stabilizer::_decrease_timer = 1000;
   const double Stabilizer::_slowdown_factor = 1.2;

   const int Stabilizer::_fast_timer_init = 1 * 1000;
   const int Stabilizer::_fast_timer_base = 20 * 1000;
   const int Stabilizer::_fast_timer_max = 2 * Stabilizer::_fast_timer_base;

   const int Stabilizer::_slow_timer_init = 1 * 1000;
   const int Stabilizer::_slow_timer_base = 8 * 1000;
   const int Stabilizer::_slow_timer_max = 2 * Stabilizer::_slow_timer_base;

   Stabilizer::Stabilizer()
     : BstTimeCbTree()
       {
	  // TODO: activate when code is ready.
	  start_fast_stabilizer();
	  start_slow_stabilizer();
       }
   
   void Stabilizer::start_fast_stabilizer()
     {
	fast_stabilize(static_cast<double>(_fast_timer_init));
     }
   
   void Stabilizer::start_slow_stabilizer()
     {
	slow_stabilize(static_cast<double>(_slow_timer_init));
     }
   
   int Stabilizer::fast_stabilize(double tround)
     {
	/**
	 * If we've on-going rpcs wrt. stabilization,
	 * we delay the next round by slowing down the calls.
	 */
	if (fast_stabilizing())
	  {
	     tround *= 2.0;
	  }
	else
	  {
	     /**
	      * decide if we decrease or increase the time between the calls.
	      */
	     if (tround >= Stabilizer::_fast_timer_base) tround -= Stabilizer::_decrease_timer;
	     if (tround < Stabilizer::_fast_timer_base) tround *= Stabilizer::_slowdown_factor;
	     
	     /**
	      * run fast stabilization on individual structures.
	      */
	     for (unsigned int i=0; i<_stab_elts_fast.size(); i++)
	       _stab_elts_fast[i]->stabilize_fast();
	  }
	
	if (tround > Stabilizer::_fast_timer_max)
	  tround = static_cast<double>(Stabilizer::_fast_timer_max);
       
	/**
	 * generate a time for the next stabilization round.
	 */
	//no need reseed random generator ?
	double t1 = Random::genUniformDbl32(0.5 * tround, 1.5 * tround);
	int sec = static_cast<int>(ceil(t1));
	int sec2 = static_cast<int>(ceil(sec / 1000));
	int nsec = (sec % 1000) * 1000000;
	
	//debug
	/* std::cout << "[Debug]:Stabilizer::fast_stabilize: sec: " << sec << " -- nsec: " << nsec 
	 << " -- t1: " << t1 << std::endl; */
	//debug
	
	/**
	 * Schedule a later callback of this same function.
	 */
	timespec tsp, tsnow;
	clock_gettime(CLOCK_REALTIME, &tsnow);
	tsp.tv_sec = tsnow.tv_sec + sec2;
	tsp.tv_nsec = tsnow.tv_nsec + nsec;
	if (tsp.tv_nsec >= 1000000000) 
	  {  
	     tsp.tv_nsec -= 1000000000;
	     tsp.tv_sec++;
	  }

	//debug
	/* if (_bstcbtree)
	 std::cout << "tree size before insertion: " << _bstcbtree->size() << std::endl; */
	//debug
	
	callback<int>::ref fscb= wrap(this, &Stabilizer::fast_stabilize, tround);
	insert(tsp, fscb);
	
	//debug
	/* if (_bstcbtree)
	 std::cout << "tree size after insertion: " << _bstcbtree->size() << std::endl; */
	//debug
	
	//debug
	std::cerr << "[Debug]:Stabilizer::fast_stabilize: tround: " << tround << std::endl;
	std::cerr << "[Debug]:Stabilizer::fast_stabilize: scheduling next call around: "
	  << tsp << std::endl;
	//std::cout << tround << std::endl;
	//debug

	return 0;
     }

   int Stabilizer::slow_stabilize(double tround)
     {
	bool stable = isstable_slow();
	
	/**
	 * TODO: if we've on-going rpcs wrt. stabilization,
	 * we delay the next round by slowing down the calls.
	 */
	if (slow_stabilizing())
	  {
	     tround *= 2.0;
	  }
	else
	  {
	     /**
	      * decide over the increase/decrease of the next call.
	      */
	     if (stable && tround <= Stabilizer::_slow_timer_max)
	       tround *= Stabilizer::_slowdown_factor;
	     else if (tround > Stabilizer::_slow_timer_base)
	       tround -= Stabilizer::_decrease_timer;
	     
	     /**
	      * run slow stabilization on individual structures.
	      */
	     for (unsigned int i=0; i<_stab_elts_slow.size(); i++)
	       _stab_elts_slow[i]->stabilize_slow();
	  }
	
	if (tround > Stabilizer::_slow_timer_max)
	  tround = static_cast<double>(Stabilizer::_slow_timer_max);
	
	/**
	 * generate a time for the next call.
	 */
	//no need to reseed random generator.
	double t1 = Random::genUniformDbl32(0.5 * tround, 1.5 * tround);
	int sec = static_cast<int>(ceil(t1));
	int sec2 = static_cast<int>(ceil(sec / 1000));
	int nsec = (sec % 1000) * 1000000;
	
	/**
	 * schedule a later callback of this same function.
	 */
	timespec tsp, tsnow;
	clock_gettime(CLOCK_REALTIME, &tsnow);
	tsp.tv_sec = tsnow.tv_sec + sec2;
	tsp.tv_nsec = tsnow.tv_nsec + nsec;
	if (tsp.tv_nsec >= 1000000000)
	  {
	     tsp.tv_nsec -= 1000000000;
	     tsp.tv_sec++;
	  }

	 //debug
	/* if (_bstcbtree)
	 std::cout << "tree size before insertion (slow): " << _bstcbtree->size() << std::endl; */
	//debug
	
	callback<int>::ref fscb= wrap(this, &Stabilizer::slow_stabilize, tround);
	insert(tsp, fscb);

	//debug
	/* if (_bstcbtree)
	 std::cout << "tree size after insertion (slow): " << _bstcbtree->size() << std::endl; */
	//debug
	
	//debug
	std::cout << "[Debug]:Stabilizer::slow_stabilize: tround: " << tround << std::endl;
	std::cout << "[Debug]:Stabilizer::slow_stabilize: scheduling next call around: "
	  << tsp << std::endl;
	//std::cout << tround << std::endl;
	//debug

	return 0;
     }
   
   bool Stabilizer::fast_stabilizing() const
     {
	for (unsigned int i=0; i<_stab_elts_fast.size(); i++)
	  if (_stab_elts_fast[i]->isStabilizingFast())
	    return true;
	return false;
     }
   
   bool Stabilizer::slow_stabilizing() const
     {
	for (unsigned int i=0; i<_stab_elts_slow.size(); i++)
	  if (_stab_elts_slow[i]->isStabilizingSlow())
	    return true;
	return false;
     }
   
   
   bool Stabilizer::isstable_slow() const
     {
	for (unsigned int i=0; i<_stab_elts_slow.size(); i++)
	  if (!_stab_elts_slow[i]->isStable())
	    return false;
	return true;
     }
   
     
   
} /* end of namespace. */