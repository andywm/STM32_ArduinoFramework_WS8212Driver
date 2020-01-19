/*------------------------------------------------------------------------------  
    ()      File: ws8212.cpp
    /\      Copyright (c) 2020 Andrew Woodward-May - See legal.txt
   //\\     
  //  \\    Description:
              Basic driver for a WS8211/WS8212 LED strip.
              https://cdn-shop.adafruit.com/datasheets/WS2811.pdf

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
// Globals within file scope.
//------------------------------------------------------------------------------
Colour3 g_pallete[MAX_COLOUR] = 
{
    {0,     0,      0},
    {255,   255,    255},
    {255,   0,      0},
    {0,     255,    0},
    {0,     0,      255},
};

namespace
{
    using InstructionPtr = void (*)();

    struct WS8212WriterDevice
    {
        uint8_t outputPin = 255;
        uint8_t nextBit = 0;
        uint16_t nextLED = 0;

        InstructionPtr resumeInstruction = nullptr;
        LEDInfoBase * ledInfo = nullptr;

        uint16_t riseTime = 0;
        uint16_t fallTime = 0;
        Colour3 currentColour = g_pallete[0];
        bool device_locked = false;

    } g_ledWriterDevice;

    HardwareTimer g_timer(1);
}

// Sequence for 3 LEDs.
// |Refresh Cycle 1|          |Refresh Cycle 2|
// (24 bit)(24 bit)(24 bit)RES(24 bit)(24 bit)(24 bit)RES

//24 bit composition RGB, big endian.
//|R7 R6 R5 R4 R3 R2 R1 R0|G7 G6 G5 G4 G3 G2 G1 G0|B7 B6 B5 B4 B3 B2 B1 B0|

//------------------------------------------------------------------------------
// Forwards
//------------------------------------------------------------------------------
void signalRise();
void signalFall();
void nextLED();
void writeColour3();

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void sequenceResetState()
{
    g_ledWriterDevice.device_locked = false;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void sequenceBreak()
{
    g_timer.setOverflow( g_timingtable[TimingEnum::Reset] );
    g_timer.attachInterrupt(1, sequenceResetState);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void initialise_ws8212_lib( uint8_t pin )
{
    g_ledWriterDevice.outputPin = pin;
    pinMode(pin, OUTPUT);
}

//------------------------------------------------------------------------------
// Locks the LED writer device, begins timing sequence.
//------------------------------------------------------------------------------
void writeToLEDs( LEDInfoBase & info )
{
    if( g_ledWriterDevice.device_locked )
        return;

    g_ledWriterDevice.device_locked = true;
    g_ledWriterDevice.ledInfo = &info;
    g_ledWriterDevice.nextLED = 0;
    g_ledWriterDevice.nextBit = 0;

    nextLED();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void nextLED()
{
    const auto & ledInfo = g_ledWriterDevice.ledInfo;

    if( g_ledWriterDevice.nextLED < ledInfo->GetNumberOfLEDs() )
    {
        g_ledWriterDevice.currentColour = g_pallete[ ledInfo->GetLEDs()[ g_ledWriterDevice.nextLED] ];
        g_ledWriterDevice.nextBit = 0;

        writeColour3();
        return;
    }

    sequenceBreak();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void writeColour3()
{
    const InstructionPtr resumeMode[2] = { writeColour3, nextLED };
    const bool bitValue = colour3__isBitSet(g_ledWriterDevice.currentColour, g_ledWriterDevice.nextBit );

    g_ledWriterDevice.riseTime = g_risefallHelper[static_cast<uint8_t>(bitValue)].rise;
    g_ledWriterDevice.fallTime = g_risefallHelper[static_cast<uint8_t>(bitValue)].fall;
    g_ledWriterDevice.resumeInstruction = resumeMode[static_cast<uint8_t>(g_ledWriterDevice.nextBit++ < 24) ];

    signalRise();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void signalRise()
{
    digitalWrite(g_ledWriterDevice.outputPin, HIGH);

    g_timer.setOverflow( g_ledWriterDevice.riseTime );
    g_timer.attachInterrupt(1, signalFall);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void signalFall()
{
    digitalWrite(g_ledWriterDevice.outputPin, LOW);

    g_timer.setOverflow( g_ledWriterDevice.fallTime );
    g_timer.attachInterrupt(1, g_ledWriterDevice.resumeInstruction);
}
