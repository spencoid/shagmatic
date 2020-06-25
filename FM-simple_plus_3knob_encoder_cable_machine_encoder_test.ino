// not sure why it works with no interrupts but "optimized" best to not use optimize?

// try changing the following

//#define ENCODER_USE_INTERRUPTS
#define ENCODER_DO_NOT_USE_INTERRUPTS
//#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>
#include "pins_arduino.h"
#include <EEPROM.h> 
#include <Bounce.h>
#define PinInt1 0 
#define PinInt2 16

#define BUTTON 1  // estop button
#define LIMIT1 9  // for limit
#define LIMIT2 10  // back limit
Bounce pushbutton = Bounce( BUTTON,100 ); 
Bounce limit1 = Bounce( LIMIT1,100 ); 
Bounce limit2 = Bounce( LIMIT2,100 ); 
Encoder readEnc1(5, 6);
//Encoder readEnc2(18,20); // encoder for first knob(speed)  if three knob mode
//
//Encoder readEnc3(7,17); // encoder for second (stroke) knob if three knob mode
//Encoder readEnc4(21,15); // encoder for third knob (offset_adjust;) if three knob mode

//Encoder readEnc5(0,16); // encoder to handle input from audio input board

#include <AccelStepper.h>
AccelStepper stepper(AccelStepper::DRIVER,3,4);

// the crazy pin use is due to change from teensy 2 to 3.1

const int enable_pin = 16;
boolean run = true;
boolean quad_out = false;
boolean forward = true;
boolean new_parameters = false;
boolean was_reset = false;
boolean three_knob = false;
boolean test_mode = false;
boolean alt_mode = false;   // these are used for diagnostic purposes only
boolean test_1 = false;
boolean test_2 = false;
boolean encoder;
volatile boolean do_CW = false;
volatile boolean do_CCW = false;
long origin = 0;
int count = 0;
int joystick_encoder_multiplier;
int joystick_encoder_divider = 2;  // number of encoder ticks to read per motor tick
int minus_joystick_encoder_divider = 2;
int scale = 2;
int min_stroke;
int offset;
int max_offset;
int max_speed;
int speed_scale;
int max_stroke_default;
int max_speed_default;
int max_stroke_setup;
int max_speed_setup;
int max_speed_vibe;
int min_speed;
int quad_delay;
int read_joystick;
int read_speed_encoder;
int read_stroke_encoder;
int read_offset_encoder;
int address = 0; //EEPROM address to start reading from
int value;  
long current_position = 0;
int abs_max_position = 10000; // just a guess
int min_position = 0; // might want to make it a little past 0
int offset_adjust;  // these were int but changed to long
int off_plus_stroke;
long stroke;
long max_stroke;
long acceleration;
long min_acceleration;
long max_acceleration;
long acceleration_setup;
long acceleration_default;
unsigned long CW_count;
unsigned long CCW_count;
int motor_speed =0;
//int read_offset;
int ratio;
int ratio_bit_0 = 0;
int ratio_bit_1 = 0;
int vibe_limit;
int loop_count =0;
int crawl_delay;
int one_count;
int zero_count;
int audio_scale;
int speed_encoder_multiplier = 10;
int stroke_encoder_multiplier = 4;
int offset_encoder_multiplier = 1;

// the following tables are used to scale the response of the speed and stroke encoders in the three knob controller
// optionally the scaling may be done using a formula. select the option you want in "read_endoders" sub

int speed_table[] = // LOG 3 with 20 offset used for speed
      {0,10,20,20,20,20,20,21,23,24,27,30,34,39,45,51,59,69,79,91,104,119,136,154,175,197,221,247,275,
      306,339,375,413,453,497,543,592,644,699,757,818,883,951,1023,1098,1177,1260,1346,1437,1531,
      1630,1732,1839,1839,1951,2067,2187,2312,2442,2576,2716,2860,3010,3164,3324,3489,3660,3836,
      4018,4205,4398,4597,4802,5013,5230,5454,5683,5919,6162,6410,6666,6928,7198,7474,7757,8047,
      8344,8649,8960,9280,9607,9941,10283,10633,10991,11356,11730,12112,12502,12900,13307,13722,
      14146,14578,15020,15020,15020,15020
}; // add a few extra to be sure

