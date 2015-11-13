  #include "SPI.h"
#include "Tone.h"

// ###################### Shiftregister #########################
//arduino spi pins nach 74HC595 (Shiftregister)
//rpm lights, gear (7segement) and tacho (english?) signallights are realised via the shift register.
int pinLatch = 8;     // blau kabel >> RCLK   (Register clock)
int pinClock = 13;   // grün kabel >> SRCLK  (Shift register clock)
int pinData  = 11;   // weis kabel >> SER    (Serial data input)

//7 segment display to display gear (0,1,2,3,4,5,6,7,r)
int mIdxGear = 0;
int mGearCodes[] = {168 , 132, 211, 214, 180, 118, 55, 196, 17};

//rpm lights
int mIdxRpmLed = 0;
int mRpmLedCodes[] = {192, 224, 240, 248, 252, 254, 255};
int mMaxRpm = 0;

//current tempo and rpm for fuel calculation
int mCurrentTempo = 0;
int mCurrentRpm = 0;

//tacho lights (2x 74HC595)
int mTachoLights = 0;
int mBitAbs = 1;
int mBitTrac = 2;
int mBitInlight = 4;

// ################# Arduino digital output ###################
int pinTempo = 2; //lila
int pinRpm = 4; //blau
int pinTemp = 5; //grün
int pinFuelLeft = 6; //grau
int pinFuelCur = 3; //braun

int pinTachoPower = 7; //weiß

//Test if PWM would be better
Tone mToneTempo;
Tone mToneRpm;

//temperatur critical value
int mTempCrit = 0;


// ######################### General ###########################

//timeout for serial read
int timeout = 10;

//maximum char[] lengh
int STRING_MAX = 255;

unsigned long mToggleTime;
int onFuelCurDuration = 5000;
int offFuelCurDuration = 1000;
int mFuelCur;


// the setup routine runs once when you press reset:
void setup() {
  mToneTempo.begin(pinTempo);
  mToneRpm.begin(pinRpm);

  pinMode(pinTemp, OUTPUT);  //fuer PWM hier was machen?
  pinMode(pinFuelLeft, OUTPUT);  //fuer PWM hier was machen?
  pinMode(pinFuelCur, OUTPUT);  //fuer PWM hier was machen?
  pinMode(pinTachoPower, OUTPUT);
  digitalWrite(pinTachoPower, HIGH);

  pinMode(pinLatch, OUTPUT);
  pinMode(pinClock, OUTPUT);
  pinMode(pinData, OUTPUT);

  digitalWrite(pinLatch, LOW);
  Serial.begin(115200);

  digitalWrite(pinTachoPower, HIGH);

  SPI.setBitOrder(MSBFIRST);
  SPI.begin();

  pinMode(12,OUTPUT);
  digitalWrite(12,LOW);

  digitalWrite(pinFuelCur, LOW);
  mFuelCur = 10;
  mToggleTime = micros() + offFuelCurDuration;
}



void toggleFuelCur() {
  
  if (digitalRead(pinFuelCur) == HIGH) {
    digitalWrite(pinFuelCur, LOW);
    mToggleTime = micros() + offFuelCurDuration;
  } else {
    digitalWrite(pinFuelCur, HIGH);
    mToggleTime = micros() + onFuelCurDuration;
  }
}

void delayApo(int ms) {

  unsigned long timeToSleep = (unsigned long)(ms*1000);
 
  //Serial.println(timeToSleep);
  
  while (timeToSleep > 0) {
    unsigned long currentMicros = micros();
    if (currentMicros > mToggleTime) {
      toggleFuelCur();
    } else {
      unsigned long timeUntilToggle = mToggleTime - micros();
      //Serial.println(mToggleTime);
    
      if (timeUntilToggle < timeToSleep) {
        delayMicroseconds(timeUntilToggle);
        timeToSleep -= timeUntilToggle;
        toggleFuelCur();
      }
    }

    //Serial.println(timeToSleep);
    unsigned long timeUntilToggle = mToggleTime - micros();
    if (timeUntilToggle > timeToSleep) {
      delayMicroseconds(timeToSleep);
      timeToSleep = 0;
    }
  }
}

