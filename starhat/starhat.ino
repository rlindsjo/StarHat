#include <Adafruit_NeoPixel.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

#define LEDS 60
#define LED_PIN 1
#define SWITCH 2
#define STAR_COUNT 4
// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
volatile uint8_t program = 0;
volatile uint8_t program_next = 0;
volatile uint16_t state = 0;
volatile uint16_t button_time = 0;
volatile boolean previous = HIGH;

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.

void setup() {
  power_adc_disable();
  for (byte i=0; i<5; i++) {     //make all pins inputs with pullups enabled
       pinMode(i, INPUT_PULLUP);
    }
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
}

ISR(INT0_vect) {
  GIMSK = 0;                     //disable external interrupts (only need one to wake up)
  program = 0;
  state = 0;
  previous = LOW;
}

void sleepNow() {
    byte adcsra, mcucr1, mcucr2;
 
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    MCUCR &= ~(_BV(ISC01) | _BV(ISC00));      //INT0 on low level
    GIMSK |= _BV(INT0);                       //enable INT0
    adcsra = ADCSRA;                          //save ADCSRA
    ADCSRA &= ~_BV(ADEN);                     //disable ADC
    cli();                                    //stop interrupts to ensure the BOD timed sequence executes as required
    mcucr1 = MCUCR | _BV(BODS) | _BV(BODSE);  //turn off the brown-out detector
    mcucr2 = mcucr1 & ~_BV(BODSE);            //if the MCU does not have BOD disable capability,
    MCUCR = mcucr1;                           //  this code has no effect
    MCUCR = mcucr2;
    sei();                                    //ensure interrupts enabled so we can wake up again
    sleep_cpu();                              //go to sleep
    sleep_disable();                          //wake up here
    ADCSRA = adcsra;                          //restore ADCSRA
}   

void loop() {
  if (digitalRead(SWITCH) == LOW) {
    delay(20);
    previous = LOW;
    button_time ++;
    return;
  } else {
    previous = HIGH;
  }

  if (button_time > 50) {
    program = 100;
    state = 0;
  } else if (button_time > 20) {
    program_next = program;
    program = 10;
    state = 0;
  } else if (button_time > 2) {
    program = program_next; 
    state = 0;
  }
  button_time = 0;
  switch(program) {
    case 0: // Blink ready blue
      program_next = 1; 
      setAll(0); 
      state++;
      if (state < 10) {
        strip.setPixelColor(LEDS - 1, state );
      } 
      if (state > 200) state = 0; 
    break;
    case 1: // Roll on rainbow
      program_next = 3;
      if (state < strip.numPixels()) {
        for(uint16_t i=0; i<state; i++) {
          strip.setPixelColor(i, Wheel((i) & 255));
        }
        delay(20);
        state++;
      } else {
        program = 2;
      }
      break;
    case 2: rainbow(); break;
    case 3: // Remove rainbow
      program_next = 4;
      if (state < strip.numPixels()) {
        strip.setPixelColor(state, 0);
        delay(20);
        state ++;
      } else {
        program = 4;
      }
      break;  
    case 4:// Clear all  
      setAll(0);
      program ++;
      state = 0;
      randomSeed(micros());
      break;
    case 5: // Single star
      program_next = 6;
      star();
      break;
    case 6:// Color in
      program_next = 7;
      if (state < strip.numPixels()) {
        for(uint16_t i=0; i<state; i++) {
          strip.setPixelColor(i, Wheel((i) & 255));
        }
        delay(20);
        state++;
      } else {
        program ++;
        state = 0;
      }
      break;
    case 7: // Color out
      if (state < strip.numPixels()) {
        strip.setPixelColor(state, 0);
        delay(20);
        state ++;
      } else {
        program ++;
        state = 0;
      }
      break;
    case 10: // Special
      if (state < strip.numPixels()) {
        for(uint16_t i=0; i<state; i++) {
          strip.setPixelColor(i, Wheel((i) & 255));
        }
        delay(20);
        state++;
      } else {
        program ++;
        state = 0;
      }
      break;
    case 11: // Special
      if (state < 2000) {
        rainbow();
        state++;
      } else {
        program ++;
        state = 0;
      }
      break;
    case 12: // Color out 
      if (state < strip.numPixels()) {
        strip.setPixelColor(state, 0);
        delay(20);
        state ++;
      } else {
        program = program_next;
        state = 0;
      }
      break;
    case 100: 
      setAll(0);
      sleepNow();
      program = 0;
      state = 0;
      break;
    default: 
      program = 0;
      program_next = 0;
      state = 0;
  }
  strip.show();
}

typedef struct STAR {
    uint32_t cc;
    uint8_t spd;
    uint8_t pixel;
    uint8_t state;
    uint8_t substate;
};

STAR stars[STAR_COUNT];
void star() {
  for (int i = 0; i < STAR_COUNT; i++) {
    if (stars[i].state == 0) {
      uint32_t channels = random(1, 8);
      stars[i].cc = (channels & 0b100) <<14 | (channels & 0b010) << 7 | (channels & 0b001) ;
      stars[i].spd = random(4, 16);
      stars[i].pixel = freeRandomPixel();
      stars[i].substate = 0;
      stars[i].state = 1;
    }
    if (++stars[i].substate == stars[i].spd) {
      stars[i].substate = 0;
      stars[i].state++;
      lightStar(&stars[i]);
    }
  }
  delay(1);
}

void lightStar(struct STAR * star) {
  uint32_t intensity;
  if (star->state < 32) {
    intensity = 8*star->state;
  } else if (star->state < 64) {
    intensity = (511 - 8*star->state);
  } else {
    intensity = 0;
    star->state = 0;
  }
  strip.setPixelColor(star->pixel, intensity * star->cc);
}

int freeRandomPixel() {
  loop: while(true) {
    int candidate = random(strip.numPixels());
    for (int i = 0; i < STAR_COUNT; i++) {
      if (candidate == stars[i].pixel) {
        goto loop;
      }
    }
    return candidate;
  }
}

uint32_t Wheel(uint8_t WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else if(WheelPos < 170) {
    WheelPos -= 85;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
}

void rainbow() {
  for(uint8_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, Wheel((i+state) & 255));
  }
  delay(1);
  state+=2;
}

void setAll(uint32_t color) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
  }
}
