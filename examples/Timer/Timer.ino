// Timer example for GEVINO Motor
// Non-blocking software timer (class Timer) bundled in gevino_motor.h
//
// Timer behaviour:
//   - reset()      restarts the count; while it keeps being reset() it never fires.
//   - stato()      returns true once the configured time has elapsed since reset().
//   - fronte()     returns true only once (rising edge) when the time elapses.
//   - tempo(ms)    sets the period.
//
// This sketch blinks Out1 every 500 ms and the yellow front LED every 1000 ms
// WITHOUT delay(), and mirrors input In1 onto Out2. This is the recommended
// non-blocking, state-based programming style.
//
// NOTE: gevino_motor.h references the microstep sine table (dataword_rom), so a
// Sinewave*.h table must be included BEFORE the library header, even when you are
// not driving the motor in this sketch.

#include "Sinewave16.h"
#include "gevino_motor.h"

Timer blinkOut(500);   // 500 ms period
Timer blinkLed(1000);  // 1000 ms period
bool outOn = false;
bool ledOn = false;

void setup() {
  SerialUSB.begin(115200);  // USB-C debug console
  gevino_motor_setup();     // configures all the board I/O
  blinkOut.reset();
  blinkLed.reset();
}

void loop() {
  if (blinkOut.stato()) {              // 500 ms elapsed
    blinkOut.reset();
    outOn = !outOn;
    if (outOn) setOut1; else resOut1;  // toggle Out1
  }

  if (blinkLed.stato()) {              // 1000 ms elapsed
    blinkLed.reset();
    ledOn = !ledOn;
    if (ledOn) setLed; else resLed;    // toggle yellow front LED
  }

  if (In1) setOut2; else resOut2;      // mirror input In1 onto Out2

  // ... add other non-blocking tasks here; never use delay() in loop().
}
