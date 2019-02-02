/*
 * Project WashShopThermo
 * Description:
 * Author:
 * Date:
 */
#include <DS18B20.h>

const int heatPin = A0;
DS18B20  tempSensor(D2, true);
const int MAXRETRY = 4;
char szInfo[64];
char hcInfo[64];

bool heatOn = false;
bool sentWarning = false;
/*
 * heatMode
 * 0 = Off
 * 1 = Low Set Point
 * 2 = High Set Point
 */
int heatMode = 0; 

int highSetPoint;
int lowSetPoint;

int setPoint;
int tempOffset;
int currentTemp;

int resetCount = 0;

int heatCounter = 0;
int avgCounter = 0;
int avgTemp = 0;

int hAddr = 10;
struct HeatInfo {
  uint8_t version;
  int highSet;
  int lowSet;
  int offset;
  int mode;
  bool status;
  int resetCount;
};

//Averaging Variables
int tempArray[10];
int max;
int maxLocation;
int min;
int minLocation;


void setup() {
  Serial.begin();
  Serial.println("Hello World!");

  Particle.variable("highSetPt", highSetPoint);
  Particle.variable("lowSetPt", lowSetPoint);
  Particle.variable("heatMode", heatMode);
  Particle.variable("heatOn", heatOn);
  Particle.variable("currentTemp", currentTemp);
  Particle.variable("tempOffset", tempOffset);

  Particle.function("setHeatMode", setMode);
  Particle.function("setHighPoint", setHigh);
  Particle.function("setLowPoint", setLow);
  Particle.function("setOffset", setOffset);
  Particle.function("rsetreset", resetReset);

  pinMode(heatPin, OUTPUT);
  digitalWrite(heatPin, LOW);
  heatOn = false;
  heatMode = 0;

  HeatInfo myObj;
  EEPROM.get(hAddr, myObj);
  if(myObj.version != 0) {
    // EEPROM was empty -> initialize myObj
    HeatInfo defaultObj = { 0, highSetPoint, lowSetPoint, tempOffset, heatMode, heatOn, resetCount};
    myObj = defaultObj;
    EEPROM.put(hAddr, myObj);
  } else {
    highSetPoint = myObj.highSet;
    lowSetPoint = myObj.lowSet;
    tempOffset = myObj.offset;
    heatMode = myObj.mode;
    if (heatMode == 1) {
      setPoint = lowSetPoint;
    } else if (heatMode == 2) {
      setPoint = highSetPoint;
    }
    heatOn = myObj.status;
    resetCount = myObj.resetCount + 1;
    HeatInfo defaultObj = { 0, highSetPoint, lowSetPoint, tempOffset, heatMode, heatOn, resetCount};
    myObj = defaultObj;
    EEPROM.put(hAddr, myObj);
  }

}

void loop() {
  /* future features
   * Add a check to see that heaters actually started (maybe out of gas)
   * - track temp after turning on to make sure that the heat rises
   * - actually hook into heater board to check if they fire
   */

  if (heatCounter%1200 == 0) {
    float _temp;

    _temp = tempSensor.getTemperature();
    sprintf(hcInfo, "%f", _temp);
    Particle.publish("count", hcInfo, PRIVATE);
    if ( _temp > -10 && _temp < 38) {
      int thisTempF = tempSensor.convertToFahrenheit(_temp);
      avgTemp += thisTempF;
      avgCounter += 1; 
    }

      
  }
  heatCounter += 1;
  Serial.println(avgCounter);
  if (avgCounter == 10) {
                    
    int tempReading = avgTemp/10;

    sprintf(szInfo, "%i", tempReading);
    Particle.publish("dsTmp", szInfo, PRIVATE);

    if (tempReading != currentTemp) {
      currentTemp = tempReading;

      if (heatMode == 0) {
        if (currentTemp < 40) {
        
          if (sentWarning == false) {
            /*
            * Send alert to owner via text saying 
            * "Temp is %temp in garage! Would you like me to turn the heat to the low set point?"
            * responsing "Yes" to text will turn on the heat to the low setpoint
            */
            bool success;
            success = Particle.publish("below-40", PRIVATE); //Setup to send text message to me
            if (success) {
              sentWarning = true;
            } 
          }
          
        }
      } else {
        checkToHeat();
      }

      avgTemp = 0;
      avgCounter = 0;
      heatCounter = 0;
    }
  }
  //if for some reason avgCounter get over 10 set to 0 and start again
  if (avgCounter > 10) {
    avgTemp = 0;
    avgCounter = 0;
    heatCounter = 0;
  }
}

int setMode(String mode) {

  if(mode == "0") {
    digitalWrite(heatPin, LOW);
    heatOn = false;
    heatMode = 0;
    writeLog();
    return 1;
  } else if (mode == "1") {
    setPoint = lowSetPoint;
    heatMode = 1;
    writeLog();
    checkToHeat();
    return 1;
  } else if (mode == "2") {
    setPoint = highSetPoint;
    heatMode = 2;
    writeLog();
    checkToHeat();
    return 1;
  } else return -1;
}

void checkToHeat() {
  
  if (heatMode != 0) {

    if (currentTemp > setPoint) {//Turn Off
      if (heatOn == true) {
        digitalWrite(heatPin, LOW);
        heatOn = false;
      }
    }

    if (currentTemp < setPoint - 3) {//Turn On
      if (heatOn == false) {
        digitalWrite(heatPin, HIGH);
        heatOn = true;
        //If being turned on after a warning and we can verify (future feature) that heat is on reset the warning
        if (sentWarning) {
          sentWarning = false;
        }
      }
    }

  }
}

void writeLog() {
  HeatInfo defaultObj = { 0, highSetPoint, lowSetPoint, tempOffset, heatMode, heatOn, resetCount};
  EEPROM.put(hAddr, defaultObj);
}

int resetReset(String set) {
  resetCount = 0;
  writeLog();
  return 1;
}

//Eventually validate that the String being sent is in fact an int
int setHigh(String temp) {
  highSetPoint = temp.toInt();
  writeLog();
  return 1;
}

int setLow(String temp) {
  lowSetPoint = temp.toInt();
  writeLog();
  return 1;
}

int setOffset(String newOffset) {
  tempOffset = newOffset.toInt();
  writeLog();
  return 1;
}