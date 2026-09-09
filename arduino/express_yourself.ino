#include "Adafruit_Keypad.h"
#include <Adafruit_NeoPixel.h>

#define SYNC_ALL  0
#define SYNC_RATE 1
#define SYNC_OFF  2

#define MOD_AMP   0
#define MOD_FREQ  1
#define MOD_PHASE 2
#define MOD_OFF   3

#define TRIANGLE  0
#define SQUARE    1
#define SINE      2

#define SET_A     0
#define SET_B     1
#define SET_RATE  2


#define RATEMIN   60000L // 1 bpm (cycle every minute) - slowest rate 
#define RATEMID   1000L  // 60 bpm - when knob is at 12:00
#define RATEMAX   60L    // 1000 bpm (~16.67 beats per second)

#define DIGIPOTMIN 10     // keep the wiper away from the end to protect against high currents
#define DIGIPOTMAX 246    // ...both ends. Not sure if either of these is needed, but here to stay on the safe side.

#define DEBOUNCE  5   // ms
#define TAP_TIME  250 // ms

#define KNOBTHRESHOLD 2  // discard changes below this threshold - jitter
#define KNOBDEBOUNCE 200  // how long after a knob stops before engaging jitter protection

// switches
#define ROWS 4
#define COLS 5

char keys[ROWS][COLS] = {
  {'B', 'A', 'D', 'C', 'E'},
  {'O', 'N', 'Q', 'P', 'F'},
  {'H', 'G', 'J', 'I', 'M'},
  {'L', 'K', 'S', 'R', 'T'}
};

byte colPins[COLS] = {4, 5, 6, 7, 8}; // connect to the row pinouts of the keypad
byte rowPins[ROWS] = {1, 0, 2, 3}; // connect to the column pinouts of the keypad

Adafruit_Keypad switches = Adafruit_Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// knobs
#define KNOBPIN1 21
#define KNOBPIN2 20
#define KNOBPIN3 19

byte knobs[3] = {KNOBPIN1, KNOBPIN2, KNOBPIN3};

// digpots
#define DATA    16
#define SCLK    15
#define CS1     14
#define CS2     10
#define CS3     18

byte potpins[3] = {CS1, CS2, CS3};

int vals[3] = {0, 0, 0};
int vals2[3] = {0, 0, 0};  // used to show rate when exp off

// neopixels
#define LED_PIN     9
#define LED_COUNT   3
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);


// pedal state

unsigned long edges[4] = {0, 0, 0, 0}; // time of last depress (now/millis)
char momentaries[4] = {'F', 'M', 'T', 'E'};  // footswitches 0 indexed, tempo = 3
byte holds[4] = {0, 0, 0, 0}; // is this switch being held down? // same index as momentaries, tempo = 3

// input positions
int shape[3] = {0, 0, 0};
int set[3] = {0, 0, 0};
int sync[3] = {SYNC_OFF, SYNC_OFF, SYNC_OFF};
int knob[3] = {0, 0, 0}; // current knob positions
int knobtempvals[3] = {0, 0, 0}; // temporary knob positions
bool knobmoving[3] = {0, 0, 0}; // is the knob 
unsigned long knobtimers[3] = {0, 0, 0}; // last time the knob changed

// calculated values
byte exps[3] = {0, 0, 0}; // exps 0 = off, 1 = on

// foot switches
byte fs[3] = {0, 0, 0}; // 0 off, 1 held

// tap tempo (last 5 taps)
#define TEMPOTIMEOUT 5000 // tap tempo timout - stop listening after being idle for 5 seconds
#define TAPS 5
unsigned long tempos[TAPS] = {0, 0, 0, 0, 0};
byte tapcount = 0;
unsigned long lasttap = 0;
unsigned long tapduration = 0;

byte mod[3] = {MOD_OFF, MOD_OFF, MOD_OFF};
bool randoms[3] = {0, 0, 0};  // random mode 0 = off, 1 = on
bool oneshot[3] = {0, 0, 0};  // oneshot mode

