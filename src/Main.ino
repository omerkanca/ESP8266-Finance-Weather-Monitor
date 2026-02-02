#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include "secrets.h"

// Kullanım
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
String apiKey = SECRET_API_KEY;
const char* googleScriptUrl = SECRET_SCRIPT_URL;
String city = "Bursa";
String countryCode = "TR";


// --- PIN DEFINITIONS ---
#define TFT_CS    15  // D8
#define TFT_DC    5   // D1
#define TFT_RST   4   // D2 
// NOTE: MOSI -> D7, SCLK -> D5 (Hardware SPI)

#define BUTTON_PIN 0  // D3 Pin (GPIO0)

// Hardware SPI usage
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// --- COLOR PALETTE ---
#define C_BG          0x2124 // Dark Slate Blue
#define C_YELLOW      0xFD20 
#define C_BLUE        0x443A 
#define C_WHITE       0xFFFF
#define C_GREY        0x9CD3 
#define C_DARK_YELLOW 0x6300 
#define C_CARD_BG     0x3186 

// Specific Finance Colors
#define C_USD_GREEN   0x05E0
#define C_GOLD        0xFDC0
#define C_SILVER      0xDEFB
#define C_BIST        0x4A1F
#define C_BTC         0xFA60

// --- VARIABLES ---
int currentScreen = 0; // 0: Weather, 1: Finance
unsigned long lastWeatherUpdate = 0;
unsigned long lastFinanceUpdate = 0;
const unsigned long weatherInterval = 900000; // 15 Minutes
const unsigned long financeInterval = 60000;  // 1 Minute

// Button Debounce
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Finance Data
float price_usd = 0, price_gold = 0, price_silver = 0, price_bist = 0, price_btc = 0;

// Weather Data
struct WeatherData {
  int temp;
  String time;
  String iconType;
  String dayName;
};
WeatherData graphData[5]; 
WeatherData dailyData[5]; 
String currentCity = "";
String currentDesc = "";
int currentTemp = 0, currentHum = 0, currentWind = 0, currentPop = 0;

// --- FUNCTION PROTOTYPES ---
void updateWeatherData();
void updateFinanceData();
void drawWeatherScreen();
void drawFinanceScreen();
void drawFinanceValues();
void drawFinanceCard(int index, String label, String formattedValue, uint16_t accentColor);
String formatCurrency(float value, int precision, String symbol); // New Helper
String getDayName(long timestamp);

void setup() {
  Serial.begin(115200);

  // Button Setup
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Display Initialization
  tft.init(240, 320);
  tft.setRotation(1);
  tft.fillScreen(C_BG);
  
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(C_WHITE);
  tft.setCursor(60, 100);
  tft.println("Connecting WiFi...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  tft.fillScreen(C_BG);
  tft.setCursor(60, 110);
  tft.println("Fetching Data...");

  // Fetch data on startup
  updateWeatherData();
  updateFinanceData();

  // Draw initial screen
  drawWeatherScreen();
}

void loop() {
  // --- BUTTON LOGIC ---
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW) { 
      while(digitalRead(BUTTON_PIN) == LOW) { yield(); } // Wait for release

      currentScreen = !currentScreen; 
      
      if (currentScreen == 0) {
        drawWeatherScreen();
      } else {
        drawFinanceScreen();
      }
    }
  }
  lastButtonState = reading;

  // --- TIMERS ---
  if (millis() - lastWeatherUpdate > weatherInterval) {
    updateWeatherData();
    if (currentScreen == 0) drawWeatherScreen();
    lastWeatherUpdate = millis();
  }

  if (millis() - lastFinanceUpdate > financeInterval) {
    updateFinanceData();
    if (currentScreen == 1) drawFinanceValues();
    lastFinanceUpdate = millis();
  }
}

// ==========================================
//           FINANCE CODE
// ==========================================

// Helper function to add commas and currency symbol
String formatCurrency(float value, int precision, String symbol) {
  if (value <= 0.001) return "----";
  
  String str = String(value, precision);
  
  // Find position of decimal point (or end of string if none)
  int dotIndex = str.indexOf('.');
  if (dotIndex == -1) dotIndex = str.length();

  // Insert commas every 3 digits moving left from the dot
  int start = dotIndex - 3;
  while (start > 0) {
     // Check to ensure we don't put a comma before a negative sign
     if (!(start == 1 && str[0] == '-')) {
        str = str.substring(0, start) + "," + str.substring(start);
        // Shift our tracking index because string grew by 1 char
        dotIndex++; 
     }
     start -= 3;
  }
  
  return str + symbol;
}

