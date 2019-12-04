/*
 * Author: Joey Broussard
 * PNI, 20191105
 * 
 * Control of the stepper motor performed on dedicated arduino
 * under control of this function. Motor states can be updated
 * via the html GUI, which are then passed to this slave arduino
 * by the master arduino controlling hardware signals. Messages
 * passed via I2C protocol.
 * 
 * todo: add functionality for inactivating the holding voltage
 * on the motor if "free mode" is selected. Requires pulling 
 * "SLP" on the Easystepper Driver to LOW.
 */

#include "Arduino.h"
#include <AccelStepper.h>
#include <Wire.h> //I2C library

//Pins
int slpPin = 9;
int stepPin = 11;
int dirPin = 12;

//Setting up the stepper object from AccelStepper library
AccelStepper stepper(AccelStepper::DRIVER,stepPin,dirPin);//motor,dir

//Setting up I2C transfer buffer
union Buffer{
  unsigned long longNumber;
  byte longBytes[4];
};
Buffer buff;

//Structure for Motor state
struct myMotor{
  int useMotor;
  boolean goCommand;
  long currentSpeed;
};

typedef struct myMotor Motor;
Motor motor;


void setup()
{  
  //Setup pin for deactivating the motor
  pinMode(slpPin,OUTPUT);
  digitalWrite(slpPin,LOW);//LOW = Motor in free run until receives commands
  
   //Motor command states
   motor.useMotor = false;
   motor.goCommand = false;
   motor.currentSpeed = 0;

   //Direction pin changes direction of running
   pinMode(dirPin,OUTPUT);
   digitalWrite(dirPin,HIGH);

   //Stepper initial conditions
   stepper.setMaxSpeed(0);//Initially motor is not running
   stepper.setAcceleration(80);
   
   //Initialize for I2C communcation as slave
   Wire.begin(8);
   Wire.onReceive(receiveEvent);

   //Initialize serial
   Serial.begin(9600);
}

void receiveEvent(int howMany){
  //Parse incoming I2C stream
    //First byte (0 = go command, 1 = useMotor state, 2 = motorSpeed state)
    //Second byte, (TF = go command/new useMotor, int = new motorSpeed
  int whichState = Wire.read();
  if (whichState==0){
    motor.goCommand = Wire.read();//boolean
    Serial.print("goCommand = ");
    Serial.println(motor.goCommand); 
  } else if (whichState==1){//0 = motorOn, 1 = motorLocked, 2 = motorFree
    motor.useMotor = Wire.read();
    Serial.print("useMotor = ");
    Serial.println(motor.useMotor);
    if (motor.useMotor==2){
      digitalWrite(slpPin,LOW);
      Serial.print("Sleep pin active");
    } else if (motor.useMotor==1) {
      stepper.stop();
      digitalWrite(slpPin,HIGH);
      Serial.print("Sleep pin inactive, slowing to stop");
    } else if (motor.useMotor==0) {
      stepper.moveTo(500000000);//Move to a really large number so that motor won't stop
      digitalWrite(slpPin,HIGH);
      Serial.print("Sleep pin inactive, accelerating to new speed");
    }
  } else if (whichState==2){
    //Read remaining bytes available on wire
    for (int i = 0; i <= howMany-1; i++){
      buff.longBytes[i] = Wire.read();
    }
    //Serial.print("I received 2, msg = ");
    //Serial.println(buff.longNumber);
    motor.currentSpeed = buff.longNumber;
    stepper.setMaxSpeed(motor.currentSpeed);
    Serial.print("I set the speed to ");
    Serial.println(motor.currentSpeed);
  }
}



void loop()
{  
  if (motor.goCommand){
    stepper.run();
  } 
   delay(0.1);
   
}
