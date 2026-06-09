#ifndef _GEVINO_MOTOR_H_
#define _GEVINO_MOTOR_H_

/*
  GEVINO Motor - single-header support library
  GEVA Elettronica - https://www.gevaelettronica.it

  Motor-control PLC based on the Microchip SAMD21 (ARM Cortex-M0+), Arduino Zero
  compatible, with a Toshiba TB67S128FTG stepper/brushed-DC motor driver.
  This single header bundles everything the board needs:

    1. Low-level SAMD21 ADC helpers        (originally ATSAMD21_ADC.h, Blake Felt)
    2. TB67S128FTG stepper driver pins      (GEVA Elettronica)
    3. Pin definitions and I/O macros       (GEVA Elettronica)
    4. Board setup and self-test routines   (GEVA Elettronica)
    5. Motor control (init, microstepping, brushed DC)
    6. Non-blocking analog polling (inputs + motor current sensing)
    7. A lightweight Timer class            (GEVA Elettronica)

  Usage (the microstep sine table MUST be included first):
    #include "Sinewave128.h"   // pick 1,2,4,8,16,32,64,128,256,512,1024 microsteps
    #include "gevino_motor.h"

  IMPORTANT:
    - This header defines global objects (ISEq, Param, analogResult, ...) and
      functions, so include it in exactly ONE translation unit (your main .ino).
    - It references dataword_rom / SinwaveTableSize, which are provided by the
      Sinewave*.h table you include BEFORE this header.
    - Do NOT initialise the I/O yourself: gevino_motor_setup() does it for you.

  Changelog
    1.0          Added non-blocking analog reading.
    22/11/2022   More output current on some pins (DRVSTR).
    08/06/2026   Bundled ATSAMD21_ADC.h and a Timer class into this single header;
                 renamed gevino_motore_setup() -> gevino_motor_setup().
*/

#include <Arduino.h>
#include <SPI.h>

// =============================================================================
// 1. Low-level SAMD21 ADC helpers
//    Originally ATSAMD21_ADC.h by Blake Felt. Generic SAMD21 ADC utilities
//    (8..16 bit, gain, reference, calibration, differential mode), marked
//    `inline` so the header stays safe to include.
// =============================================================================

