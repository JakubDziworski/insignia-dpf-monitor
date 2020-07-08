#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include "ELMduino.h"

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display
ELM327 vgate;
SoftwareSerial vgateSerial(10, 7);

const long debugRate = 115200;

void setup()
{
  Serial.begin(debugRate);
  Serial.println("START");
  setupBt();
  lcd.init();
  lcd.backlight();
}

void loop()
{
  if(isBtConfBtnPressed()) {
    pairWithVgate();
  }
  if(!vgate.connected) {
    connect();
    delay(1000);
    return;
  }
  int32_t regenStatus = getRegenerationStatus();
  if(regenStatus > 0) {
    printRegenerating(regenStatus);
    return;
  }
  printDpfStatus();
  delay(1000);
}

void connect() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("CONNECTING...");
  lcd.setCursor(0,1);
  if (!vgate.begin(vgateSerial))
  {
    Serial.println("Couldn't connect to OBD scanner");
    lcd.print("VGATE UNCONNECTED");
    return;
  }
  if(vgate.sendCommand("AT SH 7E0") != ELM_SUCCESS) {
    Serial.println("Unable to set header to 7E0");
    lcd.print("UNABLE TO SET AT SH 7E0");
    return;
  }
  lcd.print("CONNECTED");
  Serial.println("Connected to OBD scanner");
}

void printRegenerating(int32_t regenStatus) {
  int32_t dirtLevel = getDpfDirtLevel();
  String statusMessage = "STATUS: ";
  statusMessage = statusMessage + regenStatus;
  String dirtLevelMessage = "FILL: ";
  dirtLevelMessage = dirtLevelMessage + dirtLevel;
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(statusMessage);
  lcd.setCursor(0,1);
  lcd.print(dirtLevelMessage);
  lcd.noBacklight();
  delay(200);
  lcd.backlight();
  delay(200);
}

void printDpfStatus() {
  int32_t kmsSinceDpf = getKmsSinceDpf();
  int32_t dirtLevel = getDpfDirtLevel();
  lcd.clear();
  lcd.setCursor(0,0);
  String message = "LAST: ";
  message = message + kmsSinceDpf + "KM";
  Serial.println(message);
  lcd.print(message);
  lcd.setCursor(0,1);
  message = "FILL: ";
  message = message + dirtLevel + "%";
  lcd.print(message);
}

int32_t getRegenerationStatus() {
  return queryVgate(0x22, 0x3274);
}

int32_t getKmsSinceDpf() {
  return queryVgate(0x22, 0x3277);
}

int32_t getDpfDirtLevel() {
  return queryVgate(0x22, 0x3275);
}


int32_t queryVgate(uint8_t service, uint16_t pid) {
  Serial.print("Response for ");
  Serial.print(pid);
  Serial.print(": ");
  if (vgate.queryPID(service, pid))
  {
    int32_t response = vgate.findResponse();
    if (vgate.status == ELM_SUCCESS)
    {
      Serial.print("SUCCESS:"); Serial.println(response);
      return response;
    }
    else if (vgate.status == ELM_NO_RESPONSE)
      Serial.println("ERROR: ELM_NO_RESPONSE");
    else if (vgate.status == ELM_BUFFER_OVERFLOW)
      Serial.println("ERROR: ELM_BUFFER_OVERFLOW");
    else if (vgate.status == ELM_GARBAGE)
      Serial.println("ERROR: ELM_GARBAGE");
    else if (vgate.status == ELM_UNABLE_TO_CONNECT)
      Serial.println("ERROR: ELM_UNABLE_TO_CONNECT");
    else if (vgate.status == ELM_NO_DATA)
      Serial.println("ERROR: ELM_NO_DATA");
    else if (vgate.status == ELM_STOPPED)
      Serial.println("ERROR: ELM_STOPPED");
    else if (vgate.status == ELM_TIMEOUT)
      Serial.println("ERROR: ELM_TIMEOUT");
    else if (vgate.status == ELM_GENERAL_ERROR)
      Serial.println("ERROR: ELM_GENERAL_ERROR");
    else {
      Serial.print("ERROR: UNKNOWN ELM STATUS:");
      Serial.println(vgate.status);
    }
  }
  else {
    Serial.println("NOT CONNECTED");
  }
  return -1;
}





//##### BLUETOOTH AUTOCONFIGURATION SECTION #########
const int btConfPin = 3 ;
const int btConfBtn = 4;
const int btPowerPin = 5;
const long vgateRate = 9600;
const long confRate = 38400;

void setupBt() {
  vgateSerial.begin(vgateRate);
  pinMode(btPowerPin, OUTPUT);
  pinMode(btConfPin, OUTPUT);
  pinMode(btConfBtn, INPUT_PULLUP);
  digitalWrite(btPowerPin, HIGH);
  digitalWrite(btConfPin, LOW);
}

boolean isBtConfBtnPressed() {
  Serial.println(digitalRead(btConfBtn));
  return digitalRead(btConfBtn) == LOW;
}

void pairWithVgate()
{
    lcd.clear();
    vgateSerial.end();
    vgateSerial.begin(confRate);

    digitalWrite(btPowerPin, LOW);
    digitalWrite(btConfPin, HIGH);
    delay(1000);
    digitalWrite(btPowerPin, HIGH);
    delay(4000);

    sendBtConfCommand("AT+RMAAD");
    sendBtConfCommand("AT+ORGL");
    sendBtConfCommand("AT+ROLE=1");
    sendBtConfCommand("AT+RESET",4000);
    sendBtConfCommand("AT+CMODE=0");
    sendBtConfCommand("AT+BIND=86DC,3D,ABF7F1");
    sendBtConfCommand("AT+INIT");
    sendBtConfCommand("AT+PAIR=86DC,3D,ABF7F1,20",10000L);
    sendBtConfCommand("AT+LINK=86DC,3D,ABF7F1",10000L);
    Serial.println("Done");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("BLUETOOTH CONF");
    lcd.setCursor(0,1);
    lcd.print("DONE");
    delay(3000);
    vgateSerial.end();
    vgateSerial.begin(confRate);

    digitalWrite(btPowerPin, LOW);
    digitalWrite(btConfPin, LOW);
    delay(1000);
    digitalWrite(btPowerPin, HIGH);
    lcd.clear();
}

void sendBtConfCommand(char* cmd) {
  sendBtConfCommand(cmd, 2000);
}

void sendBtConfCommand(char* cmd, long timeout) {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(cmd);
  lcd.setCursor(0,1);
  Serial.println(cmd);
  long start = millis();
  vgateSerial.println(cmd);

  while((millis() - start) < timeout) {
    if (vgateSerial.available()) {
      char c = vgateSerial.read();
      lcd.print(c);
      Serial.write(c);
    }
    delay(5);
  }
  lcd.clear();
}