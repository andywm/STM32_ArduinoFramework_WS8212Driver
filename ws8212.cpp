/*------------------------------------------------------------------------------  
    ()      File: ws8212.cpp
    /\      Copyright (c) 2020 Andrew Woodward-May
   //\\     
  //  \\    Description:
              Basic driver for a WS8212B LED strip.

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

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include <io.h>
#include <stdio.h>
#include <stm32f1/include/series/timer.h>
#include <HardwareTimer.h>
#include "ws8212.h"
#include "ws8212_timing.h"
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Forwards
//------------------------------------------------------------------------------
void writeColour3();
void nextLED();
void signalRise();
void signalFall();
void onResetTimeElapsed();
void sequenceBreak();

//------------------------------------------------------------------------------
// Globals within file scope.
//------------------------------------------------------------------------------
namespace
{
    using InstructionPtr = void (*)();

    //Our current state and hardware resources.
    HardwareTimer g_timer(1);
    struct WS8212WriterDevice
    {
        uint8_t outputPin = 255;
        uint8_t nextBit = 0;
        uint16_t nextLED = 0;

        InstructionPtr resumeInstruction = nullptr;
        LEDInfoBase * ledInfo = nullptr;

        uint16_t riseTime = 0;
        uint16_t fallTime = 0;
        Colour3 currentColour = g_pallete[0]; //copy, nb. is this is slower than the indirection of a pointer?
        bool device_locked = false;

    } g_ledWriterDevice;
}

//------------------------------------------------------------------------------
// Pallete defines in GRB format. Might want to create a helper macro or constexpr
// function as this is a.. non-typicial format for humans. 
//------------------------------------------------------------------------------
Colour3 g_pallete[MAX_COLOUR] = 
{  //g      r       b 
    {0,     0,      0},         //OFF
    {255,   255,    255},       //White
    {0,     255,    0},         //Red
    {255,   0,      0},         //Green
    {0,     0,      255},       //Blue
}; //g      r       b 

//------------------------------------------------------------------------------
// Initialise the device for timers and gpio. Public Interface.
//------------------------------------------------------------------------------
void initialise_ws8212_lib( uint8_t pin )
{
    //initialise timer.
    g_timer.setMode(TIMER_CH1, timer_mode::TIMER_OUTPUT_COMPARE);
    g_timer.setCompare(TIMER_CH1, 0);
    g_timer.setPrescaleFactor(1);

    g_ledWriterDevice.outputPin = pin;
    pinMode(pin, OUTPUT);
    digitalWrite(g_ledWriterDevice.outputPin, LOW);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Sequencing and Formatting.
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// E.g. Sequencing for 2 LEDs refreshed twice.
//
// |Refresh Cycle 1|          |Refresh Cycle 2|
// (24 bit GRB)(24 bit GRB)RES(24 bit GRB)(24 bit RGB)RES
// 
// GRB Format
// 24 bit composition, built from 1: T1H T1L and 0 T0H T0L
// G7 G6 G5 G4 G3 G2 G1 G0|R7 R6 R5 R4 R3 R2 R1 R0|B7 B6 B5 B4 B3 B2 B1 B0
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Locks the LED writer device, begins the write sequence. Public Interface.
//------------------------------------------------------------------------------
void writeToLEDs( LEDInfoBase & info )
{
    if( g_ledWriterDevice.device_locked )
        return;

    g_ledWriterDevice.device_locked = true;
    g_ledWriterDevice.ledInfo = &info;
    g_ledWriterDevice.nextLED = 0;
    g_ledWriterDevice.nextBit = 0;

    g_timer.pause();
    nextLED();
}

//------------------------------------------------------------------------------
// CONTROL SEQUENCING
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// increments the led, causes a RES when no further LEDs are to be written.
// resets bit counter, increment led, sets colour. 
//------------------------------------------------------------------------------
void nextLED()
{
    const auto & ledInfo = g_ledWriterDevice.ledInfo;

    if( g_ledWriterDevice.nextLED < ledInfo->GetNumberOfLEDs() )
    {
        g_ledWriterDevice.currentColour = g_pallete[ ledInfo->GetLEDs()[ g_ledWriterDevice.nextLED++] ];
        g_ledWriterDevice.nextBit = 0;

        writeColour3();     
        return;
    }

    //once all LEDs are read ensure the RES delay is in place.
    sequenceBreak();
}

//------------------------------------------------------------------------------
// This function decides whether the current bit is 1 or 0 and sets the 
// appropriate timings for the signal interval. Then increments the current bit.
// Will also decide where control returns to once the interval is over, if there
// are more bits to be written, it is returned here. Otherwise to the nextLED
// function. 
//------------------------------------------------------------------------------
void writeColour3()
{
    const bool bitValue = colour3__isBitSet(g_ledWriterDevice.currentColour, g_ledWriterDevice.nextBit ); 

    g_ledWriterDevice.riseTime = g_risefallHelper[static_cast<uint8_t>(bitValue)].rise;
    g_ledWriterDevice.fallTime = g_risefallHelper[static_cast<uint8_t>(bitValue)].fall;

    if( g_ledWriterDevice.nextBit + 1 < 24 )
    {
        g_ledWriterDevice.resumeInstruction = writeColour3;
    }
    else
    {
        g_ledWriterDevice.resumeInstruction = nextLED;
    }
    
    g_ledWriterDevice.nextBit++;
    signalRise();
}

//------------------------------------------------------------------------------
// SIGNAL INTERVAL
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// paired with signalFall, defines an interval for a single bit.
// sets high, binds interrupt to begin low once preset period has elapsed.
//------------------------------------------------------------------------------
void signalRise()
{
    g_timer.detachInterrupt(TIMER_CH1);
    digitalWrite(g_ledWriterDevice.outputPin, HIGH);

    g_timer.pause();
    g_timer.setOverflow( g_ledWriterDevice.riseTime );
    g_timer.attachInterrupt(1, signalFall);
    g_timer.refresh();
    g_timer.resume();
}

//------------------------------------------------------------------------------
// paired with signalRise, defines an interval for a single bit.
// sets low before restoring control to either writeColour3, or nextLED, whichever
// was decided on in advance.
//------------------------------------------------------------------------------
void signalFall()
{
    g_timer.detachInterrupt(TIMER_CH1);
    digitalWrite(g_ledWriterDevice.outputPin, LOW);

    g_timer.pause();
    g_timer.setOverflow( g_ledWriterDevice.fallTime );
    g_timer.attachInterrupt(1, g_ledWriterDevice.resumeInstruction);
    g_timer.refresh();
    g_timer.resume();
}

//------------------------------------------------------------------------------
// RESET CODE
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// unlock the device post timeout.
//------------------------------------------------------------------------------
void onResetTimeElapsed()
{
    //free the device.
    g_ledWriterDevice.device_locked = false;
    g_timer.pause();
}

//------------------------------------------------------------------------------
// ensures the LED strip times out on new input before unlocking the deive.
//------------------------------------------------------------------------------
void sequenceBreak()
{
    g_timer.detachInterrupt(TIMER_CH1);
    digitalWrite(g_ledWriterDevice.outputPin, LOW);
    
    //wait out the RES time beteen sequences.
    g_timer.pause();
    g_timer.setPrescaleFactor(5);
    g_timer.setOverflow( g_timingtable[TimingEnum::Reset] );
    g_timer.attachInterrupt(1, onResetTimeElapsed);
    g_timer.refresh();
    g_timer.resume();
}
