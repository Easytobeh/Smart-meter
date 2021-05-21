/*Integrated ACS712 Current Sensor
  Compute Energy Measurement
  Added tampering features
  Added barebone billing system
  Included GSM communication
  Including WiFi capability
  GSM PIN CONNECTION
  -------------------
  TX PIN ---- ARDUINO PIN 10
  RX PIN ---- ARDUINO Pin 11

*/

//Libraries/////////////////////////////////////////////////////////////////////
#include <EEPROM.h>
#include <ACS712.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal.h>
#include <String.h>
#include "Sim800l.h"
#include "EmonLib.h"

#define BUZZER 18
#define RELAY 8
#define CUR_SENSOR A14
#define TAMPER_PIN 3
//#define MAINS_AC_INPUT 16

//Allocate memory space on EEPROM
#define addr_unit_left 0
#define addr_last_recharge 8
#define addr_total_consumption 16
//#define addr_temp_recharge 16


Sim800l sim;
ACS712 measure(DEFAULT, CUR_SENSOR);
LiquidCrystal lcd(13, 12, 7, 6, 5, 4);
SoftwareSerial espSerial =  SoftwareSerial(14, 16);  

unsigned long previousMillis = 0;
const long interval = 15000;

float unit_left = 0.00f ;
float last_recharge = 0.00f;
float total_consumption = 00.00f;

unsigned long last_time = 0;
unsigned long current_time = 0;
float current_energy, total_energy, I, P, V = 220.0;

String msg, msgNumber, msgCode;
String _authorizedNumber = "+2348134967161";

//char _total_energy[6];
//char _last_recharge[6];
//char _P[6];
//char _unit_left[6];
//char _current_energy[6];


byte pixel1[8] = {B10000, B10000, B10000, B10000, B10000, B10000, B10000, B10000,};
byte pixel2[8] = {B11000, B11000, B11000, B11000, B11000, B11000, B11000, B11000,};
byte pixel3[8] = {B11100, B11100, B11100, B11100, B11100, B11100, B11100, B11100,};
byte pixel4[8] = {B11110, B11110, B11110, B11110, B11110, B11110, B11110, B11110,};
byte pixel5[8] = {B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111,};

//===================ESP8266 PARAMETERS=====================//

String apiKey = "WXBI69W2FN8K96MW";     // replace with your channel's thingspeak WRITE API key
String ssid="mregba";    // Wifi network SSID
String password ="egbaomoh";  // Wifi network password
boolean DEBUG=true;

//======================================================================== showResponse
void showResponse(int waitTime){
//long t=millis();
//void calcPower(float, float);
//char c;
//while (t+waitTime>millis()){
if (espSerial.available()){
char c=espSerial.read();
if (DEBUG) Serial.print(c);
}

//}
}
//=======================connection to thinkspeak.com=================================================
boolean thingSpeakWrite(float value1, float value2, float value3){
String cmd = "AT+CIPSTART=\"TCP\",\"";                  // TCP connection
cmd += "184.106.153.149";                               // api.thingspeak.com
cmd += "\",80";
espSerial.println(cmd);
if (DEBUG) Serial.println(cmd);
if(espSerial.find("Error")){
if (DEBUG) Serial.println("AT+CIPSTART error");
return false;
}
String getStr = "GET /update?api_key=";   // prepare GET string
getStr += apiKey;
getStr +="&field1=";
getStr += String(value1);
getStr +="&field2=";
getStr += String(value2);
getStr +="&field3=";
getStr += String(value3);
// ...
getStr += "\r\n";
// send data length
cmd = "AT+CIPSEND=";
cmd += String(getStr.length());
if(espSerial.find(">")){
espSerial.println(cmd);
if (DEBUG)  Serial.println(cmd);
delay(100);
espSerial.print(getStr);
if (DEBUG)  Serial.print(getStr);
//lcd.clear();
lcd.setCursor(0,3);
lcd.print("Connected to:"+ssid);
delay(2000);
}
else{
Serial.println("Connection loss");
espSerial.println("AT+CIPCLOSE");
// alert user
if (DEBUG)   Serial.println("AT+CIPCLOSE");
return false;
}
return true;
}
//=========================================================//


