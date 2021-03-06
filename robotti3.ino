#include <Arduino.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <MeAuriga.h>

#include "color.h"

MeGyro gyro_0(0, 0x69);
MeEncoderOnBoard Encoder_1(SLOT1);
MeEncoderOnBoard Encoder_2(SLOT2);
MeUltrasonicSensor sonic(PORT_10);
MeLineFollower lineFinder(PORT_9);
MeLightSensor lightsensor_12(12);
MeRGBLed led( 0, 12 );

const float speedMultiplier = 1.8; // 1.3 menee, 1.8 välillä

const int actionCooldown = 0;

const float hueSpeed = 0.1;

const float track1Val = 75;

const float track2Val = 116;

int lastHueTime = 0;

float sonicValue = 400;
int lastSonicTime = 0;

int lastActionEndTime = 0;

int track = 1; // 1 = track 1, -1 = track 2

float currentHue = 0;
bool finished = false;

float target = 0;

void isr_process_encoder1(void)
{
  if(digitalRead(Encoder_1.getPortB()) == 0){
    Encoder_1.pulsePosMinus();
  }else{
    Encoder_1.pulsePosPlus();
  }
}
void isr_process_encoder2(void)
{
  if(digitalRead(Encoder_2.getPortB()) == 0){
    Encoder_2.pulsePosMinus();
  }else{
    Encoder_2.pulsePosPlus();
  }
}
void move(int direction, int speed)
{
  speed = speed*speedMultiplier;
  if (speed > 195){
    speed = 195;
  }
  int leftSpeed = 0;
  int rightSpeed = 0;
  if(direction == 1){
    leftSpeed = -speed;
    rightSpeed = speed;
  }else if(direction == 2){
    leftSpeed = speed;
    rightSpeed = -speed;
  }else if(direction == 3){
    leftSpeed = -speed;
    rightSpeed = -speed;
  }else if(direction == 4){
    leftSpeed = speed;
    rightSpeed = speed;
  }
  Encoder_1.setMotorPwm(leftSpeed);
  Encoder_2.setMotorPwm(rightSpeed);
}
void turn (double diff){
  Serial.println("turn");
    target = (gyro_0.getAngle(3) + diff);
    if(target > 180){
        target += -360;

    }
    if(target < -180){
        target += 360;

    }
    while(!(abs(target - gyro_0.getAngle(3)) <= 0.25f))
    {
      _loop();
      float diff = ((target - gyro_0.getAngle(3)));
      if(diff < 0){
          diff += 360;

      }
      if(diff > 180){
          move(3, 20 / 100.0 * 255);
      }else{
          move(4, 20 / 100.0 * 255);
      }

    }
    Encoder_1.setMotorPwm(0);
    Encoder_2.setMotorPwm(0);
    lastActionEndTime = millis();
}

void _delay(float seconds) {
  if(seconds < 0.0){
    seconds = 0.0;
  }
  long endTime = millis() + seconds * 1000;
  while(millis() < endTime) _loop();
}

void setup() {
  led.setpin( 44 );
  Serial.begin(115200);
  gyro_0.begin();
  TCCR1A = _BV(WGM10);
  TCCR1B = _BV(CS11) | _BV(WGM12);
  TCCR2A = _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS21);
  attachInterrupt(Encoder_1.getIntNum(), isr_process_encoder1, RISING);
  attachInterrupt(Encoder_2.getIntNum(), isr_process_encoder2, RISING);

  // getTrack();

  if(track == 1){
    track1();
  }else if (track == 2){
    track2();
  }

  finished = true;
  Serial.println("Done");
}

void _loop() {
  if(millis() > lastSonicTime + 100){
    sonicValue = sonic.distanceCm();
    lastSonicTime = millis();
  }
  gyro_0.update();
  Encoder_1.loop();
  Encoder_2.loop();
  
  int sinceLastHue = millis()-lastHueTime;
  if(!finished){
    
    currentHue += (hueSpeed*sinceLastHue);
    if(currentHue > 360){
      currentHue-=360.f;
    }
  }else{
    currentHue = 90;
  }

  int r,g,b;
  HSV_to_RGB(currentHue, 100, 75, r,g,b);
  led.setColor(0,r,g,b);
  led.show();
  lastHueTime = millis();
}

