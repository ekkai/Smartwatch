#include <SoftwareSerial.h>
#include <math.h>
#include "bitmap.h"
#include "U8glib.h"


/////////////////OLED///////////////
#define OLED_SCK  13
#define OLED_MOSI 11
#define OLED_CS   10
#define OLED_RST  12
#define OLED_A0    9
//U8GLIB_SSD1306_128X64 u8g(13, 11, 10, 9, 8);	// SW SPI Com: SCK = 13, MOSI = 11, CS = 10, RST = 9, A0 = 8
U8GLIB_SSD1306_128X64 u8g(OLED_SCK, OLED_MOSI, OLED_CS, OLED_A0, OLED_RST);


PROGMEM const char* strIntro[] = {
  "Nepes", "Watch", "Arduino v1.0"};
  
PROGMEM const char* msgBox[] = {
  "SMS", "CALLING", "APP" , "PUSH"};

//////////////////Bluetooth//////////////////////
#define BT_RX  4
#define BT_TX  5
//#define BT_RST 7
//SoftwareSerial BTSerial(2, 3); //Connect HC-06, RX, T

SoftwareSerial BTSerial(BT_RX, BT_TX);

//----- Display features
#define DISPLAY_MODE_START_UP 0
#define DISPLAY_MODE_CLOCK 1
#define DISPLAY_MODE_EMERGENCY_MSG 2
#define DISPLAY_MODE_NORMAL_MSG 3
#define DISPLAY_MODE_IDLE 11
byte displayMode = DISPLAY_MODE_START_UP;

#define CLOCK_STYLE_SIMPLE_ANALOG  0x01
#define CLOCK_STYLE_SIMPLE_DIGIT  0x02
#define CLOCK_STYLE_SIMPLE_MIX  0x03
byte clockStyle = CLOCK_STYLE_SIMPLE_MIX;

#define INDICATOR_ENABLE 0x01
boolean updateIndicator = true;

#define TR_MODE_IDLE 1

#define TR_MODE_SETUP 1
#define TR_MODE_NOTI 2

#define TR_MODE_SETUP_TIMETYPE 1
#define TR_MODE_SETUP_DATE 2

#define TR_MODE_SETUP_TIMETYPE_ANALOG 1
#define TR_MODE_SETUP_TIMETYPE_DIGITAL 2
#define TR_MODE_SETUP_TIMETYPE_ANALDIGI 3

#define TR_MODE_SETUP_DATE_DATE 1

#define TR_MODE_NOTI_NOTI 1

#define TR_MODE_NOTI_NOTI_SMS 1
#define TR_MODE_NOTI_NOTI_CALL 2
#define TR_MODE_NOTI_NOTI_NOTI 3
#define TR_MODE_NOTI_NOTI_APPS 4


byte TRANSACTION_POINTER = TR_MODE_IDLE;

//----- Message item buffer
#define MSG_COUNT_MAX 40
char msgBuffer[MSG_COUNT_MAX];
char msgParsingLine = 0;
char msgParsingChar = 0;
char msgCurDisp = 0;

unsigned long next_display_interval = 0;
unsigned long mode_change_timer = 0;
#define CLOCK_DISPLAY_TIME 300000
#define EMER_DISPLAY_TIME 10000
#define MSG_DISPLAY_TIME 1000

//----- Emergency item buffer
#define EMG_COUNT_MAX 3
#define EMG_BUFFER_MAX 19
char emgBuffer[EMG_COUNT_MAX][EMG_BUFFER_MAX];
char emgParsingLine = 0;
char emgParsingChar = 0;
char emgCurDisp = 0;

//////////----- Time -----///////////
#define UPDATE_TIME_INTERVAL 1000
byte iMonth = 1;
byte iDay = 1;
byte iWeek = 1;    // 1: SUN, MON, TUE, WED, THU, FRI,SAT
byte iAmPm = 0;    // 0:AM, 1:PM
byte iHour = 0;
byte iMinutes = 0;
byte iSecond = 0;

#define TIME_BUFFER_MAX 6
char timeParsingIndex = 0;
char timeBuffer[6] = {
  -1, -1, -1, -1, -1, -1};