int stroke_table[] = // old table from wheel drive
      {0,50,60,70,80,86,92,112,131,150,169,195,220,252,278,310,342,374,413,451,489,528,566,605,649,694,739,
      784,829,880,925,976,1027,1079,1130,1181,1239,1290,1347,1405,1463,1520,1578,1636,1693,1757,
      1821,1879,1943,2007,2071,2142,2206,2276,2340,2340,2411,2475,2545,2615,2686,2763,2833,2904,
      2980,3051,3128,3205,3281,3352,3435,3512,3589,3666,3749,3826,3909,3986,4069,4152,4236,4319,
      4402,4485,4575,4658,4748,4831,4921,5004,5094,5183,5273,5363,5452,5542,5638,5728,5817,5913,
      6003,6099,6195,6291,6387,6483,6500,6500, 6500
}; // add a few extra to be sure

//int stroke_table[] = // new test for cable drive
//      {0,0,0,25,30,35,50,55,60,65,70,75,80,86,92,110,120,131,141,150,169,175,185,195,210,220,230,240,252,262,278,288,
//      299,310,320,330,342,355,366,374,385,395,413,425,440,451,470,489,500,528,540,566,588,605,620,649,
//      670,694,710,739,759,771,784,800,829,835,860,880,925,950,976,980,995,1027,1044,1079,1100,1130,
//      1150,1181,1200,1239,1265,1290,1310,1347,1385,1405,1463,1520,1578,1636,1693,1757,
//      1821,1879,1943,2007,2071,2142,2206,2276,2300,2340,2340,2411,2475,2500,2545,2615,2686,2763,2833,2904,
//      2904,2904,2904,2904,2904,2904
//}; // add a few extra to be sure

int offset_table[] = // LOG 2 with 80 offset used for speed added a couple of extra at begin to stop dither when stalled 
      {70,80,86,92,112,131,150,169,195,220,252,278,310,342,374,413,451,489,528,566,605,649,694,739,
      784,829,880,925,976,1027,1079,1130,1181,1239,1290,1347,1405,1463,1520,1578,1636,1693,1757,
      1821,1879,1943,2007,2071,2142,2206,2276,2340,2340,2411,2475,2545,2615,2686,2763,2833,2904,
      2980,3051,3128,3205,3281,3352,3435,3512,3589,3666,3749,3826,3909,3986,4069,4152,4236,4319,
      4402,4485,4575,4658,4748,4831,4921,5004,5094,5183,5273,5363,5452,5542,5638,5728,5817,5913,
      6003,6099,6195,6291,6387,6483,6500,6500, 6500
}; // add a few extra to be sure

