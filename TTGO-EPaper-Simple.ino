// include library, include base class, make path known
#include <GxEPD.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <GxGDE0213B72B/GxGDE0213B72B.h>      // 2.13" b/w
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBoldOblique9pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoOblique9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBoldOblique9pt7b.h>
#include <Fonts/FreeSansOblique9pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/FreeSerifBold9pt7b.h>
#include <Fonts/FreeSerifBoldItalic9pt7b.h>
#include <Fonts/FreeSerifItalic9pt7b.h>

//#define DEFALUT_FONT  FreeMono9pt7b
// #define DEFALUT_FONT  FreeMonoBoldOblique9pt7b
// #define DEFALUT_FONT FreeMonoBold9pt7b
// #define DEFALUT_FONT FreeMonoOblique9pt7b
#define DEFALUT_FONT FreeSans9pt7b
// #define DEFALUT_FONT FreeSansBold9pt7b
// #define DEFALUT_FONT FreeSansBoldOblique9pt7b
// #define DEFALUT_FONT FreeSansOblique9pt7b
// #define DEFALUT_FONT FreeSerif9pt7b
// #define DEFALUT_FONT FreeSerifBold9pt7b
// #define DEFALUT_FONT FreeSerifBoldItalic9pt7b
// #define DEFALUT_FONT FreeSerifItalic9pt7b

const GFXfont *fonts[] = {
  &FreeMono9pt7b,
  &FreeMonoBoldOblique9pt7b,
  &FreeMonoBold9pt7b,
  &FreeMonoOblique9pt7b,
  &FreeSans9pt7b,
  &FreeSansBold9pt7b,
  &FreeSansBoldOblique9pt7b,
  &FreeSansOblique9pt7b,
  &FreeSerif9pt7b,
  &FreeSerifBold9pt7b,
  &FreeSerifBoldItalic9pt7b,
  &FreeSerifItalic9pt7b
};
#include <HTTPClient.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>

#include "SD.h"
#include "SPI.h"
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include "esp_wifi.h"
#include "Esp.h"
#include "board_def.h"
#include <Button2.h>

#define FILESYSTEM SPIFFS

//#define USE_AP_MODE

/*100 * 100 bmp fromat*/
//https://www.onlineconverter.com/jpg-to-bmp
#define BADGE_CONFIG_FILE_NAME "/badge.data"
#define DEFALUT_AVATAR_BMP "/avatar.bmp"
#define DEFALUT_QR_CODE_BMP "/qr.bmp"
#define WIFI_SSID "Tesla"
#define WIFI_PASSWORD "edison123"
#define CHANNEL_0 0
#define IP5306_ADDR 0X75
#define IP5306_REG_SYS_CTL0 0x00

typedef struct
{
  char name[32];
  char link[64];
  char tel[64];
  char company[64];
  char email[64];
  char address[128];
} Badge_Info_t;

typedef enum
{
  RIGHT_ALIGNMENT = 0,
  LEFT_ALIGNMENT,
  CENTER_ALIGNMENT,
} Text_alignment;

AsyncWebServer server(80);

GxIO_Class io(SPI, ELINK_SS, ELINK_DC, ELINK_RESET);
GxEPD_Class display(io, ELINK_RESET, ELINK_BUSY);

Badge_Info_t info;
static const uint16_t input_buffer_pixels = 20;       // may affect performance
static const uint16_t max_palette_pixels = 256;       // for depth <= 8
uint8_t mono_palette_buffer[max_palette_pixels / 8];  // palette buffer for depth <= 8 b/w
uint8_t color_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 c/w
uint8_t input_buffer[3 * input_buffer_pixels];        // up to depth 24
const char *path[2] = {DEFALUT_AVATAR_BMP, DEFALUT_QR_CODE_BMP};

Button2 *pBtns = nullptr;
uint8_t g_btns[] = BUTTONS_MAP;
int count;
const char* url = "https://services1.arcgis.com/0MSEUqKaxRlEPj5g/arcgis/rest/services/ncov_cases/FeatureServer/1/query?f=json&where=(Country_Region=%27India%27)&returnGeometry=false&outFields=Country_Region,Confirmed,Recovered,Deaths";
const char* url2 = "https://api.thingspeak.com/apps/thinghttp/send_request?api_key=QDTVRQZGP6DCAFPW";
#define updateDelay 100000
uint32_t lastUpdate = 0;