void setup()
{
  DEBUG = true;
  sim.begin();
  Serial.begin(9600);
  espSerial.begin(9600);
  lcd.begin(16, 4);
  delay(100);

  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("TEAM WIN");
  lcd.setCursor(1, 1);
  lcd.print("Smart Energy");
  lcd.setCursor(4, 2);
  lcd.print("Meter");
  delay(2000);
  
  //======ESP8266 Configuration===========///
  showResponse(1000);
//espSerial.println("AT+UART_CUR=9600,8,1,0,0");    // Enable this line to set esp8266 serial speed to 9600 bps
showResponse(1000);
espSerial.println("AT+CWMODE=1");   // set esp8266 as client
showResponse(1000);
espSerial.println("AT+CWJAP=\""+ssid+"\",\""+password+"\"");  // set your home router SSID and password
lcd.print("Configuring WiFi to");
lcd.setCursor(0,1);
lcd.print("connect to:"+ssid+"'s");
lcd.setCursor(0,2);
lcd.print("Network");
delay(5000);
showResponse(5000);
if (DEBUG)  Serial.println("Setup completed");
delay(3000);

  measure.calibrate();

  pinMode(CUR_SENSOR, INPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(TAMPER_PIN, INPUT);
  //pinMode(MAINS_AC_INPUT, INPUT);

  digitalWrite(BUZZER, false);
  digitalWrite(RELAY, false);

  lcd.createChar (0, pixel1);
  lcd.createChar (1, pixel2);
  lcd.createChar (2, pixel3);
  lcd.createChar (3, pixel4);
  lcd.createChar (4, pixel5);

  sim.delAllSms();
  readfromEEPROM();
  delay(2000);
  lcd.clear();
}

//////////////////////////////////////////////////////////////////////////////////////

void loop()
{
  displayParameters();
  msg = sim.readSms(1);
  bool tamper = digitalRead(TAMPER_PIN);
  if (tamper)
    engageTamperMode(tamper);

  //Check for energy unit
  if (unit_left > 0.00) {
    digitalWrite(RELAY, true);
    Serial.println("I am in loop");
    computeEnergy();
    lcd.clear();
    displayParameters();
    delay(1000);
  }
  if (msg.indexOf("OK") != -1) //non-empty message
    gsmFunction();

  delay(200);

  unsigned long currentMillis = millis();
 
  if(currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED 
    previousMillis = currentMillis;   
    thingSpeakWrite(P, current_energy, total_energy);
    }

}

///////////Energy Calculation ////////////////////////////////////////////

float computeEnergy() {
  Serial.println("I am in computeEnergy");
  I = measure.getCurrentAC(50);
  if (I <= 0.07)
    I = 0.00; //taking care of error margin.
  P = V * I;
  last_time = current_time;
  current_time = millis() / 1000; //convert millisecs to secs
  unsigned interval = current_time - last_time;
  current_energy = P * interval / 3600.0 / 1000 ; //convert interval from sec to hr, and Wh to kWh
  total_energy += current_energy ;

  updateMemory(0);
  Serial.print("last time:");
  Serial.println(last_time);
  Serial.print("current time:");
  Serial.println(current_time);
}

void updateMemory(int updateCode) {

  //when it loops
  if (updateCode == 0) {
    // bool mains = digitalRead(MAINS_AC_INPUT);
    if (unit_left > 0.00)
    {
      unit_left -= current_energy;
      total_consumption += current_energy;

      EEPROM.put(addr_unit_left, unit_left);
      EEPROM.put(addr_total_consumption, total_consumption);
    }
  }

  //when there is a new recharge
  else if (updateCode == 1)
  {
    lcd.clear();
    lcd.print("Updating memory");
    
    EEPROM.put(addr_unit_left, unit_left);
    EEPROM.put(addr_last_recharge, last_recharge);

    lcd.setCursor(0, 1);
    for (int i = 0; i < 16; i++)
    { //loops through lcd's 16 pixels
      for (int j = 0; j < 5; j++) { //loops through the five column of each pixel
        lcd.setCursor(i, 1);
        lcd.write(j);
        delay(100);
      }
      delay(100);
    }
    delay(1000);
    lcd.clear();
    lcd.print("Sending Message");
    sim.sendSms(_authorizedNumber, "Recharge successsful");
    delay(1000);
  }


}
///////////Display ////////////////////////////////////////////////////

void displayParameters()
{
  lcd.clear();
  Serial.println("I am in display");
  lcd.print("Current:");
  lcd.print(I);
  lcd.print("A ");

  lcd.setCursor(0, 1);
  lcd.print("Power:");
  lcd.print(P);
  lcd.print("W");
  
//  lcd.setCursor(0, 2);
//  lcd.print("L:");
//  lcd.print(last_recharge);
//  lcd.print("kWh");

  lcd.setCursor(0, 2);
  lcd.print("E:");
  lcd.print(total_energy, 5);
  lcd.print("kWh");

  lcd.setCursor(0, 3);
  lcd.print("Bal:");
  lcd.print(unit_left, 3);
  lcd.print("kWh");
}

//copying values from EEProm/////////////////////////////////////////////////////
void readfromEEPROM()
{
  EEPROM.get(addr_unit_left, unit_left);
  delay(200);
  EEPROM.get(addr_last_recharge, last_recharge);
  delay(200);
  EEPROM.get(addr_total_consumption, total_consumption);
}

/******************************************/

void engageTamperMode(bool tamperState) {
  Serial.println("I'm being tampered");
  lcd.clear();
  lcd.print("Device Tampered!");
  delay(2000);
  lcd.clear();
  lcd.print("  Initiating  ");
  lcd.setCursor(3, 1);
  lcd.print("lock mode");
  delay(3000);
  lcd.setCursor(1, 2);
  lcd.print("Engaging Alarm");
  delay(2000);
  digitalWrite(BUZZER, true); //engage buzzer
  delay(2000);
  lcd.setCursor(0, 3);
  lcd.print("Cutting off load");
  delay(3000);
  digitalWrite(RELAY, false);  //load cut off from supply

  //remain in lock mode if while device is tampered
  while (tamperState)
  { //Write message to display while in lock mode
    Serial.println("I am still tampered");
    lcd.clear();
    lcd.print("   Device in ");
    lcd.setCursor(3, 1);
    lcd.print("Lock Mode! ");
    delay(500);
    tamperState = digitalRead(TAMPER_PIN);
    delay(500);
    Serial.print("current time:");
    Serial.println(current_time);
  }
  //disengage lock mode
  disengageTamperMode();
}

//****************************************////
void disengageTamperMode() {
  Serial.println("I am now disengaging");
  lcd.clear();
  lcd.print("   Disabling");
  lcd.setCursor(2, 1);
  lcd.print(" Lock Mode");
  delay(2000);
  digitalWrite(BUZZER, false);
  lcd.clear();
  lcd.print("  Lock Mode");
  lcd.setCursor(3, 1);
  lcd.print("Disabled");
  delay(1000);
  lcd.setCursor(1, 2);
  lcd.print("Engaging Load");
  delay(2000);
  digitalWrite(RELAY, true);
  delay(1000);
  lcd.clear();

  //Set these values to zero that the interval for calculating
  //Energy in computeEnergy() would equal zero, that is no load
  //detected at that tme.
  last_time = millis() / 1000;
  current_time = last_time;
}


/////***********************************************/////

void gsmFunction() {

  lcd.clear();
  Serial.println("Incoming Message");
  lcd.print("Incoming message");
  delay(1000);
  lcd.clear();
  lcd.print("Message received");
  Serial.print("Message received");
  msgNumber = sim.getNumberSms(1);

  if (msgNumber == _authorizedNumber)
  {
    msg.toUpperCase();  //converts message to uppercase so message is not case sensitive
    int i = msg.lastIndexOf('"');
    msgCode = msg.substring((i + 1), msg.length());
    String charCode = msgCode.substring(2, 3); //Get the first letter of the message
    
    //char temp[1];
    // charCode.toCharArray(temp, 1);
    // lcd.print(temp[0]);
   // lcd.clear();
    //delay(5000);
    if (charCode == "R") {
      rechargeEnergy(); //function for recharge the
    }

    else if (charCode == "D") {
      getStatus();
    }

    else
    {
      lcd.clear();
      lcd.print("Error Code");
      delay(2000);
    }
    //    switch (charCode)
    //    {
    //      case 'R':
    //        rechargeEnergy(); //function for recharge the unit
    //        break;
    //      case 'D':
    //        getStatus(); //query device for details
    //        break;
    //      default:
    //        //non recognise message
    //        {
    //          lcd.clear();
    //          lcd.print("Error Code");
    //          delay(2000);
    //        }
    //        break;
    //    }
  }

  else             //not authorised sender number
  { lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print("Error!");
    Serial.println("Error!");
    delay(500);
    lcd.setCursor(1, 0);
    lcd.print("Unauthorised");
    Serial.println("Unauthorised");
    lcd.setCursor(0, 1);
    lcd.print("Message received");
    Serial.println("Message received");
    delay(1000);
  }
  sim.delAllSms();
}

void rechargeEnergy() {
  String temp = msgCode.substring(3, msg.length()); //get the remainder of the message content after the first letter.
  float newCredit = temp.toInt();

  last_recharge = newCredit;
  unit_left += newCredit;
  lcd.clear();
  lcd.print("Topping up your");
  lcd.setCursor(0, 1);
  lcd.print("Device with");
  lcd.setCursor(4, 2);
  lcd.print(newCredit);
  lcd.print("kWh");
  lcd.setCursor(0, 3);
  lcd.print("Credit Unit");
  delay(3000);
  updateMemory(1);
}

void getStatus() {
  String details;
  lcd.clear();
  lcd.print("Bundling data to");
  lcd.setCursor(0, 1);
  lcd.print("Send");
  for (int p = 0; p <= 5; p++) {
    lcd.setCursor((4 + p), 1);
    lcd.print(".");
    delay(800);
  }
  delay(1000);
  lcd.clear();
  lcd.print("Sending details");

  //  if (digitalRead(MAINS_AC_INPUT))
  //    powerStatus = "Mains Present";
  //  else
  //    powerStatus = "Mains Absent";

//  dtostrf(total_energy, 6, 5, _total_energy);
//  dtostrf(last_recharge, 6, 2, _last_recharge);
//  dtostrf(P, 6, 2, _P);
//  dtostrf(unit_left, 6, 2, _unit_left);
//  dtostrf(current_energy, 6, 5, _current_energy);

  details = "P:";
  details += P;
  details += "kW, E:";
  details +=  current_energy;
  details += "kWh";
  details += ", Total E:";
  details +=  total_energy;
  details += "kWh, Last Recharge:";
  details += last_recharge;
  details += "kWh, Unit left:";
  details += unit_left;
//lcd.clear();
Serial.println(details);
delay(1000);
  // details = "P:" + _P + "kW, E:" + _current_energy + "kWh";
  // details += ", Total E:" + _total_energy + "kWh, Last Recharge:";
  // details += _last_recharge + "kWh, Units left:" + _unit_left;
  sim.sendSms(_authorizedNumber, details);


}
