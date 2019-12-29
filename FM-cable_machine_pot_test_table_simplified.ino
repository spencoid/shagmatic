// not sure why it works with no interrupts but "optimized" best to not use optimize?
//#define ENCODER_USE_INTERRUPTS
#define ENCODER_DO_NOT_USE_INTERRUPTS
//#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>
#include "pins_arduino.h"
#include <EEPROM.h> 
#include <Bounce.h>

#define BUTTON 1  // estop button
Bounce pushbutton = Bounce( BUTTON,100 ); 
Encoder readEnc1(5, 6);

#include <AccelStepper.h>
AccelStepper stepper(AccelStepper::DRIVER,3,4);

// the crazy pin use is due to change from teensy 2 to 3.1
const int enable_pin = 16;
boolean run = true;
boolean quad_out = false;
boolean forward = true;
boolean three_knob = false;
int joystick_encoder_multiplier;    // multiplies the encoder count to move motor more steps than encoder ticks together with divider
int joystick_encoder_divider = 2;  // which is set with "speed switch" on the joystick this determines the ratio of encoder to motor movement
int minus_joystick_encoder_divider = 2;
int stroke = 0;
int max_stroke = 3000;  // offset plus stroke is constrained to this value so it is useful even if the max is set by the lookup table
int offset = 0;
int min_stroke = 0; // used to determine at what setting motion will begin and estop recovery occurs
int offset_plus_stroke = 0; 
int origin_offset = 0;  // origin and end offset are incremented when limit switches are hit 
int end_offset = 0;
int max_offset = 1500;
int min_offset = 0;
int max_speed = 2000;   // not used for now since vibe and longer strokes use same max and are set by lookup table
//int vibe_limit = 50;
//int max_speed_vibe = 900;
int min_speed = 10;   // was 20
long current_position = 0;
int abs_max_position = 10000; // just a guess
int acceleration = 40000;  // this determines the ramping time to speed so needs to be carefully chosen to allow for fast short strokes and appropriate long strokes
int motor_speed =0;
int loop_count = 0; // used to limit the number of pot reads 
int pot_read_count = 10000; // number of loops before the pots are read

//int speed_table[] = {// LOG 2 103 entries so div by 10 
//        0 ,0 ,0 ,1 ,3 ,4 ,7 ,9 ,12 ,15 ,19 ,23 ,28 ,33 ,38 ,44 ,50 ,56 ,63 ,70 ,78 ,86 ,94 ,103 ,112 ,122 ,132 ,142 ,153 ,164 ,176 ,
//        188 ,200 ,213 ,226 ,240 ,254 ,268 ,283 ,298 ,313 ,329 ,345 ,362 ,379 ,397 ,414 ,433 ,451 ,470 ,490 ,509 ,530 ,550 ,
//        571 ,593 ,614 ,636 ,659 ,682 ,705 ,729 ,753 ,778 ,803 ,828 ,854 ,880 ,906 ,933 ,960 ,988 ,1016 ,1044 ,1073 ,1102 ,
//        1132 ,1162 ,1192 ,1223 ,1254 ,1286 ,1318 ,1350 ,1383 ,1416 ,1450 ,1483 ,1518 ,1552 ,1588 ,1623 ,1659 ,1695 ,
//        1732 ,1769 ,1806 ,1844 ,1882 ,1921 ,1960 ,2000,2000,2000,2000,2000
//}; // add a few extra to be sure
//
//int speed_table[] = {// LOG 2 103 entries so div by 10
//0,0,1,2,4,7,10,14,18,23,29,35,42,49,57,66,75,84,95,106,117,129,142,155,169,183,198,214,230,247,264,282,301,320,
//339,360,381,402,424,447,470,494,518,543,569,595,622,649,677,706,735,735,764,795,826,857,889,922,955,989,1023,
//1058,1094,1130,1167,1204,1242,1281,1320,1359,1400,1441,1482,1524,1567,1610,1654,1698,1743,1789,1835,1882,1
//929,1977,2025,2075,2124,2175,2225,2277,2329,2382,2435,2489,2543,2598,2654,2710,2767,2824,2882,2940,3000,
//3000, 3000 ,3000 ,3000
//}; // add a few extra to be sure

