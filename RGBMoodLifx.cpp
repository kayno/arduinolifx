#include "RGBMoodLifx.h"

// Dim curve
// Used to make 'dimming' look more natural. 
uint8_t dc[256] = {
    0,   1,   1,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,
    3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   4,   4,   4,   4,
    4,   4,   4,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   6,   6,   6,
    6,   6,   6,   6,   6,   7,   7,   7,   7,   7,   7,   7,   8,   8,   8,   8,
    8,   8,   9,   9,   9,   9,   9,   9,   10,  10,  10,  10,  10,  11,  11,  11,
    11,  11,  12,  12,  12,  12,  12,  13,  13,  13,  13,  14,  14,  14,  14,  15,
    15,  15,  16,  16,  16,  16,  17,  17,  17,  18,  18,  18,  19,  19,  19,  20,
    20,  20,  21,  21,  22,  22,  22,  23,  23,  24,  24,  25,  25,  25,  26,  26,
    27,  27,  28,  28,  29,  29,  30,  30,  31,  32,  32,  33,  33,  34,  35,  35,
    36,  36,  37,  38,  38,  39,  40,  40,  41,  42,  43,  43,  44,  45,  46,  47,
    48,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,
    63,  64,  65,  66,  68,  69,  70,  71,  73,  74,  75,  76,  78,  79,  81,  82,
    83,  85,  86,  88,  90,  91,  93,  94,  96,  98,  99,  101, 103, 105, 107, 109,
    110, 112, 114, 116, 118, 121, 123, 125, 127, 129, 132, 134, 136, 139, 141, 144,
    146, 149, 151, 154, 157, 159, 162, 165, 168, 171, 174, 177, 180, 183, 186, 190,
    193, 196, 200, 203, 207, 211, 214, 218, 222, 226, 230, 234, 238, 242, 248, 255,
};

// Constructor. Start with leds off.
RGBMoodLifx::RGBMoodLifx(uint8_t rp, uint8_t gp, uint8_t bp)
{
  mode_ = FIX_MODE; // Stand still
  pins_[0] = rp;
  pins_[1] = gp;
  pins_[2] = bp;
  current_RGB_color_[0] = 0;
  current_RGB_color_[1] = 0;
  current_RGB_color_[2] = 0;
  current_HSB_color_[0] = 0;
  current_HSB_color_[1] = 0;
  current_HSB_color_[2] = 0;
  fading_max_steps_ = 200;
  fading_step_time_ = 50;
  holding_color_ = 1000;
  fading_ = false;
  last_update_ = millis();
}

/*
Change instantly the LED colors.
@param h The hue (0..65535) (Will be % 360)
@param s The saturation value (0..255)
@param b The brightness (0..255)
*/
void RGBMoodLifx::setHSB(uint16_t h, uint16_t s, uint16_t b) {
  current_HSB_color_[0] = constrain(h % 360, 0, 360);
  current_HSB_color_[1] = constrain(s, 0, 255);
  current_HSB_color_[2] = constrain(b, 0, 255);
  hsb2rgb(current_HSB_color_[0], current_HSB_color_[1], current_HSB_color_[2], current_RGB_color_[0], current_RGB_color_[1], current_RGB_color_[2]);
  fading_ = false;
}

/*
Change instantly the LED colors.
@param r The red (0..255)
@param g The green (0..255)
@param b The blue (0..255)
*/
void RGBMoodLifx::setRGB(uint16_t r, uint16_t g, uint16_t b) {
  current_RGB_color_[0] = constrain(r, 0, 255);
  current_RGB_color_[1] = constrain(g, 0, 255);
  current_RGB_color_[2] = constrain(b, 0, 255);
  fading_ = false;
}

void RGBMoodLifx::setRGB(uint32_t color) {
  setRGB((color & 0xFF0000) >> 16, (color & 0x00FF00) >> 8, color & 0x0000FF);
}

/*
Fade from current color to the one provided.
@param h The hue (0..65535) (Will be % 360)
@param s The saturation value (0..255)
@param b The brightness (0..255)
@param shortest Hue takes the shortest path (going up or down)
*/
void RGBMoodLifx::fadeHSB(uint16_t h, uint16_t s, uint16_t b, bool shortest) {
  initial_color_[0] = current_HSB_color_[0];
  initial_color_[1] = current_HSB_color_[1];
  initial_color_[2] = current_HSB_color_[2];
  if (shortest) {
    h = h % 360;
    // We take the shortest way! (0 == 360)
    // Example, if we fade from 10 to h=350, better fade from 370 to h=350.
    //          if we fade from 350 to h=10, better fade from 350 to h=370.
    // 10 -> 350
    if (initial_color_[0] < h) {
      if (h - initial_color_[0] > (initial_color_[0] + 360) - h)
        initial_color_[0] += 360;
    }
    else if (initial_color_[0] > h) { // 350 -> 10
      if (initial_color_[0] - h > (h + 360) - initial_color_[0])
        h += 360;
    }
  }
  target_color_[0] = h;
  target_color_[1] = s;
  target_color_[2] = b;
  fading_ = true;
  fading_step_ = 0;
  fading_in_hsb_ = true;
}

/*
Fade from current color to the one provided.
@param r The red (0..255)
@param g The green (0..255)
@param b The blue (0..255)
*/
void RGBMoodLifx::fadeRGB(uint16_t r, uint16_t g, uint16_t b) {
  initial_color_[0] = current_RGB_color_[0];
  initial_color_[1] = current_RGB_color_[1];
  initial_color_[2] = current_RGB_color_[2];
  target_color_[0] = r;
  target_color_[1] = g;
  target_color_[2] = b;
  fading_ = true;
  fading_step_ = 0;
  fading_in_hsb_ = false;
}