void button_handle(uint8_t gpio)
{
  switch (gpio)
  {
#if BUTTON_1
    case BUTTON_1:
      {
        // esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_1, LOW);
        esp_sleep_enable_ext1_wakeup(((uint64_t)(((uint64_t)1) << BUTTON_1)), ESP_EXT1_WAKEUP_ALL_LOW);
        Serial.println("Going to sleep now");
        delay(2000);
        esp_deep_sleep_start();
      }
      break;
#endif

#if BUTTON_2
    case BUTTON_2:
      {
        static int i = 0;
        Serial.printf("Show Num: %d font\n", i);
        i = ((i + 1) >= sizeof(fonts) / sizeof(fonts[0])) ? 0 : i + 1;
        display.setFont(fonts[i]);
        customData();
        //showMianPage();
      }
      break;
#endif

#if BUTTON_3
    case BUTTON_3:
      {
        static bool index = 1;
        if (!index)
        {
          customData();
          //showMianPage();
          index = true;
        }
        else
        {
          //showQrPage();
          index = false;
        }
      }
      break;
#endif
    default:
      break;
  }
}

void button_callback(Button2 &b)
{
  for (int i = 0; i < sizeof(g_btns) / sizeof(g_btns[0]); ++i)
  {
    if (pBtns[i] == b)
    {
      Serial.printf("btn: %u press\n", pBtns[i].getAttachPin());
      button_handle(pBtns[i].getAttachPin());
    }
  }
}

void button_init()
{
  uint8_t args = sizeof(g_btns) / sizeof(g_btns[0]);
  pBtns = new Button2[args];
  for (int i = 0; i < args; ++i)
  {
    pBtns[i] = Button2(g_btns[i]);
    pBtns[i].setPressedHandler(button_callback);
  }
}

void button_loop()
{
  for (int i = 0; i < sizeof(g_btns) / sizeof(g_btns[0]); ++i)
  {
    pBtns[i].loop();
  }
}

void displayText(const String &str, int16_t y, uint8_t alignment)
{
  int16_t x = 0;
  int16_t x1, y1;
  uint16_t w, h;
  display.setCursor(x, y);
  display.getTextBounds(str, x, y, &x1, &y1, &w, &h);

  switch (alignment)
  {
    case RIGHT_ALIGNMENT:
      display.setCursor(display.width() - w - x1, y);
      break;
    case LEFT_ALIGNMENT:
      display.setCursor(0, y);
      break;
    case CENTER_ALIGNMENT:
      display.setCursor(display.width() / 2 - ((w + x1) / 2), y);
      break;
    default:
      break;
  }
  display.println(str);
}


void WebServerStart(void)
{

#ifdef USE_AP_MODE
  uint8_t mac[6];
  char apName[18] = {0};
  IPAddress apIP = IPAddress(192, 168, 1, 1);

  WiFi.mode(WIFI_AP);

  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  esp_wifi_get_mac(WIFI_IF_STA, mac);

  sprintf(apName, "TTGO-Badge-%02X:%02X", mac[4], mac[5]);

  if (!WiFi.softAP(apName, NULL, 1, 0, 1))
  {
    Serial.println("AP Config failed.");
    return;
  }
  else
  {
    Serial.println("AP Config Success.");
    Serial.print("AP MAC: ");
    Serial.println(WiFi.softAPmacAddress());
  }
#else
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.print(".");
    esp_restart();
  }
  Serial.println(F("WiFi connected"));
  Serial.println("");
  Serial.println(WiFi.localIP());
#endif
}

void showMianPage(void)
{
  displayInit();
  display.fillScreen(GxEPD_WHITE);
  displayText(String(info.name), 30, RIGHT_ALIGNMENT);
  displayText(String(info.company), 50, RIGHT_ALIGNMENT);
  displayText(String(info.email), 70, RIGHT_ALIGNMENT);
  displayText(String(info.link), 90, RIGHT_ALIGNMENT);
  display.update();
}

void customData(void)
{
  displayInit();
  display.fillScreen(GxEPD_WHITE);
  displayText(String("Name"), 30, RIGHT_ALIGNMENT);
  displayText(String("Company"), 50, RIGHT_ALIGNMENT);
  displayText(String("Email"), 70, RIGHT_ALIGNMENT);
  displayText(String("Link"), 90, RIGHT_ALIGNMENT);
  display.update();
}


