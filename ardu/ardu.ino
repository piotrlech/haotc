/*
 PROJECT: haotc
 PROGRAMMER: Hazim Bitar (techbitar at gmail dot com)
 DATE: Oct 31, 2013
 FILE: ardu.ino
 LICENSE: Public domain
*/
#include <RCSwitch.h>
#include <Wire.h>
#include <Time.h>
#include <TimeAlarms.h>
#include <DS1307RTC.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>
#include "pass.h"

static PROGMEM prog_uint32_t crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

#define START_CMD_CHAR '*'
#define END_CMD_CHAR '#'
#define DIV_CMD_CHAR '|'

#define CMD_TURN_IT_ON_OFF 10
#define CMD_READ_STATUS 11
#define CMD_SEND_TRIGGERS 12
#define CMD_SET_TIME 13

#define MAX_COMMAND 20  // max command number code. used for error checking.
#define MIN_COMMAND 10  // minimum command number code. used for error checking. 
#define IN_STRING_LENGHT 40
#define MAX_ANALOGWRITE 255
#define PIN_HIGH 3
#define PIN_LOW 2
#define MY_1ST    "10000"
#define MY_2ND    "01000"
#define MY_3RD    "00100"
#define MY_4TH    "00010"
#define MY_5TH    "10000"
#define MY_6TH    "00010"

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
  Serial.println("haotc 0.03 (2015)");
  setSyncProvider(RTC.get);   // the function to get the time from the RTC
  if(timeStatus()!= timeSet) 
     Serial.println("Unable to sync with the RTC");
  else
     Serial.println("RTC has set the system time");      
  digitalClockDisplay();
  Serial.flush();
  // Transmitter is connected to Arduino Pin #10  
  mySwitch.enableTransmit(10);
  //setUpStatus();
  setUpAlarms();  // enable when setup after power on required
}