void setup() {
    // add benchmark routine to measure tmie of 1000 steps back and forth
//  Serial.begin(9600);   // can comment out serial cummunications if not using for diagnosis
//  delay(500);
//  Serial.println("begin setup");
//  interrupts();
//  offset_adjust = 0;
  min_stroke = 30;
  offset = 0;
  max_offset = 5000;
  max_stroke = 2000;  // 4000 for auto tech higher for gecko depending on encoder scale 7000 for encode scale 4
  max_speed = 12000;  // 12000 for auto tech 15000 for gecko
  max_speed_vibe = 22000;
  max_stroke_default = max_stroke;
  max_speed_default = max_speed;
  min_speed = 20;
  vibe_limit = 100;
  audio_scale = 2;
  acceleration = 200000;  // do not change from 200000 unless there is a really good reason
  acceleration_default = acceleration;
  joystick_encoder_multiplier = 2; joystick_encoder_divider = 2; minus_joystick_encoder_divider = -2;

  stepper.setMaxSpeed(3000.0);  // not really used but here for legacy purposes. max speed is set from value in EEPROM at boot time
  stepper.setAcceleration(acceleration);  // 80000 is fast enough might make it less
  stepper.setEnablePin(enable_pin);
  stepper.setPinsInverted(true,true,true);  
  stepper.setMinPulseWidth(20);
  stepper.setCurrentPosition(0)	;
  current_position = 0; 
//  delay(2000);  // this is needed for some fucking reason to let voltage stabilize???

  pinMode(1, INPUT_PULLUP); // determines if three knob controller is connected
  
// the followoing are for mechanical encoders, pullup required  
// pin 0 will be used for audio decoder direction output and switch 8 will be abandoned or replaced with a high number pin? 
  pinMode(PinInt1, INPUT_PULLUP); // pin 0
  pinMode(PinInt2, INPUT_PULLUP); // pin 16
  
  pinMode(1, INPUT_PULLUP); // knob pin to select 2nd knob function or to toggle 6 in 8 config
  pinMode(0, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP); // sw 7 config switch to set divider ratio bit 0
  pinMode(8, INPUT_PULLUP); // sw 6 config switch to set divider ratio bit 1
  pinMode(11, INPUT_PULLUP); // sw 4 config switch to choose seek home option at boot

  // switch 3 is left unconnected and can be jumped to an unused pin such as 13
  // switch 5 is connected to pin 12 and can be used to set speed to force encoder scale to lowest speed 
  // switch 2 is connected to pin 19 which is used for third knob can be available if third knob is not used should change this
  pinMode(7, INPUT_PULLUP); // encoder pin
  pinMode(17, INPUT_PULLUP); // encoder pin
//  pinMode(18, INPUT_PULLUP); // encoder pin
//  pinMode(20, INPUT_PULLUP); // encoder pin
  pinMode(3, OUTPUT); // step
  pinMode(4, OUTPUT); // direction 
// the following are for digital encoder, no pullup required but might be necessary for slave operation???
  pinMode(5, INPUT_PULLUP); // joystick encoder, pullup
  pinMode(6, INPUT_PULLUP); // joystick encoder, pullup    
  pinMode(9, INPUT_PULLUP); // limit switch for both directions
  pinMode(10, INPUT_PULLUP); // not used with single limit switch
  pinMode(12, INPUT_PULLUP); // joystick speed choosing switch
  pinMode(14, INPUT_PULLUP); // joystick speed choosing switch
  pinMode(15, INPUT_PULLUP);// encoder, pullup
  pinMode(16, INPUT_PULLUP);  
  pinMode(LED_BUILTIN, OUTPUT);   
  pinMode(19, INPUT_PULLUP); // test mode switch, pullup
//  pinMode(21, INPUT_PULLUP); // encoder, pullup    
  pinMode(22, OUTPUT);  // quadrature output for slave phase A
  pinMode(23, OUTPUT);  // quadrature output for slave phase B

  // set unused pins as output to limit noise?
  pinMode(24, OUTPUT); pinMode(25, OUTPUT); pinMode(26, OUTPUT); pinMode(27, OUTPUT); pinMode(28, OUTPUT); pinMode(29, OUTPUT); pinMode(30, OUTPUT); pinMode(31, OUTPUT); 
  pinMode(32, OUTPUT); pinMode(33, OUTPUT); pinMode(34, OUTPUT); pinMode(28, OUTPUT); pinMode(29, OUTPUT); pinMode(30, OUTPUT);  pinMode(31, OUTPUT);  pinMode(32, OUTPUT);  pinMode(33, OUTPUT);     
  
  attachInterrupt(PinInt1, isrFor, FALLING); // run interrupt when pin 0 goes low
  attachInterrupt(PinInt2, isrBack, FALLING); // run interrupt when pin 16 goes low
   
  digitalWriteFast(LED_BUILTIN, HIGH); 
  delay (500);  // delay to make sure inputs are ready when read in setup
  three_knob = false;
  if (digitalReadFast(1)== LOW){    // detect three knob controller by seeing e-stop pin to ground
    three_knob = true;
  }    
 // setup procedure here  initializes EEPROM on virgin teensy otherwise gets the values stored in EEPROM for use by the program
 // read values  have to set values if never set or read will be bogus
 
  address = 0; //EEPROM address to start reading from
//  EEPROM.put(address, -1);  // for testing to set EEPROM first byte to initial state
  EEPROM.get( address, value );
  if (value == -1){  // virgin teensy EEPROM empty
    EEPROM.put(address, max_speed_default);  
    address = 5;
    EEPROM.put(address, max_stroke_default);
    address = 10;
    EEPROM.put(address, acceleration);
  }
  else{
    get_parameters();   
    if ((max_speed == 0) || (max_stroke == 0) ||(acceleration == 0)) {
      max_speed = max_speed_default;  // in case 0 was set change back to default
      max_stroke = max_stroke_default;
      acceleration = acceleration_default;
      address = 0;
      EEPROM.put(address, max_speed);  
      address = 5;
      EEPROM.put(address, max_stroke);
      address = 10;
      EEPROM.put(address, acceleration); 
      run = true;
      if (motor_speed < min_speed) run = false;  
      stepper.setMaxSpeed(motor_speed);   
      stepper.setAcceleration(acceleration);
      estop();   
      was_reset = true;
    }         
    get_parameters();   
  }
  get_parameters();
  if (new_parameters == true){
    address = 0;
    EEPROM.put(address, max_speed_setup);  
    address = 5;
    EEPROM.put(address, max_stroke_setup);
    address = 10;
    EEPROM.put(address, acceleration_setup);
 }
//  if (digitalReadFast(19)== LOW){  // test switch set so run test code // no test code now, it has been removed
//    run_test();
//  } 
  if (digitalReadFast(19)== LOW){   // switch 2 used to enable/disable quad output for slave machine
    quad_out = true;
  } 
  if (digitalReadFast(11)== HIGH){  // only do seek home if switch set for option
    
//    while (digitalReadFast(10)== HIGH){ 
//      cwpulse();
//    }       
    while (digitalReadFast(9)== HIGH){  // move to rear limit switch
//      ccwpulse();
      ccwpulse_quad();
      delay (1);
    }  
    for (int i=0; i < 10; i++){ // be sure to be off switch for following test
      cwpulse();
      delay (1);
    }    
//    current_position = 0;
  }

  digitalWriteFast(LED_BUILTIN, HIGH); 
}