PROGMEM const char* weekString[] = {
  "", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
PROGMEM const char* ampmString[] = {
  "AM", "PM"};

#define IDLE_DISP_INTERVAL 300000
#define CLOCK_DISP_INTERVAL 1000
#define EMERGENCY_DISP_INTERVAL 5000
#define MESSAGE_DISP_INTERVAL 2000
unsigned long prevClockTime = 0;
unsigned long prevDisplayTime = 0;


//----- Button control
int buttonPin = 2;
int buttonPin1 = 3;
boolean isClicked = HIGH;
boolean isClicked1 = HIGH;

//-----------Any else------------

byte msg[40]; //char receive
char sendMsg[40];
int transDate[10];
byte mSize = 0;  
char isync[2];                   //convert char
unsigned long current_time = 0;

byte centerX = 64;
byte centerY = 32;
byte iRadius = 28;

//------------------Packet variable------------------------
#define MAX_PACKET_SIZE 20
#define HEADER_PAGESIZE_INDEX 4
#define HEADER_PAGEINDEX_INDEX 3

byte pageSize;    //packet size
byte pageIndex;   //packet index number
byte prevIndex = 0;
byte pCnt = 0;    //packet count
byte pSize[5];    //packet array

void setup() {
  Serial.begin(9600);
  Serial.println(F("SmartWatch v1.0"));

  //----- Set OLED reset pin HIGH  
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, HIGH);

  //----- Set button
  pinMode(buttonPin, INPUT);  // Defines button pin
  pinMode(buttonPin1, INPUT);  // Defines button pin

  //----- Set display features
  centerX = u8g.getWidth() / 2;
  centerY = u8g.getHeight() / 2;
  iRadius = centerY - 2;

  //----- Setup serial connection with BT
  BTSerial.begin(9600);  // set the data rate for the BT port
  //Serial.begin(9600);

  //----- Show logo
  drawStartUp();    // Show RetroWatch Logo
  delay(3000);
}

void loop() {
  boolean isReceived = false;

  if(digitalRead(buttonPin) == LOW) isClicked = LOW;
/*
  if(digitalRead(buttonPin1) == LOW) { 
    isClicked1 = LOW; 
    String syncTime = "1";
    syncTime.toCharArray(sendMsg, 6);
    sendBluetoothData(sendMsg);
  }
*/
  isReceived = recvBluetoothData();
  if(mSize != 0) {
    enterMenu();
  }
  mSize = 0;

  current_time = millis();
  updateTime(current_time);

  // Display routine
  onDraw(current_time);
}


boolean recvBluetoothData() {
  int isTransactionEnded = false;

  while(!isTransactionEnded) {
    if(BTSerial.available()) {
      if(pCnt < 5) {                          //Receive Packet
        pSize[pCnt] = BTSerial.read();

        Serial.print("pSize : ");
        Serial.print(pCnt);
        Serial.print(" - value : ");
        Serial.println(pSize[pCnt]); 

        pCnt++;

      }
      else {
        msg[mSize] = BTSerial.read();
        //int pos = atoi(buffer);
        if(msg[mSize] != 10) {
          Serial.print("mSize : ");
          Serial.print(mSize);
          Serial.print(" - value : ");
          Serial.println(msg[mSize]); 
          mSize++;
        }
        else {
          pCnt%=5;
          pageSize = pSize[4];
          //pageIndex = pSize[3];
          Serial.print("pageSize: ");
          Serial.println(pSize[4]);
          Serial.print("pageIndex: ");
          Serial.println(pSize[3]);
          if(prevIndex+1 != pSize[3] || pageIndex > pageSize) { 
            Serial.println("data loss!!");
            break;
          }
          else {
            prevIndex+=1;
          }
        }
      }
    }
    else {
      prevIndex = 0;
      isTransactionEnded = true;
    }
  }

}

/*
boolean sendBluetoothData(char buff[]) {
  byte sendbuff[20];
  while(buff[i] != 0x00) {
    i++;
  }
  sendbuff[0] = 0x03;
  sendbuff[1] = 0x00;
  sendbuff[2] = 0x01;
  sendbuff[3] = 0x01;
  sendbuff[4] = i/20;
  BTSerial.write();
  
}
*/