void loop()
{
  Serial.flush();
  Alarm.delay(0);
  int ard_command = 0;
  int pin_num = 0;
  int pin_value = 0;

  char get_char = ' ';  //read serial

  // wait for incoming data
  if (Serial.available() < 1) {
    return; // if serial empty, return to loop().
  }

  // parse incoming command start flag 
  get_char = Serial.read();
  if (get_char != START_CMD_CHAR) {
    return; // if no command start flag, return to loop().
  }

  // parse incoming command type
  // parse incoming pin# and value  
  ard_command = Serial.parseInt(); // read the command
  pin_num = Serial.parseInt();     // read the pin
  pin_value = Serial.parseInt();   // read the value

  // 1) Set up the triggers
  if (ard_command == CMD_SEND_TRIGGERS){   
    inText = ""; //clears variable for new input
    eeprom_addr = 0;
    while (Serial.available())  {
      char c = Serial.read();  //gets one byte from serial buffer
      Alarm.delay(5);
      if (c == END_CMD_CHAR) { // if the complete string has been read
	    // message = "*12,0,0,0#";
        if (pin_num == 0 && pin_value == 0) {
          //Serial.println("Disabling alarms");
          for (int i = 0; i < 24; i++) {
            EEPROM.write(i, 255);
            Alarm.free(i);
          }
          //Serial.flush();
        }
        else if (pin_num == 0 && pin_value == 1) {
          //Serial.println("Enabling alarms");
          //digitalClockDisplay();
          //Serial.flush();
          setUpAlarms();
        }
        else {
          //String sDisp = "pin_num = ";
          //sDisp = sDisp + pin_num + ", pin_value = " + pin_value;
          //Serial.println(sDisp);
        }
        break;
      }              
      else {
        // message = "*12," + buttonNr + ",3," + sFormatted + buttonNr + "#";
        // message = "*12," + buttonNr + ",2," + sFormatted + "#";
        if (pin_num >= 1 && pin_num <= 6) {
          int h = Serial.parseInt();
          int m = Serial.parseInt();
          int p = Serial.parseInt();
          if(pin_num == p) {
            eeprom_addr = 4 * (pin_num-1);
            if (pin_value == PIN_LOW)
              eeprom_addr = eeprom_addr + 2;
            else if (pin_value == PIN_HIGH)
              eeprom_addr = eeprom_addr + 0;
            else
  	          return; // error in pin value. return.
            if(h >=0 && h <= 23)
              EEPROM.write(eeprom_addr, h);
            if(m >= 0 && m <= 59)
              EEPROM.write(eeprom_addr+1, m);
          }
          else {
            String sOut;
            sOut = "pins error:";
            sOut = sOut + pin_num;
            sOut = sOut + " vs ";
            sOut = sOut + p;
            Serial.println(sOut);           
          }
          Alarm.delay(5);
        }
      }
    }
  }

  // 2) Turn a switch on or off
  if (ard_command == CMD_TURN_IT_ON_OFF){  
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

  // 3) Read the Arduino status
  if (ard_command == CMD_READ_STATUS) {  
    // analogWrite(  pin_num, pin_value );
    int timeout = 50;
    int sec = Serial.parseInt();
    String sOut;
    for(int i = 0; i < 6; i++) {
      sOut = "Schedule:";
      sOut = sOut + (i+1) + ":";
      eeprom_addr = 4 * i;
      sOut = sOut + EEPROM.read(eeprom_addr++) + ",";
      sOut = sOut + EEPROM.read(eeprom_addr++) + ",";
      sOut = sOut + EEPROM.read(eeprom_addr++) + ",";
      sOut = sOut + EEPROM.read(eeprom_addr++) + "#";
      Serial.println(sOut);
      Alarm.delay(timeout);
    }
    Alarm.delay(timeout);
    sOut = "Status: ";
    for(int i = 101; i <= 106; i++) {
      sOut = sOut + EEPROM.read(i) + ",";
      //Serial.print(",");
    }
    Serial.println(sOut);
    Alarm.delay(timeout);
    sOut = "Arduino time is:";
    sOut = sOut + hour() + ":" + minute() + ":" + second() + " vs Android ";
    sOut = sOut + pin_num + ":";
    sOut = sOut + pin_value + ":";
    sOut = sOut + sec;
    Serial.println(sOut);
    Alarm.delay(timeout);
    return;  // Done. return to loop();
  }

  // 4) Set Arduino time
  if (ard_command == CMD_SET_TIME) { 
    // char send_to_android[] = "Place your text here." ;
    // Serial.println(send_to_android);   // Example: Sending text
    int y = pin_num;
    int mo= pin_value;
    int d = Serial.parseInt();
    int h = Serial.parseInt();
    int m = Serial.parseInt();
    int s = Serial.parseInt();
    String sDebug = "Enabling alarms at ";
    sDebug = sDebug + y + "." + mo + "." + d + " " + h + ":" + m + ":" + s;
    //Serial.println(sDebug);
    tmElements_t tm;
    
    tm.Year = CalendarYrToTm(2000+y);
    tm.Month = mo;
    tm.Day = d;
    tm.Hour = h;
    tm.Minute = m;
    tm.Second = s;
    if (RTC.write(tm)) {
      Serial.println(sDebug);
    }
    Serial.flush();
    return;  // Done. return to loop();
  }
}

void setUpStatus()
{
  for(int i = 0; i < 6; i++) {
    if(EEPROM.read(101+i) == HIGH) {
      switch (i+1) {
      case 1:
        mySwitch.switchOn(MY_GROUP2, MY_1ST);
        break;
      case 2:
        mySwitch.switchOn(MY_GROUP2, MY_2ND);
        break;
      case 3:
        mySwitch.switchOn(MY_GROUP2, MY_3RD);
        break;
      case 4:
        mySwitch.switchOn(MY_GROUP2, MY_4TH);
        break;
      case 5:
        mySwitch.switchOn(MY_GROUP2, MY_5TH);
        break;
      case 6:
        mySwitch.switchOn(MY_GROUP2, MY_6TH);
        break;
      }
    } else {
      switch (i+1) {
      case 1:
        mySwitch.switchOff(MY_GROUP2, MY_1ST);
        break;
      case 2:
        mySwitch.switchOff(MY_GROUP2, MY_2ND);
        break;
      case 3:
        mySwitch.switchOff(MY_GROUP2, MY_3RD);
        break;
      case 4:
        mySwitch.switchOff(MY_GROUP2, MY_4TH);
        break;
      case 5:
        mySwitch.switchOff(MY_GROUP2, MY_5TH);
        break;
      case 6:
        mySwitch.switchOff(MY_GROUP2, MY_6TH);
        break;
      }
    }
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
        Alarm.alarmRepeat(h, m, 1, onAlarm1);
        break;
      case 2:
        Alarm.alarmRepeat(h, m, 0, onAlarm2);
        Alarm.alarmRepeat(h, m, 1, onAlarm2);
        break;
      case 3:
        Alarm.alarmRepeat(h, m, 0, onAlarm3);
        Alarm.alarmRepeat(h, m, 1, onAlarm3);
        break;
      case 4:
        Alarm.alarmRepeat(h, m, 0, onAlarm4);
        Alarm.alarmRepeat(h, m, 1, onAlarm4);
        break;
      case 5:
        Alarm.alarmRepeat(h, m, 0, onAlarm5);
        Alarm.alarmRepeat(h, m, 1, onAlarm5);
        break;
      case 6:
        Alarm.alarmRepeat(h, m, 0, onAlarm6);
        Alarm.alarmRepeat(h, m, 1, onAlarm6);
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
        Alarm.alarmRepeat(h, m, 1, offAlarm1);
        break;
      case 2:
        Alarm.alarmRepeat(h, m, 0, offAlarm2);
        Alarm.alarmRepeat(h, m, 1, offAlarm2);
        break;
      case 3:
        Alarm.alarmRepeat(h, m, 0, offAlarm3);
        Alarm.alarmRepeat(h, m, 1, offAlarm3);
        break;
      case 4:
        Alarm.alarmRepeat(h, m, 0, offAlarm4);
        Alarm.alarmRepeat(h, m, 1, offAlarm4);
        break;
      case 5:
        Alarm.alarmRepeat(h, m, 0, offAlarm5);
        Alarm.alarmRepeat(h, m, 1, offAlarm5);
        //Serial.print("(alarm off)");
        break;
      case 6:
        Alarm.alarmRepeat(h, m, 0, offAlarm6);
        int iRes = Alarm.alarmRepeat(h, m, 1, offAlarm6);
        //String sRes = "iRes = ";
        //sRes = sRes + iRes;
        //Serial.println(sRes);
        break;
      }
    }
   //Serial.println();
  }
  Serial.flush();
}

