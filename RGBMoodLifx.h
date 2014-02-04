/*
  Created by Harold Waterkeyn, February 1, 2012
  V0.3 : Feb 4, 2012
  
  Modified (slightly) for LIFX bulb - just so we can send RGB 0,0,0 to turn the bulb off!
  
  Credits:
  Inspired from the Moodlight Library by Kasper Kamperman
    http://www.kasperkamperman.com/blog/arduino-moodlight-library/
*/

#ifndef GUARD_RGB
#define GUARD_RGB
#include "Arduino.h"
class RGBMoodLifx {
  public:
    enum Modes {
      FIX_MODE,
      RANDOM_HUE_MODE,
      RAINBOW_HUE_MODE,
      RED_MODE,
      BLUE_MODE,
      GREEN_MODE,
      FIRE_MODE
    };

    RGBMoodLifx(uint8_t = 0, uint8_t = 0, uint8_t = 0); // New instance with output pin specified.
    void setHSB(uint16_t, uint16_t, uint16_t);     // Set a fixed color from HSB color space.
    void setRGB(uint16_t, uint16_t, uint16_t);     // Set a fixed color from RGB color space.
    void setRGB(uint32_t); // Using Color class.
    void fadeHSB(uint16_t, uint16_t, uint16_t, bool = true);    // Fade to a new color (given in HSB color space).
    void fadeRGB(uint16_t, uint16_t, uint16_t);    // Fade to a new color (given in RGB color space).
    void fadeRGB(uint32_t); // Using Color class.
    void tick();                    // Update colors if needed. (call this in the loop function)
    void hsb2rgb(uint16_t, uint16_t, uint16_t, uint16_t&, uint16_t&, uint16_t&); // Used internally to convert HSB to RGB
    bool isFading() {return fading_;}     // True we are currently fading to a new color.
    bool isStill() {return not fading_;}  // True if we are not fading to a new color.
    void setMode(Modes m) {mode_ = m;}  // Change the mode.
    void setHoldingTime(uint16_t t) {holding_color_ = t;}     // How many ms do we keep a color before fading to a new one.
    void setFadingSpeed(uint16_t t) {fading_step_time_ = t;}  // How many ms between each step when fading.
    void setFadingSteps(uint16_t t) {fading_max_steps_ = t;}  // How many steps for fading from a color to another.
    uint16_t red() {return current_RGB_color_[0];}                // The current red color.
    uint16_t green() {return current_RGB_color_[1];}              // The current green color.
    uint16_t blue() {return current_RGB_color_[2];}               // The current blue color.
  private:
    Modes mode_;
    uint8_t pins_[3];           // The pins for color output. (PWM)
    uint16_t current_RGB_color_[3];
    uint16_t current_HSB_color_[3];
    uint16_t initial_color_[3]; // Used when fading.
    uint16_t target_color_[3];  // Used when fading.
    uint16_t fading_step_;      // Current step of the fading.
    uint16_t fading_max_steps_; // The total number of steps when fading.
    uint16_t fading_step_time_; // The number of ms between two variation of color when fading.
    uint16_t holding_color_;    // The number of ms to hold color before fading again. (when cycling i.e. mode_ != 0)
                                // Todo, why not using 2 boundary and random between the 2 ?
    bool fading_in_hsb_;        // Are we fading between colors in HSB color space ?
    bool fading_;               // Are we fading now ?
    unsigned long last_update_; // Last time we did something.
    void fade();                // Used internaly to fade
};

class Color {
  public:
    static const uint32_t RED = 0xFF0000;
    static const uint32_t GREEN = 0x00FF00;
    static const uint32_t BLUE = 0x0000FF;
    static const uint32_t AQUAMARINE = 0x7FFFD4;
    static const uint32_t AIRFORCEBLUE = 0x5D8AA8;
    static const uint32_t AMARANTH = 0xE52B50;
    static const uint32_t ASPARAGUS = 0x87A96B;
};

#endif
