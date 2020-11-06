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

bool heatOn = false;
bool sentWarning = false;
/*
 * heatMode
 * 0 = Off
 * 1 = Low Set Point to prevent freezing
 * 2 = High Set Point temp for when your working in the building
 */
int heatMode = 1; 

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

int redPin = A3;
int greenPin = D8;
int bluePin = D4;

//Start Value
static enum { RED, BLUE } cColor;

//Current LED Value
int cLEDValue = 0;

int ledSteps = 25;

unsigned long lastMillis = 0;
unsigned long delayMillis = 80;
static enum { FADING_IN, FADING_OUT } led_state;

int buttonPin = A1;
int buttonState;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

void setup() {
  Serial.begin();
  Serial.println("Hello World!");

  Particle.variable("highSetPt", highSetPoint);
  Particle.variable("lowSetPt", lowSetPoint);
  Particle.variable("heatMode", heatMode);
  Particle.variable("heatOn", heatOn);
  Particle.variable("currentTemp", currentTemp);
  Particle.variable("tempOffset", tempOffset);
  Particle.variable("reset", resetCount);

  Particle.variable("steps", ledSteps);
  Particle.variable("delay", delayMillis);

  Particle.function("setHeatMode", setMode);
  Particle.function("setHighPoint", setHigh);
  Particle.function("setLowPoint", setLow);
  Particle.function("setOffset", setOffset);
  Particle.function("rsetreset", resetReset);

  Particle.function("steps", setLedSteps);
  Particle.function("delay", setdelay);

  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  pinMode(buttonPin, INPUT_PULLUP);

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
    setColor(0, 255, 0); // green
  } else {
    highSetPoint = myObj.highSet;
    lowSetPoint = myObj.lowSet;
    tempOffset = myObj.offset;
    heatMode = myObj.mode;
    if (heatMode == 1) {
      setPoint = lowSetPoint;
      setColor(0, 0, 255); // blue
    } else if (heatMode == 2) {
      setPoint = highSetPoint;
      setColor(255, 0, 0); // red
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

  //toggleColor();

  if (heatOn) {
    unsigned long currentMillis = millis();

  if ((currentMillis - lastMillis) > delayMillis) {
    lastMillis = currentMillis;
    switch (led_state) {
      case FADING_IN:
        Serial.println("*--- Fade In");
        if ((cLEDValue + ledSteps) > 255) {
          cLEDValue = 255;
        } else {
          cLEDValue = cLEDValue + ledSteps;
        }

        switch (cColor) {
          case RED:
            fadeColor(cLEDValue, 0, 0);
            break;
          case BLUE:
            fadeColor(0, 0, cLEDValue);
            break;
          default:
            break;
        }

        if (cLEDValue == 255) {
          led_state = FADING_OUT;
          break;
        }
        break;

      case FADING_OUT:
        Serial.println("*--- Fade Out");
        if ((cLEDValue - ledSteps) < 1) {
          cLEDValue = 0;
        } else {
          cLEDValue = cLEDValue - ledSteps;
        }

        switch (cColor) {
          case RED:
            fadeColor(cLEDValue, 0, 0);
            break;
          case BLUE:
            fadeColor(0, 0, cLEDValue);
            break;
          default:
            break;
        }

        if (cLEDValue == 0) {
          led_state = FADING_IN;
          break;
        }
        break;
      default:
      return;
    }
    
  }

  }

  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:
 
    // if the button state has changed:
    if (reading != buttonState) {
      buttonState = reading;
      Serial.print("*--- Button ---* "); Serial.println(buttonState);
      if (buttonState == LOW) {
        if (heatMode == 0) {
          setMode("1");
        } else if (heatMode == 1) {
          setMode("2");
        } else {
          setMode("0");
        }
      }
    }
  }
  lastButtonState = reading;

  if (heatCounter%1200 == 0) {
    float _temp;
    // int   i = 0;

    // do {
    //   _temp = tempSensor.getTemperature();
    // } while (!tempSensor.crcCheck() && MAXRETRY > i++);
    _temp = tempSensor.getTemperature();
    if (tempSensor.crcCheck()) {
      
      if ( _temp > -10 && _temp < 38) {
        int thisTempF = tempSensor.convertToFahrenheit(_temp) + tempOffset;
        
        avgTemp += thisTempF;
        avgCounter += 1; 
      }
    }

    if (avgCounter == 10) {
                    
      int tempReading = avgTemp/10;
      
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

    if (avgCounter > 10) {
      avgTemp = 0;
      avgCounter = 0;
      heatCounter = 0;
    }

  }
  heatCounter += 1;

}

int setMode(String mode) {
  Serial.print("Mode: "); Serial.println(mode);
  if(mode == "0") {
    digitalWrite(heatPin, LOW);
    heatOn = false;
    heatMode = 0;
    setColor(0, 255, 0); // green
    writeLog();
    return 1;
  } else if (mode == "1") {
    setPoint = lowSetPoint;
    heatMode = 1;
    setColor(0, 0, 255); // blue
    writeLog();
    checkToHeat();
    return 1;
  } else if (mode == "2") {
    setPoint = highSetPoint;
    heatMode = 2;
    setColor(255, 0, 0); // red
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
        switch (cColor) {
          case RED:
            setColor(255, 0, 0);
            break;
          case BLUE:
            setColor(0, 0, 255);
            break;
          default:
            break;
        }
      }
    }

    if (currentTemp < setPoint - 3) {//Turn On
      if (heatOn == false) {
        digitalWrite(heatPin, HIGH);
        heatOn = true;
        led_state = FADING_OUT;
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
  if (heatMode == 2) {
    setPoint = highSetPoint;
  }
  writeLog();
  return 1;
}

int setLow(String temp) {
  lowSetPoint = temp.toInt();
  if (heatMode == 1) {
    setPoint = lowSetPoint;
  }
  writeLog();
  return 1;
}

int setOffset(String newOffset) {
  tempOffset = newOffset.toInt();
  writeLog();
  return 1;
}

void setColor(int red, int green, int blue) {

  if (red == 255) {
    cColor = RED;
  } else if (blue == 255) {
    cColor = BLUE;
  }

  cLEDValue = 255;

  red = 255 - red;
  green = 255 - green;
  blue = 255 - blue;

  Serial.println("Set");
  Serial.print(red); Serial.print(", "); Serial.print(green); Serial.print(", "); Serial.println(blue);

  analogWrite(redPin, red); analogWrite(greenPin, green); analogWrite(bluePin, blue);
}

void fadeColor(int red, int green, int blue) {

  red = 255 - red;
  green = 255 - green;
  blue = 255 - blue;

  Serial.println("Fade");
  Serial.print(red); Serial.print(", "); Serial.print(green); Serial.print(", "); Serial.println(blue);

  analogWrite(redPin, red); analogWrite(greenPin, green); analogWrite(bluePin, blue);
}

int setLedSteps(String steps) {
  ledSteps = steps.toInt();
  return 1;
}

int setdelay(String delay) {
  unsigned long newLong = delay.toInt();
  delayMillis = newLong;
  return 1;
}