void loop() {
   check_encoders();
   stepper.run(); 
   if (pushbutton.update()){ if (pushbutton.risingEdge()) estop(); }  // this flavor of using bounce seems to work better. it debounces the e-stop line which is prone to noise
   read_encoders();  // read encoders and do appropriate things 
   if (digitalReadFast(10)== LOW)  current_position = abs_max_position; // keep track of current position using these calls and limit the moves if abs_max will be hit
   if (digitalReadFast(9)== LOW)  current_position = min_position;
   while (digitalReadFast(10)== LOW){
        ccwpulse(); delay (1);
        origin ++;
        read_encoders();
        stepper.run();
   }  
   while (digitalReadFast(9)== LOW){
        cwpulse(); delay (1);
        stroke --;
        read_encoders();
        stepper.run();
   }       
   if (run){    // if three knob is in use and setting accel stepper values then act on them
//       stepper.run(); // don't know why it is needed here too but it is, leave it
       if (forward == true) {stepper.moveTo (stroke);}
       else{stepper.moveTo(origin);} // only need to move to new offset position if there is no stroke
       stepper.run(); // don't know why it is needed here too but it is, leave it
       if ((stepper.distanceToGo() == 0) && (stroke > 0)) {
            read_encoders();
//            readEnc3.write(0);
//            forward = !forward;	   // reverse direction
        }
     }
     stepper.run();   // this should probably be conditional on run boolean being high
     read_encoders();  // read encoders and do appropriate things    
     stepper.run(); // don't know why it is needed here too but it is, leave it
}
void read_encoders(){   // this is the main code. it acts on both joystick encoder and three knob controller
    read_joystick_encoder();
    if (three_knob == true){    // only if three knob was sensed at startup 
//      if(digitalReadFast(14) == LOW){ get_speed();}   // set encoder scaling ratio
//      if(digitalReadFast(12) == LOW){ get_stroke();}  
     int read_analog1;
     int read_analog2;
     int read_analog3;
     read_analog1 = analogRead(15);   
     read_analog2 = analogRead(17);   
     read_analog3 = analogRead(20); 
     
     motor_speed =  (read_analog1);
     motor_speed = map(motor_speed, 0, 1023, 0, max_speed);   
     
     stroke = read_analog2;
     stroke = map(stroke, 0, 1023, 0, max_stroke);
     if (stroke < vibe_limit) {motor_speed = constrain (motor_speed,0,max_speed_vibe); } // vibe limit is the limit used when stroke is short to allow for vibration faster than with longer strokes
     else{motor_speed = constrain (motor_speed ,0,max_speed);} 
     stepper.setMaxSpeed(motor_speed);    
     
     offset =  (read_analog3);
     offset  = map(offset , 0, 1023, 0, max_offset);     
     
//      get_speed();
//      get_stroke();
//      do_offset();
      run = true;
      if ((motor_speed < min_speed) || (stroke < min_stroke)) run = false;
//      readEnc2.write(0); 
//      readEnc3.write(0);
//        readEnc4.write(0);        
      if (run) stepper.run();
    }  
} 
void read_joystick_encoder(void){
      joystick_encoder_divider = 2; minus_joystick_encoder_divider = -2;
      if(digitalReadFast(14) == LOW){ joystick_encoder_divider = 1; minus_joystick_encoder_divider = -1;}   // set encoder scaling ratio
      if(digitalReadFast(12) == LOW){ joystick_encoder_divider = 3; minus_joystick_encoder_divider = -3;}   
      read_joystick = readEnc1.read();
      if ((read_joystick > joystick_encoder_divider)||(read_joystick < minus_joystick_encoder_divider)) {    // jostick read ?
          if (read_joystick < 0) { 
             if (digitalReadFast(10) == HIGH) cwpulse_quad();    // cwpulse();
          }          
          else if (read_joystick > 0) {    
            if (digitalReadFast(9) == HIGH)  ccwpulse_quad();    // ccwpulse();
          }    
    readEnc1.write(0);    // this must be here or it can accumulate high values and delay moving off limit switch
    }
}
//void get_speed(void){
//      read_speed_encoder = readEnc1.read();  
//      if (read_speed_encoder != 0){
//            motor_speed +=  (read_speed_encoder);
//            motor_speed = constrain (motor_speed,0,max_speed);
//            if (run) stepper.run();
//            readEnc1.write(0);
//      }
//}
//void get_stroke(void){  // try using joy encoder to set stroke  use estop button to choose what is done with encoder
//     read_stroke_encoder = readEnc1.read();
//     if (read_stroke_encoder != 0){
//          stroke += (read_stroke_encoder);
//          stroke = constrain (stroke,0,max_stroke);
//          if (stroke < vibe_limit) {motor_speed = constrain (motor_speed,0,max_speed_vibe); } // vibe limit is the limit used when stroke is short to allow for vibration faster than with longer strokes
//          else{motor_speed = constrain (motor_speed ,0,max_speed);} 
//          stepper.setMaxSpeed(motor_speed);     
//          if (run) stepper.run();
//          readEnc1.write(0);
//      }      
//}
//void do_offset(void){
//      read_offset_encoder = readEnc4.read(); 
//      if (read_offset_encoder < 0) { 
//        if (digitalReadFast(9) == HIGH){
//            for (int i=0; i < offset_encoder_multiplier; i++){
//                ccwpulse_quad();    // ccwpulse();
//            }         
//        } 
//        readEnc4.write(0);
//      } 
//      else if (read_offset_encoder > 0) {
//         if (digitalReadFast(10) == HIGH){
//            for (int i=0; i < offset_encoder_multiplier; i++){
//                cwpulse_quad();    // cwpulse();
//            }           
//         }
//         readEnc4.write(0);
//      }
//      readEnc4.write(0);
//}
void check_encoders(){
    readEnc1.read();
//    readEnc2.read();
//    readEnc3.read();
//    readEnc4.read();
}
void cwpulse(void){
  digitalWriteFast(LED_BUILTIN, HIGH);
  digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN));