bool readSerialMessage(char* key, char *value)
{
  int ret = -1;
  int state = 0;
  int count = 0;
  int keyCount = 0;
  int valueCount = 0;

  unsigned long previousMillis = millis();
  while ((millis() - previousMillis) < timeout && count < STRING_MAX - 1) {

    if (Serial.available() > 0 ) {

      bool keyOrValue = false;
      char c = Serial.read();


      switch (c) {
        case '^':
          if (state == 0)
            state = 1;
          break;
        case ',':
          if (state == 1)
            state = 2;
          break;
        case '\n':
          if (state == 2)
            state = 3;
          count = STRING_MAX;
          break;
        default:
          keyOrValue = true;
          break;
      }

      if (keyOrValue) {
        switch (state) {
          case 1:
            key[keyCount] = c;
            keyCount++;
            break;
          case 2:
            value[valueCount] = c;
            valueCount++;
            break;
        }
      }
      count++;
    } else {
      delayApo(1);
    }
  }

  value[valueCount] = '\0';
  key[keyCount] = '\0';

  //DEBUG
  //if (state > 0)
  //Serial.println(state);

  if (state == 3) {
    return true;
  }
  else {
    return false;
  }
}

//Testen ob pwm sinnvoll ist
void setTempo(int tempo)
{
  mCurrentTempo = tempo;
  updateFuelCur();
  //map: werte aus kopf. alles noch testen!
  int tempoInHz = map(tempo, 0, 255, 0, 326);

  if (tempoInHz < 32) {
    mToneTempo.stop();
  } else {
    if (tempoInHz > 318) {
      tempoInHz = 318;
    }
    mToneTempo.play(tempoInHz);
  }
}

//testen ob pwm sinnvoll ist und werte sind geraten
void setRpm(int rpm) {

  mCurrentRpm = rpm;
  updateFuelCur();
  //map: werte sind geraten ...
  int rpmInHz = map(rpm, 0, 10600, 8, 350);

  if (rpmInHz < 32) {
    if (rpmInHz == 0) {
      mToneRpm.stop();
    } else {
      mToneRpm.play(32);
    }
  } else {
    if (rpmInHz > 350) {
      rpmInHz = 350;
    }
    mToneRpm.play(rpmInHz);
  }

  //sollte man sich noch anschauen, ob das so gut aussieht
  int maxrpm16 = mMaxRpm >> 4;
  if (rpm < maxrpm16 * 10) {
    mIdxRpmLed = 0;
  } else if (rpm < maxrpm16 * 11) {
    mIdxRpmLed = 1;
  } else if (rpm < maxrpm16 * 12) {
    mIdxRpmLed = 2;
  } else if (rpm < maxrpm16 * 13) {
    mIdxRpmLed = 3;
  } else if (rpm < maxrpm16 * 14) {
    mIdxRpmLed = 4;
  } else if (rpm < maxrpm16 * 15) {
    mIdxRpmLed = 5;
  } else {
    mIdxRpmLed = 6;
  }
}

void setFuelLeft(int fuelLeft) {

  int fuel = map(fuelLeft, 0, 65, 13, 155);

  if (fuel > 155) {
    fuel = 155;
  }
  if (fuel < 13) {
    fuel = 13;
  }

  analogWrite(pinFuelLeft, fuel);
}

// Fuel usage at the moment..
void setFuelCur(int fuel) { 
  mFuelCur = fuel;
}

void updateFuelCur() {
  onFuelCurDuration = (int)floor((mFuelCur * mCurrentTempo * 15.0f) / mCurrentRpm * 1000);
  char test[32];
  sprintf(test, "%d ###", (int)onFuelCurDuration);
  Serial.write(test);
  sprintf(test, "Fuel: %d\n", mFuelCur);
  Serial.write(test);
}

void setTemp(int temp) {
  //map, pwm usw
  //Bereich keine ahnung :)
}