void enterMenu() {
  TRANSACTION_POINTER = TR_MODE_IDLE;
  if(TRANSACTION_POINTER == TR_MODE_IDLE) {
    TRANSACTION_POINTER = msg[0];
    Serial.print("IDLE : ");
    Serial.println(msg[0]);    
    parseCommand();
  }
}

void parseCommand() {
  if(TRANSACTION_POINTER == TR_MODE_SETUP) {
    TRANSACTION_POINTER = msg[1];
    //////////////////////////////////TIME TYPE SETTING///////////////////////////////////////
    if(TRANSACTION_POINTER == TR_MODE_SETUP_TIMETYPE) {
      TRANSACTION_POINTER = msg[2];
      if(TRANSACTION_POINTER == TR_MODE_SETUP_TIMETYPE_ANALOG) {
        drawClock(CLOCK_STYLE_SIMPLE_ANALOG);
      }
      else if(TRANSACTION_POINTER == TR_MODE_SETUP_TIMETYPE_DIGITAL) {
        drawClock(CLOCK_STYLE_SIMPLE_DIGIT);
      }
      else if(TRANSACTION_POINTER == TR_MODE_SETUP_TIMETYPE_ANALDIGI) {
        drawClock(CLOCK_STYLE_SIMPLE_MIX);
      }
    }
    //////////////////////////////////TIME SETTING///////////////////////////////////////
    else if(TRANSACTION_POINTER == TR_MODE_SETUP_DATE) {
      TRANSACTION_POINTER = msg[2];
      /////////////////////////////////DATE SYNCONIZE/////////////////////////////////////
      if(TRANSACTION_POINTER == TR_MODE_SETUP_DATE_DATE) {
        Serial.println("DATE CHECK");
        parseDate();
        setNextDisplayTime(current_time, 0); 
      }
    }
  }
  else if(TRANSACTION_POINTER == TR_MODE_NOTI) {
    TRANSACTION_POINTER = msg[1];
    //////////////////////////////////TIME TYPE SETTING///////////////////////////////////////
    if(TRANSACTION_POINTER == TR_MODE_NOTI_NOTI) {
      TRANSACTION_POINTER = msg[2];
      if(TRANSACTION_POINTER == TR_MODE_NOTI_NOTI_SMS) {
        displayMode = DISPLAY_MODE_NORMAL_MSG;
        Serial.println("SMS");
        byteToChar();
      }
      else if(TRANSACTION_POINTER == TR_MODE_NOTI_NOTI_CALL) {
        Serial.println("CALL");
        mSize = 3;
        while(msg[mSize]!=0x00) {
          Serial.print((char)msg[mSize]);
          mSize++;
        }
      }
      else if(TRANSACTION_POINTER == TR_MODE_NOTI_NOTI_NOTI) {
        Serial.println("NOTI");
        mSize = 3;
        while(msg[mSize]!=0x00) {
          Serial.print((char)msg[mSize]);
          mSize++;
        }
      }
      else if(TRANSACTION_POINTER == TR_MODE_NOTI_NOTI_APPS) {
        Serial.println("APPS");
        mSize = 3;
        while(msg[mSize]!=0x00) {
          Serial.print((char)msg[mSize]);
          mSize++;
        }
      }
    }
  }
}

void byteToChar() {
  mSize = 3;
  int i = 0;
  while(msg[mSize]!=0x00) {
    msgBuffer[i] = (char)msg[mSize];
    mSize++;
    i++;
  }
}

void parseDate() {
  transDate[0] = sync_dataValue(5,6);  //year

  transDate[1] = sync_dataValue(7,8);  //month

  transDate[2] = sync_dataValue(9,10);  //day
  iDay = transDate[2];
  transDate[3] = sync_dataValue(11,12);  //hour
  iHour = transDate[3];

  if(transDate[3] > 12)
    iAmPm = 1;
  else 
    iAmPm = 0;

  transDate[4] = sync_dataValue(13,14);
  iMinutes = transDate[4];

  transDate[5] = sync_dataValue(15,16);
  iSecond = transDate[5];

  transDate[6] = sync_dataValue(17,18);
  iWeek = transDate[6];

}

int sync_dataValue(int arr, int arr_1) {
  for(int i = arr; i<=arr_1; i++) {
    isync[i-arr] = msg[i];        
  }
  int d = atoi(isync);
  //Serial.print("sync : ");
  //Serial.println(d);
  return d;
}