void displayInit(void)
{
  static bool isInit = false;
  if (isInit)
  {
    return;
  }
  isInit = true;
  display.init();
  display.setRotation(1);
  display.eraseDisplay();
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&DEFALUT_FONT);
  display.setTextSize(0);
  display.update();
  delay(1000);
}


bool setPowerBoostKeepOn(int en)
{
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  if (en)
    Wire.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  else
    Wire.write(0x35); // 0x37 is default reg value
  return Wire.endTransmission() == 0;
}

void setup()
{
  Serial.begin(115200);
  delay(500);

#ifdef ENABLE_IP5306
  Wire.begin(I2C_SDA, I2C_SCL);
  bool ret = setPowerBoostKeepOn(1);
  Serial.printf("Power KeepUp %s\n", ret ? "PASS" : "FAIL");
#endif

  // It is only necessary to turn on the power amplifier power supply on the T5_V24 board.
#ifdef AMP_POWER_CTRL
  pinMode(AMP_POWER_CTRL, OUTPUT);
  digitalWrite(AMP_POWER_CTRL, HIGH);
#endif

  WebServerStart();
  checkCovidData();
  button_init();
}

void loop()
{
  button_loop();
  if (millis() - lastUpdate > updateDelay) {
    checkCovidData();
    lastUpdate = millis();
  }
}

void checkCovidData() {
  HTTPClient https;
  String data;
  String payload2;

  https.begin(url);
  int httpCode = https.GET();
  if (httpCode > 0) { //Check for the returning code
    String payload = https.getString();
    char charBuf[500];
    payload.toCharArray(charBuf, 500);
    //Serial.println(payload);
    const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(4) + JSON_OBJECT_SIZE(1) + 2 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + 3 * JSON_OBJECT_SIZE(6) + 2 * JSON_OBJECT_SIZE(7) + 690;
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc, payload);
    JsonArray fields = doc["fields"];
    JsonObject features_0_attributes = doc["features"][0]["attributes"];
    long features_0_attributes_Last_Update = features_0_attributes["Last_Update"];
    int features_0_attributes_Confirmed = features_0_attributes["Confirmed"];
    int features_0_attributes_Deaths = features_0_attributes["Deaths"];
    int features_0_attributes_Recovered = features_0_attributes["Recovered"];
    if (count < 3) {
      //Serial.println(features_0_attributes_Confirmed);
      Serial.print("INDIA Confirmed:");
      Serial.print(features_0_attributes_Confirmed);
      //Serial.println(features_0_attributes_Recovered);
      Serial.print(" INDIA Recovered:");
      Serial.print(features_0_attributes_Recovered);
      Serial.print(" INDIA Deaths:");
      Serial.println(features_0_attributes_Deaths);

      String indiaConfirmed = "INDIA Confirmed: " + String(features_0_attributes_Confirmed);
      String indiaRecovered = "INDIA Recovered: " + String(features_0_attributes_Recovered);
      String indiaDeaths = "INDIA Deaths: " + String(features_0_attributes_Deaths);
      String worldInfected = "World Infected: ";
      String worldDeaths = "World Deaths: ";

      //Serial.println(indiaConfirmed);

      https.end();
      delay(200);
      HTTPClient https2;
      https2.begin(url2);
      int httpCode2 = https2.GET();
      if (httpCode2 > 0) { //Check for the returning code
        payload2 = https2.getString();
        Serial.println("TamilNadu Infected: ");
        Serial.print(payload2);
      }
      https2.end();
      String TNInfected = "Tamilnadu Infected: " + String(payload2);

      displayInit();
      display.fillScreen(GxEPD_WHITE);
      displayText(String("COVID19 Live Tracker"), 15, CENTER_ALIGNMENT);
      displayText(indiaConfirmed, 35, LEFT_ALIGNMENT);
      displayText(indiaRecovered, 55, LEFT_ALIGNMENT);
      displayText(indiaDeaths, 75, LEFT_ALIGNMENT);
      displayText(String(TNInfected), 95, LEFT_ALIGNMENT);
      displayText(String("Wash Hands with SOAP!"), 117, CENTER_ALIGNMENT);


      //displayText(worldInfected, 95, LEFT_ALIGNMENT);
      //displayText(worldDeaths, 115, LEFT_ALIGNMENT);
      display.update();
    }
  }

  count++;
}
