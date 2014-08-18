/*
 PROJECT: haotc
 PROGRAMMER: Hazim Bitar (techbitar at gmail dot com)
 DATE: Oct 31, 2013
 PROGRAMMER: P. Lechowicz
 DATE: Aug 11, 2014
 FILE: ardu.ino
 LICENSE: Public domain
*/
#include <RCSwitch.h>
#include <Wire.h>
#include <Time.h>
#include <TimeAlarms.h>
#include <DS1307RTC.h>
#include <EEPROM.h>

#define START_CMD_CHAR '*'
#define END_CMD_CHAR '#'
#define DIV_CMD_CHAR '|'
#define CMD_DIGITALWRITE 10
#define CMD_SET_TIME 11
#define CMD_TEXT 12
#define CMD_READ_ARDUDROID 13
#define MAX_COMMAND 20  // max command number code. used for error checking.
#define MIN_COMMAND 10  // minimum command number code. used for error checking. 
#define IN_STRING_LENGHT 40
#define MAX_ANALOGWRITE 255
#define PIN_HIGH 3
#define PIN_LOW 2
#define MY_GROUP "00001"

String inText;
RCSwitch mySwitch = RCSwitch();
// the current address in the EEPROM (i.e. which byte
// we're going to write to next)
int eeprom_addr = 0;
AlarmId onTimer = 255;
AlarmId offTimer = 255;
tmElements_t tm;

void setup() {
  Serial.begin(9600);
  Serial.println("haotc 0.01 Alpha by P.Lechowicz (2014)");
  setSyncProvider(RTC.get);   // the function to get the time from the RTC
  if(timeStatus()!= timeSet) 
     Serial.println("Unable to sync with the RTC");
  else
     Serial.println("RTC has set the system time");      
  digitalClockDisplay();
  Serial.flush();
  // Transmitter is connected to Arduino Pin #10  
  mySwitch.enableTransmit(10);
  setUpAlarms();
}

void loop()
{
  Serial.flush();
  int ard_command = 0;
  int pin_num = 0;
  int pin_value = 0;

  char get_char = ' ';  //read serial

  // wait for incoming data
  if (Serial.available() < 1) {
    Alarm.delay(0);
    return; // if serial empty, return to loop().
  }

  // parse incoming command start flag 
  get_char = Serial.read();
  if (get_char != START_CMD_CHAR) return; // if no command start flag, return to loop().

  // parse incoming command type
  ard_command = Serial.parseInt(); // read the command
  
  // parse incoming pin# and value  
  pin_num = Serial.parseInt(); // read the pin
  pin_value = Serial.parseInt();  // read the value

  // 1) GET TEXT COMMAND FROM ARDUDROID
  if (ard_command == CMD_TEXT){   
    inText = ""; //clears variable for new input
    eeprom_addr = 0;
    while (Serial.available())  {
      char c = Serial.read();  //gets one byte from serial buffer
      Alarm.delay(5);
      if (c == END_CMD_CHAR) { // if the complete string has been read
        digitalClockDisplay();
        if (pin_num == 0 && pin_value == 0) {
          for (int i = 0; i < 24; i++) {
            EEPROM.write(i, 255);
            Alarm.disable(i);
          }
        }
        else {
          setUpAlarms();
        }
        break;
      }              
      else {
        if (pin_num >= 1 && pin_num <= 6) {
          int h = Serial.parseInt();
          int m = Serial.parseInt();
          eeprom_addr = 4 * (pin_num-1);
          if (pin_value == PIN_LOW)
            eeprom_addr = eeprom_addr + 2;
          else if (pin_value == PIN_HIGH)
            eeprom_addr = eeprom_addr + 0;
          else
	    return; // error in pin value. return.
          EEPROM.write(eeprom_addr, h);
          EEPROM.write(eeprom_addr+1, m);
          Alarm.delay(5);
        }
      }
    }
  }

  // 2) GET digitalWrite DATA FROM ARDUDROID
  if (ard_command == CMD_DIGITALWRITE){  
    if (pin_value == PIN_LOW)
	  pin_value = LOW;
    else if (pin_value == PIN_HIGH)
	  pin_value = HIGH;
    else
	  return; // error in pin value. return.
	if (pin_num >= 1 && pin_num <= 6)
	  pin_num = 14 - pin_num;    // 1 --> 13, 6 --> 8
    else
	  return; // error in pin value. return.
    set_digitalwrite( pin_num,  pin_value);
    return;  // return from start of loop()
  }

  // 3) GET analogWrite DATA FROM ARDUDROID
  if (ard_command == CMD_SET_TIME) {  
    // analogWrite(  pin_num, pin_value );
	int sec = Serial.parseInt();
    Serial.print("Android time");
	printDigits(pin_num);
	printDigits(pin_value);
	printDigits(sec);
    return;  // Done. return to loop();
  }

  // 4) SEND DATA TO ARDUDROID
  if (ard_command == CMD_READ_ARDUDROID) { 
    // char send_to_android[] = "Place your text here." ;
    // Serial.println(send_to_android);   // Example: Sending text
    Serial.print(" Analog 0 = "); 
    Serial.println(analogRead(A0));  // Example: Read and send Analog pin value to Arduino
    return;  // Done. return to loop();
  }
}