//    if (digitalReadFast(10) == LOW){  // trying to move forward by audio or joystick but limit is reached so backoff routine needs to set
//      digitalWriteFast(4, HIGH);      // direction high to allow backoff therefore this is necessary because audio pulses are via an interrupt
//    }                                 // however the pulse is in the loop triggered by a flag so maybe it is not necessary, have to test this
//    else{
      digitalWriteFast(4, LOW); // pin 4 low for CW 
//    }
    delayMicroseconds(3);  //dir must preceed step by 200 ns
    for (int i=0; i < joystick_encoder_multiplier; i++){
      current_position ++;
      digitalWriteFast(3, HIGH); // step occurs on positive transition
      delayMicroseconds(3);      // needs to be 2, 1 is not enough for Chinese stepper might be enough for Gecko
      digitalWriteFast(3, LOW); // pin 3 remains low until next step pulse
      delayMicroseconds(3);   // needed for time to read the analog input 
    }
} 
void ccwpulse(void){       // these are the working versions of the pulsing code. other versions are used in setup etc but these are the ones that do most of the work
    digitalWriteFast(LED_BUILTIN, LOW);
    digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN));
    digitalWriteFast(4, HIGH); // pin 4 high for CCW 
    delayMicroseconds(3);  //dir must preceed step by 200 ns
     for (int i=0; i < joystick_encoder_multiplier; i++){
      current_position --;
      digitalWriteFast(3, HIGH); // pin 3 high
      delayMicroseconds(3);  
      digitalWriteFast(3, LOW); // pin 3 low
      delayMicroseconds(3);   // needed for time to read the analog input 
    }    
}
void cwpulse_quad(void){  // combined pulse and quad out 
  digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN));
  digitalWriteFast(4, LOW); // pin 4 low for CW 
    delayMicroseconds(1);
    for (int i=0; i < joystick_encoder_multiplier; i++){
       current_position ++;
      digitalWriteFast(22, LOW);  // A phase leads
      digitalWriteFast(3, HIGH); // pin 3 high
      delayMicroseconds(3);
      digitalWriteFast(23, LOW);
      digitalWriteFast(3, LOW); // pin 3 low
      delayMicroseconds(2);   // needed for time to read the analog input 
      digitalWriteFast(22, HIGH);
      delayMicroseconds(2);
      digitalWriteFast(23, HIGH); 
      delayMicroseconds(2);     
    }
} 
void ccwpulse_quad(void){  // combined pulse and quad out
  digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN));
    digitalWriteFast(4, HIGH); // pin 4 high for CCW 
      delayMicroseconds(1);
    for (int i=0; i < joystick_encoder_multiplier; i++){
      current_position --;
      digitalWriteFast(23, LOW);  // B phase leads
      digitalWriteFast(3, HIGH); // pin 3 high
      delayMicroseconds(3);  
      digitalWriteFast(22, LOW);
      digitalWriteFast(3, LOW); // pin 3 low
      delayMicroseconds(2);   // needed for time to read the analog input 
      digitalWriteFast(23, HIGH);
      delayMicroseconds(2);
      digitalWriteFast(22, HIGH);
      delayMicroseconds(2);
    }    
}
void estop(){
//  readEnc2.write(0);
//  readEnc3.write(0);  
//  read_speed = 0;
//  read_stroke = 0;   
  motor_speed = 0;
  stepper.setMaxSpeed(motor_speed); 
  stroke = 0;
  offset_adjust = 0;
  off_plus_stroke = 0;
  run = false;
  while (digitalReadFast(9)== HIGH){  // 
    ccwpulse();
    delayMicroseconds(3000);    
  }    
}
void do_serial(){   // prints to computer via serial port can edit this code to show useful stuff
  byte incomingByte;   
  if (Serial.available() > 0) {
      while(Serial.available()){
        incomingByte = Serial.read();
      }     
      Serial.print( "CW count: " );
      Serial.print(CW_count);  
      Serial.print( " CCW count: " );
      Serial.println(CCW_count);    
      CW_count = 0;
      CCW_count = 0;   
  }    
}
void get_parameters(){
    address = 0;
    EEPROM.get(address, max_speed);    
    address = 5;
    EEPROM.get(address, max_stroke);
    address = 10;
    EEPROM.get(address, acceleration); 
//    Serial.print( "max speed: " );
//    Serial.println(max_speed);  
//    Serial.print( "max stroke: " );
//    Serial.println(max_stroke);     
}    
//void set_parameters() {
//   read_encoders_setup();
//   while(digitalReadFast(10) == LOW){
//       ccwpulse(); 
//       delay(3);     
//   }
//   while(digitalReadFast(9) == LOW){
//       cwpulse(); 
//       delay(3);      
//   }   
//   if (forward == true) {
//        stepper.moveTo (off_plus_stroke);
//   }
//   else{  
//        stepper.moveTo(offset_adjust;);  // only need to move to new offset position if there is no stroke
//   }
//   if (stepper.distanceToGo() == 0){    
//     if (stroke > 0) forward = !forward;  // no need to switch if no stoke
//   }
//   stepper.run(); 
//}