void onAlarm1(){
  mySwitch.switchOn(MY_GROUP2, MY_1ST);
  EEPROM.write(101, HIGH);
}

void offAlarm1(){
  mySwitch.switchOff(MY_GROUP2, MY_1ST);
  EEPROM.write(101, LOW);
}

void onAlarm2(){
  mySwitch.switchOn(MY_GROUP2, MY_2ND);
  EEPROM.write(102, HIGH);
}

void offAlarm2(){
  mySwitch.switchOff(MY_GROUP2, MY_2ND);
  EEPROM.write(102, LOW);
}

void onAlarm3(){
  mySwitch.switchOn(MY_GROUP2, MY_3RD);
  EEPROM.write(103, HIGH);
}

void offAlarm3(){
  mySwitch.switchOff(MY_GROUP2, MY_3RD);
  EEPROM.write(103, LOW);
}

void onAlarm4(){
  mySwitch.switchOn(MY_GROUP2, MY_4TH);
  EEPROM.write(104, HIGH);
}

void offAlarm4(){
  mySwitch.switchOff(MY_GROUP2, MY_4TH);
  EEPROM.write(104, LOW);
}

void onAlarm5(){
  digitalClockDisplay();
  Serial.println("Alarm#5: - turn it on");           
  Serial.flush();
  mySwitch.switchOn(MY_GROUP2, MY_5TH);
  EEPROM.write(105, HIGH);
}

void offAlarm5(){
  digitalClockDisplay();
  if(weekday() == 2 || weekday() == 3 || weekday() == 5 || weekday() == 6){
    Serial.println("Alarm#5: - turn it off");
    Serial.flush();
    mySwitch.switchOff(MY_GROUP2, MY_5TH);
    EEPROM.write(105, LOW);
  }
}