void moveFwd(float time, int percentage){
  Serial.println("moveFwd");
  move(1, percentage/ 100.0 * 255);
  _delay(time);
  move(1,0);
  lastActionEndTime = millis();
}

void moveFwdUntil(float distance, int percentage){
  Serial.println("moveFwdUntil");
  move(1, percentage/ 100.0 * 255);
  while(sonicValue > distance){
    _loop();
  }
  move(1,0);
  lastActionEndTime = millis();
}

void getTrack(){
  turn(-90);
  moveFwd(0.1, 20);
  _delay(1);
  const float diff1 = abs(sonicValue - track1Val);
  const float diff2 = abs(sonicValue - track2Val);

  Serial.println(sonicValue);

  if(diff1 < diff2 && sonicValue < 399){ // is sonic nearer to track1 value or track 2 value
    Serial.println("Track 1");
    track = 1;
  }else if(sonicValue < 399){
    Serial.println("Track 2");
    track = 2;
  }else{
    Serial.println("Sonic 400, choosing track 2");
    track = 2;
  }
  _delay(1);
  turn(90);
}

void followLine(int percentage, float endTime){
  Serial.println("followLine");
  int sensorState = lineFinder.readSensors();
  while(sensorState == S1_IN_S2_IN){
    sensorState = lineFinder.readSensors();
    move(1, percentage);
    _loop();
  }
  move(1,0);
  while(sensorState != S1_IN_S2_IN){ //  || lastActionEndTime > millis()+actionCooldown 

    _loop();
    sensorState = lineFinder.readSensors();
    switch (sensorState)
    {
    case S1_IN_S2_OUT:
      move(4, percentage);
      break;
    case S1_OUT_S2_IN:
      move(3,percentage);
      break;
    case S1_OUT_S2_OUT:
      move(1,percentage);
      break;
    default:
      move(1,percentage);
      break;
    }
  }
  move(1,percentage);
  _delay(endTime);
  move(1,0);
}

/*
  --- TRACK 1 ---
*/

void track1(){
  // follow line 1st
  followLine(30, 0.20);

  // turn to bump
  turn(-90);

  // follow line to bump
  followLine(30, 0);

  // bump
  moveFwd(0.3, 50);

  // follow line after bump
  followLine(40,0.1); // bit more power to get through

  // turn after bump to ramp
  turn(-90);

  
  // ramp
  followLine(30, 0);

  // break after ramp
  moveFwd(0.3, 30);
  
  // follow after break
  followLine(30, 0.1);

  // corner 1 to 2
  turn(-90);

  // side 2
  followLine(30, 0.1);

  // corner 2 to 3
  turn(-90);

  // side 3
  followLine(30, 0.1);

  // corner 3 to 4
  turn(-90);

  // side 4 and break
  followLine(30, 0);

  followLine(30, 0);
  

  // turn back to ramp
  turn(90);


  // follow ramp
  followLine(40, 0); // a bit more power to get through bump at ramp start

  // turn after ramp
  turn(90);
  
  // followline to bump
  followLine(30,0);

  // bump
  moveFwd(0.25, 50);

  // afterbump
  followLine(40, 0.1);
  
  // turn to end
  turn(90);
  // followline to end
  followLine(30,0);
}

/*
  --- TRACK 2 ---
*/

void track2(){
  // follow line 1st
  followLine(30, 0.30);

  // turn to bump
  turn(90);

  // follow line to bump
  followLine(30, 0);

  // bump
  moveFwd(0.25, 60);

  // follow line after bump
  followLine(30,0.1);

  // turn after bump to ramp
  turn(90);

  // ramp
  followLine(30, 0);

  // break after ramp
  moveFwd(0.3, 30);
  
  // follow after break
  followLine(30, 0.3);

  // corner 1 to 2
  turn(90);

  // side 2
  followLine(30, 0.3);

  // corner 2 to 3
  turn(90);

  // side 3
  followLine(30, 0.3);

  // corner 3 to 4
  turn(90);

  // side 4 and break
  followLine(30, 0.5);

  // turn back to ramp
  turn(-90);

  // follow ramp
  followLine(30, 0.3);

  // turn after ramp
  turn(-90);

  // followline to bump
  followLine(30,0.3);

  // bump
  moveFwd(0.25, 60);

  // afterbump
  followLine(30, 0);
  
  // turn to end
  turn(-90);
  // followline to end
  followLine(30,0.5);
}

void loop() {
  _loop();
}