////////////////////////////////////////////////////draw and exsting source////////////////////////////////////////////
void drawStartUp() {
  // picture loop  
  u8g.firstPage();  
  do {
    u8g_prepare();
    // draw logo
    u8g.drawBitmapP( 10, 15, 3, 24, IMG_logo_24x24);
    // show text
    u8g.setFont(u8g_font_courB14);
    u8g.setFontRefHeightExtendedText();
    u8g.setDefaultForegroundColor();
    u8g.setFontPosTop();
    u8g.drawStr(45, 12, (char*)pgm_read_word(&(strIntro[0])));
    u8g.drawStr(45, 28, (char*)pgm_read_word(&(strIntro[1])));

    u8g.setFont(u8g_font_fixed_v0);
    u8g.setFontRefHeightExtendedText();
    u8g.setDefaultForegroundColor();
    u8g.setFontPosTop();
    u8g.drawStr(45, 45, (char*)pgm_read_word(&(strIntro[2])));
  } 
  while( u8g.nextPage() );

  startClockMode();
}

void u8g_prepare(void) {
  // Add display setting code here
  //u8g.setFont(u8g_font_6x10);
  //u8g.setFontRefHeightExtendedText();
  //u8g.setDefaultForegroundColor();
  //u8g.setFontPosTop();
}


void updateTime(unsigned long current_time) {
  if(iMinutes >= 0) {
    if(current_time - prevClockTime > UPDATE_TIME_INTERVAL) {
      // Increase time
      iSecond++;
      if(iSecond >= 60) {
        iSecond = 0;
        iMinutes++;
      }
      if(iMinutes >= 60) {
        iMinutes = 0;
        iHour++;
        if(iHour > 12) {
          iHour = 1;
          (iAmPm == 0) ? iAmPm=1 : iAmPm=0;
          if(iAmPm == 0) {
            iWeek++;
            if(iWeek > 7)
              iWeek = 1;
            iDay++;
            if(iDay > 30)  // Yes. day is not exact.
              iDay = 1;
          }
        }
      }
      prevClockTime = current_time;
    }
  }
  else {
    displayMode = DISPLAY_MODE_START_UP;
  }
}



void onDraw(unsigned long currentTime) {
  if(!isDisplayTime(currentTime))    // Do not re-draw at every tick
    return;

  if(displayMode == DISPLAY_MODE_START_UP) {
    drawStartUp();
  }
  else if(displayMode == DISPLAY_MODE_CLOCK) {
    if(isClicked == LOW) {    // User input received
      startEmergencyMode();
      setPageChangeTime(0);    // Change mode with no page-delay
      setNextDisplayTime(currentTime, 0);    // Do not wait next re-draw time
    }
    else {
      if(clockStyle == CLOCK_STYLE_SIMPLE_DIGIT) {
        drawClock(CLOCK_STYLE_SIMPLE_DIGIT);
      }
      else if(clockStyle == CLOCK_STYLE_SIMPLE_ANALOG) {
        drawClock(CLOCK_STYLE_SIMPLE_ANALOG);
      }
      else {
        drawClock(CLOCK_STYLE_SIMPLE_MIX);
      }

      if(isPageChangeTime(currentTime)) {  // It's time to go into idle mode
        startIdleMode();
        setPageChangeTime(currentTime);  // Set a short delay
      }
      setNextDisplayTime(currentTime, CLOCK_DISP_INTERVAL);
    }
  }
  else if(displayMode == DISPLAY_MODE_NORMAL_MSG) {
    if(msgCurDisp>6) {
      msgCurDisp = 0;
      startClockMode();
      setPageChangeTime(currentTime);
      setNextDisplayTime(currentTime, 0);
    }
    else {
      drawMessage(msgCurDisp);
      setNextDisplayTime(currentTime, MESSAGE_DISP_INTERVAL);
      msgCurDisp++;
    }
  }
  else if(displayMode == DISPLAY_MODE_IDLE) {
    if(isClicked == LOW) {    // Wake up watch if there's an user input
      startClockMode();
      setPageChangeTime(currentTime);
      setNextDisplayTime(currentTime, 0);
    } 
    else {
      drawIdleClock();
      setNextDisplayTime(currentTime, IDLE_DISP_INTERVAL);
    }
  }
  else {
    startClockMode();    // This means there's an error
  }

  isClicked = HIGH;
}  // End of onDraw()