void drawFinanceScreen() {
  tft.fillScreen(C_BG);
  
  // Header
  tft.setFont(&FreeSans12pt7b);
  tft.setTextColor(C_WHITE);
  tft.setCursor(20, 35);
  tft.println("Market Data");
  
  tft.drawFastHLine(0, 50, 320, C_GREY);

  // Footer
  tft.setFont();
  tft.setTextColor(C_GREY);
  tft.setCursor(180, 225);
  tft.print("Source: Google Fin");

  drawFinanceValues(); 
}

void drawFinanceValues() {
  if (currentScreen != 1) return;

  // Format values with commas and symbols
  // "TL" is used because standard fonts lack the '₺' glyph
  String s_usd = formatCurrency(price_usd, 2, " TL");
  String s_gold = formatCurrency(price_gold, 0, " TL");
  String s_silver = formatCurrency(price_silver, 2, " TL");
  String s_bist = formatCurrency(price_bist, 0, " TL"); // Index points, no unit
  String s_btc = formatCurrency(price_btc, 0, " $");

  drawFinanceCard(0, "USD/TRY", s_usd, C_USD_GREEN);
  drawFinanceCard(1, "GOLD (Gr)", s_gold, C_GOLD);
  drawFinanceCard(2, "SILVER", s_silver, C_SILVER);
  drawFinanceCard(3, "BIST 100", s_bist, C_BIST);
  drawFinanceCard(4, "BTC (USD)", s_btc, C_BTC);
}

void drawFinanceCard(int index, String label, String formattedValue, uint16_t accentColor) {
  int cardH = 28;
  int cardW = 280;
  int startX = 20;
  int startY = 60 + (index * (cardH + 5)); 

  // 1. Draw Card Background
  tft.fillRoundRect(startX, startY, cardW, cardH, 6, C_CARD_BG);

  // 2. Draw Accent Bar
  tft.fillRoundRect(startX, startY, 6, cardH, 6, accentColor);
  tft.fillRect(startX+3, startY, 3, cardH, accentColor);

  // 3. Draw Label
  tft.setFont(&FreeSans9pt7b);
  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(startX + 15, startY + 20);
  tft.print(label);

  // 4. Draw Formatted Value (Right Aligned)
  tft.setTextColor(accentColor);
  
  // Estimate text width to align right (approx 9-10px per char for this font)
  // FreeSans is variable width, but this estimation works well enough.
  int textWidth = 0;
  for(unsigned int i=0; i<formattedValue.length(); i++) {
     char c = formattedValue[i];
     if(c == '.' || c == ',' || c == ' ') textWidth += 5;
     else textWidth += 10;
  }
  
  int numberX = (startX + cardW) - textWidth - 10;
  tft.setCursor(numberX, startY + 20);
  tft.print(formattedValue);
}

void updateFinanceData() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    if (http.begin(client, googleScriptUrl)) {
      int httpCode = http.GET();
      if (httpCode > 0) {
        String payload = http.getString();
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
          if (doc["usd"].as<float>() > 0) price_usd = doc["usd"];
          if (doc["gold"].as<float>() > 0) price_gold = doc["gold"];
          if (doc["silver"].as<float>() > 0) price_silver = doc["silver"];
          if (doc["bist"].as<float>() > 0) price_bist = doc["bist"];
          if (doc["btc"].as<float>() > 0) price_btc = doc["btc"];
        }
      }
      http.end();
    }
  }
}

// ==========================================
//           WEATHER CODE
// ==========================================

String getDayName(long timestamp) {
  int dayIndex = (((timestamp + 10800) / 86400L) + 4) % 7;
  switch(dayIndex) {
    case 0: return "Sun"; case 1: return "Mon"; case 2: return "Tue";
    case 3: return "Wed"; case 4: return "Thu"; case 5: return "Fri";
    case 6: return "Sat"; default: return "";
  }
}

