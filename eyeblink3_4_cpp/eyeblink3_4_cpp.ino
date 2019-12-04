/*
 * Author: Joey Broussard
 * PNI, 20191030
 * 
 * V2.0 - added pins and timing signals for CS and US, currently assumes 5V digital logic; using inPulse as signal
 * V3.0 - moved motor start into setup block with delay prior to trial start. Here, just give commands to turn motor
 *        on and off and pass motor speed via I2C
 * V3.1 - changing over structure member names for new required fields
 *        across all code
 * V3.2 - test changing all variables across all code
 * V3.3 - now on pi for direct uploads
 * V3.4 - added an output that reflects trial running state,
 *        removed all the fictive scanimage stuff
 * todo: -Get ScanImage incorporated into hardware interactions
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
  unsigned long interTrialIntervalLow; //ms lowest inter-trial interval
  unsigned long interTrialIntervalHigh; //ms highest inter-trial interval  
  unsigned long ITIstartMillis;//ms time at which interTrialInterval starts
  //Trial pin
  boolean pinOnOff;//controls transitioning pin state
  int trialPin;//pin for projecting current trial state
  //CS and US
  unsigned long CSstartMillis; //millis at start of currentPulse
  unsigned long  preCSdur; //ms time in trial before CS
  int CSdur; //ms CS duration
  unsigned long USdur;//ms
  unsigned long CS_USinterval;//ms
  unsigned int percentUS;//percent trials user wants to be US only trials
  unsigned int percentCS;//percent trials user wants to be CS only
  
  //motor
  int useMotor;//{motorOn,motorLocked,motorFree}
  unsigned long motorSpeed; //rev/sec

  
};

unsigned long msIntoSession;
unsigned long msIntoTrial;
unsigned long interTrialInterval;//ms
unsigned long sumITI;//ms holds sum of all ITIs to calculate when to end session
unsigned long tmpCSdur;
boolean inPreCS;
boolean inCS;
int tmpTrial;
String stimPairType;// rng used to determine CS_US, CS, or US trial type


struct rotaryencoder
{
  int pinA = 2; // use pin 2
  int pinB = 3; // use pin 3
  signed long pos = 0; //setting initial position of encoder
  long count = 0; //setting up to query the position only once every 100 ms
};

struct ledCS
{
  boolean isOnLED; //
  int ledPin; // 
};

struct puffUS
{
  boolean isOnPuff;//
  int puffPin;//
};


//Version, defining structures and aliases
String versionStr = "eyeblink3_4.cpp";
typedef struct trial Trial;
typedef struct rotaryencoder RotaryEncoder;
typedef struct ledCS LedCS;
typedef struct puffUS PuffUS;

//Buffer for I2C communication >1byte
union Buffer{
  unsigned long longNumber;
  byte longBytes[4];
};
Buffer buff;

//Instances of all hardware-associated structures
Trial trial;
RotaryEncoder rotaryencoder;
LedCS ledCS;
PuffUS puffUS;

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
  trial.numTrial = 1;
  
  trial.sessionDur = (trial.numTrial*trial.trialDur); //

  trial.useMotor = 0; //0 = motorOn, 1 = motorLocked, 2 = motorFree
  trial.motorSpeed = 500; //step/sec

  trial.preCSdur = 3000;
  trial.CSdur = 350;
  trial.USdur = 50;
  trial.CS_USinterval = trial.CSdur - trial.USdur;
  trial.interTrialIntervalLow = 5000;//ms
  trial.interTrialIntervalHigh = 20000;//ms
  trial.ITIstartMillis = 0;//ms
  trial.percentUS = 0;//percent US only trials
  trial.percentCS = 10;//percent CS only trials

  trial.trialPin = 7;//pin for conveying trial state
  trial.pinOnOff = false;//trial didn't just end
  pinMode(trial.trialPin, OUTPUT);
  digitalWrite(trial.trialPin, LOW);
  
  sumITI = 0;
  //motor.resetPin = xxx;
  //
  //rotary encoder
  rotaryencoder.pinA = 3;
  rotaryencoder.pinB = 2;
  //
  
  //Activate pin 13 for testing
  pinMode(13, OUTPUT);
  digitalWrite(13,LOW);

  //CS/US structure and pin settings, intially at Arduino grnd
  ledCS.ledPin = 4;
  ledCS.isOnLED = false;
  pinMode(ledCS.ledPin,OUTPUT);
  digitalWrite(ledCS.ledPin,LOW);
  puffUS.puffPin = 5;
  puffUS.isOnPuff = false;
  pinMode(puffUS.puffPin,OUTPUT);
  digitalWrite(puffUS.puffPin,LOW);
  
  //Initialize serial
  Serial.begin(115200);

  //Initialize as I2C master
  Wire.begin();//join I2C bus with no given address you master, you
 
}

/////////////////////////////////////////////////////////////
/*Starting and ending Trials (sessions)*/
//Start session
void startSession(unsigned long now) {
  if (trial.trialIsRunning==false) {
    trial.sessionNumber += 1;
    
    trial.sessionStartMillis = now;
    trial.trialStartMillis = now;

    trial.sessionDur = trial.trialDur * trial.numTrial;
    serialOut(now, "sessionDur",trial.sessionDur);
    serialOut(now, "numTrial", trial.numTrial);
    serialOut(now, "trialDur", trial.trialDur);
    trial.currentTrial = 0;
    
    serialOut(now, "startSession", trial.sessionNumber);
    serialOut(now, "startTrial", trial.currentTrial);

    trial.sessionIsRunning = true;
    trial.trialIsRunning = true;

    //Calculate trial type
    unsigned int RNG = random(1,101);
    if (RNG <= trial.percentUS){
       stimPairType = "US";
    } else if (RNG > trial.percentUS && RNG <= (trial.percentUS + trial.percentCS)){
      stimPairType = "CS";
    } else if (RNG > (trial.percentUS + trial.percentCS)){
      stimPairType = "CS_US";
    }
    serialOut(now,stimPairType,trial.currentTrial);
    //scanImageStart_(now);
    
  }
}

