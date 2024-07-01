#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
#include <WiFi.h>
#include <OneButton.h>


#include "pin_config.h"
#include "time.h"


#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 170
#define PIN_INPUT_BUTTON 14


TaskHandle_t drawGuiUpdate;


TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h
TFT_eSprite sprite = TFT_eSprite(&tft);

const char* ssid = "WIFI SSID";
const char* password = "WIFI PASSWORD";
const char* ntpServer = "time.google.com";
const long gmtOffset = 3600;
const int daylightOffset = 0;
const int wifiTimeout = 10000;

const int fontSize = 15;

// Splash screen config
String productName = "ServerMon";
String productVer = "v0.1";
String productType = "For Dell R740xd";

// Graph data
const int cpuUsagePoints = 100;
int cpuUsage[cpuUsagePoints] = {0, };

// Metrics gui config
const int cpu1TempPosX = 6;
const int cpu1TempPosY = 43;
const int cpu2TempPosX = 6;
const int cpu2TempPosY = 58;
const int cpuUsePosX = 6;
const int cpuUsePosY = 73;
const int statusPosX = 160;
const int statusPosY = 43;
const int memoryPosX = 160;
const int memoryPosY = 58;
const int loadAvgPosX = 160;
const int loadAvgPosY = 73;
const int metricsValueSpacing = 104;

// Disk gui config
const int diskTextSize = 8;
const int diskPosX = 0;
const int diskPosY = 110;
const int totalDiskCount = 12;
const int diskRow = 3;
const int cellSpacing = 2;
const int cellWidth = 40;
const int cellHeight = 20;
// 1 = ReadIO, 2 = WriteIO
int diskUsageRW = 1;

// Cpu graph gui config
const int graphPosX = 160;
const int graphPosY = 110;
const int graphHeight = 60;
const int graphWidth = 160;
const int legendSize = 22;

// Status modelbox config
const int mbPosX = 220;
const int mbPosY = 4;
const String mbName = "Dell R740xd";

char attributeMarker = ':';
char valueMarker = ',';
char valueSpace = '|';
String buff = "";
String property = "";
String value = "";

int ntpUpdateTimer = 0;
int graphUpdateTimer = 0;

int overallStatus = -1;
float cpuPercentage = -1;
float loadAvg15 = -1;
float memPercentage = -1;
String diskUsage = "";
String cpuTemp = "";
typedef struct cpuTemp_ {
  int cpuNum = 0;
  float temperature = -1;
} cpuTemp_st;
typedef struct diskUsage_ {
  int diskRead = -1;
  int diskWrite = -1;
  int diskStatus = -1;
} diskUsage_st;

// Button config
OneButton button(PIN_INPUT_BUTTON, true);


void drawGraphDots(int offsetX, int offsetY, int width, int height) {
  // Draw graph dots
  int dotPerRow = 5;
  int dotPerCol = 3;
  int dotSpacingX = width / (dotPerRow + 1);
  int dotSpacingY = height / (dotPerCol + 1);
  int posX = offsetX + (dotSpacingX / 2);
  int posY = offsetY + dotSpacingY;
  for (int i=0; i<dotPerCol; i++) {
    for (int j=0; j<dotPerRow; j++) {
      sprite.drawPixel(posX, posY, TFT_YELLOW);
      posX = posX + dotSpacingX;
    }
    posX = offsetX + (dotSpacingX / 2);
    posY = posY + dotSpacingY;
  }

  sprite.pushSprite(0, 0);
}


void drawAndSaveCpuGraph(int data, int offsetX, int offsetY, int width, int height, bool flush) {
  offsetX = offsetX + 1 + legendSize;
  offsetY = offsetY - 1;

  if(flush) {
    sprite.fillRect(offsetX, offsetY - height, width, height, TFT_BLACK);
    // Redraw dots
    drawGraphDots(offsetX, offsetY - height, width, height);
  }

  width = width - legendSize;
  
  // Plot and save datapoints
  for (int i=0; i<cpuUsagePoints; i++) {
    if ((i + 1) >= cpuUsagePoints) {
      cpuUsage[i] = 0;
    }
    else {
      cpuUsage[i] = cpuUsage[i+1];
    }
  }
  if (data > 100) {
    cpuUsage[cpuUsagePoints - 1] = 100;
  }
  else if (data < 0) {
    cpuUsage[cpuUsagePoints - 1] = 0;
  }
  else {
    cpuUsage[cpuUsagePoints - 1] = data;
  }

  float interval = width / (float) cpuUsagePoints;
  float prevPosX, nextPosX;
  int prevPosY, nextPosY;
  prevPosX = offsetX;
  prevPosY = offsetY - cpuUsage[0];
  for(int j=0; j<cpuUsagePoints; j++) {
    nextPosX = prevPosX + interval;
    nextPosY = map(cpuUsage[j], 0, 100, offsetY - 1, offsetY - height + 1);
    sprite.drawLine(round(prevPosX), prevPosY, round(nextPosX), nextPosY, TFT_GREEN);
    prevPosX = nextPosX;
    prevPosY = nextPosY;
  }

  sprite.pushSprite(0, 0);
}