#ifdef __cplusplus
  extern "C" {
#endif

#ifndef ADC_CTRLB_RESSEL_12BIT_Val
#define ADC_CTRLB_RESSEL_8BIT_Val   0x03
#define ADC_CTRLB_RESSEL_10BIT_Val  0x02 // default by Arduino
#define ADC_CTRLB_RESSEL_12BIT_Val  0x00
#endif
#define ADC_CTRLB_RESSEL_16BIT_Val  0x01 // used for averaging mode output

#define ADC_PIN_TEMP                0x18 // positive mux, pg 870
#define ADC_PIN_BANDGAP             0x19
#define ADC_PIN_SCALEDCOREVCC       0x1A
#define ADC_PIN_SCALEDIOVCC         0x1B
#define ADC_PIN_DAC                 0x1C

#define ADC_PIN_GND                 0x18 // negative mux, pg 869
#define ADC_PIN_IOGND               0x19

#define ADC_GAIN_1                  0x00 // pg 868
#define ADC_GAIN_2                  0x01
#define ADC_GAIN_4                  0x02
#define ADC_GAIN_8                  0x03
#define ADC_GAIN_16                 0x04
#define ADC_GAIN1_DIV2              0x0F // default by Arduino

#define ADC_REF_INT1V               0x00 // 1.0V reference, pg 861
#define ADC_REF_INTVCC0             0x01 // 1/1.48 VDDANA
#define ADC_REF_INTVCC1             0x02 // 1/2 VDDANA (only for VDDANA > 2.0V) // default
#define ADC_REF_VREFA               0x03 // external reference
#define ADC_REF_VREFB               0x04 // external reference

#define ADC_PRESCALER_DIV4          0x00 // pg 864
#define ADC_PRESCALER_DIV8          0x01
#define ADC_PRESCALER_DIV16         0x02
#define ADC_PRESCALER_DIV32         0x03
#define ADC_PRESCALER_DIV64         0x04
#define ADC_PRESCALER_DIV128        0x05
#define ADC_PRESCALER_DIV256        0x06
#define ADC_PRESCALER_DIV512        0x07 // Arduino default

// NVM Software Calibration Area Mapping, pg 32. Address starting at NVMCTRL_OTP4.
#define NVM_ADC_LINEARITY_POS         27
#define NVM_ADC_LINEARITY_SIZE         8
#define NVM_ADC_BIASCAL_POS           35
#define NVM_ADC_BIASCAL_SIZE           3

// Wait for synchronization of registers between the clock domains
static __inline__ void syncADC() __attribute__((always_inline, unused));
static void syncADC() {
  while(ADC->STATUS.bit.SYNCBUSY == 1);
}
static __inline__ void syncDAC() __attribute__((always_inline, unused));
static void syncDAC() {
  while (DAC->STATUS.bit.SYNCBUSY == 1);
}

// taken from Arduino IDE, changes the pin to an input:
int pinPeripheral( uint32_t ulPin, EPioType ulPeripheral );

// Allows the ADC to read 8/10/12 bits normally or 13..16 bits via oversampling.
inline uint8_t analogReadExtended(uint8_t bits) {
  switch(bits) {
    case 8:
      ADC->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_8BIT_Val;
      ADC->AVGCTRL.bit.ADJRES = 0x0;
      ADC->AVGCTRL.bit.SAMPLENUM = 0x0;
      return 0;
    case 10:
      ADC->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_10BIT_Val;
      ADC->AVGCTRL.bit.ADJRES = 0x0;
      ADC->AVGCTRL.bit.SAMPLENUM = 0x0;
      return 0;
    case 12:
      ADC->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_12BIT_Val;
      ADC->AVGCTRL.bit.ADJRES = 0x0;
      ADC->AVGCTRL.bit.SAMPLENUM = 0x0;
      return 0;
    case 13:
      ADC->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_16BIT_Val;
      ADC->AVGCTRL.bit.ADJRES = 0x1;
      ADC->AVGCTRL.bit.SAMPLENUM = 0x2;
      return 0;
    case 14:
      ADC->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_16BIT_Val;
      ADC->AVGCTRL.bit.ADJRES = 0x2;
      ADC->AVGCTRL.bit.SAMPLENUM = 0x4;
      return 0;
    case 15:
      ADC->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_16BIT_Val;
      ADC->AVGCTRL.bit.ADJRES = 0x1;
      ADC->AVGCTRL.bit.SAMPLENUM = 0x6;
      return 0;
    case 16:
      ADC->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_16BIT_Val;
      ADC->AVGCTRL.bit.ADJRES = 0x0;
      ADC->AVGCTRL.bit.SAMPLENUM = 0x8;
      return 0;
    default:
      return -1;
  }
}

// returns the internal ADC channel of the specified Arduino pin
inline uint8_t internalPinValue(uint8_t pin) {
  return g_APinDescription[pin].ulADCChannelNumber;
}

// reads the difference between two pins (single conversion, blocking)
inline int16_t analogDifferential(uint8_t pin_pos, uint8_t pin_neg) {
  if(pin_pos<A0) pin_pos += A0;
  if(pin_neg<A0) pin_neg += A0;

  if((g_APinDescription[pin_neg].ulADCChannelNumber>0x07) && (pin_neg<ADC_PIN_GND)) {
    return 0;
  }

  uint32_t value_read = 0;

  pinPeripheral(pin_pos, PIO_ANALOG);
  pinPeripheral(pin_neg, PIO_ANALOG);

  if((pin_pos == A0) || (pin_neg == A0)) { // Disable DAC
    syncDAC();
    DAC->CTRLA.bit.ENABLE = 0x00;
    syncDAC();
  }

  syncADC();
  ADC->INPUTCTRL.bit.MUXPOS = g_APinDescription[pin_pos].ulADCChannelNumber;
  ADC->INPUTCTRL.bit.MUXNEG = g_APinDescription[pin_neg].ulADCChannelNumber;

  syncADC();
  ADC->CTRLA.bit.ENABLE = 0x01;
  ADC->CTRLB.bit.DIFFMODE = 1;

  syncADC();
  ADC->SWTRIG.bit.START = 1;

  ADC->INTFLAG.reg = ADC_INTFLAG_RESRDY;
  syncADC();

  ADC->SWTRIG.bit.START = 1; // restart, as changing inputs messes up first conversion

  while(ADC->INTFLAG.bit.RESRDY == 0);
  value_read = ADC->RESULT.reg;

  syncADC();
  ADC->CTRLA.bit.ENABLE = 0x00;
  ADC->CTRLB.bit.DIFFMODE = 0;
  ADC->INPUTCTRL.bit.MUXNEG = ADC_PIN_GND;
  syncADC();

  return value_read;
}

// raw differential read: no error checking, no pin reconfiguration, no DAC handling
inline int16_t analogDifferentialRaw(uint8_t mux_pos, uint8_t mux_neg) {
  uint32_t value_read = 0;

  syncADC();
  ADC->INPUTCTRL.bit.MUXPOS = mux_pos;
  ADC->INPUTCTRL.bit.MUXNEG = mux_neg;

  syncADC();
  ADC->CTRLA.bit.ENABLE = 0x01;
  ADC->CTRLB.bit.DIFFMODE = 1;

  syncADC();
  ADC->SWTRIG.bit.START = 1;

  ADC->INTFLAG.reg = ADC_INTFLAG_RESRDY;
  syncADC();

  ADC->SWTRIG.bit.START = 1;

  while(ADC->INTFLAG.bit.RESRDY == 0);
  value_read = ADC->RESULT.reg;

  syncADC();
  ADC->CTRLA.bit.ENABLE = 0x00;
  ADC->CTRLB.bit.DIFFMODE = 0;
  ADC->INPUTCTRL.bit.MUXNEG = ADC_PIN_GND;
  syncADC();

  return value_read;
}

// sets the gain of the ADC (pg 868)
inline void analogGain(uint8_t gain) {
  syncADC();
  ADC->INPUTCTRL.bit.GAIN = gain;
  syncADC();
}

// calibrates bias and linearity from the NVM software calibration area (pg 32, 882)
inline void analogCalibrate() {
  syncADC();
  uint32_t adc_linearity = (*((uint32_t *)(NVMCTRL_OTP4)
          + (NVM_ADC_LINEARITY_POS / 32))
          >> (NVM_ADC_LINEARITY_POS % 32))
          & ((1 << NVM_ADC_LINEARITY_SIZE) - 1);

  uint32_t adc_biascal = (*((uint32_t *)(NVMCTRL_OTP4)
          + (NVM_ADC_BIASCAL_POS / 32))
          >> (NVM_ADC_BIASCAL_POS % 32))
          & ((1 << NVM_ADC_BIASCAL_SIZE) - 1);

  ADC->CALIB.bit.LINEARITY_CAL = adc_linearity;
  ADC->CALIB.bit.BIAS_CAL = adc_biascal;
  syncADC();
}

// sets the analog reference voltage with all available options (pg 861)
inline void analogReference2(uint8_t ref) {
  syncADC();
  ADC->REFCTRL.bit.REFSEL = ref;
  syncADC();
}

// enables reference buffer offset compensation (pg 861)
inline void analogReferenceCompensation(uint8_t val) {
  if(val>0) val = 1;
  syncADC();
  ADC->REFCTRL.bit.REFCOMP = val;
  syncADC();
}

// sets the ADC clock prescaler relative to the peripheral clock (pg 864)
inline void analogPrescaler(uint8_t val) {
  syncADC();
  ADC->CTRLB.bit.PRESCALER = val;
  syncADC();
}

// resets the ADC (pg 860)
inline void analogReset() {
  syncADC();
  ADC->CTRLA.bit.SWRST = 1;
  while(ADC->CTRLA.bit.SWRST==1);
  syncADC();
}

#ifdef __cplusplus
  }