int speed_table[] = {// LOG 2 103 entries so div by 10
0,0,0,1,4,7,12,17,24,31,39,49,59,70,82,96,110,125,141,158,176,196,216,237,259,282,306,331,357,384,412,441,471,501,
533,566,600,635,671,707,745,784,823,864,906,948,992,1037,1082,1129,1176,1225,1225,1274,1325,1376,1429,1482,
1537,1592,1648,1706,1764,1823,1884,1945,2007,2070,2135,2200,2266,2333,2401,2470,2540,2611,2684,2757,2831,
2906,2982,3059,3136,3215,3295,3376,3458,3541,3625,3709,3795,3882,3970,4058,4148,4239,4330,4423,4517,4611,
4707,4803,4901,5000,5000, 5000, 5000, 5000, 5000
}; // add a few extra to be sure

int stroke_table[] = {  // LOG 2 103 entries so div by 10
        0 ,0 ,0, 1 ,2 ,4 ,7 ,10 ,14 ,18 ,23 ,29 ,35 ,42 ,49 ,57 ,66 ,75 ,84 ,95 ,106 ,117 ,129 ,142 ,155 ,169 ,183 ,198 ,214 ,230 
        ,247 ,264 ,282 ,301 ,320 ,339 ,360 ,381 ,402 ,424 ,447 ,470 ,494 ,518 ,543 ,569 ,595 ,622 ,649 ,677 ,706 ,735 ,764 
        ,795 ,826 ,857 ,889 ,922 ,955 ,989 ,1023 ,1058 ,1094 ,1130 ,1167 ,1204 ,1242 ,1281 ,1320 ,1359 ,1400 ,1441 ,1482 
        ,1524 ,1567 ,1610 ,1654 ,1698 ,1743 ,1789 ,1835 ,1882 ,1929 ,1977 ,2025 ,2075 ,2124 ,2175 ,2225 ,2277 ,2329 
        ,2382 ,2435 ,2489 ,2543 ,2598 ,2654 ,2710 ,2767 ,2824 ,2882 ,2940 ,3000,3000,3000,3000,3000
}; // add a few extra to be sure

int offset_table[] ={ // 103 entries
    0 ,0 ,0 ,1 ,2 ,3 ,5 ,7 ,9 ,11 ,14 ,17 ,21 ,24 ,28 ,33 ,37 ,42 ,47 ,53 ,58 ,64 ,71 ,77 ,84 ,91 ,99 ,107 ,115 ,123 ,132 ,
    141 ,150 ,160 ,169 ,180 ,190 ,201 ,212 ,223 ,235 ,247 ,259 ,271 ,284 ,297 ,311 ,324 ,338 ,353 ,367 ,382 ,397 ,
    413 ,428 ,444 ,461 ,477 ,494 ,511 ,529 ,547 ,565 ,583 ,602 ,621 ,640 ,660 ,679 ,700 ,720 ,741 ,762 ,783 ,805 ,
    827 ,849 ,871 ,894 ,917 ,941 ,964 ,988 ,1012 ,1037 ,1062 ,1087 ,1112 ,1138 ,1164 ,1191 ,1217 ,1244 ,1271 ,
    1299 ,1327 ,1355 ,1383 ,1412 ,1441 ,1470 ,1500,1500,1500,1500,1500
}; // added a few extra to be sure

int accel_table[] ={     // used for testing different accelerations
0,19,78,176,313,490,705,960,1254,1588,1960,2372,2823,3313,3842,4411,5019,5666,6352,7077,7842,8646,
9489,10371,11293,12253,13253,14292,15371,16488,17645,18841,20076,21350,22664,24017,25409,26840,
28310,29820,31369,32957,34584,36251,37957,39701,41486,43309,45172,47073,49014,49014,50995,53014,
55073,57170,59307,61484,63699,65954,68248,70581,72953,75365,77815,80305,82835,85403,88010,90657,
93343,96069,98833,101637,104479,107362,110283,113243,116243,119282,122360,125477,128634,131830,
135065,138339,141652,145005,148397,151828,155298,158807,162356,165944,169571,173237,176943,
180688,184472,188295,192157,196059,200000,200000, 200000 , 200000, 200000
}; // added a few extra to be sure