void RGBMoodLifx::fadeRGB(uint32_t color) {
  fadeRGB((color & 0xFF0000) >> 16, (color & 0x00FF00) >> 8, color & 0x0000FF);
}

/*
This function needs to be called in the loop function.
*/
void RGBMoodLifx::tick() {
  unsigned long current_millis = millis();
  if (fading_) {
    // Enough time since the last step ?
    if (current_millis - last_update_ >= fading_step_time_) {
      fading_step_++;
      fade();
      if (fading_step_ >= fading_max_steps_) {
        fading_ = false;
        if (fading_in_hsb_) {
          current_HSB_color_[0] = target_color_[0] % 360;
          current_HSB_color_[1] = target_color_[1];
          current_HSB_color_[2] = target_color_[2];
        }
      }
      last_update_ = current_millis;
    }
  }
  else if (mode_ != FIX_MODE) {
    // We are not fading.
    // If mode_ == 0, we do nothing.
    if (current_millis - last_update_ >= holding_color_) {
      last_update_ = current_millis;
      switch(mode_) {
        case RANDOM_HUE_MODE:
          fadeHSB(random(0, 360), current_HSB_color_[1], current_HSB_color_[2]);
          break;
        case RAINBOW_HUE_MODE:
          fadeHSB(360, current_HSB_color_[1], current_HSB_color_[2], false);
          break;
        case RED_MODE:
          fadeHSB(random(335, 400), random(190, 255), random(120, 255));
          break;
        case BLUE_MODE:
          fadeHSB(random(160, 275), random(190, 255), random(120, 255));
          break;
        case GREEN_MODE:
          fadeHSB(random(72, 160), random(190, 255), random(120, 255));
          break;
        case FIRE_MODE:
          setHSB(random(345, 435), random(190, 255), random(120,255));
          holding_color_ = random(10, 500);
          break;
      }
    }
  }
  //if (pins_[0] > 0) {
  if (pins_[0] >= 0) { // this is the modification for LIFX - allows 0 to be written for each pin to power off the LED
    analogWrite(pins_[0], current_RGB_color_[0]);
    analogWrite(pins_[1], current_RGB_color_[1]);
    analogWrite(pins_[2], current_RGB_color_[2]);
  }
}

/*
Convert a HSB color to RGB
This function is used internally but may be used by the end user too. (public).
@param h The hue (0..65535) (Will be % 360)
@param s The saturation value (0..255)
@param b The brightness (0..255)
*/
void RGBMoodLifx::hsb2rgb(uint16_t hue, uint16_t sat, uint16_t val, uint16_t& red, uint16_t& green, uint16_t& blue) {
  val = dc[val];
  sat = 255-dc[255-sat];
  hue = hue % 360;

  int r;
  int g;
  int b;
  int base;

  if (sat == 0) { // Acromatic color (gray). Hue doesn't mind.
    red   = val;
    green = val;
    blue  = val;
  } else  {
    base = ((255 - sat) * val)>>8;
    switch(hue/60) {
      case 0:
		    r = val;
        g = (((val-base)*hue)/60)+base;
        b = base;
        break;
	
      case 1:
        r = (((val-base)*(60-(hue%60)))/60)+base;
        g = val;
        b = base;
        break;
	
      case 2:
        r = base;
        g = val;
        b = (((val-base)*(hue%60))/60)+base;
        break;

      case 3:
        r = base;
        g = (((val-base)*(60-(hue%60)))/60)+base;
        b = val;
        break;
	
      case 4:
        r = (((val-base)*(hue%60))/60)+base;
        g = base;
        b = val;
        break;
	
      case 5:
        r = val;
        g = base;
        b = (((val-base)*(60-(hue%60)))/60)+base;
        break;
    }  
    red   = r;
    green = g;
    blue  = b; 
  }
}

/*  Private functions
------------------------------------------------------------ */

/*
This function is used internaly to do the fading between colors.
*/
void RGBMoodLifx::fade()
{
  if (fading_in_hsb_) {
    current_HSB_color_[0] = (uint16_t)(initial_color_[0] - (fading_step_*((initial_color_[0]-(float)target_color_[0])/fading_max_steps_)));
    current_HSB_color_[1] = (uint16_t)(initial_color_[1] - (fading_step_*((initial_color_[1]-(float)target_color_[1])/fading_max_steps_)));
    current_HSB_color_[2] = (uint16_t)(initial_color_[2] - (fading_step_*((initial_color_[2]-(float)target_color_[2])/fading_max_steps_)));
    hsb2rgb(current_HSB_color_[0], current_HSB_color_[1], current_HSB_color_[2], current_RGB_color_[0], current_RGB_color_[1], current_RGB_color_[2]);
  }
  else {
    current_RGB_color_[0] = (uint16_t)(initial_color_[0] - (fading_step_*((initial_color_[0]-(float)target_color_[0])/fading_max_steps_)));
    current_RGB_color_[1] = (uint16_t)(initial_color_[1] - (fading_step_*((initial_color_[1]-(float)target_color_[1])/fading_max_steps_)));
    current_RGB_color_[2] = (uint16_t)(initial_color_[2] - (fading_step_*((initial_color_[2]-(float)target_color_[2])/fading_max_steps_)));
  }
}
