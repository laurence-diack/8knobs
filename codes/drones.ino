#include <MozziGuts.h>
#include <mozzi_utils.h>
#include <Oscil.h>
#include <tables/sin8192_int8.h>
#include <tables/cos8192_int8.h>
#include <tables/saw4096_int8.h>

#include <Mux.h>
#include <ReverbTank.h>

using namespace admux;

// ReverbTank reverb;

#define CONTROL_RATE 64
#define DENOISE 16

#define SENSOR_MIN 100
#define SENSOR_MAX 150

const int SMOOTHING_WINDOW = 10; // Size of the smoothing window
int lavalampReadings[SMOOTHING_WINDOW] = {0}; // Array to store previous readings
int readingIndex = 0; // Index to keep track of the current reading position

int16_t prevReadings[8] = {0, 0, 0, 0, 0, 0, 0, 0};

Mux mux(Pin(A0, INPUT, PinType::Analog), Pinset(7, 8, 9, 10));

// harmonics
Oscil<SIN8192_NUM_CELLS, AUDIO_RATE> aSin1(SIN8192_DATA);
Oscil<SIN8192_NUM_CELLS, AUDIO_RATE> aSin2(SIN8192_DATA);
Oscil<SAW4096_NUM_CELLS, AUDIO_RATE> aSin3(SAW4096_DATA);
Oscil<SAW4096_NUM_CELLS, AUDIO_RATE> aSin4(SAW4096_DATA);
Oscil<SAW4096_NUM_CELLS, AUDIO_RATE> aSin5(SAW4096_DATA);
Oscil<COS8192_NUM_CELLS, AUDIO_RATE> aSin6(COS8192_DATA);
Oscil<COS8192_NUM_CELLS, AUDIO_RATE> aSin7(COS8192_DATA);
Oscil<COS8192_NUM_CELLS, AUDIO_RATE> aSin8(COS8192_DATA);

// volume controls
Oscil<SAW4096_NUM_CELLS, CONTROL_RATE> kVol1(SAW4096_DATA);
Oscil<SIN8192_NUM_CELLS, CONTROL_RATE> kVol2(SIN8192_DATA);
Oscil<SIN8192_NUM_CELLS, CONTROL_RATE> kVol3(SIN8192_DATA);
Oscil<SIN8192_NUM_CELLS, CONTROL_RATE> kVol4(SIN8192_DATA);
Oscil<SAW4096_NUM_CELLS, CONTROL_RATE> kVol5(SAW4096_DATA);
Oscil<SAW4096_NUM_CELLS, CONTROL_RATE> kVol6(SAW4096_DATA);
Oscil<COS8192_NUM_CELLS, CONTROL_RATE> kVol7(COS8192_DATA);
Oscil<COS8192_NUM_CELLS, CONTROL_RATE> kVol8(COS8192_DATA);

// audio volumes updated each control interrupt and reused in audio till next control
char v1,v2,v3,v4,v5,v6,v7,v8;

void setup(){

  // set harmonic frequencies - change these?
  aSin1.setFreq(100);
  aSin2.setFreq(100);
  aSin3.setFreq(100);
  aSin4.setFreq(100);
  aSin5.setFreq(100);
  aSin6.setFreq(100);
  aSin7.setFreq(100);
  aSin8.setFreq(100);

// set volume change frequencies - change these?
  kVol1.setFreq(0.43f); // more of a pulse // change for pulsating while mute
  kVol2.setFreq(0.0245f);
  kVol3.setFreq(0.019f);
  kVol4.setFreq(0.07f);
  kVol5.setFreq(0.047f);
  kVol6.setFreq(0.031f);
  kVol7.setFreq(0.0717f);
  kVol8.setFreq(0.041f);

  v1=v2=v3=v4=v5=v6=v7=v8=127;

  Serial.begin();

  setupFastAnalogRead();
  
  startMozzi(CONTROL_RATE);
}

void loop(){
  audioHook();
}

int16_t readMuxPin(int pin) {
  mux.channel(pin);
  return analogRead(A0);
}

void updateControl() {
  for (int i = 0; i < 8; i++) {
    int16_t currentReading = readMuxPin(i);
    // Check if the read value has changed more than the DENOISE threshold
    if (abs(currentReading - prevReadings[i]) > DENOISE) {
      prevReadings[i] = currentReading; // Update the previous reading
      // Print the pin number and its new value
      Serial.print("Pin ");
      Serial.print(i);
      Serial.print(" changed to ");
      Serial.println(currentReading);

      // Update the frequency based on the current reading
      switch (i) {
        case 0: aSin1.setFreq(currentReading * 2); break;
        case 1: aSin2.setFreq(currentReading * 2); break;
        case 2: aSin3.setFreq(currentReading * 2); break;
        case 3: aSin4.setFreq(currentReading * 2); break;
        case 4: aSin5.setFreq(currentReading * 2); break;
        case 5: aSin6.setFreq(currentReading * 2); break;
        case 6: aSin7.setFreq(currentReading * 2); break;
        // case 7: aSin8.setFreq(currentReading * 2); break;
      }
    }
  }

  // This stuff handles the lava lamp
  int lavalamp = analogRead(A1);
  // Add the new reading to the array and increment the index
  lavalampReadings[readingIndex] = lavalamp;
  readingIndex = (readingIndex + 1) % SMOOTHING_WINDOW;

  // Calculate the average of the readings in the smoothing window
  int sum = 0;
  for (int i = 0; i < SMOOTHING_WINDOW; i++) {
    sum += lavalampReadings[i];
  }
  int smoothedLavalamp = sum / SMOOTHING_WINDOW;

  // Now apply constrainment and mapping to the smoothed value
  int constrainedReading = constrain(smoothedLavalamp, SENSOR_MIN, SENSOR_MAX);
  int mappedReading = map(constrainedReading, SENSOR_MIN, SENSOR_MAX, 0, 1023);

  Serial.print("Smoothed Lavalamp changed to ");
  Serial.println(mappedReading);
  // Update the frequency like with the others ^
  aSin8.setFreq(mappedReading * 2);

  // Update volume values
  v1 = kVol1.next();
  v2 = kVol2.next();
  v3 = kVol3.next();
  v4 = kVol4.next();
  v5 = kVol5.next();
  v6 = kVol6.next();
  v7 = kVol7.next();
  v8 = kVol8.next();
}

int updateAudio(){
  long asig = (long)
    aSin1.next()*v1 +
    aSin2.next()*v2 +
    aSin3.next()*v3 +
    aSin4.next()*v4 +
    aSin5.next()*v5 +
    aSin6.next()*v6 +
    aSin7.next()*v7 +
    aSin8.next()*v8;
  asig >>= 9;

  // int arev = reverb.next(asig);
  // return MonoOutput::fromAlmostNBit(9, arev + (arev>>3));
  return (int) asig;
}