#endif

// =============================================================================
// 2. Toshiba TB67S128FTG stepper/brushed-DC driver (SPI)
// =============================================================================
#define spiClk 24000000                     // 24 MHz SPI clock
#define LATCH 6
#define BANK_EN 45
#define STANDBY 42
#define setLATCH digitalWrite(LATCH, HIGH)
#define resLATCH digitalWrite(LATCH, LOW)
#define setBANK_EN digitalWrite(BANK_EN, HIGH)
#define resBANK_EN digitalWrite(BANK_EN, LOW)
#define setSTANDBY digitalWrite(STANDBY, LOW)
#define resSTANDBY digitalWrite(STANDBY, HIGH)

// =============================================================================
// 3. Pin definitions and I/O macros (GEVA Elettronica)
//    5 PNP inputs, 4 protected PNP outputs, 3 analog inputs, RS485 transmit
//    enable, and the chip-selects for the SD card and optional W5500 Ethernet.
// =============================================================================
#define In1_name  27
#define In2_name  9
#define In3_name  8
#define In4_name  14  //  A0
#define In5_name  38
#define An1_name  A1
#define An2_name  A2
#define An3_name  A3
#define SD_CS 12
#define ETH_RES 11
#define ETH_CS 10

#define In1  digitalRead(In1_name)
#define In2  digitalRead(In2_name)
#define In3  digitalRead(In3_name)
#define In4  digitalRead(In4_name)
#define In5  digitalRead(In5_name)
#define Out1_name 2
#define Out2_name 3
#define Out3_name 4
#define Out4_name 5
#define Out1 digitalRead(Out1_name)
#define Out2 digitalRead(Out2_name)
#define Out3 digitalRead(Out3_name)
#define Out4 digitalRead(Out4_name)
#define setOut1 digitalWrite(Out1_name, HIGH)
#define resOut1 digitalWrite(Out1_name, LOW)
#define setOut2 digitalWrite(Out2_name, HIGH)
#define resOut2 digitalWrite(Out2_name, LOW)
#define setOut3 digitalWrite(Out3_name, HIGH)
#define resOut3 digitalWrite(Out3_name, LOW)
#define setOut4 digitalWrite(Out4_name, HIGH)
#define resOut4 digitalWrite(Out4_name, LOW)
#define setLed  digitalWrite(LED_BUILTIN, HIGH) // Yellow front Led
#define resLed  digitalWrite(LED_BUILTIN, LOW)  // Yellow front Led
#define Rs485_TxEn  7
#define set_Rs485_TxEn  digitalWrite(Rs485_TxEn, HIGH)
#define res_Rs485_TxEn  digitalWrite(Rs485_TxEn, LOW)