void drawDiskGrid(int offsetX, int offsetY) {
  // Draw grid title
  if (diskUsageRW == 1) {
    sprite.drawString("Disk Read IOPS    ", offsetX, offsetY - fontSize);
  }
  else if (diskUsageRW == 2) {
    sprite.drawString("Disk Write IOPS    ", offsetX, offsetY - fontSize);
  }

  // Draw disk shelf grid
  sprite.setCursor(offsetX, offsetY);
  for(int i=0; i<diskRow; i++) {
    for(int j=0; j<(totalDiskCount / diskRow); j++) {
      int posX = (cellWidth * j) + offsetX;
      int posY = (cellHeight * i) + offsetY;
      sprite.drawRect(posX, posY, cellWidth, cellHeight, TFT_DARKGREY);
    }
  }
}

void drawGraph(int offsetX, int offsetY) {
  // Change init pos to graph
  int startX = offsetX;
  int startY = SCREEN_HEIGHT - offsetY;

  // Draw graph title
  sprite.drawString("CPU Usage(%) - 1 Hour", offsetX, offsetY - fontSize);

  // Draw graph
  sprite.setTextDatum(MR_DATUM);
  int labelSpacing = graphHeight / 12;
  sprite.drawString("100", offsetX + 20, offsetY + labelSpacing);
  sprite.drawString("50", offsetX + 20, offsetY + (labelSpacing * 6));
  sprite.drawString("0", offsetX + 20, offsetY + (labelSpacing * 11));
  sprite.setTextDatum(ML_DATUM);
  sprite.drawLine(offsetX + legendSize, offsetY, offsetX + legendSize, offsetY + graphHeight, TFT_DARKGREY);
  sprite.drawLine(offsetX + legendSize, offsetY + graphHeight - 1, offsetX + legendSize + graphWidth, offsetY + graphHeight - 1, TFT_DARKGREY);

  // // Draw dots
  // drawGraphDots(offsetX + legendSize, offsetY, graphWidth, graphHeight);
  // // Draw actual graph
  // drawCpuGraph(0, offsetX + legendSize, offsetY + graphHeight, graphWidth, graphHeight, false);
}


void drawDiskMetrics(diskUsage_st *output, int offsetX, int offsetY) {
  int startX = offsetX + (cellWidth / 7);
  int startY = SCREEN_HEIGHT - (diskRow * cellHeight) - offsetY + (cellHeight / 2);
  unsigned int color;

  for(int i=0; i<diskRow; i++) {
    for(int j=0; j<(totalDiskCount / diskRow); j++) {
      int posX = (cellWidth * j) + startX;
      int posY = (cellHeight * i) + startY;

      sprite.drawString("   ", posX, posY);
      if (diskUsageRW == 1) {
        sprite.drawString(String(output[(i + (diskRow * j))].diskRead), posX, posY);
      }
      else if (diskUsageRW == 2) {
        sprite.drawString(String(output[(i + (diskRow * j))].diskWrite), posX, posY);
      }

      posX = (cellWidth * j) + cellWidth - 5;
      posY = (cellHeight * i) + (SCREEN_HEIGHT - (diskRow * cellHeight) - offsetY) + 1;
      int status = output[i + (diskRow * j)].diskStatus;
      if (status == -1) {
        color = TFT_ORANGE;
      }
      else if (status == 1) {
        color = TFT_GREEN;
      }
      else if (status == 2) {
        color = TFT_RED;
      }
      else {
        color = TFT_CYAN;
      }
      sprite.fillRect(posX, posY, 4, 4, color);
    }
  }

  sprite.pushSprite(0, 0);
}


