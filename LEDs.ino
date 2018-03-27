#include <noise.h>
#include <bitswap.h>
#include <fastspi_types.h>
#include <pixelset.h>
#include <fastled_progmem.h>
#include <led_sysdefs.h>
#include <hsv2rgb.h>
#include <fastled_delay.h>
#include <colorpalettes.h>
#include <color.h>
#include <fastspi_ref.h>
#include <fastspi_bitbang.h>
#include <controller.h>
#include <fastled_config.h>
#include <colorutils.h>
#include <chipsets.h>
#include <pixeltypes.h>
#include <fastspi_dma.h>
#include <fastpin.h>
#include <fastspi_nop.h>
#include <platforms.h>
#include <lib8tion.h>
#include <cpp_compat.h>
#include <fastspi.h>
#include <FastLED.h>
#include <dmx.h>
#include <power_mgt.h>

// pin and LED info
#define LED_PIN     5       // arduino pin 5
#define NUM_LEDS    129     // number of LEDs on the strip
#define BRIGHTNESS  200     // max brightness = 255
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];        // led strip array

#define UPDATES_PER_SECOND 0  // update speed 

CRGBPalette16 currentPalette;   // the current color palette (changes in loop)
TBlendType currentBlending;  // blending (LINEARBLEND or NOBLEND)

bool partyMode = false;     // party mode vs weather mode

// these macros define the beggining and end indecies of the
// different regions in the LED array. See the diagram below
#define R_GROUND_S1     1
#define R_GROUND_E1     12
#define R_GROUND_S2     81
#define R_GROUND_E2     129
#define R_SKY_S1        13
#define R_SKY_E1        80

/*
    Window regions:
    ---------------------------------
    |░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░| 
    |░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░|     <- Sky Region
    |░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░|
    |░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░|
SE1 *░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░* SS1
GS2 *▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒* GE1
    |▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒|     <- Ground Region
    |▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒|
    -------------------------------** GS1
                                  GE2

    The regions are defined by start and end indecies. The ground region
    begins with GS1 and goes to GE1, then resumes at GS2 and ends at GE2.
    The sky region is the remaning part of the array, from SS1 to SE2. 
*/    

// the weather struct is a container for the current weather passed to the 
// functions that change the region's colors. 
struct weather {
    bool windy;
    // use windForce?
    int wVal;
    /*
    wVal options:
    1- sunny 
    2- partly cloudy
    3- mostly cloudy 
    4- cloudy
    5- rainy
    6- snowy 
    */
    int windForce;
    /*
    windForce options:
    1- light 
    2- moderate
    */
};

// get a random number x, where min <= x <= max
int randNum(int min, int max) 
{
    return rand() % (max - min + 1) + min;
} 

// set up LEDs
void setup() 
{
    delay(1000); // power-up safety delay
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip); 
    FastLED.setBrightness(BRIGHTNESS);
    
    if (partyMode)
    {
        currentPalette = RainbowColors_p; // init with a palette 
        currentBlending = LINEARBLEND;
        
    }
}

// loop functon called during excecution
void loop()
{
    if (partyMode) 
    {
         // if we're in party mode, set up a palette and use that to control the LEDs

        ChangePalettePeriodically();    // setting the current palette 

        static uint8_t startIndex = 1;  // start at index 1 (ignore the 1st LED)
        startIndex = startIndex + 1;    // motion speed 

        FillLEDsFromPaletteColors(startIndex); // filling the LEDs with the current palette
    }
    else
    {
        // Otherwise, we want to control the LEDs by a series of functions that 
        // individually change the LEDs by region based on the weather. We use the 
        // region start and ends defined above to access the parts of the array used 
        // for each of the two regions
        /*
        TODO:
        get either-
            a) a wifi/ethernet connection and get a realtime weather data json
            b) a historical weather json 
        Parse this source to get current conditions and change currWeather struct
        accordingly on every update
        */

        // some weather controls here

        // make a weather struct 
        struct weather currWeather;
        // set the values of the struct
        currWeather.windy = false;

        // select one of these for sky condition: 
        //currWeather.wVal = 1;   // clear
        currWeather.wVal = 2;   // partly cloudy
        //currWeather.wVal = 3;   // more clouds
        //currWeather.wVal = 4;   // cloudy

        // and one of these for ground:
        //fillGround(1, false);   // spring
        fillGround(2, false);   // summer
        //fillGround(3, false);   // fall
        //fillGround(4, false);   // winter
        //fillGround(4, true);   // winter, snowy
        //fillGround(1, true);   // spring, snowy
        //fillGround(3, true);   // fall, snowy
        
        fillSky(currWeather);
    }
    
    FastLED.show();     // show changes
    FastLED.delay(1000 / UPDATES_PER_SECOND);   // re-run periodically 
}

