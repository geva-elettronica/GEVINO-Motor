// Test for GEVINO Motor v1.0
// 22/08/2020   Giorgio Evangelista 
#include "Sinewave128.h"  // Change to 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 microsteps
#include "gevino_motor.h"

#define Test_IO
#define StepperMode
//#define BrushedMode

// Install https://github.com/sandeepmistry/arduino-CAN/Also from Arduino Library Manager
// Change on MCP2515.h
//#define MCP2515_DEFAULT_CS_PIN          10
//#define MCP2515_DEFAULT_INT_PIN         -1
#include <CAN.h>
byte CanOk = 0;

// LoRa
#include <LoRa.h>
const int csPin = 10;          // LoRa radio chip select
const int resetPin = 11;       // LoRa radio reset
const int irqPin = -1;         // change for your board; must be a hardware interrupt pin

// W5500
#include <SPI.h>
#include <Ethernet.h>
byte mac[] = {  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 199);
EthernetServer server(80);

// RTC
#include <Wire.h>
#include <DS3231.h>
DS3231 clock;
RTCDateTime dt;

//  TEST_SD
#include <SD.h>
File myFile;

int TestResult;

void setup(){
  
  SerialUSB.begin(115000);
  Serial1.begin(115200);
  Serial.begin(9600);
  
#ifdef Test_IO
  testLeds();
#endif

  gevino_motor_setup();
  // Analog input Setup
  analogPrescaler(ADC_PRESCALER_DIV32);  // DIV4 DIV8 DIV16 DIV32 DIV64 DIV128 DIV256 DIV512 
  analogReadExtended(16); // 16 bit = 256 sample
  analogGain(ADC_GAIN_1);
  analogReference2(ADC_REF_INT1V); // 1.0V reference
  analogCalibrate();
  analogEnable();

  resSTANDBY;         // the engine gets stuck in its position

  ISEq.AGC = 1;       // AGC Enable
  ISEq.CLIM = 0;      // AGC CURRENT LIMITER 45%/50%/55%/60%/65%/70%/75%/80%
  ISEq.FLIM = 0;      // AGC Frequency limit function off/225 Hz/450 Hz/675 Hz
  ISEq.BST = 0;       // Current Boost Function 5/7/9/11 steps
  ISEq.RS_SEL = 0;    // ACDS (RS resistor less) / External sense RS resistor
  ISEq.GAIN_SEL = 0;  // Vref(gain) to 1/5 / 1/10
  MotorInit(); 

  SerialUSB.println("If there are ETH must be connected to the LAN\n\n");

#ifdef Test_IO
// TEST_SD  -  Write file on SD card.
  TestResult = SD.begin(SD_CS);
  if( TestResult ) SerialUSB.println("SD OK"); 
  else  SerialUSB.println("SD FAIL"); 
  
// Test RTC
  TestResult = clock.begin();
  TestResult = clock.isReady();
  if( TestResult ) SerialUSB.println("RTC OK"); 
  else  SerialUSB.println("RTC FAIL");

// W5500
  Ethernet.init(ETH_CS);
  TestResult = Ethernet.begin(mac);
  if( TestResult ) SerialUSB.println("ETH OK"); 
  else  SerialUSB.println("ETH FAIL");   

// CAN
  if (!CAN.begin(500E3)) SerialUSB.println("CAN FAIL");
  else{
    SerialUSB.println("CAN Ok");
    CanOk = 1;
  }

// LoRa at 433Mhz
  if (!LoRa.begin(433E6)) SerialUSB.println("LoRa FAIL");
  else  SerialUSB.println("LoRa OK");
  gevino_motor_setup();  // LoRa changes In2 (D9) into an output.
  MotorInit();           // gevino_motor_setup() set BANK_EN back to LOW (init mode): resend ISEq and put the chip back in operating mode
  
// Test SPP-C
  SerialUSB.println("Test SPP-C");
  Serial.println("AT");
  delay(500);
  while (Serial.available())  SerialUSB.write(Serial.read());
  Serial.println("AT+VERSION");
  delay(500);
  SerialUSB.println("");
  while (Serial.available())  SerialUSB.write(Serial.read());
  Serial.println("AT+NAME");
  delay(500);
  SerialUSB.println("");
  while (Serial.available())  SerialUSB.write(Serial.read());
  SerialUSB.println("");

#endif  

  Microstep();  // Set SPI parameter

}