int a[3] = {768, 786, 768}; // a values
int b[3] = {256, 256, 256}; // b values
int r[3] = {512, 512, 512}; // rates (wavelength in milliseconds)

int ra[3] = {0, 0, 0}; // randomized a values
int rb[3] = {0, 0, 0}; // randomized b values
int rc[3] = {0, 0, 0}; // randomized c values (aka, next a)

unsigned long starts[3] = {0, 0, 0};  // millis value - when an exp was turned on
unsigned long cyclestarts[3] = {0, 0, 0};  // millis - when an exp's current cycle is started
unsigned long cycleends[3] = {0, 0, 0};  // millis - when an exp's current cycle is expected to end -- used to calculate progress and changing rates
float mults[3] = {0, 0, 0}; // multiplication values - used when multiplication factors change

unsigned long now = 0;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  switches.begin();

  pinMode(DATA, OUTPUT);
  pinMode(SCLK, OUTPUT);
  digitalWrite(DATA, LOW);
  digitalWrite(SCLK, LOW);
  for (int i = 0; i < 3; i++) {
    pinMode(potpins[i], OUTPUT);
    digitalWrite(potpins[i], HIGH);
  }

  strip.begin();
  strip.setBrightness(64);
  strip.show();

  while(!Serial);
  // Serial.println("\n\nInitialize Serial Monitor");
  


}

void loop() {
  switches.tick();
  // Not doing anything special for this value overflowing.
  // It's ulikely anyone would leave their pedal on for a month and a half.
  // Sorry if bad things happen to you because of this.

  // This is offset by the slowest rate, multiplied by 1/8 and 1/8 
  // Allows for the start to go back in time. Yes, this does mean the slowest
  // the third exp can operate is just over an hour per cycle.
  now = millis() + RATEMIN * 64;


  readMomentarySwitches();
  readToggleSwitches();
  readKnobs();
  
  calculateOutputValues();

  turnDigiPots();
  setLeds();

  Serial.print(0);
  Serial.print(",");
  Serial.print(1024);
  Serial.print(",");
  for (int i = 0; i < 3; i++) {
    Serial.print(vals[i]);
    Serial.print(",");
  }
  Serial.println("");
}

void readMomentarySwitches() {
    // reset tap tempo if idle
    if (now > lasttap + TEMPOTIMEOUT) {
      tapcount = 0;
    }

    for (int i = 0; i < 4; i++) {
      if (switches.isPressed(momentaries[i])) {
        // switch is down
        if (edges[i] == 0) {
          edges[i] = now;
        } else {
          if (edges[i] < now + TAP_TIME) {
            holds[i] = 1;
          }
        }
      } else {
        // switch is up
        // if press < DEBOUNCE time, it's noise - ignore
        // if press > TAP_TIME, it's a hold - no special action
        if (edges[i] != 0 && (edges[i] + DEBOUNCE) < now && (edges[i] + TAP_TIME) > now) {
          // tap detected
          if (i == 3) {
            // tap tempo
            if (now > lasttap + TEMPOTIMEOUT) {
              // first tap in a sequence
              tapcount = 0;
            } else {
              // subsequent taps
              for (int j = 0; j < TAPS - 1; j++) {
                tempos[j] = tempos[j + 1];
              }
              tempos[TAPS - 1] = edges[3] - lasttap; 

              if (tapcount < TAPS) {
                tapcount++;
              }

              unsigned long total = 0;
              for (int j = TAPS - tapcount; j < TAPS; j++) {
                total += tempos[j];
              }
          
              tapduration = total / tapcount;
            }

            lasttap = edges[i];

          } else {
            // toggle exp state on/off
            if (exps[i] == 0) {

              exps[i] = 1;
              starts[i] = now;
              cyclestarts[i] = now;
            } else {
              exps[i] = 0;
              starts[i] = 0;
            }
          }
        }
        
        edges[i] = 0;
        holds[i] = 0;
      }
    }
}

