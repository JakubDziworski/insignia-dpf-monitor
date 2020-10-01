#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include "ELMduino.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display
ELM327 vgate;
SoftwareSerial vgateSerial(12, 7);

const long debugRate = 115200;

void setup()
{
  Serial.begin(debugRate);
  Serial.println("START");
  setupBt();
  setupTFT();
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
  String voltage = getVoltage();
  printFrontLcd();
  int32_t regenStatus = getRegenerationStatus();
  if(regenStatus > 0) {
    printRegenerating(regenStatus);
    return;
  }
  printDpfStatus(voltage);
  delay(100);
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
  int32_t percentRegenerated = map(regenStatus, 0, 255, 0, 100);
  String statusMessage = "      ";
  statusMessage = statusMessage + percentRegenerated + "%      ";
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("!!REGENERATING!!");
  lcd.setCursor(0,1);
  lcd.print(statusMessage);
  lcd.noBacklight();
  delay(200);
  lcd.backlight();
  delay(200);
}

void printDpfStatus(String voltage) {
  int32_t kmsSinceDpf = getKmsSinceDpf();
  int32_t dirtLevel = getDpfDirtLevel();
  lcd.clear();
  lcd.setCursor(0,0);
  String message = "LAST: ";
  message = message + kmsSinceDpf + "KM";
  lcd.print(message);
  lcd.setCursor(0,1);
  message = "FILL: ";
  message = message + dirtLevel + "%";
  lcd.print(message);
  lcd.setCursor(11,1);
  message = voltage + "V";
  lcd.print(message);
}

int32_t lastPressure = 0;
void printFrontLcd() {
  int32_t oilPressure = getOilPressure();
  if(lastPressure == oilPressure) {
    return;
  }
  uint32_t rpm = vgate.rpm();
  printOilPressure(oilPressure, rpm, false);
}

int32_t getRegenerationStatus() {
  return queryPID(0x22, 0x3274);
}

int32_t getKmsSinceDpf() {
  return queryPID(0x22, 0x3277);
}

int32_t getDpfDirtLevel() {
  return queryPID(0x22, 0x3275);
}

int32_t getOilPressure() {
  return queryPID(0x22, 0x1470)*4;
}

String getVoltage() {
  int32_t resp = queryCommand(READ_VOLTAGE);
  if(resp == -1) {
    return "-1";
  }
  String result;
  for(int i =0; i<vgate.recBytes; i++) {
    char character = vgate.payload[i];
    if(isDigit(character) || character == '.') { //sometimes there is some garbage chars in response
      result += character;
    }
  }
  return result;
}

int32_t queryPID(uint8_t service, uint16_t pid) {
  int8_t result = vgate.queryPID(service, pid);
  return handleResponse(pid,result);
}

int32_t queryCommand(const char* command) {
  int8_t result = vgate.sendCommand(command);
  return handleResponse(command, result);
}

int32_t handleResponse(const char* command, uint8_t result) {
  Serial.print("Response for ");
  Serial.print(command);
  Serial.print(": ");
  if (vgate.status == ELM_SUCCESS)
  {
    int32_t response = vgate.findResponse();
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
  return -1;
}


//##### TFT SECTION #########

#define TFT_CS        10
#define TFT_RST        8 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC         9

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);


void setupTFT() {
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST77XX_WHITE);
  tft.setRotation(1);
  tft.setTextWrap(false);
}


void drawProgressBar(float value, float maxValue, int x, int y, int w, int h, const char* unit, uint16_t color,unsigned const int decimal_places) {
    int progress = min(w,(value/maxValue)*w);
    tft.fillRect(x,y,progress,h,color);
    tft.fillRect(x+progress,y,w-progress,h,tft.color565(180,180,180));
    tft.setTextSize(3);
    tft.setTextColor(ST77XX_WHITE);
    int16_t  tx, ty;
    uint16_t tw, th;
    tft.getTextBounds(String(value,decimal_places), x+w/2, y+h/2, &tx, &ty, &tw, &th);
    tft.setCursor(x+w/2-tw/2, y+h/2-th/2);
    tft.print(value,decimal_places);
    tft.setTextSize(1);
    tft.getTextBounds(unit, x+w/2, y+h/2, &tx, &ty, &tw, &th);
    tft.setCursor(x+w-tw-2,y+2);
    tft.print(unit);
    tft.drawRoundRect(x-1, y-1, w+2, h+2, 3, tft.color565(234, 234, 234));
}


void printOilPressure(int pressure, int rpm, bool is_critical) {
  drawProgressBar(pressure, 500, 5,5,tft.width() - 10, tft.height()/2-10, "KPA", tft.color565(0, 168, 87),0);
  drawProgressBar((float)pressure/(float)rpm, 0.5f, 5, tft.height()/2+5, tft.width() - 10, tft.height()/2-10, "KPA/RPM", tft.color565(0, 140, 168),2);
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

//##### AUTOCONFIGURATION SECTION #########
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