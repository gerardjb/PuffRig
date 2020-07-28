/*
 * Author: Joey Broussard
 * PNI, 20200723
 * 
 * V1.0 - Adapted from eyeblink3_4cpp.ino
 */

//For information on exposing low-vevel interrupts on Arduino Uno
// http://www.geertlangereis.nl/Electronics/Pin_Change_Interrupts/PinChange_en.html

#include "Arduino.h"
#include <Wire.h>

#include <Encoder.h> // http://www.pjrc.com/teensy/td_libs_Encoder.html


/////////////////////////////////////////////////////////////
/*Structure and state definitions*/
//Defining trial structure with associated attributes
struct trial
{
  //Session timing and numbering
  boolean sessionIsRunning;//flag for starting and stopping session
  int sessionNumber;//correspnds to session number
  unsigned long sessionStartMillis;//ms time at which session starts
  unsigned long sessionDur; //ms, numEpoch*epochDur
  //Trial timing and numbering
  boolean trialIsRunning;
  int currentTrial;
  unsigned long trialDur;//ms trial duration
  unsigned long numTrial;//number of trials we want
  unsigned long trialStartMillis; //ms time trial starts
  unsigned long interTrialInterval; //ms inter-trial interval 
  unsigned long ITIstartMillis;//ms time at which interTrialInterval starts
  //Trial pin
  boolean pinOnOff;//controls transitioning pin state
  int trialPin;//pin for projecting current trial state
  //Puff stuff
  unsigned long PuffStartMillis; //millis at start of current Puff
  unsigned long prePuffDur; //ms time in trial before puff
  unsigned long puffNum; //number of puffs to be applied
  float puffFreq; //frequency of puff delivery
  
};

unsigned long msIntoSession;
unsigned long msIntoTrial;
unsigned long interTrialInterval;//ms
unsigned long sumITI;//ms holds sum of all ITIs to calculate when to end session
boolean inPrePuff;
boolean inPuff;
int iPuff;
int tmpTrial;


struct rotaryencoder
{
  int pinA = 2; // use pin 2
  int pinB = 3; // use pin 3
  signed long pos = 0; //setting initial position of encoder
  long count = 0; //setting up to query the position only once every 100 ms
};

struct ledPuff
{
  boolean isOnLED; //
  int ledPin; // 
};


//Version, defining structures and aliases
String versionStr = "puffStim1_0.cpp";
typedef struct trial Trial;
typedef struct rotaryencoder RotaryEncoder;
typedef struct ledPuff LedPuff;


//Instances of all hardware-associated structures
Trial trial;
RotaryEncoder rotaryencoder;
LedPuff ledPuff;

//Stepper and Encoder objects defined per relevant libraries
Encoder myEncoder(rotaryencoder.pinA, rotaryencoder.pinB);

/////////////////////////////////////////////////////////////
/*Setup, mostly declaring default structure values*/
void setup()
{
  //trial
  trial.sessionIsRunning = false;
  trial.sessionNumber = 0;
  trial.sessionStartMillis = 0;

  trial.trialIsRunning = false;
  trial.trialDur = 1000; // epoch has to be >= (preDur + xxx + postDur)
  trial.numTrial = 2;
  
  trial.sessionDur = (trial.numTrial*trial.trialDur); //

  trial.prePuffDur = 250;
  trial.puffNum = 2;
  trial.puffFreq = 10;
  trial.interTrialInterval = 500;//ms
  trial.ITIstartMillis = 0;//ms

  trial.trialPin = 7;//pin for conveying trial state
  trial.pinOnOff = false;//trial didn't just end
  pinMode(trial.trialPin, OUTPUT);
  digitalWrite(trial.trialPin, LOW);
  
  sumITI = 0;

  //rotary encoder
  rotaryencoder.pinA = 3;
  rotaryencoder.pinB = 2;
  //
  
  //Activate pin 13 for testing
  pinMode(13, OUTPUT);
  digitalWrite(13,LOW);

  //CS/US structure and pin settings, intially at Arduino grnd
  ledPuff.ledPin = 5;
  ledPuff.isOnLED = false;
  pinMode(ledPuff.ledPin,OUTPUT);
  digitalWrite(ledPuff.ledPin,LOW);
  
  //Initialize serial
  Serial.begin(115200);
 
}

