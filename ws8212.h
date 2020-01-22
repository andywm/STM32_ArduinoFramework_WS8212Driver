/*------------------------------------------------------------------------------  
    ()      File: ws8212.h
    /\      Copyright (c) 2020 Andrew Woodward-May
   //\\     
  //  \\    Description:
              Public interface for basic ws8212 driver.

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
// Colour
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
union Colour3 // GRB, maybe add a define for RGB?
{
    struct 
    {
        uint8_t green;
        uint8_t red;
        uint8_t blue;
    } colour;
    uint8_t as_array[3];
};

enum Colour {OFF, White, Red, Green, Blue, MAX_COLOUR };
extern Colour3 g_pallete[MAX_COLOUR];

//------------------------------------------------------------------------------
// Helper function to determine if a particular bit 1-24 is set inside a colour3.
//------------------------------------------------------------------------------
static inline bool colour3__isBitSet( Colour3 & colour, uint8_t bit )
{
    uint8_t index = static_cast<uint8_t>(bit>=8) + static_cast<uint8_t>(bit>=16);
    uint8_t mask = 0x1 << ( bit - (index*8) );
    return ( (colour.as_array[index] & mask) != 0);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Device
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Not ideal, introduces a vmap look up. But I want to ensure the size of the led
// array set in one place in an object oriented fashion, and is then constant.
// only way I can think to do that is this.
//------------------------------------------------------------------------------
class LEDInfoBase
{
public:
    virtual uint8_t * GetLEDs() = 0;
    virtual uint16_t GetNumberOfLEDs() = 0;
};

template<uint16_t LEDCOUNT>
class LEDStripDefinition final : public LEDInfoBase
{
public:
    uint8_t leds[LEDCOUNT];

    virtual uint8_t * GetLEDs() { return &leds[0]; }
    virtual uint16_t GetNumberOfLEDs() { return LEDCOUNT; }
};

//------------------------------------------------------------------------------
// Initialise the hardware.
//------------------------------------------------------------------------------
void initialise_ws8212_lib( uint8_t pin );

//------------------------------------------------------------------------------
// Update LED strip.
//------------------------------------------------------------------------------
void writeToLEDs( LEDInfoBase & info );