// do I need to debounce these? is it already done in the keypad library?
void readToggleSwitches() {
  bool anyfsheld = holds[0] || holds[1] || holds[2];
  bool tempoheld = holds[3];

  // shape switches
  byte oldshapes[3] = {shape[0], shape[1], shape[2]};

  byte shapeswitchvalue[3] = {
    checkSwitch('A', 'B'),
    checkSwitch('G', 'H'),
    checkSwitch('N', 'O')
  };
  for (byte i = 0; i < 3; i++) {
    if (shape[i] != shapeswitchvalue[i]) {
      shape[i] = shapeswitchvalue[i]; // transfer value
      oneshot[i] = tempoheld;         // set flag for special modes
      randoms[i] = anyfsheld;         // set flag for special modes
    }
  }

  // set switches
  byte setswitchvalue[3] = {
    checkSwitch('C', 'D'),
    checkSwitch('I', 'J'),
    checkSwitch('P', 'Q')
  };

  for (byte i = 0; i < 3; i++) {
    if (set[i] != setswitchvalue[i]) {
      knobmoving[i] = false;  // stop knob
      knobtimers[i] = 0;  // reset knob moving timer

      set[i] = setswitchvalue[i]; // transfer value
    }
  }

  // sync/mod switches
  byte oldsync[3] = {0, sync[1], sync[2]}; 
  byte oldmod[3] = {0, mod[1], mod[2]};
  byte syncsw[3] = {0, checkSwitch('K', 'L'), checkSwitch('R', 'S')};

  for (byte i = 1; i < 3; i++) {  // starts at 1  
    // if mod off, change if switch doesnt match old sync val
    // if mod not off, change if switch doesn't match old mod val
    if ((oldmod[i] == MOD_OFF && oldsync[i] != syncsw[i]) ||
        (oldmod[i] != MOD_OFF && oldmod[i] != syncsw[i])) {
      if (anyfsheld) {
        mod[i] = syncsw[i]; // fs down - mod
      } else {
        sync[i] = syncsw[i]; // fs up - sync
        mod[i] = MOD_OFF; // switch changed - clear mod flag
      }
    }
  }

}

// turns an on-off-on switch into a 0, 1, or 2
byte checkSwitch(char a, char b) {
  if (switches.isPressed(a)) { return 0; }
  if (switches.isPressed(b)) { return 2; }
  return 1;
}

// potentiometer nonsense

void readKnobs() {
  for (int i = 0; i < 3; i++) {
  
    int knobval = analogRead(knobs[i]);
    bool overthreshold = abs(knobtempvals[i] - knobval) > KNOBTHRESHOLD;

    // soft debounce:
    // ignore small changes in knob values (e.g., jitter/noise), but once the
    // knob starts moving, read continuously until it stops again.
    if (!overthreshold && !knobmoving[i]) {
      continue;
    }

    // check if the knob has stopped moving
    if (!overthreshold) {
        // knob has stopped moving, and has for more than our debounce time
        if (now > knobtimers[i] + KNOBDEBOUNCE) {
          knobmoving[i] = false;
          knobtimers[i] = 0;
          continue;
        }
    } else {
        // update knob timers and knobtempvals
        knobtimers[i] = now;
        knobtempvals[i] = knobval;
    }

    // if we are here, the knob is moving
    if (!knobmoving[i]) {
      knobmoving[i] = true;
      knob[i] = knobval;  // update previous knob val
    }
    
    setVal(i, relativeKnobVal(getVal(i), knobval, knob[i]));

    knob[i] = knobval;  // update previous knob val
  }
}

// relative knobs:
// when switching between a, b, and rate, the knob position and value may not
// match. in this case, the knob should control the value in a relative way -
// e.g., turning the knob to the right raises the value, and turning left
// lowers the value.
int relativeKnobVal(int setting, int knobval, int oldknobval) {
  int change = knobval - oldknobval;
  
  // allow value to catch knob if knob moving away
  if ((knobval > setting && change > 0) ||
      (knobval < setting && change < 0 )) {
    change = min(change * 2, knobval - setting);
  }

  return constrain(setting + change, 0, 1023);
}

