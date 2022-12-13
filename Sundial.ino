#include <Stepper.h>
#include <NTPClient.h>
#include <WiFi101.h>
#include <WiFiUdp.h>

// Wifi information
const char *ssid     = "Brown-Guest";
const char *password = "";

// Input/output pin numbers
int inc_hour = 0;
int dec_hour = 1;
int stepper_set_switch = 2;
int light = 3;
int potentiometer = A1;

// physical parameters
const int ClockResolution = 10; // How many tick marks per hour (should be a factor of 60)
const int StepsMotor = 32; // How many steps per full revolution at motor 360/(5.625)
const int gear_ratio = 64; // How many rotations of stepper to one rotation at output (64:1 gear ratio is built in)
// ---- computed constants ----
const int MinutesPerTick = 60 / ClockResolution; // How many minutes between each clock tick
const int STEPS = StepsMotor * gear_ratio; 
const int VirtSTEPS = ClockResolution * 12; // How many virtual steps do we want per revolution
const int VirtStepSize = STEPS / VirtSTEPS; // Hpw many real steps to approximate one virtual step
const int StepDiff = STEPS - (VirtStepSize * VirtSTEPS); // How many additional steps are required to return to the 0 position after a full virtual revolution
// ---- other globals ----
int hour_offset = 0; // hour offset since time defaults to G
bool timezone_update = false; // true signals main loop to update dial position prematurely (used when hour offset changes)
Stepper clockStepper = Stepper(StepsMotor, 11, 9, 10, 8); // base stepper object (pin order needs to be checked experimentally)
int StepperVirtPosition = 0; // Position of stepper in virtual steps
WiFiUDP ntpUDP; // udp instance for ntp client
NTPClient timeClient(ntpUDP); // ntp client instance for pulling time over wifi

typedef enum {
  s_CALIBRATING = 1,
  s_WAITING = 2,
  s_STEP = 3
} state;

state CURRENT_STATE;
int saved_clock;
bool timezone_update_inc, timezone_update_dec;


// Increase the hour offset by 1
void IncrementHour(){
  //hour_offset += 1;
  Serial.println("Setting  hour forward");
  //timeClient.setTimeOffset(hour_offset * 60 * 60);
  timezone_update_inc = true; // do not directly call VirtualStep within the interupt handler
}
// Decrease the hour offset by 1
void DecrementHour(){
  //hour_offset -= 1;
  Serial.println("Setting  hour back");
  //timeClient.setTimeOffset(hour_offset * 60 * 60);
  timezone_update_dec = true; // do not directly call VirtualStep within the interupt handler
}

void setup() {
  // pin initializaton
  pinMode(inc_hour, INPUT);
  pinMode(dec_hour, INPUT);
  pinMode(stepper_set_switch, INPUT);
  pinMode(potentiometer, INPUT);
  
  attachInterrupt(digitalPinToInterrupt(inc_hour), IncrementHour, FALLING);
  attachInterrupt(digitalPinToInterrupt(dec_hour), DecrementHour, FALLING);
  // wifi initialization
  WiFi.begin(ssid);
  while ( WiFi.status() != WL_CONNECTED ) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("setup");

  // Start ntp client for time synchronization
  timeClient.begin();
  timeClient.update();

  
  // Stepper config and test
//  CalibrateStepper();
//  printStepperConfig();
//  Serial.println("Testing");
  clockStepper.setSpeed(60);
  //delay(3000);
  //TestStepper();
  Serial.println("testing done");

  initialize_watchdog();

  timezone_update_inc = false;
  timezone_update_dec = false;
  saved_clock = millis();

  CURRENT_STATE = s_CALIBRATING;

  //test_all_tests();
}

void initialize_watchdog() {
  
  NVIC_DisableIRQ(WDT_IRQn);
  NVIC_ClearPendingIRQ(WDT_IRQn);
  NVIC_SetPriority(WDT_IRQn, 0);
  NVIC_EnableIRQ(WDT_IRQn);

  // TODO: Configure and enable WDT GCLK:
  GCLK->GENDIV.reg = GCLK_GENDIV_DIV(4) | GCLK_GENDIV_ID(5);
  while (GCLK->STATUS.bit.SYNCBUSY);
  // set GCLK->GENCTRL.reg and GCLK->CLKCTRL.reg

  GCLK->GENCTRL.reg = GCLK_GENCTRL_DIVSEL | GCLK_GENCTRL_ID(5) | GCLK_GENCTRL_GENEN | GCLK_GENCTRL_SRC_OSCULP32K;
  while (GCLK->STATUS.bit.SYNCBUSY);

  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK5 | GCLK_CLKCTRL_ID(3);

  // TODO: Configure and enable WDT:
  // use WDT->CONFIG.reg, WDT->EWCTRL.reg, WDT->CTRL.reg
  WDT->CONFIG.reg = 9;
  WDT->EWCTRL.reg = 8;
  WDT->CTRL.reg = WDT_CTRL_ENABLE;
  while (WDT->STATUS.bit.SYNCBUSY);
  
  WDT->INTENSET.reg = WDT_INTENSET_EW;

  Serial.println("Initialized WDT!");
}