// =============================================================================
// 4. Board setup (GEVA Elettronica)
// =============================================================================
void gevino_motor_setup( void ){

  PORT->Group[PORTB].PINCFG[10].bit.DRVSTR = 1;  // More current on output pin MOSI
  PORT->Group[PORTB].PINCFG[11].bit.DRVSTR = 1;  // More current on output pin SCK
  PORT->Group[PORTA].PINCFG[19].bit.DRVSTR = 1;  // More current on output pin D12 cs-SD
  PORT->Group[PORTA].PINCFG[18].bit.DRVSTR = 1;  // More current on output pin D10 cs-Eth
  PORT->Group[PORTA].PINCFG[10].bit.DRVSTR = 1;  // More current on output pin Serial1-Tx
  PORT->Group[PORTB].PINCFG[37].bit.DRVSTR = 1;  // More current on output pin Serial-Tx
  PORT->Group[PORTA].PINCFG[22].bit.DRVSTR = 1;  // More current on output pin SDA
  PORT->Group[PORTA].PINCFG[23].bit.DRVSTR = 1;  // More current on output pin SCL
  PORT->Group[PORTA].PINCFG[3].bit.DRVSTR = 1;   // More current on output pin D42
  PORT->Group[PORTA].PINCFG[20].bit.DRVSTR = 1;  // More current on output pin D6
  PORT->Group[PORTA].PINCFG[31].bit.DRVSTR = 1;  // More current on output pin D45

// Define Output
  pinMode(Out1_name, OUTPUT);       // Set Out1_name digital pin as Output
  pinMode(Out2_name, OUTPUT);
  pinMode(Out3_name, OUTPUT);
  pinMode(Out4_name, OUTPUT);
  resOut1;
  resOut2;
  resOut3;
  resOut4;
  pinMode(ETH_RES, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT); // Yellow front Led

// Define Input
  pinMode(In1_name, INPUT);
  pinMode(In2_name, INPUT);
  pinMode(In3_name, INPUT);
  pinMode(In4_name, INPUT);
  pinMode(In5_name, INPUT);
  pinMode(Rs485_TxEn, OUTPUT);
  res_Rs485_TxEn;

  pinMode(An1_name, INPUT);
  pinMode(An2_name, INPUT);
  pinMode(An3_name, INPUT);


  pinMode(LATCH, OUTPUT);
  pinMode(BANK_EN, OUTPUT);
  pinMode(STANDBY, OUTPUT);
  resLATCH;
  resBANK_EN;
  resSTANDBY;

  digitalWrite(ETH_RES, LOW);
  delay(300);
  digitalWrite(ETH_RES, HIGH);
  delay(300);
}

