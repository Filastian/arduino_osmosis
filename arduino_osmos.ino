#include <LiquidCrystal_I2C.h>

const int lcdRows = 2;      // Number of LCD display rows
const int lcdColumns = 16;  // Number of characters in the LCD display row

LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

enum FlushingStep {
  None,
  PumpStartup,
  DryRunDelay,
  RestartingStartup,
  Production,
  ExternalStop,
  ProductionDryRunDelay,
  ProductionRestarting,
  FirstWashing,
  PumpShutdown,
  SecondWashing
};

// ========== Pins ==========
const int lowPressureSensor = 2;   // Low pressure sensor
const int highPressureSensor = 3;  // High pressure sensor
const int floatSensor = 4;         // Float
const int externalStop = 5;        // External stop
const int inputValvePin = 8;       // Input valve
const int pumpPin = 9;             // Pump
const int flushValve1Pin = 10;     // Flush valve 1
const int flushValve2Pin = 11;     // Flush valve 2

// ========== Time constants ==========
const int pumpStartupTime = 3;         // Pump startup time in seconds
const int pumpShutdownTime = 3;        // Pump shutdown time in seconds
const int dryRunDelayTime = 5;         // Dry run delay time in seconds
const int restartingTime = 8;          // Restarting time in seconds
const int productionMaxTimeHours = 3;  // Max hours working in Production mode
const int washingTime1 = 6;            // Washing time 1 in seconds
const int washingTime2 = 6;            // Washing time 2 in seconds

// ========== Additional variables ==========
FlushingStep _currentStep = None;
int _currentCounterTime = -1;
int _productionTime = -1;

// Displays the specified string in the middle of the specified line on the LCD panel
void printCentered(const String &str, int row = 0) {
  int size = str.length();
  int offset = 0;

  if (size < lcdColumns) {
    offset = (lcdColumns - size) / 2;
  }

  lcd.setCursor(offset, row);
  lcd.print(str);
}

// Displays the specified string to the right of the specified line on the LCD panel
void printRightAligned(const String &str, int row = 0) {
  int size = str.length();
  int offset = size < lcdColumns ? lcdColumns - size : 0;

  lcd.setCursor(offset, row);
  lcd.print(str);
}

void setup() {
  pinMode(lowPressureSensor, INPUT_PULLUP);
  pinMode(highPressureSensor, INPUT_PULLUP);
  pinMode(floatSensor, INPUT_PULLUP);
  pinMode(externalStop, INPUT_PULLUP);

  pinMode(inputValvePin, OUTPUT);
  pinMode(pumpPin, OUTPUT);
  pinMode(flushValve1Pin, OUTPUT);
  pinMode(flushValve2Pin, OUTPUT);

  lcd.init();
  lcd.begin(lcdColumns, lcdRows);
}

void loop() {
  processTick();

  delay(1000);
}

void setCurrentStep(FlushingStep step) {
  _currentStep = step;
  _currentCounterTime = -1;
}

void processTick() {
  int highPressureSensorValue = digitalRead(highPressureSensor);
  int floatSensorValue = digitalRead(floatSensor);

  if (highPressureSensorValue == LOW && floatSensorValue == LOW) {
    writeReles(LOW, LOW, LOW, LOW);

    lcd.clear();
    printCentered("Waiting");

    return;
  }

  if (highPressureSensorValue == HIGH && floatSensorValue == HIGH) {
    if (_currentStep == None) {
      setCurrentStep(PumpStartup);
    }

    switch (_currentStep) {
      case PumpStartup:
        proccessPumpStartup();
        break;
      case DryRunDelay:
        proccessDryRunDelay(RestartingStartup);
        break;
      case RestartingStartup:
        proccessRestartingStartup(PumpStartup);
        break;
      case Production:
        proccessProduction();
        break;
      case ExternalStop:
        proccessExternalStop();
        break;
      case ProductionDryRunDelay:
        proccessDryRunDelay(ProductionRestarting);
        break;
      case ProductionRestarting:
        proccessRestartingStartup(Production);
        break;
      case FirstWashing:
        proccessFirstWashing();
        break;
      case PumpShutdown:
        proccessPumpShutdown();
        break;
      case SecondWashing:
        proccessSecondWashing();
        break;
    }
  }
}

void proccessPumpStartup() {
  // If the pump has not yet started
  if (_currentCounterTime == -1) {
    _currentCounterTime = pumpStartupTime;

    // Turning on the input valve
    writeReles(HIGH, LOW, LOW, LOW);
  }

  lcd.clear();
  printCentered("Pump starting");
  printRightAligned(String(_currentCounterTime--), 1);

  if (_currentCounterTime != 0) {
    return;
  }

  // If pump start-up has ended, set dry run delay or production step
  int lowPressureSensorValue = digitalRead(lowPressureSensor);
  setCurrentStep(lowPressureSensorValue == LOW ? DryRunDelay : Production);
}

