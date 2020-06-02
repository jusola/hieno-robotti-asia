/**
 * \par Copyright (C), 2012-2016, MakeBlock
 * @file    Me_Auriga_encoder_direct.ino
 * @author  MakeBlock
 * @version V1.0.0
 * @date    2016/07/14
 * @brief   Description: this file is sample code for auriga encoder motor device.
 *
 * Function List:
 *    1. uint8_t MeEncoderOnBoard::getPortB(void);
 *    2. uint8_t MeEncoderOnBoard::getIntNum(void);
 *    3. void MeEncoderOnBoard::pulsePosPlus(void);
 *    4. void MeEncoderOnBoard::pulsePosMinus(void);
 *    5. void MeEncoderOnBoard::setMotorPwm(int pwm);
 *    6. double MeEncoderOnBoard::getCurrentSpeed(void);
 *    7. void MeEncoderOnBoard::updateSpeed(void);
 *
 * \par History:
 * <pre>
 * <Author>     <Time>        <Version>      <Descr>
 * Mark Yan     2016/07/14    1.0.0          build the new
 * </pre>
 */

#include <MeAuriga.h>

MeEncoderOnBoard Encoder_1(SLOT1);
MeEncoderOnBoard Encoder_2(SLOT2);
MeLineFollower lineFinder(PORT_9);

const float startSpeed = 0.5;

float turnDirection = 0; // 0 = ahead, 1 = right, -1 = left
float speed = startSpeed; //0 to 1
float forward = 1;

int timeOutsideLine = 0;
int timeOutsideLineThreshold = 25; // time before backing up
int timeOutsideLineThresholdBack = 35;

const int motorPWM1 = -100;
const int motorPWM2 = 100;

void isr_process_encoder1(void)
{
  if(digitalRead(Encoder_1.getPortB()) == 0)
  {
    Encoder_1.pulsePosMinus();
  }
  else
  {
    Encoder_1.pulsePosPlus();;
  }
}

void isr_process_encoder2(void)
{
  if(digitalRead(Encoder_2.getPortB()) == 0)
  {
    Encoder_2.pulsePosMinus();
  }
  else
  {
    Encoder_2.pulsePosPlus();
  }
}

void setup()
{
  attachInterrupt(Encoder_1.getIntNum(), isr_process_encoder1, RISING);
  attachInterrupt(Encoder_2.getIntNum(), isr_process_encoder2, RISING);
  Serial.begin(115200);
  
  //Set PWM 8KHz
  TCCR1A = _BV(WGM10);
  TCCR1B = _BV(CS11) | _BV(WGM12);

  TCCR2A = _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS21);
}

void loop()
{
  Encoder_1.updateSpeed();
  Encoder_2.updateSpeed();
  int sensorState = lineFinder.readSensors();
  
  switch(sensorState)
  {
    case S1_IN_S2_IN: // both out -- forward a bit, if no line, back and turn
      if(timeOutsideLine == 0){
        speed = 2.5*startSpeed;
        turnDirection = 0;
        forward = 1;
      }
      if(timeOutsideLine > timeOutsideLineThreshold){
        speed = 2.5*startSpeed;
        turnDirection = 0;
        forward = -1;
      }
      if(timeOutsideLine > timeOutsideLineThreshold+timeOutsideLineThresholdBack){
        speed = startSpeed / 2;
        forward = 1;
        turnDirection = 1;
      }
      timeOutsideLine++;
      break;
    case S1_IN_S2_OUT: // left out - turn right
      timeOutsideLine = 0;
      forward = 1;
      speed = startSpeed;
      turnDirection = 1;
      break;
    case S1_OUT_S2_IN: // right out - turn left
      timeOutsideLine = 0;
      forward = 1;
      speed = startSpeed;
      turnDirection = -1;
      break;
    case S1_OUT_S2_OUT: // both in -- ahead
      timeOutsideLine = 0;
      forward = 1;
      speed = startSpeed;
      turnDirection = 0;
      break;
    default:
      forward = 1;
      speed = startSpeed;
      turnDirection = 0;
      break;
  }
  drive();
}

void drive()
{
  float left = speedL(turnDirection, speed*forward);
  float right = speedR(turnDirection, speed*forward);
  setSpeeds(left, right);
}

void setSpeeds(float left, float right){
  Encoder_1.setMotorPwm(left*motorPWM1);
  Encoder_2.setMotorPwm(right*motorPWM2);
}

float speedL(float turn, float speed){
  return speed*((-2)*turn+1);
}

float speedR(float turn, float speed){
  return speed*(2*turn+1);
}