// reads appropriate value for given set switch
int getVal(byte i) {
  if (set[i] == 0) {
    return a[i];
  }
  if (set[i] == 1) {
    return b[i];
  }
  if (set[i] == 2) {
    return r[i];
  }
}

// sets appropriate value given set switch
void setVal(byte i, int val) {
   if (set[i] == 0) {
      a[i] = val;
    }
    if (set[i] == 1) {
      b[i] = val;
    }
    if (set[i] == 2) {
      r[i] = val;
      if (i == 0) {
        tapduration = 0;  // if changing the rate knob on the first exp, reset tap tempo
        tapcount = 0;
        lasttap = 0;
      }

    }
}

void calculateOutputValues() {
  for (int i = 0; i < 3; i++) {
    int aa = a[i];
    int bb = b[i];
    int cc = a[i];  // c values - usually matching a, except for random mode
    int rr = r[i];
    unsigned long start = cyclestarts[i];
    unsigned long end = cycleends[i];
    byte waveshape = shape[i];
    
    // rate modulation
    // replace rate knob with val of exp to left
    if (i > 0 && mod[i] == MOD_FREQ) {
      rr = vals[i-1];
    }

    // amplitude modulation
    // scale a and b about their centerpoint, using output from exp to left
    // down = 0x, centered = 1x, up = 2x
    if (i > 0 && mod[i] == MOD_AMP) {
      int center = (aa + bb) / 2;
      float ampfactor = mapf(vals[i-1], 0, 1023, 0.0, 2.0);
      if (aa > center) {
        // note ampfactor goes from 0.0 to 2.0, so this can push a and b beyond
        // their original values
        int scaleda = mapf(ampfactor, 0.0, 1.0, center, aa);
        int scaledb = mapf(ampfactor, 0.0, 1.0, center, bb);
        aa = constrain(scaleda, 0, 1023);
        bb = constrain(scaledb, 0, 1023);
      } 
    }

    // sync == all
    float multiplier = 1.0;
    if (syncAll(i)) {
      aa = a[i-1];
      bb = b[i-1];
      waveshape = shape[i-1];
      if (syncAll(i-1)) {
        aa = a[i-2];
        bb = b[i-2];
        waveshape = shape[i-2];
      }
    }
    // sync == all or rate
    if (syncRate(i)) {
      multiplier = rateMultiplier(rr);  // special treatment of rate knob when sync is on
      if (syncRate(i-1)) {
        if (i == 2 & mod[1] == MOD_FREQ) {
          multiplier = rateMultiplier(vals[1]) * rateMultiplier(rr);  // handling of sync & mod_freq for exp 3
        } else {
          multiplier = rateMultiplier(r[i-1]) * rateMultiplier(rr);  // multiply the previous multiplier if both syncs are on
        }
        rr = r[i-2];
      } else {
        rr = r[i-1];
      }
    }

    mults[i] = multiplier;  // update multiplier

    // Serial.print(multiplier);
    // Serial.print(",");

    // unsigned long start = cyclestarts[i];
    long wavelength = calcWavelength(rr) * multiplier;

    if (tapduration != 0) { // tap tempo wavelength takeover
      if (
          (i == 0) ||
          (i == 1 && syncRate(1) ||
          (i == 2 && syncRate(1) && syncRate(2)))
       ) {
        wavelength = tapduration * multiplier;
      }
    }
    
    long prevwavelength = cycleends[i] - cyclestarts[i];
    if (prevwavelength == 0) { //startup fix
      prevwavelength = wavelength;
    } 

    float progress = calcProgress(start, end);
    // Serial.print(progress);
    // Serial.print(",");
    // Serial.print(start);
    // Serial.print(",");
    // Serial.print(end);
    // Serial.print(",");

    progress = progress - (int) progress; // strip to decimal portion only

    if (wavelength != prevwavelength) {
      start = now - wavelength * progress;
      end = start + wavelength;
    }

    // past the end of the wavelength - move cyclestart and reset randoms
    if (now >= end) {
      start = getAdjustedStart(start + wavelength, progress, wavelength, i);
      end = start + wavelength;

      regenerateRandoms(i, aa, bb);
    }

    cyclestarts[i] = start;
    cycleends[i] = end;

    // phase modulation
    // shift progress forward or backward according to val of exp to the left
    if (i > 0 && mod[i] == MOD_PHASE) {
      float phasefactor = mapf(vals[i-1], 0, 1023, -1.0, 1.0);
      progress = progress + phasefactor;
      if (progress > 1.0) {
        progress = progress - 1.0;
      }
      if (progress < 0.0) {
        progress = progress + 1.0;
      }
    }
    
    if (randoms[i]) {
      aa = ra[i];
      bb = rb[i];
      cc = rc[i];
    }

    if (waveshape == TRIANGLE) {
      vals[i] = triangleWave(progress, aa, bb, cc);
    }
    if (waveshape == SQUARE) {
      vals[i] = squareWave(progress, aa, bb, cc);
    }
    if (waveshape == SINE) {
      vals[i] = sineWave(progress, aa, bb, cc);
    }

  }

  // Serial.println("");
}