void proccessDryRunDelay(FlushingStep nextStep) {
  // If the dry run delay has not started
  if (_currentCounterTime == -1) {
    _currentCounterTime = dryRunDelayTime;
  }

  lcd.clear();
  printCentered("Dry run");
  printRightAligned(String(_currentCounterTime--), 1);

  if (_currentCounterTime != 0) {
    return;
  }

  // If dry run delay has ended, set specified or production step
  int lowPressureSensorValue = digitalRead(lowPressureSensor);
  setCurrentStep(lowPressureSensorValue == LOW ? nextStep : Production);
}

void proccessRestartingStartup(FlushingStep nextStep) {
  // If the restart has not started
  if (_currentCounterTime == -1) {
    _currentCounterTime = restartingTime;

    // Shut everything down
    writeReles(LOW, LOW, LOW, LOW);
  }

  lcd.clear();
  printCentered("Restarting");
  printRightAligned(String(_currentCounterTime--), 1);

  if (_currentCounterTime != 0) {
    return;
  }

  // If the restarting time has ended, proceed to the next step
  setCurrentStep(nextStep);
}

void proccessProduction() {
  // If production has not yet started
  if (_productionTime == -1) {
    // Start the pump
    writeReles(HIGH, HIGH, LOW, LOW);
  }

  lcd.clear();
  printCentered("Production");
  printCentered(constructTimeString(++_productionTime), 1);

  int externalStopValue = digitalRead(externalStop);
  if (externalStopValue == LOW) {
    setCurrentStep(ExternalStop);
  }

  int lowPressureSensorValue = digitalRead(lowPressureSensor);
  if (lowPressureSensorValue == LOW) {
    setCurrentStep(ProductionDryRunDelay);
  }

  if (_productionTime == 3600 * productionMaxTimeHours) {
    setCurrentStep(FirstWashing);
    _productionTime = -1;
  }
}

void proccessExternalStop() {
  writeReles(LOW, LOW, LOW, LOW);

  lcd.clear();
  printCentered("External stop");

  int externalStopValue = digitalRead(externalStop);
  if (externalStopValue == HIGH) {
    setCurrentStep(Production);
  }
}

void proccessFirstWashing() {
  // If washing 1 has not started
  if (_currentCounterTime == -1) {
    _currentCounterTime = washingTime1;

    writeReles(HIGH, HIGH, HIGH, LOW);
  }

  lcd.clear();
  printCentered("Washing 1");
  printRightAligned(String(_currentCounterTime--), 1);

  if (_currentCounterTime != 0) {
    return;
  }

  // If washing 1 is finished, switch off the pump
  setCurrentStep(PumpShutdown);
}

void proccessPumpShutdown() {
  // If pump shutdown has not started
  if (_currentCounterTime == -1) {
    _currentCounterTime = pumpShutdownTime;

    writeReles(HIGH, LOW, HIGH, LOW);
  }

  lcd.clear();
  printCentered("Pump shutdown");
  printRightAligned(String(_currentCounterTime--), 1);

  if (_currentCounterTime != 0) {
    return;
  }

  // If the pump is switched off, start washing 2
  setCurrentStep(SecondWashing);
}

void proccessSecondWashing() {
  // If washing 2 has not started
  if (_currentCounterTime == -1) {
    _currentCounterTime = washingTime2;

    writeReles(LOW, LOW, HIGH, HIGH);
  }

  lcd.clear();
  printCentered("Washing 2");
  printRightAligned(String(_currentCounterTime--), 1);

  if (_currentCounterTime != 0) {
    return;
  }

  // If flushing time 2 has expired, exit
  setCurrentStep(None);
}

String constructTimeString(int ticks) {
  int hours = ticks / 3600;
  int minutes = (ticks % 3600) / 60;
  int seconds = (ticks % 3600) % 60;

  char formattedTime[20];

  sprintf(formattedTime, "%dh %dm %ds", hours, minutes, seconds);

  return formattedTime;
}

void writeReles(int inputValvePinValue, int pumpPinValue, int flushValve1PinValue, int flushValve2PinValue) {
  digitalWrite(inputValvePin, inputValvePinValue);
  digitalWrite(pumpPin, pumpPinValue);
  digitalWrite(flushValve1Pin, flushValve1PinValue);
  digitalWrite(flushValve2Pin, flushValve2PinValue);
}