// fills the ground based off of the season, given in values 1-4
// also takes parameter snow, which includes snow with ground color
void fillGround(int season, bool snow)
{
    switch (season)
    {
        case 1:
        // case 1: spring
        for (unsigned int i = 0; i <= NUM_LEDS; i++)
        {
            if ((i >= R_GROUND_S1 && i <= R_GROUND_E1) || 
                (i >= R_GROUND_S2 && i <= R_GROUND_E2))
            // set ground 
            leds[i] = CRGB::Green;
        } 
        for (unsigned int j = 0; j < 20; j++) 
        {   
            // set ground accents
            leds[randNum(R_GROUND_S2, R_GROUND_E2)] = CRGB::ForestGreen;
        }
        for (unsigned int j = 0; j < 5; j++) 
        {   
            // set ground accents
            leds[randNum(R_GROUND_S2, R_GROUND_E2)] = CRGB::Yellow;
        }
        for (unsigned int j = 0; j < 5; j++) 
        {   
            // set ground accents
            leds[randNum(R_GROUND_S2, R_GROUND_E2)] = CRGB::FireBrick;
        }
        // set some accents on first section of ground
        leds[6] = CRGB::Yellow;
        leds[5] = CRGB::ForestGreen;
        leds[6] = CRGB::ForestGreen;
        leds[2] = CRGB::ForestGreen;
        leds[3] = CRGB::ForestGreen;
        break;

        case 2:
        // case 2: summer
        for (unsigned int i = 0; i <= NUM_LEDS; i++)
        {
            if ((i >= R_GROUND_S1 && i <= R_GROUND_E1) || 
                (i >= R_GROUND_S2 && i <= R_GROUND_E2))
            // set ground 
            leds[i] = CRGB::Green;
        } 
        for (unsigned int j = 0; j < 30; j++) 
        {   
            // set ground accents
            leds[randNum(R_GROUND_S2, R_GROUND_E2)] = CRGB::LawnGreen;
        }
        // set some accents on first section of ground
        leds[6] = CRGB::LawnGreen;
        leds[2] = CRGB::LawnGreen;
        leds[3] = CRGB::LawnGreen;
        break;

        case 3:
        // case 3: fall
        for (unsigned int i = 0; i <= NUM_LEDS; i++)
        {
            if ((i >= R_GROUND_S1 && i <= R_GROUND_E1) || 
                (i >= R_GROUND_S2 && i <= R_GROUND_E2))
            // set ground 
            leds[i] = CRGB::Orange;
        } 
        for (unsigned int j = 0; j < 25; j++) 
        {   
            // set ground accents
            leds[randNum(R_GROUND_S2, R_GROUND_E2)] = CRGB::SaddleBrown;
        }
        // set some accents on first section of ground
        leds[6] = CRGB::SaddleBrown;
        leds[2] = CRGB::SaddleBrown;
        leds[3] = CRGB::SaddleBrown;
        break;

        case 4:
         // case 4: winter
        for (unsigned int i = 0; i <= NUM_LEDS; i++)
        {
            if ((i >= R_GROUND_S1 && i <= R_GROUND_E1) || 
                (i >= R_GROUND_S2 && i <= R_GROUND_E2))
            // set ground 
            leds[i] = CRGB::Olive;
        } 
        for (unsigned int j = 0; j < 25; j++) 
        {   
            // set ground accents
            leds[randNum(R_GROUND_S2, R_GROUND_E2)] = CRGB::OliveDrab;
        }
        // set some accents on first section of ground
        leds[6] = CRGB::OliveDrab;
        leds[2] = CRGB::OliveDrab;
        leds[3] = CRGB::OliveDrab;
        break;

        default:
        break;
    }

    // if there's snow, randomly populate half the ground with white
    if (snow)
    {  
        for (unsigned int j = 0; j < 30; j++) 
        {   
            // set ground accents
            leds[randNum(R_GROUND_S2, R_GROUND_E2)] = CRGB::CadetBlue;
        }
        // set some accents on first section of ground
        leds[12] = CRGB::CadetBlue;
        leds[11] = CRGB::CadetBlue;
        leds[6] = CRGB::CadetBlue;
        leds[2] = CRGB::CadetBlue;
        leds[3] = CRGB::CadetBlue;
    }

}