int countMessage() {
  int count = 0;
  for(int i=0; i<MSG_COUNT_MAX; i++) {
    if(msgBuffer[i] != 0x00)
      count++;
  }
  return count;
}

// Draw normal message page
void drawMessage(int toggle) {
  // picture loop  
  u8g.firstPage();  
  do {
    u8g_prepare();

    u8g.setFont(u8g_font_courB14);
    u8g.setFontRefHeightExtendedText();
    u8g.setDefaultForegroundColor();
    u8g.setFontPosTop();
    if(toggle%2 == 0) {
      u8g.drawStr( centerX - 59, centerY, msgBuffer);
    } 
    else {
      drawIcon(centerX-49, centerY-24, 1);
      u8g.drawStr(centerX+6, centerY-10, (char*)pgm_read_word(&(msgBox[0])));
      u8g.drawStr(centerX+6, centerY+10, (char*)pgm_read_word(&(msgBox[3])));
    }
  } 
  while( u8g.nextPage() );
}


void startClockMode() {
  displayMode = DISPLAY_MODE_CLOCK;
}

void startEmergencyMode() {
  displayMode = DISPLAY_MODE_EMERGENCY_MSG;
  emgCurDisp = 0;
}

void startMessageMode() {
  displayMode = DISPLAY_MODE_NORMAL_MSG;
  msgCurDisp = 0;
}

void startIdleMode() {
  displayMode = DISPLAY_MODE_IDLE;
}

void drawIndicator() {
  if(updateIndicator) {
    int msgCount = countMessage();
    int drawCount = 1;

    if(msgCount > 0) {
      u8g.drawBitmapP( 127 - 8, 1, 1, 8, IMG_indicator_msg);

      // Show message count
      //      String strTemp = String(msgCount);
      //      char buff[strTemp.length()+1];
      //      for(int i=0; i<strTemp.length()+1; i++) buff[i] = 0x00;
      //      strTemp.toCharArray(buff, strTemp.length());
      //      buff[strTemp.length()] = 0x00;
      //      
      //      u8g.setFont(u8g_font_fixed_v0);
      //      u8g.setFontRefHeightExtendedText();
      //      u8g.setDefaultForegroundColor();
      //      u8g.setFontPosTop();
      //      u8g.drawStr(127 - 15, 1, buff);

      drawCount++;
    }
  }
}

boolean isDisplayTime(unsigned long currentTime) {
  if(currentTime - prevDisplayTime > next_display_interval) {
    return true;
  }
  if(isClicked == LOW) {
    delay(500);
    return true;
  }
  return false;
}

void setNextDisplayTime(unsigned long currentTime, unsigned long nextUpdateTime) {
  next_display_interval = nextUpdateTime;
  prevDisplayTime = currentTime;
}

// Decide if it's the time to change page(mode)
boolean isPageChangeTime(unsigned long currentTime) {
  if(displayMode == DISPLAY_MODE_CLOCK) {
    if(currentTime - mode_change_timer > CLOCK_DISPLAY_TIME)
      return true;
  }
  return false;
}

void setPageChangeTime(unsigned long currentTime) {
  mode_change_timer = currentTime;
}