void drawCloudIcon(int x, int y, int size, uint16_t color) {
  tft.fillCircle(x, y, size, color);
  tft.fillCircle(x + (size*0.8), y - (size*0.6), size*0.9, color);
  tft.fillCircle(x + (size*1.6), y, size*0.8, color);
}

void drawRainIcon(int x, int y, bool small = false) {
  int s = small ? 8 : 10;
  drawCloudIcon(x, y, s, C_GREY);
  tft.fillTriangle(x+s/2, y+s*1.5, x+s/2-3, y+s*1.5+7, x+s/2+3, y+s*1.5+7, C_BLUE);
  tft.fillCircle(x+s/2, y+s*1.5+7, 3, C_BLUE);
}

void drawSunIcon(int x, int y, bool small = false) {
  int s = small ? 10 : 12;
  tft.fillCircle(x+s, y, s, C_YELLOW);
}

void drawWeatherIcon(String type, int x, int y, bool small = false) {
  if(type == "Rain") drawRainIcon(x, y, small);
  else if(type == "Clear") drawSunIcon(x, y, small);
  else drawCloudIcon(x, y, small ? 8 : 10, C_GREY);
}

void drawWeatherScreen() {
  tft.fillScreen(C_BG);

  // --- SECTION 1: HEADER ---
  String mainCond = graphData[0].iconType;
  int iconX = 45; int iconY = 50;
  if(mainCond == "Rain") {
    tft.fillCircle(iconX, iconY, 20, C_GREY);
    tft.fillCircle(iconX+20, iconY, 17, C_GREY);
    tft.fillCircle(iconX+10, iconY-15, 20, C_GREY);
    tft.fillCircle(iconX+10, iconY+25, 5, C_BLUE);
  } else if(mainCond == "Clear") {
     tft.fillCircle(iconX+10, iconY, 28, C_YELLOW);
  } else {
     tft.fillCircle(iconX, iconY, 20, C_GREY);
     tft.fillCircle(iconX+20, iconY, 17, C_GREY);
     tft.fillCircle(iconX+10, iconY-15, 20, C_GREY);
  }

  tft.setFont(&FreeSansBold18pt7b);
  tft.setTextColor(C_WHITE);
  tft.setCursor(95, 65);
  tft.print(currentTemp);

  tft.setFont(&FreeSans12pt7b);
  tft.setCursor(tft.getCursorX() + 2, 50);
  tft.print("o");
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(tft.getCursorX() + 1, 50);
  tft.print("C");

  tft.setFont(); 
  tft.setTextSize(1);
  int rightX = 190;
  tft.setTextColor(C_WHITE);
  tft.setCursor(rightX, 20); tft.print("Weather");
  tft.setTextColor(C_GREY);
  tft.setCursor(rightX, 35); tft.print(city);

  int detailY = 55;
  int valX = rightX + 55;
  tft.setCursor(rightX, detailY);     tft.print("Rain:");
  tft.setCursor(valX, detailY);       tft.print(currentPop); tft.print("%");
  tft.setCursor(rightX, detailY+12);  tft.print("Hum:");
  tft.setCursor(valX, detailY+12);    tft.print(currentHum); tft.print("%");
  tft.setCursor(rightX, detailY+24);  tft.print("Wind:");
  tft.setCursor(valX, detailY+24);    tft.print(currentWind); tft.print(" km/h");

  // --- SECTION 2: GRAPH ---
  int gTop = 115, gBot = 155;
  int xStep = 62, xStart = 35;
  int minT = 100, maxT = -100;
  
  for(int i=0; i<5; i++) { 
    if(graphData[i].temp < minT) minT = graphData[i].temp;
    if(graphData[i].temp > maxT) maxT = graphData[i].temp;
  }
  minT -= 2; maxT += 2;

  tft.drawFastHLine(xStart-10, gBot, (xStep*4)+20, C_GREY);

  for(int i=0; i<4; i++) { 
    int x1 = xStart + (i * xStep);
    int y1 = map(graphData[i].temp, minT, maxT, gBot, gTop);
    int x2 = xStart + ((i+1) * xStep);
    int y2 = map(graphData[i+1].temp, minT, maxT, gBot, gTop);

    tft.drawLine(x1, y1, x2, y2, C_YELLOW);
    tft.drawLine(x1, y1+1, x2, y2+1, C_YELLOW);
    for(int k=x1; k<x2; k+=2) {
      int interpolatedY = map(k, x1, x2, y1, y2);
      tft.drawFastVLine(k, interpolatedY+2, gBot - interpolatedY -1, C_DARK_YELLOW);
    }
    tft.setTextColor(C_WHITE);
    tft.setCursor(x1-3, y1-8); tft.print(graphData[i].temp);
    tft.setTextColor(C_GREY);
    tft.setCursor(x1-8, gBot + 12); tft.print(graphData[i].time);
  }
  
  int lastX = xStart + (4 * xStep);
  int lastY = map(graphData[4].temp, minT, maxT, gBot, gTop);
  tft.setTextColor(C_WHITE);
  tft.setCursor(lastX-3, lastY-8); tft.print(graphData[4].temp);
  tft.setTextColor(C_GREY);
  tft.setCursor(lastX-8, gBot + 12); tft.print(graphData[4].time);

  // --- SECTION 3: DAILY CARDS ---
  int cardY = 185;
  int cardW = 58; int cardH = 50;

  for(int i=0; i<5; i++) {
    int cx = 12 + (i * 62);
    if(i==0) tft.fillRoundRect(cx-4, cardY-5, cardW, cardH, 8, C_CARD_BG);

    tft.setFont(&FreeSans9pt7b);
    tft.setTextColor(C_GREY);
    tft.setCursor(cx+8, cardY+10); tft.print(dailyData[i].dayName);
    drawWeatherIcon(dailyData[i].iconType, cx+10, cardY+25, true);
    
    tft.setFont();
    tft.setTextSize(1);
    tft.setTextColor(C_WHITE);
    tft.setCursor(cx+35, cardY+28);
    tft.print(dailyData[i].temp); tft.print("o");
  }
}