void drawMetricText(int type, float data, int offsetX, int offsetY) {
  int indicatorPosX = offsetX + 33;
  int textLength;
  int fontWidth = 6;
  unsigned int color;
  String textBuff = "NO DATA";

  sprite.setTextColor(TFT_RED,TFT_BLACK);
  if (data == -1.0) {
    sprite.fillRect(offsetX - 5, offsetY - (fontSize / 2), textBuff.length() * fontWidth + 10, fontSize - 2, TFT_BLACK);
    sprite.drawString(textBuff, offsetX, offsetY);
    sprite.setTextColor(TFT_WHITE,TFT_BLACK);
    return;
  }
  
  sprite.setTextColor(TFT_WHITE,TFT_BLACK);
  sprite.drawString("        ", offsetX, offsetY);
  
  textBuff = String(data, 1);
  textBuff += "   ";
  sprite.drawString(textBuff, offsetX, offsetY);

  switch (type)
  {
    // Type = CPU Temperature
  case 1:
    if (data <= 45) {
      color = TFT_GREEN;
    }
    if (data > 45) {
      color = TFT_YELLOW;
    }
    if (data > 55) {
      color = TFT_ORANGE;
    }
    if (data > 60) {
      color = TFT_RED;
    }
    sprite.fillCircle(indicatorPosX, offsetY, 3, color);
    break;
  
  case 2:
    if (data <= 70) {
      color = TFT_GREEN;
    }
    if (data > 70) {
      color = TFT_YELLOW;
    }
    if (data > 80) {
      color = TFT_ORANGE;
    }
    if (data > 90) {
      color = TFT_RED;
    }
    sprite.fillCircle(indicatorPosX, offsetY, 3, color);
    break;

  case 3:
    if (data <= 70) {
      color = TFT_GREEN;
    }
    if (data > 70) {
      color = TFT_YELLOW;
    }
    if (data > 80) {
      color = TFT_ORANGE;
    }
    if (data > 90) {
      color = TFT_RED;
    }
    sprite.fillCircle(indicatorPosX, offsetY, 3, color);
    break;

  case 4:
    if (data <= 12) {
      color = TFT_GREEN;
    }
    if (data > 14) {
      color = TFT_YELLOW;
    }
    if (data > 17) {
      color = TFT_ORANGE;
    }
    if (data > 20) {
      color = TFT_RED;
    }
    sprite.fillCircle(indicatorPosX, offsetY, 3, color);
    break;

  case 5:
    if (data == 200) {
      textBuff = "  OK  ";
      color = TFT_GREEN;
    }
    else if (data == 400) {
      textBuff = " FAULT ";
      color = TFT_RED;
    }
    else {
      textBuff = "           ";
      color = TFT_PURPLE;
    }
    sprite.fillRect(offsetX - 5, offsetY - (fontSize / 2), textBuff.length() * fontWidth + 10, fontSize - 2, TFT_BLACK);
    sprite.drawString(textBuff, offsetX, offsetY);
    sprite.drawRect(offsetX - 5, offsetY - (fontSize / 2), textBuff.length() * fontWidth + 10, fontSize - 2, color);
    break;

  default:
    break;
  }
}


void drawNtpTime() {
  char textBuff[9];
  struct tm timeinfo;

  char hour[3];
  char minute[3];
  char second[3];

  if(getLocalTime(& timeinfo)) {
    strftime(hour, sizeof(hour), "%H", & timeinfo);
    strftime(minute, sizeof(minute), "%M", & timeinfo);
    strftime(second, sizeof(second), "%S", & timeinfo);
  }

  sprintf(textBuff, "%s:%s:%s", String(hour), String(minute), String(second));
  sprite.drawString(textBuff, 120, 6);
}