void setGear(char gear) {
  switch (gear) {
    case 'r':
      mIdxGear = 8;
      break;
    case '0':
      mIdxGear = 0;
      break;
    case '1':
      mIdxGear = 1;
      break;
    case '2':
      mIdxGear = 2;
      break;
    case '3':
      mIdxGear = 3;
      break;
    case '4':
      mIdxGear = 4;
      break;
    case '5':
      mIdxGear = 5;
      break;
    case '6':
      mIdxGear = 6;
      break;
    case '7':
      mIdxGear = 7;
      break;
  }
}

// AND und OR nochmal testen ob das so geht ;)
void setAbs(char abs_) {
  if (abs_ == '1') {
    mTachoLights |= mBitAbs;
  } else if (abs_ == '0') {
    mTachoLights &= mBitAbs;
  }
}

void setTraction(char traction) {
  if (traction == '1') {
    mTachoLights |= mBitTrac;
  } else if (traction == '0') {
    mTachoLights &= mBitTrac;
  }
}

void setIngameLight(char inlight) {
  if (inlight == '1') {
    mTachoLights |= mBitInlight;
  } else if (inlight == '0') {
    mTachoLights &= mBitInlight;
  }
}


void sendSPI() {
  digitalWrite(pinLatch, HIGH);


  SPI.transfer(0);
  SPI.transfer(mTachoLights);

  SPI.transfer(mRpmLedCodes[mIdxRpmLed]);
  SPI.transfer(mGearCodes[mIdxGear]);
  digitalWrite(pinLatch, LOW);
}

// the loop routine runs over and over again forever:
void loop() {
  //setTempo(220);
  //setRpm(7000);

  
  /*analogWrite(pinTemp, 255);
  analogWrite(pinFuelLeft, 170);

  while (true) {
    int fulltime = 1100;
    digitalWrite(pinFuelCur, HIGH);
    delayMicroseconds(16000);
    digitalWrite(pinFuelCur, LOW);
    delayMicroseconds(1000);
  }
  */
  /*
  for (i;i>160;i--) {
        analogWrite(pinTemp,i);
        delay(100);
  }*/
  // Vorbelegung auf 50°c
  //analogWrite(pinTemp,0);
  //delay(500);


  // Vorbelegung auf 30 Ltr.
  // analogWrite(pinFuelLeft,i);
  // analogWrite(pinFuelCur,i);

  //analogWrite(pinTemp,i+50);


  doStuff();

  delayApo(1);
}

void doStuff() {
  char key[STRING_MAX]; char value[STRING_MAX];
  bool ret = readSerialMessage(key, value);
  //bool ret = false;
  if (ret) {
    if (strcmp(key, "gear") == 0) {
      setGear(value[0]);
    } else if (strcmp (key, "rpm") == 0) {
      setRpm(atoi(value));
    } else if (strcmp (key, "maxrpm") == 0) {
      mMaxRpm = atoi(value);
    } else if (strcmp (key, "tempo") == 0) {
      setTempo(atoi(value));
    } else if (strcmp (key, "fuelleft") == 0) {
      setFuelLeft(atoi(value));
    } else if (strcmp (key, "fuelcur") == 0) {
      setFuelCur(atoi(value));
    } else if (strcmp (key, "temp") == 0) {
      setTemp(atoi(value));
    } else if (strcmp (key, "tempcrit") == 0) {
      mTempCrit = atoi(value);
    } else if (strcmp (key, "abs") == 0) {
      setAbs(value[0]);
    } else if (strcmp (key, "trac") == 0) {
      setTraction(value[0]);
    } else if (strcmp (key, "inlight") == 0) {
      setIngameLight(value[0]);
    }
  }
  sendSPI();
  /*for (int i=0; i<sN; ++i)
  {
    for (int j=0; j<sLED; ++j)
    {
      digitalWrite(pinLatch, HIGH);
      SPI.transfer(N[i]);
      SPI.transfer(LED[j]);
      //SPI.transfer(248);
      digitalWrite(pinLatch, LOW);
      delay(150);

      setSpeed(35+((i*sN)+j)*2.5);
    }

    float x = map(analogRead(0),250,700,14,441);  // analog pin reads 250-700, corresponds to 1.4C to 44.1C
    x /= 10.0;          // divide by 10; map() uses integers
    Serial.print("Temperatur: ");
    Serial.println(x);  // output to serial

  }*/
}