// =============================================================================
// 5. Motor control (TB67S128FTG)
//    ISEq  : initialization sequence register (AGC, current limit, etc.)
//    Param : per-step parameters (torque, decay mode, direction, current)
// =============================================================================
struct {
  uint32_t no1:2;
  uint32_t AGC:1;  	// AGC Enable
  uint32_t CLIM:3; 	// AGC CURRENT LIMITER 0=45% 1=50% 2=55% 3=60% 4=65% 5=70% 6=75% 7=80%
  uint32_t FLIM:2; 	// AGC Frequency limit function 0=off 1=225 Hz 2=450 Hz 3=675 Hz
  uint32_t BST:2;  	// Current Boost Function 0=5 1=7 2=9 3=11 steps
  uint32_t no2:2;
  uint32_t RS_SEL:1;	// 0=ACDS (RS resistor less) 1=External sense RS resistor
  uint32_t GAIN_SEL:1;// Vref(gain) 0=1/5 1=1/10
  uint32_t no3:18;
} ISEq;

struct {
  uint32_t no1:2;
  uint32_t Torque:3;  // 0=100% 1=85% 2=70% 3= 60% 4=50% 5=40% 6=10%
  uint32_t no2:1;
  uint32_t MDTA:2;    // 0=Fast Decay: 50% (Fast Decay = OSCM × 8) 1=Fast Decay: 37.5% (Fast Decay = OSCM × 6) 2=Fast Decay Only 3=ADMD
  uint32_t Mot1dir:1; // 0/1
  uint32_t Mot1:10;   // 0 to 1023
  uint32_t MDTB:2;    // 0=Fast Decay: 50% (Fast Decay = OSCM × 8) 1=Fast Decay: 37.5% (Fast Decay = OSCM × 6) 2=Fast Decay Only 3=ADMD
  uint32_t Mot2dir:1; // 0/1
  uint32_t Mot2:10;   // 0 to 1023
} Param = {0, 0, 0, 0, 0, 0, 0, 0, 0};

void MotorInit(){         // Send the 32-bit initialization sequence via the manual SPI interface
  SPI.begin ();                                                           // Initialize SPI (only for arduino boards)
  resBANK_EN;                                                             // Set BANK_EN "LOW" to make initial settings
  SPI.beginTransaction(SPISettings(spiClk, LSBFIRST, SPI_MODE1));         // initialize the SPI transfer
  SPI.transfer(((uint8_t*)&ISEq)[0]);                                       // Send the initialisation sequence
  SPI.transfer(((uint8_t*)&ISEq)[1]);
  SPI.transfer(((uint8_t*)&ISEq)[2]);
  SPI.transfer(((uint8_t*)&ISEq)[3]);
  SPI.endTransaction();
  setLATCH;                                                               // set LATCH to "HIGH" to advice the S128 to perfrom the initialisation with given parameter
  delayMicroseconds(1);                                                   // Delay 1 us
  resLATCH;                                                               // set LATCH to "LOW" to advice the S128 to finish the initialisation
  delayMicroseconds(1);
  setBANK_EN;
}