void drawInitScreen() {
  String textBuff = " ";

  tft.setTextSize(fontSize);

  // Config current time
  configTzTime("KST-9", ntpServer);

  // Draw NTP TIme
  textBuff = "Time(Asia/Seoul) :";
  sprite.drawString(textBuff, 6, 6);

  // Draw modelbox
  sprite.drawString(mbName, mbPosX + 8, mbPosY + 10);
  sprite.drawRoundRect(mbPosX, mbPosY, 80, 20, 4, TFT_DARKGREY);

  // Draw current IP
  textBuff = "ESP32 IP(WiFi) : ";
  char ipChar[16];
  IPAddress ip = WiFi.localIP();
  sprintf(ipChar, "%u.%u.%u.%u", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
  String ipStr(ipChar); //convert char array to string here.
  sprite.drawString(textBuff + ipStr, 6, 21);

  // Draw ping status
  // textBuff = "Ping : ";
  // sprite.drawString(textBuff, 100, 30);

  // Draw current CPU Temperature
  textBuff = "CPU1 Temp(C) : ";
  sprite.drawString(textBuff, cpu1TempPosX, cpu1TempPosY);
  drawMetricText(1, -1, cpu1TempPosX + metricsValueSpacing, cpu1TempPosY);
  textBuff = "CPU2 Temp(C) : ";
  sprite.drawString(textBuff, cpu2TempPosX, cpu2TempPosY);
  drawMetricText(1, -1, cpu2TempPosX + metricsValueSpacing, cpu2TempPosY);

  // Draw total CPU/RAM usage in percentage
  textBuff = "CPU Usage(%) : ";
  sprite.drawString(textBuff, cpuUsePosX, cpuUsePosY);
  drawMetricText(2, -1, cpuUsePosX + metricsValueSpacing, cpuUsePosY);
  textBuff = "RAM Usage(%) : ";
  sprite.drawString(textBuff, memoryPosX, memoryPosY);
  drawMetricText(3, -1, memoryPosX + metricsValueSpacing, memoryPosY);

  // Draw Load Average
  textBuff = "Load Avg 15m : ";
  sprite.drawString(textBuff, loadAvgPosX, loadAvgPosY);
  drawMetricText(4, -1, loadAvgPosX + metricsValueSpacing, loadAvgPosY);

  // Draw Overall Status
  textBuff = "Overall Status : ";
  sprite.drawString(textBuff, statusPosX, statusPosY);
  drawMetricText(5, -1, statusPosX + metricsValueSpacing, statusPosY);

  drawDiskGrid(diskPosX, diskPosY);
  drawGraph(graphPosX, graphPosY);
  // drawDiskStatusIndicator(0, 0);

  sprite.pushSprite(0, 0);
}


void parseCpuTemp(String data, cpuTemp_st *output) {
  int r=0,t=0;
    
  for(int i=0;i<data.length();i++)
  {
    if(data[i] == '|')
    {
      if (i-r > 1)
      {
        output[t].cpuNum = t;
        output[t].temperature = data.substring(r,i).toFloat();
        t++;
      }
      r = (i+1);
    }
  }
}


void parseDiskUsage(String data, diskUsage_st *output) {
  String temp[totalDiskCount];

  int idx = 0;
  int startIdx = 0;
  for(int i=0; i<data.length(); i++) {
    if(data[i] == '|') {
      temp[idx] = data.substring(startIdx, i);
      startIdx = i + 1;
      idx++;
    }
  }

  idx = 0;
  startIdx = 0;
  int mType = 0;
  for(int i=0; i<totalDiskCount; i++) {
    for(int j=0; j<temp[i].length(); j++) {
      if(temp[i][j] == '-') {
        if(mType == 0) {
          idx = temp[i].substring(startIdx, j).toInt();
        }
        else if(mType == 1) {
          output[idx].diskRead = temp[i].substring(startIdx, j).toInt();
        }
        else if(mType == 2) {
          output[idx].diskWrite = temp[i].substring(startIdx, j).toInt();
        }
        else if(mType == 3) {
          output[idx].diskStatus = temp[i].substring(startIdx, j).toInt();
        }
        else {
          
        }
        mType++;
        startIdx = j + 1;
      }
    }
    mType = 0;
    startIdx = 0;
    idx = 0;
  }
}


void drawGuiUpdateTask(void * pvParameters) {
  for(;;) {
    diskUsage_st diskUsageParsed[12];
    parseDiskUsage(diskUsage, diskUsageParsed);

    drawDiskMetrics(diskUsageParsed, 0, 0);
    
    randomSeed(analogRead(2));
    
    if (millis() > graphUpdateTimer + 10000) {
      // Draw actual graph
      drawAndSaveCpuGraph(cpuPercentage, graphPosX, graphPosY + graphHeight, graphWidth, graphHeight, true);
      graphUpdateTimer = millis();
    }

    cpuTemp_st cpuTempParsed[2];
    parseCpuTemp(cpuTemp, cpuTempParsed);

    drawMetricText(1, cpuTempParsed[0].temperature, cpu1TempPosX + metricsValueSpacing, cpu1TempPosY);
    drawMetricText(1, cpuTempParsed[1].temperature, cpu2TempPosX + metricsValueSpacing, cpu2TempPosY);
    drawMetricText(2, cpuPercentage, cpuUsePosX + metricsValueSpacing, cpuUsePosY);
    drawMetricText(3, memPercentage, memoryPosX + metricsValueSpacing, memoryPosY);
    drawMetricText(4, loadAvg15, loadAvgPosX + metricsValueSpacing, loadAvgPosY);
    drawMetricText(5, overallStatus, statusPosX + metricsValueSpacing, statusPosY);

    delay(50);
  }
}


void changeDiskIoStat() {
  if (diskUsageRW == 1) {
    diskUsageRW = 2;
  }
  else {
    diskUsageRW = 1;
  }
  drawDiskGrid(diskPosX, diskPosY);
}


void setup(void) {
  int timeoutCounter = 0;
  bool isWifiConnnected = false;
  // put your setup code here, to run once:
  Serial.begin(115200);

  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  tft.init();
  tft.setRotation(3);
  tft.setSwapBytes(true);

  sprite.createSprite(320, 170);
  sprite.setTextColor(TFT_WHITE,TFT_BLACK);
  sprite.setTextDatum(ML_DATUM);

  // Splash screen
  // sprite.fillScreen(TFT_BLACK);
  // sprite.setTextDatum(MC_DATUM);
  // sprite.drawString(productName + "  " + productVer, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 3);
  // sprite.drawString(productType, SCREEN_WIDTH / 2, (SCREEN_HEIGHT / 3) + 20);
  // sprite.pushSprite(0, 0);
  // delay(2000);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");
    delay(500);
    timeoutCounter += 500;
    if (timeoutCounter > wifiTimeout) {
      Serial.println("WiFi timeout reached");
      break;
    }
    isWifiConnnected = true;
  }
  if (isWifiConnnected) {
    Serial.println("WiFi Connected");
  }
  else {
    Serial.println("WiFi Failed");
  }

  // tft.fillScreen(TFT_BLACK);
  // sprite.setTextDatum(ML_DATUM);
  // sprite.pushSprite(0, 0);
  // delay(500);

  button.attachClick(changeDiskIoStat);

  drawInitScreen();
  // drawPVGraph("Test");

  xTaskCreatePinnedToCore(
    drawGuiUpdateTask,
    "drawGuiUpdate",
    10000,
    NULL,
    1,
    &drawGuiUpdate,
    0
  );

  Serial.println("Initialization Done");
}