void updateWeatherData() {
  WiFiClient client;
  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/forecast?q=" + city + "," + countryCode + "&cnt=40&units=metric&appid=" + apiKey;

  http.begin(client, url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    DynamicJsonDocument filter(512);
    filter["list"][0]["dt"] = true;
    filter["list"][0]["dt_txt"] = true;
    filter["list"][0]["main"]["temp"] = true;
    filter["list"][0]["main"]["humidity"] = true;
    filter["list"][0]["wind"]["speed"] = true;
    filter["list"][0]["pop"] = true;
    filter["list"][0]["weather"][0]["main"] = true;
    filter["list"][0]["weather"][0]["description"] = true;
    filter["city"]["name"] = true;

    DynamicJsonDocument doc(5120);
    DeserializationError error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));

    if (!error) {
      currentCity = doc["city"]["name"].as<String>();
      currentTemp = doc["list"][0]["main"]["temp"].as<int>();
      currentHum = doc["list"][0]["main"]["humidity"].as<int>();
      float windMs = doc["list"][0]["wind"]["speed"];
      currentWind = windMs * 3.6;
      float popVal = doc["list"][0]["pop"].as<float>(); 
      currentPop = (int)(popVal * 100);
      currentDesc = doc["list"][0]["weather"][0]["description"].as<String>();
      if(currentDesc.length() > 0) currentDesc[0] = toupper(currentDesc[0]);

      for(int i=0; i<5; i++) {
        graphData[i].temp = doc["list"][i]["main"]["temp"].as<int>();
        String rawDate = doc["list"][i]["dt_txt"].as<String>();
        String rawTime = rawDate.substring(11, 16); 
        int hour = rawTime.substring(0, 2).toInt();
        hour = (hour + 3) % 24; 
        String hourStr = (hour < 10) ? "0" + String(hour) : String(hour);
        graphData[i].time = hourStr + ":00";
        graphData[i].iconType = doc["list"][i]["weather"][0]["main"].as<String>();
      }

      int indices[] = {0, 8, 16, 24, 32};
      for(int i=0; i<5; i++) {
        int idx = indices[i];
        if(idx >= doc["list"].size()) break;
        dailyData[i].temp = doc["list"][idx]["main"]["temp"].as<int>();
        long ts = doc["list"][idx]["dt"];
        dailyData[i].dayName = getDayName(ts);
        dailyData[i].iconType = doc["list"][idx]["weather"][0]["main"].as<String>();
      }
    }
  }
  http.end();
}