/*------------------------------------------------------------------------------  
    ()      File: ws8212_timing.h
    /\      Copyright (c) 2020 Andrew Woodward-May
   //\\     
  //  \\    Description:
              Timing definitions for ws8212.

------------------------------
------------------------------
License Text - The MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy of 
this software and associated documentation files (the "Software"), to deal in the
Software without restriction, including without limitation the rights to use, copy,
modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, 
and to permit persons to whom the Software is furnished to do so, subject to the 
following conditions:

The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION 
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

------------------------------------------------------------------------------*/
#pragma once
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include <stdio.h>
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Timing
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
enum TimingEnum { T0H, T1H, T0L, T1L, Reset, MAX_TIMES };
//------------------------------------------------------------------------------
// For smaller periods than HardwareTimer::SetPeriod can compute.
// Calculates the number of iterations for HardwareTimer required to appropximate
// a time period in _nanoseconds_. 
//
// _Caveat_ Max Possible Interval is unsigned_short_int * 1.0/cpu_freq in
// nanoseconds. Scalar not considered.
//------------------------------------------------------------------------------
constexpr uint16_t IterationsForPeriod_ns( uint32_t timeInNanoSeconds )
{
   // Closest Approximation = ROUND( Desired time in seconds / CPU clock in seconds )
   return static_cast<uint16_t>( (static_cast<double>(timeInNanoSeconds)*1.0e-9) / ((1.0/static_cast<double>(F_CPU)) ) + 0.5 );
}

//------------------------------------------------------------------------------
// Low Speed Mode
//  TOH  : 0 code, high voltage time = 0.5us (+- 150ns)
//  T1H  : 1 code, high voltage time = 1.2us (+- 150ns)
//  T0L  : 0 code, low voltage time = 2.0us (+- 150ns)
//  T1L  : 1 code, low voltage time = 1.3us (+- 150ns)
//  RES  : low voltage time >50us
//------------------------------------------------------------------------------
uint16_t g_timingtable[TimingEnum::MAX_TIMES] = 
{
    IterationsForPeriod_ns(500),
    IterationsForPeriod_ns(1200),
    IterationsForPeriod_ns(2000),
    IterationsForPeriod_ns(1300),
    IterationsForPeriod_ns(50000),
};

//------------------------------------------------------------------------------
// Codes
//  0 == T0H T0L
//  1 == T1H T1L
//------------------------------------------------------------------------------
struct RiseFall { uint16_t rise; uint16_t fall; };
RiseFall g_risefallHelper[2] = 
{ 
    { g_timingtable[T0H], g_timingtable[T0L] },  // 0
    { g_timingtable[T1H], g_timingtable[T1L] }   // 1
};
  