/////////////////////////////////////////////////////////////
/*Starting and ending Trials (sessions)*/
//Start session
void startSession(unsigned long now) {
  if (trial.trialIsRunning==false) {
    trial.sessionNumber = 1;
    
    trial.sessionStartMillis = now;
    trial.trialStartMillis = now;

    trial.trialDur = trial.puffNum * 1/trial.puffFreq * 1000 + 500;
    serialOut(now, "sessionDur",0);
    serialOut(now, "numTrial", trial.numTrial);
    serialOut(now, "trialDur", trial.trialDur);
    trial.currentTrial = 0;
    
    serialOut(now, "startSession", trial.sessionNumber);
    serialOut(now, "startTrial", trial.currentTrial);

    trial.sessionIsRunning = true;
    trial.trialIsRunning = true;
    interTrialInterval = trial.interTrialInterval*1000;
    
  }
}

//Start trial
void startTrial(unsigned long now){
  if (trial.trialIsRunning==false){
    trial.currentTrial += 1;

    trial.trialDur = trial.puffNum * 1/trial.puffFreq * 1000 + 500;
    trial.trialStartMillis = now;
    
    serialOut(now,"startTrial",trial.currentTrial);
    serialOut(now, "numTrial", trial.numTrial);
    serialOut(now, "trialDur", trial.trialDur);
    
    trial.trialIsRunning = true;


  }
}

//End trial
void stopTrial(unsigned long now) {

  //If this is the last trial, end session
  if (trial.currentTrial == trial.numTrial) {
  stopSession(now);
  }
  //
  trial.trialIsRunning = false;
  serialOut(now, "stopTrial", trial.currentTrial);
  
  //Wait for next trial params to become available
  int i = 0;
  while (Serial.available() == 0) {
	delay(10);
	serialOut(now,"Waiting",i);
	i+=i;
  }
  //Now collect them
  while (Serial.available() > 0) {
	String inString = Serial.readStringUntil('\n');
    inString.replace("\n","");
    inString.replace("\r","");
    SerialIn(now, inString);
    delay(10);
  }
  
  //Set time to wait until next trial starts
  now = millis();//cause we don't know how long we'll have to wait to receive signal
  interTrialInterval = trial.interTrialInterval*1000;
  //delay(interTrialInterval);
  trial.ITIstartMillis = now;
  //sum ITIs so they can be counted towards total session time
  sumITI = sumITI + interTrialInterval;
  
}

//End Session
void stopSession(unsigned long now) {
    if (trial.pinOnOff){
    digitalWrite(trial.trialPin,LOW);
    digitalWrite(13,LOW);
    trial.pinOnOff = false;
    }
    trial.sessionIsRunning = false;
    serialOut(now,"stopSession",trial.sessionNumber);
    
    if (trial.trialIsRunning){
      trial.trialIsRunning = false;
      serialOut(now,"stopTrial",trial.currentTrial);
    }
}
/////////////////////////////////////////////////////////////
/*Communication via serial port */
//Outputting info over the serial port
void serialOut(unsigned long now, String str, unsigned long val) {
  Serial.println(String(now) + "," + str + "," + String(val));
}

//Respond to incoming commands over serial
void SerialIn(unsigned long now, String str) {
  String delimStr = ",";
    
  if (str.length()==0) {
    return;
  }
  if (str == "version") {
    Serial.println("version=" + versionStr);
  } else if (str == "startSession") {
    startSession(now);
  } else if (str == "stopSession") {
    stopSession(now);
  }
  else if (str.startsWith("getState")) {
    GetState();
  }
  else if (str.startsWith("settrial")) {
    //set is {set,name,value}
    int firstComma = str.indexOf(delimStr,0);
    int secondComma = str.indexOf(delimStr,firstComma+1);
    String nameStr = str.substring(firstComma+1,secondComma); //first is inclusive, second is exclusive
    String valueStr = str.substring(secondComma+1,str.length());
    SetTrial(nameStr, valueStr);
  }
  else {
    Serial.println("SerialIn() did not handle: '" + str + "'");
  }
    
}

//Get the current experiment parameters;
//This generates the headers for the output files
void GetState() {
  //trial
  Serial.println("sessionNumber=" + String(trial.sessionNumber));
  Serial.println("sessionDur=" + String(trial.sessionDur));

  Serial.println("numTrial=" + String(trial.numTrial));
  Serial.println("trialDur=" + String(trial.trialDur));
  Serial.println("interTrialInteval=" + String(trial.interTrialInterval)); 

  Serial.println("prePuffDur=" + String(trial.prePuffDur));
  
  //Specific for Marlies and Mikhail puff code 
  Serial.println("puffNum=" + String(trial.puffNum));
  Serial.println("puffFreq=" + String(trial.puffFreq));

  
  Serial.println("versionStr=" + String(versionStr));
  
}