//Start trial
void startTrial(unsigned long now){
  if (trial.trialIsRunning==false){
    trial.currentTrial += 1;

    trial.trialStartMillis = now;
    serialOut(now,"startTrial",trial.currentTrial);

    trial.trialIsRunning = true;

    //Calculate trial type
    unsigned int RNG = random(1,101);
    if (RNG <= trial.percentUS){
       stimPairType = "US";
    } else if (RNG > trial.percentUS && RNG <= (trial.percentUS + trial.percentCS)){
      stimPairType = "CS";
    } else if (RNG > (trial.percentUS + trial.percentCS)){
      stimPairType = "CS_US";
    }
    serialOut(now,stimPairType,trial.currentTrial);
  }
}

//End trial
void stopTrial(unsigned long now) {
  //
  trial.trialIsRunning = false;
  serialOut(now, "stopTrial", trial.currentTrial);
  //Set time to wait until next trial starts
  interTrialInterval = random(trial.interTrialIntervalLow,trial.interTrialIntervalHigh);
  trial.ITIstartMillis = now;
  //sum ITIs so they can be counted towards total session time
  sumITI = sumITI + interTrialInterval;
  
}

//End Session
void stopSession(unsigned long now) {
    if (trial.trialIsRunning){
      trial.trialIsRunning = false;
      serialOut(now,"stopTrial",trial.currentTrial);
    }
    trial.sessionIsRunning = false;
    serialOut(now,"stopSession",trial.sessionNumber);
    
  //At manual or normal session end, stop motor
    /*I2C-directed*/
    Wire.beginTransmission(8);
    Wire.write(int(0));//command slave Arduino to stop motor
    Wire.write(boolean(false));
    Wire.endTransmission();
    Wire.beginTransmission(8);
    Wire.write(int(1));//command slave arduino to make motor free
    Wire.write(int(2));
    Wire.endTransmission();

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
  }
  else if (str == "stopSession") {
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
  trial.CS_USinterval = trial.CSdur - trial.USdur;
    
}

//Get the current experiment parameters;
//This generates the headers for the output files
void GetState() {
  //trial
  Serial.println("sessionNumber=" + String(trial.sessionNumber));
  Serial.println("sessionDur=" + String(trial.sessionDur));

  Serial.println("numTrial=" + String(trial.numTrial));
  Serial.println("trialDur=" + String(trial.trialDur));
  Serial.println("interTrialInteval=" + String(trial.interTrialIntervalLow) + String(trial.interTrialIntervalHigh)); 

  Serial.println("preCSdur=" + String(trial.preCSdur));
  Serial.println("CSdur=" + String(trial.CSdur));
  Serial.println("USdur=" + String(trial.USdur));
  Serial.println("CS_USinterval=" + String(trial.CS_USinterval));
  Serial.println("percentUS=" + String(trial.percentUS));
  Serial.println("percentCS=" + String(trial.percentCS));

  Serial.println("useMotor=" + String(trial.useMotor));
  Serial.println("motorSpeed=" + String(trial.motorSpeed));
  
  Serial.println("versionStr=" + String(versionStr));

}