void SendBrushed(void){            											// Send struct to motor controller
  SPI.beginTransaction(SPISettings(spiClk, LSBFIRST, SPI_MODE1));           // initialize the SPI transfer
  SPI.transfer (((uint8_t*)&Param)[0]);
  SPI.transfer (((uint8_t*)&Param)[1]);
  SPI.transfer (((uint8_t*)&Param)[2]);
  SPI.transfer (((uint8_t*)&Param)[3]);
  setLATCH;                                                 				// set LATCH to "HIGH" to advice the S128 to perfrom the step with given parameter
  resLATCH;                                                 				// set LATCH to "LOW" to advice the S128 to finish the step #
  SPI.endTransaction();                                                     // end the SPI transfer
}

uint8_t  *End_point = (uint8_t*)dataword_rom + SinwaveTableSize*3*4;
uint8_t  *point = (uint8_t*)dataword_rom;
void MicrostepUp( void ){
  if( point >= End_point ) point = (uint8_t*)dataword_rom;
//  SPI.beginTransaction(SPISettings(spiClk, LSBFIRST, SPI_MODE1));           // initialize the SPI transfer
  SPI.transfer (((uint8_t*)&Param)[0]);
  SPI.transfer (*(point++));
  SPI.transfer (*(point++) | ((uint8_t*)&Param)[2]);	// MTDB
  SPI.transfer (*(point++));
  setLATCH;
  resLATCH;
//  SPI.endTransaction();                                                      // end the SPI transfer
}
void MicrostepDw( void ){
  point -= 6;
  if( point < (uint8_t*)dataword_rom ) point = End_point-3;
//  SPI.beginTransaction(SPISettings(spiClk, LSBFIRST, SPI_MODE1));           // initialize the SPI transfer
  SPI.transfer (((uint8_t*)&Param)[0]);
  SPI.transfer (*(point++));
  SPI.transfer (*(point++) | ((uint8_t*)&Param)[2]);	// MTDB
  SPI.transfer (*(point++));
  setLATCH;
  resLATCH;
//  SPI.endTransaction();                                                      // end the SPI transfer
}
void Microstep( void ){														// Prepare SPI if used from other device or first time
  SPI.beginTransaction(SPISettings(spiClk, LSBFIRST, SPI_MODE1));           // initialize the SPI transfer                                               // end the SPI transfer
}
void MicrostepRefresh( void ){    // Send Param
  SPI.transfer (((uint8_t*)&Param)[0]);
  SPI.transfer (*(point++));
  SPI.transfer (*(point++) | ((uint8_t*)&Param)[2]);	// MTDB
  SPI.transfer (*(point++));
  setLATCH;
  resLATCH;
}

void Microsteps(long msteps, int dir, int count){            // Perform the microsteps using SPI and control-sequence array values
  int l;

  if(count>=16384) count=16383;
  SPI.beginTransaction(SPISettings(spiClk, LSBFIRST, SPI_MODE1));           // initialize the SPI transfer
  if(dir==0){                                                               // perform move for dir "0"
    for (l=0;l<=msteps-1;l++){                                              // initilize loop for required steps
      MicrostepUp();
      delayMicroseconds(count);                                             // Wait the Step delay time in us max 16383 us !
  }                                                                         // repeat until required steps are done
  SPI.endTransaction();                                                     // end the SPI transfer
}else{                                                                      // perform move for dir "0"
 for (l=msteps-1;l>=0;l--){                                                 // initilize loop for required steps now in the oposite direction (using "--")
   MicrostepDw();
   delayMicroseconds(count);                                                // Wait the Step delay time in us
  }                                                                         // repeat until required steps are done
 SPI.endTransaction();                                                      // end the SPI transfer
 }
}