void loop(){
  static int steps = 0, dir = 0, AnCnt;
  static byte tmp;
  static unsigned long TimerForSec;

  analogPolling();  // Read Analog input and put value on analogResult[];
// analogResult[1] = Ana1
// analogResult[2] = Ana2
// analogResult[3] = Ana3
// analogResult[4] = Current Mot A
// analogResult[5] = Current Mot B

#ifdef StepperMode 
  Param.Torque = 6;  // 0=100% 1=85% 2=70% 3= 60% 4=50% 5=40% 6=10%
  Param.MDTA = 3;      // 0=Fast Decay: 50% (Fast Decay = OSCM × 8) 1=Fast Decay: 37.5% (Fast Decay = OSCM × 6) 2=Fast Decay Only 3=ADMD
  Param.MDTB = 3;      // 0=Fast Decay: 50% (Fast Decay = OSCM × 8) 1=Fast Decay: 37.5% (Fast Decay = OSCM × 6) 2=Fast Decay Only 3=ADMD

  if( ++steps > 100000){
    steps = 0;
    if( dir ) dir = 0;
    else dir = 1;
  }
  if( dir) MicrostepUp();
  else MicrostepDw();
   delayMicroseconds(1);

//  Microstep(10, dir, 10);  
//  if( steps++ >= 1000 ){
//    steps = 0;
//    if( !dir ){
// //      if(++Param.Torque > 6) Param.Torque = 0;    // 0=100% 1=85% 2=70% 3= 60% 4=50% 5=40% 6=10%
//      dir = 1;
//    }else dir =0;
//  }    
#endif

#ifdef BrushedMode
  static unsigned long delay = micros();
  static byte Acc = 1;
  static int Vel = 0, Stop = 0;

  if( micros() - delay >= 200){
    delay = micros();

    if( !Stop ){
      if(Acc){                                          // Acceleration phase
        Vel += 1;
        if( Vel > 1024 ) Acc = 0;
        else Param.Mot1 = Vel;
      }else{
        Vel -= 1;
        if( Vel < 0 ){
          Acc = 1;
          if( Param.Mot1dir ) Param.Mot1dir = 0;    // Direction reversal
          else Param.Mot1dir = 1;
          Param.Mot1 = 0;
          Stop = 2000;                                  // 0.4 Sec pause after reversing
        }else Param.Mot1 = Vel;        
      }
      SendBrushed();
    }else Stop--;
  }
#endif

    
  if(In1 || In5) resOut1;
  else setOut1;
  if(In2 || (analogResult[1] > 0x8000)) resOut2;
  else setOut2;
  if(In3 || (analogResult[2] > 0x8000)) resOut3;
  else setOut3;
  if(In4 || (analogResult[3] > 0x8000)) resOut4;
  else setOut4;  

  if( millis() - TimerForSec > 200 ){
    TimerForSec = millis();
    SerialUSB.print(analogResult[1]);
    SerialUSB.print("\t");
    SerialUSB.print(analogResult[2]);
    SerialUSB.print("\t");
    SerialUSB.print(analogResult[3]);
    SerialUSB.print("\t");
    SerialUSB.print(analogResult[4]);
    SerialUSB.print("\t");
    SerialUSB.println(analogResult[5]);
  }


#ifdef Test_IO
  if( Serial.available() )   SerialUSB.write(Serial.read());
  if( Serial1.available() )   SerialUSB.write(Serial1.read());
  if( SerialUSB.available()){
    tmp = SerialUSB.read();
    Serial.write(tmp);
    Serial1.write(tmp);
  }

  if( millis() - TimerForSec > 1000 ){
    TimerForSec = millis();
    if(CanOk){
      CAN.beginPacket(0x12);
      CAN.write('h');
      CAN.write('e');
      CAN.write('l');
      CAN.write('l');
      CAN.write('o');
      CAN.endPacket();    
    }
  }
#endif 
}