//Setting experiment parameters
void SetTrial(String name, String strValue) {
  int value = strValue.toInt();

  //trial
  if (name == "numTrial") {
    trial.numTrial = value;
    Serial.println("trial.numTrial=" + String(trial.numTrial));
  } else if (name=="trialDur") {
    trial.trialDur = value;
    Serial.println("trial.trialDur=" + String(trial.trialDur));
  } else if (name=="interTrialInterval") {
    trial.interTrialInterval = value;
    Serial.println("trial.interTrialInterval=" + String(trial.interTrialInterval));
	interTrialInterval = trial.interTrialInterval;
  } else if (name=="prePuffDur") {
    trial.prePuffDur = value;
    Serial.println("trial.prePuffDur=" + String(trial.prePuffDur));   
  } else if (name=="puffNum") {
    trial.puffNum = value;
    Serial.println("trial.puffNum=" + String(trial.puffNum));
  } else if (name=="puffFreq") {
    trial.puffFreq = value;
    Serial.println("trial.puffFreq=" + String(trial.puffFreq));
    
  }else {
    Serial.println("SetValue() did not handle '" + name + "'");
  }
  
}
/////////////////////////////////////////////////////////////
/*Interacting with hardware components*/
//Rotary encoder
//Updating the position read off of the rotary encoder, dumping
//difference to file if during a trial and changed after >100msec
void updateEncoder(unsigned long now) {
  signed long posNow = myEncoder.read();
  
  if (trial.trialIsRunning && (now-trial.trialStartMillis<3)){
    rotaryencoder.count = now;
    rotaryencoder.pos = posNow;
  }
  
  if (trial.trialIsRunning){
    
    long diff = now - rotaryencoder.count;
    
    if (diff>=100){
      signed long dist =  rotaryencoder.pos - posNow;
      serialOut(now, "rotary", dist);
      rotaryencoder.count = now;
      rotaryencoder.pos = posNow;
    }
  }
  
}

//Triggering the Puff
void updatePuff(unsigned long now){
  
  if (!trial.trialIsRunning) {//wating for trial to begin
	  iPuff = 0;
  }else if(iPuff == trial.puffNum){//got all the puffs

  }else if (trial.trialIsRunning){//starting an stopping puffs
    //Turning Puff on and off while correct trial type running
    unsigned long puffStart = trial.trialStartMillis + trial.prePuffDur + round(1/trial.puffFreq*1000*iPuff);
    unsigned long puffStop = 250 + puffStart;//after 5 millis, turn TTL off
    if (!ledPuff.isOnLED && now >= puffStart && now <= puffStop){
      ledPuff.isOnLED = true;
      trial.PuffStartMillis = now;
      serialOut(now,"Puff",iPuff);
      digitalWrite(ledPuff.ledPin,HIGH);
    } else if(ledPuff.isOnLED && now>puffStop){
      ledPuff.isOnLED = false;
      digitalWrite(ledPuff.ledPin,LOW);
	    iPuff += 1;
    }
  }
}


//Conveying trial state
void updateTrialPin(unsigned long now){
  if (trial.trialIsRunning && !trial.pinOnOff){
    digitalWrite(trial.trialPin,HIGH);
    digitalWrite(13,HIGH);
    trial.pinOnOff = true;
  } else if (!trial.trialIsRunning && trial.pinOnOff){
    digitalWrite(trial.trialPin,LOW);
    digitalWrite(13,LOW);
    trial.pinOnOff = false;
  }
}
/////////////////////////////////////////////////////////////
/*Loop*/
void loop()
{
  //Counting for each session/trial
  unsigned long now = millis();
  msIntoSession = now-trial.sessionStartMillis;
  msIntoTrial = now-trial.trialStartMillis;
  
  if (Serial.available() > 0) {
    String inString = Serial.readStringUntil('\n');
    inString.replace("\n","");
    inString.replace("\r","");
    SerialIn(now, inString);
  }

  //Stop at end of trialDur if trialIsRunning
  if (now > (trial.trialStartMillis + trial.trialDur) && trial.trialIsRunning){
    stopTrial(now);
    //we set ITI inside stopTrial function
  }

  //Start a trial at the end of the ITI period
  if (now>trial.ITIstartMillis + interTrialInterval && !trial.trialIsRunning && trial.sessionIsRunning){
    startTrial(now);
  }

  //Updating all hardware components
  updateEncoder(now);

  updatePuff(now);

  updateTrialPin(now);
  
  delay(1); //ms

}