void setup() {
  joystick_encoder_multiplier = 2; joystick_encoder_divider = 2; minus_joystick_encoder_divider = -2;
  analogReference(EXTERNAL);  //  needs to be on for 3.2 and off for 4
  stepper.setMaxSpeed(3000.0);  // not really used but here for legacy purposes. max speed is set from read encoders
  stepper.setAcceleration(acceleration);  // 80000 is fast enough might make it less
  stepper.setEnablePin(enable_pin);
  stepper.setPinsInverted(true,true,true);  
  stepper.setMinPulseWidth(20); // was 20
  stepper.setCurrentPosition(0)	;
  current_position = 0; 

// pin 0 will be used for audio decoder direction output, not sure what switch 8 does?
  pinMode(0, INPUT_PULLUP);
  pinMode(1, INPUT_PULLUP); // determines if three knob controller is connected 
  pinMode(2, INPUT_PULLUP); // sw 7 available for use
  pinMode(8, INPUT_PULLUP); // sw 6 available for use
  pinMode(11, INPUT_PULLUP); // sw 4 config switch to choose seek home option at boot
  // switch 3 is left unconnected and can be jumped to an unused pin such as 13
  // switch 5 is connected to pin 12 and can be used to set speed to force encoder scale to lowest speed 
  // switch 2 is connected to pin 19 this is quad output select
  pinMode(7, INPUT_PULLUP); // encoder pin
  pinMode(3, OUTPUT); // step
  pinMode(4, OUTPUT); // direction 
  pinMode(5, INPUT_PULLUP); // joystick encoder, pullup
  pinMode(6, INPUT_PULLUP); // joystick encoder, pullup    
  pinMode(9, INPUT_PULLUP); // limit switch
  pinMode(10, INPUT_PULLUP); // limit switch
  pinMode(12, INPUT_PULLUP); // joystick speed choosing switch
  pinMode(14, INPUT_PULLUP); // joystick speed choosing switch
  pinMode(16, INPUT_PULLUP);  
  pinMode(LED_BUILTIN, OUTPUT);   
  pinMode(19, INPUT_PULLUP); // quad out select seitch switch, pullup  
  pinMode(22, OUTPUT);  // quadrature output for slave phase A
  pinMode(23, OUTPUT);  // quadrature output for slave phase B
  // set unused pins as output to limit noise?//  
  pinMode(24, OUTPUT); pinMode(25, OUTPUT); pinMode(26, OUTPUT); pinMode(27, OUTPUT); pinMode(28, OUTPUT); pinMode(29, OUTPUT); 
  pinMode(30, OUTPUT); pinMode(31, OUTPUT); pinMode(32, OUTPUT); pinMode(33, OUTPUT); pinMode(34, OUTPUT); pinMode(28, OUTPUT); 
  pinMode(29, OUTPUT); pinMode(30, OUTPUT); pinMode(31, OUTPUT);  pinMode(32, OUTPUT);  pinMode(33, OUTPUT);     
  
  digitalWriteFast(LED_BUILTIN, HIGH); 
  delay (2000);  // delay to make sure inputs are ready when read in setup
  three_knob = false;
  if (digitalReadFast(1)== LOW){    // detect three knob controller by seeing e-stop pin to ground
    three_knob = true;
  }    
  if (digitalReadFast(19)== LOW){   // switch 2 used to enable/disable quad output for slave machine
    quad_out = true;
  } 
  if (digitalReadFast(11)== HIGH){  // only do seek home if switch set for option
    while (digitalReadFast(9)== HIGH){  // move to rear limit switch
      ccwpulse_quad();  //      ccwpulse();
      delay (1);
    }  
    for (int i=0; i < 10; i++){ // be sure to be off switch for following test
      cwpulse();
      delay (1);
    }    
  }
  digitalWriteFast(LED_BUILTIN, HIGH); 
//    digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN)); // here just for copying if needed elsewhere  
}
void loop() {
   loop_count ++;
   read_joystick_encoder();
   if (run) stepper.run(); 
   if (pushbutton.update()){ if (pushbutton.risingEdge()) estop(); }  // debounces the e-stop line which is prone to noise
   if (loop_count == pot_read_count){    // pot voltage is persistent so no need to read often, might be able to do it even less?
    loop_count = 0;
    read_pots();  // read encoders and do appropriate things 
   }
//   if (digitalReadFast(10)== LOW)  current_position = abs_max_position;  // keep track of current position using these calls and limit the moves if abs_max will be hit
//   if (digitalReadFast(9)== LOW)  current_position = min_position;            // probably not needed with new limit switch control but left in case it is needed
   while (digitalReadFast(10)== LOW){
        if (quad_out){
            ccwpulse_quad(); 
       }
       else{
            ccwpulse(); 
       }
        delay (1);
        origin_offset ++;              
        read_joystick_encoder();      // probably not needed
        read_pots();
   }  
   while (digitalReadFast(9)== LOW){
       if (quad_out){       // not really necessary because this is so slow but WTF?
            cwpulse_quad(); 
       }
       else{
            cwpulse(); 
       }
        delay (1);
        end_offset ++;
        read_joystick_encoder();      // probably not needed
        read_pots();
   }       
   if (run){    // if three knob is in use and setting accel stepper values then act on them
        if (forward == true) {stepper.moveTo (offset_plus_stroke);}
            else{stepper.moveTo(offset);} // only need to move to new offset position if there is no stroke
//        if (forward == true) {stepper.moveTo (offset_plus_stroke - end_offset);}
//            else{stepper.moveTo(offset + origin_offset);} // only need to move to new offset position if there is no stroke       
       stepper.run(); // don't know why it is needed here too but it is, leave it
       if ((stepper.distanceToGo() == 0) && (stroke > 0)) {
            forward = !forward;	   // reverse direction
        }
     }
     if (run) stepper.run();   // don't know why it is needed here too but it is, leave it
     read_joystick_encoder();
     if (run) stepper.run(); // don't know why it is needed here too but it is, leave it
}
void read_pots(){   // this is the main code. it acts on both joystick encoder and three knob controller
    if (three_knob == true){    // only if three knob was sensed at startup 
         static int speed_index =  (analogRead(15)) /10;   
         motor_speed = speed_table[speed_index];    // will be constrained later in this sub after stroke is set     
         static int stroke_index = (analogRead(17)) / 10;  
         stroke = stroke_table[stroke_index] - end_offset;
//         stroke = stroke_table[stroke_index];
//         if (stroke < vibe_limit) {motor_speed = constrain (motor_speed,0,max_speed_vibe); } 
         stepper.setMaxSpeed(motor_speed);    
         static int offset_index = (analogRead(20)) / 10;  // table has 103 entries     
         offset = offset_table[offset_index] - origin_offset;
//         offset = offset_table[offset_index];         
//         int accel = accel_table[offset_index];   // used for testing different accelerations
//         stepper.setAcceleration(accel ); 
//         offset_plus_stroke = offset + stroke; // can combine these two lines
//         offset_plus_stroke = constrain (offset_plus_stroke,0,max_stroke);
         offset_plus_stroke = constrain ((offset + stroke),0,max_stroke);
         run = true;
         if ((motor_speed < min_speed) || (stroke < min_stroke)) run = false;  // change this to && to allow offset only ???
         if (run) stepper.run();
    }  
} 
void read_joystick_encoder(void){
      joystick_encoder_divider = 2; minus_joystick_encoder_divider = -2;
      if(digitalReadFast(14) == LOW){ joystick_encoder_divider = 1; minus_joystick_encoder_divider = -1;}   // set encoder scaling ratio
      if(digitalReadFast(12) == LOW){ joystick_encoder_divider = 3; minus_joystick_encoder_divider = -3;}   
      int read_joystick = readEnc1.read();
      if (read_joystick < minus_joystick_encoder_divider){  // cw encoder movement
        if (quad_out){       // use the quad version
            if (digitalReadFast(10) == HIGH) cwpulse_quad();   
            readEnc1.write(0);    // this must be here or it can accumulate high values and delay moving off limit switch              
        }
        else{
            if (digitalReadFast(10) == HIGH) cwpulse();
            readEnc1.write(0);    // this must be here or it can accumulate high values and delay moving off limit switch           
        }
      }
      if (read_joystick > joystick_encoder_divider){  // ccw encoder movement
        if (quad_out){       // use the quad version
            if (digitalReadFast(10) == HIGH) ccwpulse_quad();
            readEnc1.write(0);    // this must be here or it can accumulate high values and delay moving off limit switch              
       }
       else{
            if (digitalReadFast(10) == HIGH) ccwpulse();    // cwpulse(); cwpulse_quad()
            readEnc1.write(0);    // this must be here or it can accumulate high values and delay moving off limit switch           
       }
    }
}
void cwpulse(void){
    digitalWriteFast(4, LOW); // pin 4 low for CW 
    delayMicroseconds(3);  //dir must preceed step by 200 ns
    for (int i=0; i < joystick_encoder_multiplier; i++){
      digitalWriteFast(3, HIGH); // step occurs on positive transition
      delayMicroseconds(3);      // needs to be 2, 1 is not enough for Chinese stepper might be enough for Gecko
      digitalWriteFast(3, LOW); // pin 3 remains low until next step pulse
      delayMicroseconds(3);   // needed for time to read the analog input 
    }
} 
void ccwpulse(void){
    digitalWriteFast(4, HIGH); // pin 4 high for CCW 
    delayMicroseconds(3);  //dir must preceed step by 200 ns
     for (int i=0; i < joystick_encoder_multiplier; i++){
      digitalWriteFast(3, HIGH); // pin 3 high
      delayMicroseconds(3);  
      digitalWriteFast(3, LOW); // pin 3 low
      delayMicroseconds(3);   // needed for time to read the analog input 
    }    
}
void cwpulse_quad(void){  // combined pulse and quad out 
    digitalWriteFast(4, LOW); // pin 4 low for CW 
    delayMicroseconds(1);   // direction needs to preceed step
    for (int i=0; i < joystick_encoder_multiplier; i++){
      digitalWriteFast(22, LOW);  // A phase leads
      digitalWriteFast(3, HIGH);
      delayMicroseconds(2);
      digitalWriteFast(23, LOW);
      digitalWriteFast(3, LOW);
      delayMicroseconds(2);
      digitalWriteFast(22, HIGH);
      delayMicroseconds(2);
      digitalWriteFast(23, HIGH); 
      delayMicroseconds(2);     
    }
} 
void ccwpulse_quad(void){  // combined pulse and quad out
    digitalWriteFast(4, HIGH); // pin 4 high for CCW 
    delayMicroseconds(1);
    for (int i=0; i < joystick_encoder_multiplier; i++){
      digitalWriteFast(23, LOW);  // B phase leads
      digitalWriteFast(3, HIGH);
      delayMicroseconds(2);  
      digitalWriteFast(22, LOW);
      digitalWriteFast(3, LOW);
      delayMicroseconds(2);
      digitalWriteFast(23, HIGH);
      delayMicroseconds(2);
      digitalWriteFast(22, HIGH);
      delayMicroseconds(2);
    }    
}
void estop(){
  motor_speed = 0;
  offset = 0;
  origin_offset = 0;
  end_offset = 0;
  stepper.setMaxSpeed(motor_speed); 
  stroke = 0;
  offset_plus_stroke = 0;
  run = false;
  while (digitalReadFast(9)== HIGH){  // 
    ccwpulse();
    delayMicroseconds(3000);    
  }    
  boolean reset = false;
  while (reset == false){
     int index =  (analogRead(15)) /10;
     int estop_speed = speed_table[index];    // will be constrained later in this sub after stroke is set     
     index = (analogRead(17)) / 10;  
     int estop_stroke = stroke_table[index];
     if((estop_speed < 10) && (estop_stroke < 25)) reset =  true;   // both pots turned down almost to 0
  }
}