void regenerateRandoms(byte i, int aa, int bb) {
  int randomb = 0;
  int randomc = 0;
  if (aa > bb) {
    randomb = random(bb, aa);  // calculate new random values
    randomc = random(bb, aa);
  } else {
    randomb = random(aa, bb);  // calculate new random values
    randomc = random(aa, bb);
  }
  
  // move c value to a
  ra[i] = rc[i];

  if (randomc > randomb) {  // set random vals such that c > b
    rb[i] = randomb;
    rc[i] = randomc;
  } else {
    rb[i] = randomc;
    rc[i] = randomb;
  }
}

float calcProgress(unsigned long start, unsigned long end) {
  long wavelength = end - start;
  if (wavelength == 0 || now == start) {
    return 0.0;
  }

  if (now > start) {
    return ((now - start) % wavelength) / (wavelength * 1.0);
  } else {
    return 1.0 - ((start - now) % wavelength) / (wavelength * 1.0);
  }
}


unsigned long getAdjustedStart(unsigned long start, float progress, int wavelength, byte i) {
  // calculate a new start
  unsigned long newstart = start;

  if (!syncRate(i)) {
    return newstart;
  }

  // find previous exp's closest start to newstart
  unsigned long parentstart = cyclestarts[i-1];
  long parentwavelength = cycleends[i-1] - cyclestarts[i-1];
  float mult = mults[i];

  if (mult > 1.0) {
    // this exp has a bigger multiplier than the parent
    // determine how many parent wavelengths fit in this wavelength at the given progress
    // offset parent start by that many parent wavelengths
    long cyclesofar = wavelength * progress;
    long parentcyclecount = floor(cyclesofar / parentwavelength); 
    
    // offset will always be the same or earlier
    return parentstart - parentwavelength * parentcyclecount;

  } else if (mult == 1.0) {
    // this exp has the same multiplier as the parent - just use that value
    return parentstart;
  } else {
    // this exp has a smaller multiplier than the parent
    // determine how many wavelengths fit in the parent's wavelength at the parent's progress
    // offset parentstart by that many wavelengths
    float parentprogress = (now - parentstart) / (parentwavelength * 1.0);  // TODO now - start
    long parentcyclesofar = parentwavelength * parentprogress;
    long childcyclecount = floor(parentcyclesofar / wavelength);

    // offset will always be the same or later
    return parentstart + wavelength * childcyclecount;
  }  
}

// is sync set to all?
bool syncAll(byte i) {
  return (i > 0 && sync[i] == SYNC_ALL);
}

// is sync set to all or rate?
bool syncRate(byte i) {
  return (i > 0 && (sync[i] == SYNC_ALL || sync[i] == SYNC_RATE));
}

// sets digipot values
void turnDigiPots() {
  for (int i = 0; i < 3; i++) {
    long val = vals[i];

    // switch state overrides
    if (set[i] == 0 || exps[i] == 0) {
      val = a[i]; // set is in 'A' position -or- exp is off
    }
    if (set[i] == 1) {
      val = b[i]; // set is in 'B' position
    }

    // scale to digipot value
    setPotValue(i, val);
  }
}