void setUpAlarms()
{
  for(int i = 0; i < 6; i++) {
    eeprom_addr = 4 * i;
    //Serial.print(i+1);
    //Serial.print(" --> ");
    int h = EEPROM.read(eeprom_addr++);
    int m = EEPROM.read(eeprom_addr++);
    //printTimeStamp(h, m);
    //Serial.print(",  ");
    if (h < 24 && m < 60) {
      switch (i+1) {
      case 1:
        Alarm.alarmRepeat(h, m, 0, onAlarm1);
        break;
      case 2:
        Alarm.alarmRepeat(h, m, 0, onAlarm2);
        break;
      case 3:
        Alarm.alarmRepeat(h, m, 0, onAlarm3);
        break;
      case 4:
        Alarm.alarmRepeat(h, m, 0, onAlarm4);
        break;
      case 5:
        Alarm.alarmRepeat(h, m, 0, onAlarm5);
        //Serial.print("(alarm on)");
        break;
      case 6:
        Alarm.alarmRepeat(h, m, 0, onAlarm6);
        //Serial.print("(alarm on)");
        break;
      }
    }
    h = EEPROM.read(eeprom_addr++);
    m = EEPROM.read(eeprom_addr++);
    //printTimeStamp(h, m);
    if (h < 24 && m < 60) {
      switch (i+1) {
      case 1:
        Alarm.alarmRepeat(h, m, 0, offAlarm1);
        break;
      case 2:
        Alarm.alarmRepeat(h, m, 0, offAlarm2);
        break;
      case 3:
        Alarm.alarmRepeat(h, m, 0, offAlarm3);
        break;
      case 4:
        Alarm.alarmRepeat(h, m, 0, offAlarm4);
        break;
      case 5:
        Alarm.alarmRepeat(h, m, 0, offAlarm5);
        //Serial.print("(alarm off)");
        break;
      case 6:
        Alarm.alarmRepeat(h, m, 0, offAlarm6);
        //Serial.print("(alarm off)");
        break;
      }
    }
   //Serial.println();
  }
  //Serial.flush();
}

void onAlarm1(){
  mySwitch.switchOn(MY_GROUP, "10000");
}

void offAlarm1(){
  mySwitch.switchOff(MY_GROUP, "10000");
}

void onAlarm2(){
  mySwitch.switchOn(MY_GROUP, "01000");
}

void offAlarm2(){
  mySwitch.switchOff(MY_GROUP, "01000");
}

void onAlarm3(){
  mySwitch.switchOn(MY_GROUP, "00100");
}

void offAlarm3(){
  mySwitch.switchOff(MY_GROUP, "00100");
}

void onAlarm4(){
  mySwitch.switchOn(MY_GROUP, "00010");
}

void offAlarm4(){
  mySwitch.switchOff(MY_GROUP, "00010");
}

void onAlarm5(){
  digitalClockDisplay();
  Serial.println("Alarm#5: - turn it on");           
  Serial.flush();
  mySwitch.switchOn(MY_GROUP, "00001");
}

