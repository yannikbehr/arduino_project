#include <Stepper.h>
const int stepsPerRevolution = 2048;
// change this to fit the number of steps per revolution
// for your motor
// initialize the stepper library on pins 8 through 11:
Stepper myStepper(stepsPerRevolution, 8, 10, 9, 11);

void setup() {
   // set the speed at 60 rpm:
   myStepper.setSpeed(10);
   // initialize the serial port:
   Serial.begin(9600);

   // temp
   pinMode(A0, INPUT);
   // buttons
   pinMode(A1, INPUT);
   // LEDs
   pinMode(2, OUTPUT);
   pinMode(3, OUTPUT);
   pinMode(4, OUTPUT);
   pinMode(5, OUTPUT);
   pinMode(6, OUTPUT);
   pinMode(7, OUTPUT);
   // stepper
   pinMode(8, OUTPUT);
   pinMode(9, OUTPUT);
   pinMode(10, OUTPUT);
   pinMode(11, OUTPUT);
   // N-channel mosfet to gate the stepper
   pinMode(12, OUTPUT);

   // switch stepper on
   digitalWrite(12, LOW);
}

int minClosed = 0;
int maxOpen = 0;
int current = 0;
int stepSize = 10;

void printStat(const char* str){
    Serial.print(str);
    Serial.print(current);
    Serial.print(" min: ");
    Serial.print(minClosed);
    Serial.print(" max: ");
    Serial.println(maxOpen);
}

float tempSensor(bool printB){
  float voltage = analogRead(A0)/1024.0 * 5.0;
  float tmp = (voltage - 0.5) * 100;
  if (printB) {
    Serial.print(" voltage ");
    Serial.print(voltage);
    Serial.print(" tmp: ");
    Serial.println(tmp);

    for (int led = 2; led<8; led++){
      digitalWrite(led, LOW);
    }

    int t = tmp + .5; // round
    for (int led = 2; led<8; led++){
      if (t % 2 == 1){
        digitalWrite(led, HIGH);
        t = t-1;
      }
      t /= 2; 
    }
  }
  return tmp;
}

void openD(){
  digitalWrite(12, HIGH);
  myStepper.step(-stepSize);
  current += stepSize;
  if (current>maxOpen)
    maxOpen = current; 
  digitalWrite(12, LOW);
}

void closeD(){
  digitalWrite(12, HIGH);
  myStepper.step(stepSize);
  current -= stepSize;
  if (current<minClosed)
    minClosed = current; 
  digitalWrite(12, LOW);
}

float tempC = 21.0;
int i = 0;

void loop() {

  if (i%100 == 0){
    i=0;
    tempC = tempSensor(true);
  }
  i++; 
  
  int switchState = analogRead(A1);
  if (switchState > 1010){
    printStat("Switch high ");
    openD();
    delay(1);
  }
  else if (switchState > 920){
    printStat("Switch low ");
    closeD();
    delay(1);
  }
  else if (switchState > 420) {
    Serial.println("Switch reset");
    current = 0;
    minClosed = 0;
    maxOpen = 100;
    delay(1);
  }
  else {
    if (tempC > 28.0 && current < maxOpen){
      printStat("Switch open auto ");
      openD();
    }
    else if (tempC < 22.0 && current > minClosed){
      closeD();
      printStat("Switch close auto ");
    }
    else {
      delay(100);
    }
  }
}
