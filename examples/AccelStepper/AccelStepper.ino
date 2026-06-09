// GEVINO Motor v1.0  using AccelStepper Library
// https://www.airspayce.com/mikem/arduino/AccelStepper/
// 29/08/2020   Giorgio Evangelista 
#include "Sinewave16.h"  // Change to 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 microsteps
#include "gevino_motor.h"
#include <AccelStepper.h>

AccelStepper stepper(MicrostepUp, MicrostepDw);

//#define ConstantSpeed
#define Positioning

void setup()
{  
  SerialUSB.begin(115200);
  SerialUSB.println("Stepper test!");

  gevino_motor_setup();

  ISEq.AGC = 1;       // AGC Enable
  ISEq.CLIM = 0;      // AGC CURRENT LIMITER 45%/50%/55%/60%/65%/70%/75%/80%
  ISEq.FLIM = 0;      // AGC Frequency limit function off/225 Hz/450 Hz/675 Hz
  ISEq.BST = 0;       // Current Boost Function 5/7/9/11 steps
  ISEq.RS_SEL = 0;    // ACDS (RS resistor less) / External sense RS resistor
  ISEq.GAIN_SEL = 0;  // Vref(gain) to 1/5 / 1/10
  MotorInit(); 
  Param.Torque = 6;  // 0=100% 1=85% 2=70% 3= 60% 4=50% 5=40% 6=10%
  Param.MDTA = 3;      // 0=Fast Decay: 50% (Fast Decay = OSCM × 8) 1=Fast Decay: 37.5% (Fast Decay = OSCM × 6) 2=Fast Decay Only 3=ADMD
  Param.MDTB = 3;      // 0=Fast Decay: 50% (Fast Decay = OSCM × 8) 1=Fast Decay: 37.5% (Fast Decay = OSCM × 6) 2=Fast Decay Only 3=ADMD  
  resSTANDBY;
  Microstep();  // Set SPI parameter

#ifdef ConstantSpeed
  stepper.setMaxSpeed(100000);  
  stepper.setSpeed(100000);
#endif

#ifdef Positioning
  stepper.setMaxSpeed(10000);	
  stepper.setAcceleration(500);
  stepper.moveTo(1000);
#endif
}

void loop()
{  
#ifdef ConstantSpeed
  stepper.runSpeed();
#endif
   
#ifdef Positioning
   stepper.run();
   if (stepper.distanceToGo() == 0) stepper.moveTo(-stepper.currentPosition());
#endif
   
}
