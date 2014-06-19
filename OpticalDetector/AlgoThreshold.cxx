////////////////////////////////////////////////////////////////////////
//
//  AlgoThreshold source
//
////////////////////////////////////////////////////////////////////////

#ifndef ALGOFIXEDWINDOW_CC
#define ALGOFIXEDWINDOW_CC

#include "AlgoThreshold.h"

namespace pmtana{

  //############################
  AlgoThreshold::AlgoThreshold()
  //############################
  {

    _adc_thres = 3;
  
    _nsigma = 5;

    Reset();

  }

  //***************************************************************
  AlgoThreshold::~AlgoThreshold()
  //***************************************************************
  {}

  //***************************************************************
  void AlgoThreshold::Reset()
  //***************************************************************
  {
    PMTPulseRecoBase::Reset();
  }

  //***************************************************************
  bool AlgoThreshold::RecoPulse(const std::vector<uint16_t> *wf)
  //***************************************************************
  {
    bool fire = false;
    
    double counter=0;

    double threshold = ( _adc_thres > (_nsigma * _ped_rms) ? _adc_thres : (_nsigma * _ped_rms) );

    threshold += _ped_mean;

    Reset();

    for(auto value : *wf){
    
      //std::cout << "Threshold=" << threshold << ", value=" << value << ", counter=" << counter << std::endl;

      if( !fire && ((double)value) >= threshold ){

	// Found a new pulse

	fire = true;

	_pulse.t_start = counter;

      }
    
      if( fire && ((double)value) < threshold ){
      
	// Found the end of a pulse

	fire = false;

	_pulse.t_end = counter - 1;
      
	_pulse_v.push_back(_pulse);

	_pulse.reset_param();

      }


      //std::cout << "\tFire=" << fire << std::endl;

      if(fire){

	// Add this adc count to the integral

	_pulse.area += ((double)value - (double)_ped_mean);

	if(_pulse.peak < ((double)value - (double)_ped_mean)) {

	  // Found a new maximum
	  
	  _pulse.peak = ((double)value - (double)_ped_mean);

	  _pulse.t_max = counter;

	}

      }
    
      counter++;
    }

    if(fire){

      // Take care of a pulse that did not finish within the readout window.
    
      fire = false;
    
      _pulse.t_end = counter - 1;
    
      _pulse_v.push_back(_pulse);
    
      _pulse.reset_param();

    }

    return true;

  }

}

#endif