void drawClock(byte type) {

  clockStyle = type;
  // picture loop  
  u8g.firstPage();  
  do {
    u8g_prepare();
    // draw indicator
    if(updateIndicator)
      drawIndicator();
    // draw clock
    // CLOCK_STYLE_SIMPLE_DIGIT
    if(clockStyle == CLOCK_STYLE_SIMPLE_DIGIT) {
      // Show text
      u8g.setFont(u8g_font_courB14);
      u8g.setFontRefHeightExtendedText();
      u8g.setDefaultForegroundColor();
      u8g.setFontPosTop();
      u8g.drawStr(centerX - 34, centerY - 17, (const char*)pgm_read_word(&(weekString[iWeek])));
      u8g.drawStr(centerX + 11, centerY - 17, (const char*)pgm_read_word(&(ampmString[iAmPm])));

      String strDate = String("");
      char buff[6];
      if(iHour < 10)
        strDate += "0";
      strDate += iHour;
      strDate += ":";
      if(iMinutes < 10)
        strDate += "0";
      strDate += iMinutes;
      strDate.toCharArray(buff, 6);
      buff[5] = 0x00;

      u8g.setFont(u8g_font_courB14);
      u8g.setFontRefHeightExtendedText();
      u8g.setDefaultForegroundColor();
      u8g.setFontPosTop();
      u8g.drawStr(centerX - 29, centerY + 6, buff);
    }
    // CLOCK_STYLE_SIMPLE_MIX
    else if(clockStyle == CLOCK_STYLE_SIMPLE_MIX) {
      u8g.drawCircle(centerY, centerY, iRadius - 6);
      showTimePin(centerY, centerY, 0.1, 0.4, iHour*5 + (int)(iMinutes*5/60));
      showTimePin(centerY, centerY, 0.1, 0.70, iMinutes);
      showTimePin(centerY, centerY, 0.1, 0.70, iSecond);

      // Show text
      u8g.setFont(u8g_font_fixed_v0);
      u8g.setFontRefHeightExtendedText();
      u8g.setDefaultForegroundColor();
      u8g.setFontPosTop();
      u8g.drawStr(centerX + 3, 23, (const char*)pgm_read_word(&(weekString[iWeek])));
      u8g.drawStr(centerX + 28, 23, (const char*)pgm_read_word(&(ampmString[iAmPm])));

      String strDate = String("");
      char buff[6];
      if(iHour > 12)
        iHour -= 12;
      if(iHour < 10)
        strDate += "0";
      strDate += iHour;
      strDate += ":";
      if(iMinutes < 10)
        strDate += "0";
      strDate += iMinutes;
      strDate.toCharArray(buff, 6);
      buff[5] = 0x00;

      u8g.setFont(u8g_font_courB14);
      u8g.setFontRefHeightExtendedText();
      u8g.setDefaultForegroundColor();
      u8g.setFontPosTop();
      u8g.drawStr(centerX, 37, buff);
    }
    else {
      // CLOCK_STYLE_SIMPLE_ANALOG.
      u8g.drawCircle(centerX, centerY, iRadius);
      showTimePin(centerX, centerY, 0.1, 0.5, iHour*5 + (int)(iMinutes*5/60));
      showTimePin(centerX, centerY, 0.1, 0.78, iMinutes);
      showTimePin(centerX, centerY, 0.1, 0.80, iSecond);

    }
  } 
  while( u8g.nextPage() );
}


void drawIdleClock() {
  // picture loop  
  u8g.firstPage();  
  do {
    u8g_prepare();
    // draw indicator
    if(updateIndicator)
      drawIndicator();
    // show time
    String strDate = String("");
    char buff[6];
    if(iHour < 10)
      strDate += "0";
    strDate += iHour;
    strDate += ":";
    if(iMinutes < 10)
      strDate += "0";
    strDate += iMinutes;
    strDate.toCharArray(buff, 6);
    buff[5] = 0x00;

    u8g.setFont(u8g_font_courB14);
    u8g.setFontRefHeightExtendedText();
    u8g.setDefaultForegroundColor();
    u8g.setFontPosTop();
    u8g.drawStr(centerX - 29, centerY - 4, buff);
  } 
  while( u8g.nextPage() );

}

// Calculate clock pin position
double RAD=3.141592/180;
double LR = 89.99;
void showTimePin(int center_x, int center_y, double pl1, double pl2, double pl3) {
  double x1, x2, y1, y2;
  x1 = center_x + (iRadius * pl1) * cos((6 * pl3 + LR) * RAD);
  y1 = center_y + (iRadius * pl1) * sin((6 * pl3 + LR) * RAD);
  x2 = center_x + (iRadius * pl2) * cos((6 * pl3 - LR) * RAD);
  y2 = center_y + (iRadius * pl2) * sin((6 * pl3 - LR) * RAD);

  u8g.drawLine((int)x1, (int)y1, (int)x2, (int)y2);
}

void drawIcon(int posx, int posy, int icon_num) {
  if(icon_num < 0 || icon_num >= ICON_ARRAY_SIZE)
    return;

  u8g.drawBitmapP( posx, posy, 6, 48, (const unsigned char*)pgm_read_word(&(bitmap_array[icon_num])));
}