void pet_watchdog() {
  WDT->CLEAR.reg = WDT_CLEAR_CLEAR_KEY;
}


typedef enum {
  FORWARD = 0,
  REVERSE = 1
} DIR; 

void StepIndicator(int dir){
  int sign;
  if (dir == FORWARD){
    sign = 1;}
  else if (dir == REVERSE){
    sign = -1;
  }
  clockStepper.step(VirtStepSize*sign);
  if (StepperVirtPosition == 0 && sign == -1){
      clockStepper.step(-StepDiff);
      StepperVirtPosition = VirtSTEPS - 1;
  }else if (StepperVirtPosition + 1 == VirtSTEPS && sign == 1) {
      clockStepper.step(StepDiff);
      StepperVirtPosition = 0;
  }else{
     StepperVirtPosition += sign;
  }
}
// Brings the dial back to its set point at position 0
void CalibrateStepper(){
  clockStepper.setSpeed(30);
  while (digitalRead(stepper_set_switch) != HIGH){
    clockStepper.step(1); // calibrate in actual steps to prevent error build up over time
  }
  StepperVirtPosition = 0;
  Serial.println("Stepper calibrated");
}
// Return the current time in minutes
int getTimePosition(){
  timeClient.update();
  String time = timeClient.getFormattedTime();
  int hour = time.substring(0, 2).toInt() % 12;
  int minute = time.substring(3, 5).toInt();
  int time_position = (hour * 60 + minute) / MinutesPerTick;
//  Serial.print(hour);
//  Serial.print(" | ");
//  Serial.print(minute);
//  Serial.print(" | ");
//  Serial.println(time_position);
  return time_position;
}

//void loop() {
//  int time_pos = getTimePosition();
//
//  int numSteps = time_pos - StepperVirtPosition;
//  Serial.println("starting long step");
//  for (int i = 0; i < numSteps; i++) {
//    VirtualStep(1);
//    pet_watchdog();
//  }
////  Serial.println("started step");
////  VirtualStep(time_pos - StepperVirtPosition);
//  Serial.println("finished long step");
//  
//  //Serial.println(StepperVirtPosition);
//  // Wait ~2 minutes, but check for time zone updates every second
//  for (int i = 0; i < 120; i++){
//    delay(1000);
//    if (timezone_update){
//      timezone_update = false;
//      break;
//    }
//    pet_watchdog();
//  }
//
//}


void WDT_Handler() {
  // TODO: Clear interrupt register flag
  // (reference register with WDT->register_name.reg)

  Serial.println("warning!");
  WDT->INTFLAG.reg = 1;
  
  // TODO: Warn user that a watchdog reset may happen

}

void loop() {
  //Serial.println(analogRead(potentiometer));
  analogWrite(light, map(analogRead(potentiometer), 0, 1024, 0, 255));
  bool cal_switch_on = digitalRead(stepper_set_switch) == HIGH;
  int time_pos = getTimePosition();
  CURRENT_STATE = update_fsm(CURRENT_STATE, millis(), cal_switch_on, timezone_update_inc, timezone_update_dec, time_pos);
  delay(10);
  pet_watchdog();
}


state update_fsm(state cur_state, long mils, bool cal_switch_on, bool inc_timezone, bool dec_timezone, int time_pos) {
  int position_difference = time_pos - StepperVirtPosition;
  state next_state;
  Serial.print("LT : ");
  Serial.print(analogRead(potentiometer));
  Serial.print(" STATE :");
  Serial.print(cur_state);
  Serial.print(" POS : ");
  Serial.print(StepperVirtPosition);
  Serial.print(" CLK : ");
  Serial.println(time_pos);
  switch(cur_state) {
    case s_CALIBRATING:
      if (cal_switch_on) {
        next_state = s_STEP;
      } else {
        clockStepper.step(1);
        next_state = s_CALIBRATING;
      }
      break;
    case s_STEP:
      if (time_pos == StepperVirtPosition) {
          next_state = s_WAITING;
      } else if (abs(position_difference) <= (VirtSTEPS / 2)) {
          StepIndicator((position_difference > 0) ? FORWARD : REVERSE);
          next_state = s_STEP;
      } else if (abs(position_difference) > (VirtSTEPS / 2)) {
          StepIndicator((position_difference < 0) ? FORWARD : REVERSE);
          next_state = s_STEP;
      }
      saved_clock = mils;
      break;
    case s_WAITING:
      if (mils - saved_clock >= 120000) {
        next_state = s_STEP;
      } else if (inc_timezone) {
        hour_offset += 1;
        timeClient.setTimeOffset(hour_offset * 60 * 60);
        timezone_update_inc = false;
        next_state = s_STEP;
      } else if (dec_timezone) {
        hour_offset -= 1;
        timeClient.setTimeOffset(hour_offset * 60 * 60);
        timezone_update_dec = false;
        next_state = s_STEP;
      } else {
        next_state = s_WAITING;
      }
      break;
  }
  return next_state;
}