void offAlarm5(){
  digitalClockDisplay();
  Serial.println("Alarm#5: - turn it off");
  Serial.flush();
  mySwitch.switchOff(MY_GROUP, "00001");
}

void onAlarm6(){
  digitalClockDisplay();
  Serial.println("Alarm#6: - turn it on");           
  Serial.flush();
  mySwitch.switchOn(MY_GROUP, "00011");
}

void offAlarm6(){
  digitalClockDisplay();
  Serial.println("Alarm#6: - turn it off");           
  Serial.flush();
  mySwitch.switchOff(MY_GROUP, "00011");
}

// 2a) select the requested pin# for DigitalWrite action
void set_digitalwrite(int pin_num, int pin_value)
{
  tmElements_t tm;
    
  switch (pin_num) {
  case 13:
    pinMode(13, OUTPUT);  // #1
    digitalWrite(13, pin_value);
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP, "10000");
    else
      mySwitch.switchOff(MY_GROUP, "10000");
    break;
  case 12:
    pinMode(12, OUTPUT);  // #2
    digitalWrite(12, pin_value);   
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP, "01000");
    else
      mySwitch.switchOff(MY_GROUP, "01000");
    break;
  case 11:
    pinMode(11, OUTPUT);  // #3
    digitalWrite(11, pin_value);         
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP, "00100");
    else
      mySwitch.switchOff(MY_GROUP, "00100");
    break;
  case 10:
    pinMode(10, OUTPUT);  // #4
    digitalWrite(10, pin_value);         
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP, "00010");
    else
      mySwitch.switchOff(MY_GROUP, "00010");
    break;
  case 9:
    pinMode(9, OUTPUT);  // #5
    digitalWrite(9, pin_value);         
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP, "00001");
    else
      mySwitch.switchOff(MY_GROUP, "00001");
    break;
  case 8:
    pinMode(8, OUTPUT);  // #6
    digitalWrite(8, pin_value);         
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP, "00011");
    else
      mySwitch.switchOff(MY_GROUP, "00011");
    break;
  case 7:
    pinMode(7, OUTPUT);  // #7
    digitalWrite(7, pin_value);         
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP, "00101");
    else
      mySwitch.switchOff(MY_GROUP, "00101");
    break;
  case 6:
    pinMode(6, OUTPUT);  // #8
    digitalWrite(6, pin_value);         
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP, "00110");
    else
      mySwitch.switchOff(MY_GROUP, "00110");
    break;
  case 5:
    pinMode(5, OUTPUT);  // #9
    digitalWrite(5, pin_value); 
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP, "00111");
    else
      mySwitch.switchOff(MY_GROUP, "00111");
    break;
  case 4:
    pinMode(4, OUTPUT);  // #10
    digitalWrite(4, pin_value);         
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP, "01001");
    else
      mySwitch.switchOff(MY_GROUP, "01001");
    break;
  case 3:
    pinMode(3, OUTPUT);  // #11
    digitalWrite(3, pin_value);         
    if (RTC.read(tm)) {
      Serial.print("Ok, Time = ");
      print2digits(tm.Hour);
      Serial.write(':');
      print2digits(tm.Minute);
      Serial.write(':');
      print2digits(tm.Second);
      Serial.print(", Date (D/M/Y) = ");
      Serial.print(tm.Day);
      Serial.write('/');
      Serial.print(tm.Month);
      Serial.write('/');
      Serial.print(tmYearToCalendar(tm.Year));
      Serial.println();
    } 
    break;
  case 2:
    pinMode(2, OUTPUT);  // #12
    digitalWrite(2, pin_value); 
    // add your code here       
    break;      
    // default: 
    // if nothing else matches, do the default
    // default is optional
  } 
}

void print2digits(int number) {
  if (number >= 0 && number < 10) {
    Serial.write('0');
  }
  Serial.print(number);
}

void printTimeStamp(int h, int m)
{
  Serial.print(h);
  printDigits(m);
}

void digitalClockDisplay()
{
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.println(); 
}

void printDigits(int digits)
{
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}