//Setting experiment parameters
void SetTrial(String name, String strValue) {
  int value = strValue.toInt();
  //Anytime trial params updated, start treadmill
  /*I2C-directed*/
    Wire.beginTransmission(8);
    Wire.write(int(0));//command slave arduino to default motor behavior
    Wire.write(boolean(true));//this is a go command for the motor
    Wire.endTransmission();
    /**/
  //trial
  if (name == "numTrial") {
    trial.numTrial = value;
    Serial.println("trial.numTrial=" + String(trial.numTrial));
  } else if (name=="trialDur") {
    trial.trialDur = value;
    Serial.println("trial.trialDur=" + String(trial.trialDur));
    
  } else if (name=="interTrialIntervalHigh") {
    trial.interTrialIntervalHigh = value;
    Serial.println("trial.interTrialIntervalHigh=" + String(trial.interTrialIntervalHigh));
  } else if (name=="interTrialIntervalLow") {
    trial.interTrialIntervalLow = value;
    Serial.println("trial.interTrialIntervalLow=" + String(trial.interTrialIntervalLow));
    
  } else if (name=="preCSdur") {
    trial.preCSdur = value;
    Serial.println("trial.preCSdur=" + String(trial.preCSdur));
    
  } else if (name=="CSdur") {
    trial.CSdur = value;
    Serial.println("trial.CSdur=" + String(trial.CSdur));
  } else if (name=="USdur") {
    trial.USdur = value;
    Serial.println("trial.USdur=" + String(trial.USdur));
    
  } else if (name=="percentCS") {
    trial.percentCS = value;
    Serial.println("trial.percentCS=" + String(trial.percentCS));
  } else if (name=="percentUS") {
    trial.percentUS = value;
    Serial.println("trial.percentUS=" + String(trial.percentUS));
    
  } else if (name=="useMotor") {
    if (strValue=="motorOn") {//0 for forced run, 1 for locked, 2 for free run
      trial.useMotor = 0;
    } else if (strValue=="motorLocked"){
      trial.useMotor = 1;
    } else if (strValue=="motorFree"){
      trial.useMotor = 2;
    }
    Serial.println("trial.useMotor=" + String(strValue));
    /*I2C-directed*/
    Wire.beginTransmission(8);
    Wire.write(int(1));
    Wire.write(int(trial.useMotor));
    Wire.endTransmission();
    /**/
  } else if (name=="motorSpeed") {
    trial.motorSpeed = value;
    Serial.println("trial.motorSpeed=" + String(trial.motorSpeed));
    /*I2C-directed*/
    Wire.beginTransmission(8);
    Wire.write(int(2));
    buff.longNumber = trial.motorSpeed;
    Wire.write(buff.longBytes,4);
    Wire.endTransmission();
    /**/
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
    
    if (diff>=400){
      signed long dist =  rotaryencoder.pos - posNow;
      serialOut(now, "rotary", dist);
      rotaryencoder.count = now;
      rotaryencoder.pos = posNow;
    }
  }
  
}

//Triggering the LED CS
void updateLED(unsigned long now){
  if (trial.trialIsRunning && (stimPairType=="CS"||stimPairType=="CS_US")){
    //Turning CS on and off while correct trial type running
    unsigned long ledStart = trial.trialStartMillis + trial.preCSdur;
    unsigned long ledStop = ledStart + trial.CSdur;
    if (!ledCS.isOnLED && now >= ledStart && now <= ledStop){
      ledCS.isOnLED = true;
      trial.CSstartMillis = now;
      serialOut(now,"ledCSon",trial.currentTrial);
      digitalWrite(ledCS.ledPin,HIGH);
    } else if(ledCS.isOnLED && now>ledStop){
      ledCS.isOnLED = false;
      serialOut(now,"ledCSoff",trial.currentTrial);
      digitalWrite(ledCS.ledPin,LOW);
    }
  }
}

//Triggering the puff US
void updatePuff(unsigned long now){
  if (trial.trialIsRunning && (stimPairType=="US"||stimPairType=="CS_US")){
    //Turning US on and off while correct trial type is running
    unsigned long puffStart = trial.trialStartMillis + trial.preCSdur + trial.CS_USinterval;
    unsigned long puffStop = puffStart + trial.USdur;
    if (!puffUS.isOnPuff && now >= puffStart && now <= puffStop){
      puffUS.isOnPuff = true;
      serialOut(now,"puffUSon",trial.currentTrial);
      digitalWrite(puffUS.puffPin,HIGH);
    } else if (puffUS.isOnPuff && now > puffStop){
      puffUS.isOnPuff = false;
      serialOut(now,"puffUSoff",trial.currentTrial);
      digitalWrite(puffUS.puffPin,LOW);
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
  
  //Counting for the CS/US pairs
  tmpCSdur = trial.CSdur;
  inPreCS = msIntoTrial < trial.preCSdur;
  inCS = (msIntoTrial > trial.preCSdur) && (msIntoTrial < (trial.preCSdur + tmpCSdur));
  
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
  
  //Stop session after sessionDur if sessionIsRunning
  if (now > (trial.sessionStartMillis + trial.sessionDur + sumITI) && trial.sessionIsRunning) {
    stopSession(now);
  }

  //Updating all hardware components
  updateEncoder(now);

  updateLED(now);

  updatePuff(now);

  updateTrialPin(now);
  
  delay(1); //ms

}