// everything below this point is either used for different versions or is only used occasionally and can generally be ignored

void isrFor()  // FASTRUN speeds up the routine by running in RAM? these are not used except in audio version
{
  cli();
//  zero_count ++;
  do_CW = true;  // flag set so will run cwpulse when loop cycles, this is to avoid using delay in an ISRj
//  cwpulse();  // the test version will not send pulses if limit switch is LOW
  sei();
} 
 void isrBack()  // FASTRUN speeds up the routine by running in RAM?
{
  cli();
//  one_count ++;
  do_CCW = true;
//  ccwpulse();
  sei();
} 
void cwpulse_audio(void){  // only used in audio encoding version
//   CW_count ++;
//  digitalWriteFast(LED_BUILTIN, HIGH);
//  digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN));
//    noInterrupts();
    if (digitalReadFast(10) == LOW){  // trying to move forward by audio or joystick but limit is reached so backoff routine needs to set
      digitalWriteFast(4, HIGH);      // direction high to allow backoff therefore this is necessary because audio pulses are via an interrupt
    }                                 // however the pulse is in the loop triggered by a flag so maybe it is not necessary, have to test this
    else{
      digitalWriteFast(4, LOW); // pin 4 low for CW 
    }
    delayMicroseconds(1);  //dir must preceed step by 200 ns
    for (int i=0; i < joystick_encoder_multiplier; i++){
//      delayMicroseconds(1); 
      digitalWriteFast(3, HIGH); // step occurs on positive transition
      delayMicroseconds(2);      // was 2 try 3 to see if it works for single pulse
      digitalWriteFast(3, LOW); // pin 3 remains low until next step pulse
      delayMicroseconds(2);   // was 2 try 3 to see if it works for single pulse 
    }
} 
void quad_for(void){  // quadrture out on pins 22 and 23 to drive slave machine
     digitalWriteFast(22, LOW);  // A phase leads
     delayMicroseconds(quad_delay);
     digitalWriteFast(23, LOW);
     delayMicroseconds(quad_delay);
     digitalWriteFast(22, HIGH);
     delayMicroseconds(quad_delay);
     digitalWriteFast(23, HIGH);
     current_position ++;
}
void quad_back(void){  // quadrture out on pins 22 and 23
     digitalWriteFast(23, LOW);  // B phase leads
     delayMicroseconds(quad_delay);
     digitalWriteFast(22, LOW);
     delayMicroseconds(quad_delay);
     digitalWriteFast(23, HIGH);
     delayMicroseconds(quad_delay);
     digitalWriteFast(22, HIGH);    
     current_position --;
}
void cwpulse_test(void){
    stepper.setMaxSpeed(12000);   // set very high for just one step
    stepper.move(-10); // need to set speed and accel
    stepper.run(); 
    stepper.setMaxSpeed(motor_speed);
    current_position ++;
}
void ccwpulse_test(void){
    stepper.setMaxSpeed(12000);   // set very high for just one step
    stepper.move(10); // need to set speed and accel
    stepper.run(); 
    stepper.setMaxSpeed(motor_speed);
    current_position --;
}
void ccwpulse_audio(void){   // only used in audio encoding version
//   CCW_count ++;
//    digitalWriteFast(LED_BUILTIN, LOW);
//  digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN));
    digitalWriteFast(4, HIGH); // pin 4 high for CCW 
    delayMicroseconds(1);  //dir must preceed step by 200 ns
    for (int i=0; i < joystick_encoder_multiplier; i++){
//      delayMicroseconds(1); 
      digitalWriteFast(3, HIGH); // pin 3 high
      delayMicroseconds(2);  // was 2 try 3 to see if it works for single pulse
      digitalWriteFast(3, LOW); // pin 3 low  
      delayMicroseconds(2);   // was 2 try 3 to see if it works for single pulse 
    }    
}
void read_encoders_setup(){ // read encoders during parameter setup
//    read_speed_encoder = readEnc2.read();
//    read_stroke_encoder = readEnc3.read();  
//    read_offset_encoder = readEnc4.read();  
    acceleration += (read_offset_encoder * 500);  
    if (acceleration < min_acceleration) acceleration = min_acceleration;
    if (acceleration > max_acceleration) acceleration = max_acceleration;   
//    read_speed = constrain ((read_speed + read_speed_encoder),0,102);
//    speed = constrain ((speed_table[read_speed]),0,max_speed_default);
    run = true;
    if (motor_speed < min_speed) run = false;
//    speed = constrain ((speed_table[read_speed]),0,max_speed_default);
//    read_stroke = constrain ((read_stroke + read_stroke_encoder),0,102); 
//    stroke = constrain ((stroke_table[read_stroke]),0,max_stroke_default);
    off_plus_stroke = constrain ((offset_adjust + stroke),0,max_stroke_default);       
//    speed = constrain ((speed_table[read_speed]),0,max_speed);
    stepper.setMaxSpeed(motor_speed);   
    stepper.setAcceleration(acceleration);
    acceleration_setup = acceleration;
    max_stroke_setup = stroke;
    max_speed_setup = motor_speed;
//    readEnc2.write(0);
//    readEnc3.write(0);  
//    readEnc4.write(0);   
//    stroke = read_stroke * (max_stroke / 100);
}
void print_serial(){    // not used, this allows the printing of values in response to a serial port poke this is to allow showing of values without printing continuously and crashing java
  if (Serial.available() > 0) {
      while(Serial.available()){
        int incomingByte = Serial.read();
      }     
      Serial.print( "zero count: " );
      Serial.println(zero_count);  
      Serial.print( "one count: " );
      Serial.println(one_count);      
  }      
}
/*    
to do

stepper functions
     stepper.moveTo(offset_adjust;);  
     stepper.distanceToGo();
     stepper.run();
     stepper.stepper.runSpeed();
     move(long relative);
     stepper.setCurrentPosition(long position);
     stepper.currentPosition(); returns
     stepper.setMaxSpeed(float speed)
     stepper.setAcceleration(float acceleration)
     stepper.speed(); returns speed
     stepper.disableOutputs();
     stepper.enableOutputs();
     stepper.runSpeedToPosition(); blocks 
     
// integrate the code from quad_for and guad_back into swpulse and ccwpulse so as to not add additional delays
//   if (limit1.update()) {   // doesn't work with bounce
//       if (limit1.fallingEdge()){ 
//          ccwpulse();
//       }       
//   } 
//   if (limit2.update()) { 
//       if (limit2.fallingEdge()){ 
//          cwpulse();
//       }       
//   } 
*/    