void loop() {
  // put your main code here, to run repeatedly:
  
  /* Expected input format from Serial */
  /*
    Format(comma seperated) : cpu:<value>,memory:<value>,network:<value>,disk:<value>
    Value description : 
      cpu -> float number within the range of 0.0 ~ 100.0
      memory -> 
      network ->
      disk ->
  */

  if (millis() > ntpUpdateTimer + 1000) {
    drawNtpTime();
    ntpUpdateTimer = millis();
  }

  button.tick();

  while (Serial.available()) {
    char input = Serial.read();

    if (input == ':') {
      property = buff;
      buff = "";
    }
    else if (input == valueMarker || input == '\n') {
      value = buff;
      buff = "";
      
      Serial.println("=========================");
      Serial.println("property: " + property + ", value: " + value);
      Serial.println("=========================");

      if (property == "CPU") {
        cpuPercentage = value.toFloat();
      }
      else if (property == "LAVG") {
        loadAvg15 = value.toFloat();
      }
      else if (property == "MEM") {
        memPercentage = value.toFloat();
      }
      else if (property == "DISKIO") {
        diskUsage = value;
      }
      else if (property == "CPUTEMP") {
        cpuTemp = value;
      }
      else if (property == "OVERALL") {
        overallStatus = value.toInt();
      }
      else {

      }
    } 
    else {
      buff += input;
    }
  }
}

// TFT Pin check
#if PIN_LCD_WR  != TFT_WR || \
    PIN_LCD_RD  != TFT_RD || \
    PIN_LCD_CS    != TFT_CS   || \
    PIN_LCD_DC    != TFT_DC   || \
    PIN_LCD_RES   != TFT_RST  || \
    PIN_LCD_D0   != TFT_D0  || \
    PIN_LCD_D1   != TFT_D1  || \
    PIN_LCD_D2   != TFT_D2  || \
    PIN_LCD_D3   != TFT_D3  || \
    PIN_LCD_D4   != TFT_D4  || \
    PIN_LCD_D5   != TFT_D5  || \
    PIN_LCD_D6   != TFT_D6  || \
    PIN_LCD_D7   != TFT_D7  || \
    PIN_LCD_BL   != TFT_BL  || \
    TFT_BACKLIGHT_ON   != HIGH  || \
    170   != TFT_WIDTH  || \
    320   != TFT_HEIGHT
#error  "Error! Please make sure <User_Setups/Setup206_LilyGo_T_Display_S3.h> is selected in <TFT_eSPI/User_Setup_Select.h>"
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
#error  "The current version is not supported for the time being, please use a version below Arduino ESP32 3.0"
#endif