void onAlarm6(){
  digitalClockDisplay();
  if(weekday() == 2 || weekday() == 3 || weekday() == 5 || weekday() == 6){
    Serial.println("Alarm#6: - turn it on");           
    Serial.flush();
    mySwitch.switchOn(MY_GROUP2, MY_6TH);
    EEPROM.write(106, HIGH);
  }
}

void offAlarm6(){
  digitalClockDisplay();
  Serial.println("Alarm#6: - turn it off");           
  Serial.flush();
  mySwitch.switchOff(MY_GROUP2, MY_6TH);
  EEPROM.write(106, LOW);
}

// 2a) select the requested pin# for DigitalWrite action
void set_digitalwrite(int pin_num, int pin_value)
{
  tmElements_t tm;
    
  switch (pin_num) {
  case 13:
    //pinMode(13, OUTPUT);  // #1
    //digitalWrite(13, pin_value);
    EEPROM.write(101, pin_value);
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP2, MY_1ST);
    else
      mySwitch.switchOff(MY_GROUP2, MY_1ST);
    break;
  case 12:
    //pinMode(12, OUTPUT);  // #2
    //digitalWrite(12, pin_value);   
    EEPROM.write(102, pin_value);
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP2, MY_2ND);
    else
      mySwitch.switchOff(MY_GROUP2, MY_2ND);
    break;
  case 11:
    //pinMode(11, OUTPUT);  // #3
    //digitalWrite(11, pin_value);         
    EEPROM.write(103, pin_value);
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP2, MY_3RD);
    else
      mySwitch.switchOff(MY_GROUP2, MY_3RD);
    break;
  case 10:
    //pinMode(10, OUTPUT);  // #4
    //digitalWrite(10, pin_value);         
    EEPROM.write(104, pin_value);
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP2, MY_4TH);
    else
      mySwitch.switchOff(MY_GROUP2, MY_4TH);
    break;
  case 9:
    //pinMode(9, OUTPUT);  // #5
    //digitalWrite(9, pin_value);         
    EEPROM.write(105, pin_value);
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP2, MY_5TH);
    else
      mySwitch.switchOff(MY_GROUP2, MY_5TH);
    break;
  case 8:
    //pinMode(8, OUTPUT);  // #6
    //digitalWrite(8, pin_value);         
    EEPROM.write(106, pin_value);
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP2, MY_6TH);
    else
      mySwitch.switchOff(MY_GROUP2, MY_6TH);
    break;
  case 7:
    //pinMode(7, OUTPUT);  // #7
    //digitalWrite(7, pin_value);         
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP2, "00101");
    else
      mySwitch.switchOff(MY_GROUP2, "00101");
    break;
  case 6:
    //pinMode(6, OUTPUT);  // #8
    //digitalWrite(6, pin_value);         
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP2, "00110");
    else
      mySwitch.switchOff(MY_GROUP2, "00110");
    break;
  case 5:
    //pinMode(5, OUTPUT);  // #9
    //digitalWrite(5, pin_value); 
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP2, "00111");
    else
      mySwitch.switchOff(MY_GROUP2, "00111");
    break;
  case 4:
    //pinMode(4, OUTPUT);  // #10
    //digitalWrite(4, pin_value);         
    if(pin_value == HIGH)
      mySwitch.switchOn(MY_GROUP2, "01001");
    else
      mySwitch.switchOff(MY_GROUP2, "01001");
    break;
  case 3:
    //pinMode(3, OUTPUT);  // #11
    //digitalWrite(3, pin_value);         
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
    //pinMode(2, OUTPUT);  // #12
    //digitalWrite(2, pin_value); 
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

unsigned long crc_update(unsigned long crc, byte data)
{
    byte tbl_idx;
    tbl_idx = crc ^ (data >> (0 * 4));
    crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
    tbl_idx = crc ^ (data >> (1 * 4));
    crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
    return crc;
}

unsigned long crc_string(char *s)
{
  unsigned long crc = ~0L;
  while (*s)
    crc = crc_update(crc, *s++);
  crc = ~crc;
  return crc;
}

void print_crc()
{
  Serial.println(crc_string("HELLO"), HEX);
}