/*    
scratch pad
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

encoder reading stuff be sure to declare the encoders
void get_speed(void){
      read_speed_encoder = readEnc1.read();  
      if (read_speed_encoder != 0){
            motor_speed +=  (read_speed_encoder);
            motor_speed = constrain (motor_speed,0,max_speed);
            if (run) stepper.run();
            readEnc1.write(0);
      }
}
void get_stroke(void){  // try using joy encoder to set stroke  use estop button to choose what is done with encoder
     read_stroke_encoder = readEnc1.read();
     if (read_stroke_encoder != 0){
          stroke += (read_stroke_encoder);
          stroke = constrain (stroke,0,max_stroke);
          if (stroke < vibe_limit) {motor_speed = constrain (motor_speed,0,max_speed_vibe); } // vibe limit is the limit used when stroke is short to allow for vibration faster than with longer strokes
          else{motor_speed = constrain (motor_speed ,0,max_speed);} 
          stepper.setMaxSpeed(motor_speed);     
          if (run) stepper.run();
          readEnc1.write(0);
      }      
}
void do_offset(void){
      read_offset_encoder = readEnc4.read(); 
      if (read_offset_encoder < 0) { 
        if (digitalReadFast(9) == HIGH){
            for (int i=0; i < offset_encoder_multiplier; i++){
                ccwpulse_quad();    // ccwpulse();
            }         
        } 
        readEnc4.write(0);
      } 
      else if (read_offset_encoder > 0) {
         if (digitalReadFast(10) == HIGH){
            for (int i=0; i < offset_encoder_multiplier; i++){
                cwpulse_quad();    // cwpulse();
            }           
         }
         readEnc4.write(0);
      }
      readEnc4.write(0);
}
*/    