// fills the sky based off of the weather values passed. 6 possible sky
// events
void fillSky(struct weather w)
{
    // set the cloud cover
    switch(w.wVal)
    {
        case 1: 
        // case 1: sunny
        for (unsigned int i = R_SKY_S1; i <= R_SKY_E1; i++)
        {
            // set sky
            leds[i] = CRGB::Blue;
        } 
        break;

        case 2: 
        // case 2: partly cloudy
        for (unsigned int i = R_SKY_S1; i <= R_SKY_E1; i++)
        {
            // set sky
            leds[i] = CRGB::Blue;
        } 
        for (unsigned int j = 0; j < 30; j++)
        {
            // set clouds
            leds[randNum(R_SKY_S1, R_SKY_E1)] = CRGB::MediumSeaGreen;
        }
        break;

        case 3: 
        // case 3: mostly cloudy
        for (unsigned int i = R_SKY_S1; i <= R_SKY_E1; i++)
        {
            // set sky
            leds[i] = CRGB::Blue;
        } 
        for (unsigned int j = 0; j < 70; j++)
        {
            // set clouds
            leds[randNum(R_SKY_S1, R_SKY_E1)] = CRGB::MediumSeaGreen;
        }
        break;

        case 4:
        // case 4: cloudy
        for (unsigned int i = R_SKY_S1; i <= R_SKY_E1; i++)
        {
            // set clouds
            leds[i] = CRGB::CadetBlue;
        } 
        for (unsigned int j = 0; j < 24; j++)
        {
            // set dark clouds
            leds[randNum(R_SKY_S1, R_SKY_E1)] = CRGB::Black;
        } 
        break;

        case 5: 
        // case 5: rainy (same as 4 for now)
        for (unsigned int i = R_SKY_S1; i <= R_SKY_E1; i++)
        {
            // set clouds
            leds[i] = CRGB::CadetBlue;
        } 
        for (unsigned int j = 0; j < 24; j++)
        {
            // set dark clouds
            leds[randNum(R_SKY_S1, R_SKY_E1)] = CRGB::Black;
        } 
        break;

        case 6: 
        // case 6: snowy (same as 4 for now)
        for (unsigned int i = R_SKY_S1; i <= R_SKY_E1; i++)
        {
            // set clouds
            leds[i] = CRGB::CadetBlue;
        } 
        for (unsigned int j = 0; j < 24; j++)
        {
            // set dark clouds
            leds[randNum(R_SKY_S1, R_SKY_E1)] = CRGB::Black;
        }  
        break;

        default:
        // set to sunny on default
        for (unsigned int i = R_SKY_S1; i <= R_SKY_E1; i++)
        {
            // set sky
            leds[i] = CRGB::Blue;
        } 
        break;
    }
}

void FillLEDsFromPaletteColors(uint8_t colorIndex)
{
    uint8_t brightness = 255;
    
    for (unsigned int i = 0; i < NUM_LEDS; i++) 
    {
        leds[i] = ColorFromPalette(currentPalette, colorIndex, brightness, currentBlending);
        colorIndex += 3;
    }
}

void ChangePalettePeriodically()
{
    uint8_t secondHand = (millis() / 1000) % 60;
    static uint8_t lastSecond = 99;
    
    if( lastSecond != secondHand) {
        lastSecond = secondHand;
        if( secondHand ==  0)  { currentPalette = RainbowColors_p;         currentBlending = LINEARBLEND; }
        if( secondHand == 10)  { currentPalette = RainbowStripeColors_p;   currentBlending = NOBLEND;  }
        if( secondHand == 15)  { currentPalette = RainbowStripeColors_p;   currentBlending = LINEARBLEND; }
        if( secondHand == 20)  { currentPalette = RainbowColors_p;         currentBlending = LINEARBLEND; }
        if( secondHand == 25)  { SetupTotallyRandomPalette();              currentBlending = LINEARBLEND; }
        if( secondHand == 30)  { SetupBlackAndWhiteStripedPalette();       currentBlending = NOBLEND; }
        if( secondHand == 35)  { SetupBlackAndWhiteStripedPalette();       currentBlending = LINEARBLEND; }
        if( secondHand == 40)  { currentPalette = CloudColors_p;           currentBlending = LINEARBLEND; }
        if( secondHand == 45)  { currentPalette = PartyColors_p;           currentBlending = LINEARBLEND; }
        if( secondHand == 50)  { currentPalette = RainbowColors_p;         currentBlending = NOBLEND;  }
        if( secondHand == 55)  { currentPalette = RainbowColors_p;         currentBlending = LINEARBLEND; }
    }
}

// This function fills the palette with totally random colors
void SetupTotallyRandomPalette()
{
    for (int i = 0; i < 16; i++) {
        currentPalette[i] = CHSV(random8(), 255, random8());
    }
}

// This function sets up a palette of black and white stripes,
// using code.  Since the palette is effectively an array of
// sixteen CRGB colors, the various fill_* functions can be used
// to set them up.
void SetupBlackAndWhiteStripedPalette()
{
    // 'black out' all 16 palette entries...
    fill_solid(currentPalette, 16, CRGB::Black);
    // and set every fourth one to white.
    currentPalette[0] = CRGB::White;
    currentPalette[4] = CRGB::White;
    currentPalette[8] = CRGB::White;
    currentPalette[12] = CRGB::White;
    
}