// =============================================================================
// 6. Self-test (GEVA Elettronica)
// =============================================================================
void testLeds( void ){       // Turns on most of the LEDs / outputs for photos and bench testing
// Define Output
  pinMode(Out1_name, OUTPUT);       // Set Out1_name digital pin as Output
  pinMode(Out2_name, OUTPUT);
  pinMode(Out3_name, OUTPUT);
  pinMode(Out4_name, OUTPUT);

  pinMode(26, OUTPUT); // Tx Led
  digitalWrite(26, LOW);
  delay(200);
  digitalWrite(26, HIGH);

  pinMode(25, OUTPUT); // Rx Led
  digitalWrite(25, LOW);
  delay(200);
  digitalWrite(25, HIGH);

  pinMode(LED_BUILTIN, OUTPUT); // Led
  digitalWrite(LED_BUILTIN, HIGH);
  delay(200);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(In1_name, OUTPUT); // In01
  digitalWrite(In1_name, HIGH);
  delay(200);
  digitalWrite(In1_name, LOW);

  pinMode(In2_name, OUTPUT); // In02
  digitalWrite(In2_name, HIGH);
  delay(200);
  digitalWrite(In2_name, LOW);

  pinMode(In3_name, OUTPUT);  // In03
  digitalWrite(In3_name, HIGH);
  delay(200);
  digitalWrite(In3_name, LOW);

  pinMode(In4_name, OUTPUT);  // In04
  digitalWrite(In4_name, HIGH);
  delay(200);
  digitalWrite(In4_name, LOW);

  pinMode(In5_name, OUTPUT);  // In05
  digitalWrite(In5_name, HIGH);
  delay(200);
  digitalWrite(In5_name, LOW);


// RS485
  pinMode(Rs485_TxEn, OUTPUT);       // Set Tx Enable
  digitalWrite(Rs485_TxEn, HIGH);
  pinMode(1, OUTPUT);        // Set Tx
  digitalWrite(1, HIGH);
  pinMode(0, OUTPUT);        // Set Rx
  digitalWrite(0, HIGH);

// RS485 Tx
  delay(200);
  digitalWrite(1, LOW);
  delay(200);
  digitalWrite(1, HIGH);

// RS485 Rx
  digitalWrite(0, LOW);
  delay(200);
  digitalWrite(0, HIGH);

  pinMode(Out1_name, OUTPUT);       // Set Out1_name digital pin as Output
  setOut1;
  delay(200);
  resOut1;

  pinMode(Out2_name, OUTPUT);
  setOut2;
  delay(200);
  resOut2;

  pinMode(Out3_name, OUTPUT);
  setOut3;
  delay(200);
  resOut3;

  pinMode(Out4_name, OUTPUT);
  setOut4;
  delay(200);
  resOut4;

  delay(200);
// All On
  digitalWrite(1, LOW);
  digitalWrite(0, LOW);
  digitalWrite(26, LOW); // Tx Led
  digitalWrite(25, LOW); // Rx Led
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(In1_name, HIGH);
  digitalWrite(In2_name, HIGH);
  digitalWrite(In3_name, HIGH);
  digitalWrite(In4_name, HIGH);
  digitalWrite(In5_name, HIGH);

  setOut1;
  setOut2;
  setOut3;
  setOut4;

  delay(1000);
// All off
  digitalWrite(1, HIGH);
  digitalWrite(0, HIGH);
  digitalWrite(26, HIGH);  // Tx
  digitalWrite(25, HIGH);  // Rx
  digitalWrite(LED_BUILTIN, LOW);  // Led
  digitalWrite(In1_name, LOW);
  digitalWrite(In2_name, LOW);
  digitalWrite(In3_name, LOW);
  digitalWrite(In4_name, LOW);
  digitalWrite(In5_name, LOW);
  resOut1;
  resOut2;
  resOut3;
  resOut4;

// Restore the inputs as inputs after the test
  pinMode(In1_name, INPUT);
  pinMode(In2_name, INPUT);
  pinMode(In3_name, INPUT);
  pinMode(In4_name, INPUT);
  pinMode(In5_name, INPUT);
}


// =============================================================================
// 7. Non-blocking analog polling (GEVA Elettronica)
//    The standard Arduino analogRead() freezes the program for several
//    milliseconds. analogPolling() cycles through the channels without blocking
//    and stores the latest conversions in analogResult[].
// =============================================================================
uint32_t analogResult[6];