void setPotValue(byte i, long val) {
  long outval = map(val, 0, 1023, 0, 255);  // note, if you don't map this to the max digipot value, you get some cool wavefolding effects.

  byte pin = potpins[i];
  digitalWrite(pin, LOW);
  shiftOut(DATA, SCLK, MSBFIRST, 0x11);
  shiftOut(DATA, SCLK, MSBFIRST, outval);
  digitalWrite(pin, HIGH);
}


void setLeds() {
  // set neopixel values
  for (int i = 0; i < 3; i++) {
    int red = map(vals[i], 0, 1023, 0, 255);
    int grn = map(vals[i], 0, 1023, 0, 255);
    int blu = map(vals[i], 0, 1023, 0, 255);

    int aa = map(a[i], 0, 1023, 0, 255);
    int bb = map(b[i], 0, 1023, 0, 255);

    if (set[i] == 0) {    // set is in 'a' position
      strip.setPixelColor(i, aa, 0, 0);
    } else if (set[i] == 1) { // set is in 'b' position
      strip.setPixelColor(i, 0, bb, 0);
    } else {
      if (exps[i] == 1) { // exp is on
        strip.setPixelColor(i, red, grn, blu);  // white
      } else {  // exp is off
        if (mod[i] != MOD_OFF) {  //modulating
          strip.setPixelColor(i, red, 0, blu);  // magenta
        } else if (sync[i] != SYNC_OFF) {       //syncing
          strip.setPixelColor(i, 0, grn, blu);  // cyan
        } else {
          strip.setPixelColor(i, red, grn, 0);  // yellow
        }
      }
    }

  }
  strip.show();
}

// middle of the knob should be 1bpm, so this is two lines of different slope
// ...basically a bad expo knob
long calcWavelength(int rate) {
  long wavelength = 0;
  if (rate >= 0 && rate <= 512) {
    wavelength = map(rate, 0L, 512L, RATEMIN, RATEMID);
  } else if (rate > 512 && rate <= 1023) {
    wavelength = map(rate, 512L, 1023L, RATEMID, RATEMAX);
  }

  if (wavelength > RATEMIN) {
    return RATEMIN;
  }
  if (wavelength < RATEMAX) {
    return RATEMAX;
  }
  return wavelength;
}

// progress = value between 0 and 1, aa = a value, bb = b value, cc = next a
float sineWave(float progress, int aa, int bb, int cc) {
  float cosrads = cos(progress * 2 * PI);

  // cos, as we want to go from a at 0 to b at 50% to c at 100%
  if (progress <= 0.5) {
    return mapf(cosrads, 1.0, -1.0, aa, bb);
  } else {
    return mapf(cosrads, 1.0, -1.0, cc, bb);
  }
}

// progress = value between 0 and 1, aa = a value, bb = b value, cc = next a
float triangleWave(float progress, int aa, int bb, int cc) {
  int output = 0;

  if (progress <= 0.5) {
    output = mapf(progress, 0.0, 0.5, aa, bb);
  } else {
    output = mapf(progress, 0.5, 1.0, bb, cc);
  }
  
  return output;
}

// progress = value between 0 and 1, aa = a value, bb = b value, cc = next a
float squareWave(float progress, int aa, int bb, int cc) {
  if (progress <= 0.5) {
    return aa * 1.0;
  }
  
  return bb * 1.0;
}


// multiply rate by 1/8, 1/4, 1/2, 1x, 2x, 3x, and 4x.
// rate: 0 - 1023 (analog knob value)
float rateMultiplier(int rate) {
  if (rate < 146) return 4.0;   // ~1/7 of 1023
  if (rate < 292) return 3.0;
  if (rate < 438) return 2.0;
  if (rate < 585) return 1.0;   
  if (rate < 731) return 0.5;
  if (rate < 877) return 0.25;
  return 0.125;
}

// map, but for floats
float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