void analogPolling(void){
// analogResult[1] = Ana1
// analogResult[2] = Ana2
// analogResult[3] = Ana3
// analogResult[4] = Current Mot A
// analogResult[5] = Current Mot B
  static byte pin = 2;


  if( ADC->INTFLAG.bit.RESRDY ){				// if conversion is done
    ADC->INTFLAG.reg = ADC_INTFLAG_RESRDY;
    switch(pin){
	  case 2:
	    analogResult[1] = ADC->RESULT.reg;	// save conversion value
	    pin = 3;
        ADC->INPUTCTRL.bit.MUXPOS = pin; // Selection new conversion pin
        ADC->SWTRIG.bit.START = 1;            // start new conversion
	    break;
	  case 3:
	    analogResult[2] = ADC->RESULT.reg;	// save conversion value
	    pin = 4;
        ADC->INPUTCTRL.bit.MUXPOS = pin; // Selection new conversion pin
        ADC->SWTRIG.bit.START = 1;            // start new conversion
	    break;
	  case 4:
	    analogResult[3] = ADC->RESULT.reg;	// save conversion value
	    pin = 5;
        ADC->INPUTCTRL.bit.MUXPOS = pin; // Selection new conversion pin
        ADC->SWTRIG.bit.START = 1;            // start new conversion
	  break;
	    case 5:
	    analogResult[4] = ADC->RESULT.reg;	// save conversion value
	    pin = 10;
        ADC->INPUTCTRL.bit.MUXPOS = pin; // Selection new conversion pin
        ADC->SWTRIG.bit.START = 1;            // start new conversion
	    break;
	  case 10:
	    analogResult[5] = ADC->RESULT.reg;	// save conversion value
	    pin = 2;
        ADC->INPUTCTRL.bit.MUXPOS = pin; // Selection new conversion pin
        ADC->SWTRIG.bit.START = 1;            // start new conversion
	    break;
	  }
  }
}


void analogEnable( void ){
  while (ADC->STATUS.bit.SYNCBUSY == 1);
  ADC->CTRLA.bit.ENABLE = 0x01;             // Enable ADC
  while (ADC->STATUS.bit.SYNCBUSY == 1);
//  ADC->INPUTCTRL.bit.MUXNEG = ADC_PIN_IOGND; // set back muxneg to internal ground  ADC_PIN_GND / ADC_PIN_IOGND
  ADC->INPUTCTRL.bit.MUXNEG = ADC_INPUTCTRL_MUXNEG_IOGND; // set back muxneg to internal ground  ADC_PIN_GND / ADC_PIN_IOGND
  while (ADC->STATUS.bit.SYNCBUSY == 1);
  ADC->INPUTCTRL.bit.MUXPOS = 2;             // Selection new conversion pin
  while (ADC->STATUS.bit.SYNCBUSY == 1);
  ADC->SWTRIG.bit.START = 1;                 // start new conversion
}

// =============================================================================
// 8. Timer class (GEVA Elettronica - Giorgio Evangelista)
//    Simple non-blocking software timer based on millis().
//
//    Behaviour:
//      - While it keeps being reset(), it never fires.
//      - Once the set time elapses without being reset, it fires.
//
//    See the Timer example.
// =============================================================================
class Timer
{
  public:
    Timer( unsigned long _t ){ _Time = _t; _Timer = millis()+_Time; };
    Timer() {_Timer = millis();}
    void reset(void){ _Timer = millis(); }
    void set(void){ _Timer = millis() + _Time; }
    bool stato(void){ return ( millis() - _Timer >= _Time ); }
    bool fronte(void){
	  if ( !ff && ( millis() - _Timer >= _Time )){
		  ff = 1;
		  return 1;
	  }else{
		  if ( millis() - _Timer <= _Time ) ff=0;
		  return 0;
	  }
	}
    void tempo(unsigned long _t){ _Time = _t; _Timer = millis()+_t;}

  private:
    unsigned long _Time;
    unsigned long _Timer;
	bool ff = 0;
};

#endif // _GEVINO_MOTOR_H_
