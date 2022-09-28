//-------------- Olimex ESP32-EVB ------------------------


//-------------- MOD-LCD2.8RTP Screen configuration ------------------------
#include "Wire.h"
#include "Adafruit_STMPE610.h"

#include <Arduino_GFX_Library.h>

#define TFT_DC 15
#define TFT_CS 17
#define TFT_MOSI 2
#define TFT_CLK 14
#define TFT_MISO 0
#define TFT_RST 0

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_CLK, TFT_MOSI, TFT_MISO);
Arduino_GFX *gfx = new Arduino_ILI9341(bus, TFT_RST, 0 /* rotation */);
/*******************************************************************************
   End of Arduino_GFX setting
 ******************************************************************************/


//-------------- Touch screen ------------------------
// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 290
#define TS_MINY 285
#define TS_MAXX 7520
#define TS_MAXY 7510
#define TS_I2C_ADDRESS 0x4d

Adafruit_STMPE610 ts = Adafruit_STMPE610();

//-------------- Relay pins ------------------------
#define RELAY1  32
#define RELAY2  33

//-------------- LittleFS ------------------------

#define FS_NO_GLOBALS //allow LittleFS to coexist with SD card, define BEFORE including FS.h
#include "FS.h"
#include "LittleFS.h"
#include "SDMMC_func.h"   //auxiliary file with all necessary functions for files"

//-------------- PNG images ------------------------
#include <PNGdec.h>
PNG png;
fs::File pngFile;
int16_t w, h, xOffset, yOffset;
#include "PNG_func.h"   //auxiliary file with all necessary functions for PNG images"

//-------------- Task handle ------------------------
TaskHandle_t Task1;

// semafore info https://github.com/RalphBacon/ESP32-Variable-Passing-Using-Queues-Global-Vars/blob/master/ESP32_GLOBAL_VARS.ino
SemaphoreHandle_t Semaphore;

//-------------- Global variables ------------------------
unsigned long TimePressed, TimeNow, TimeLastChangedON, TimeLastChangedOFF, TimeStart;
int ButtonPressed = 0;
volatile bool Relay1ON = false;
int RelayCase = 0;
int TimeON = 10;
int TimeOFF = 10;
const int TimeMax = 60;
const int TimeMin = 4;

void setup()
{
  Serial.begin(115200);
  // while (!Serial);

  // ----------- Initialize TFT screen --------------------------
  // Screen can only start after closing SDMMC, above
  gfx->begin();
  gfx->fillScreen(WHITE);
  PrintCharTFT("Prof. Ruben Mercade Prieto", 40, 140, BLACK, WHITE, 1);
  PrintCharTFT("Source code available at: ", 5, 220, BLACK, WHITE, 1);
  PrintCharTFT("https://github.com/RubenMercadePrieto/", 5, 230, BLACK, WHITE, 1);
  PrintCharTFT("/Electromagnet_DPP_ESP32", 15, 240, BLACK, WHITE, 1);

  // ----------- Display TimeMin and TimeMax --------------------------
  PrintCharTFT("Smallest time allowed (s): " + String(TimeMin), 20, 20, BLACK, WHITE, 1);
  PrintCharTFT("Largest time allowed (s): " + String(TimeMax), 20, 30, BLACK, WHITE, 1);

  // ----------- Setup and main loop running in Core 1 --------------------------
  Serial.print("setup() running on core ");
  Serial.println(xPortGetCoreID());

  // Simple flag, up or down
  Semaphore = xSemaphoreCreateMutex();

  // ----------- Create a Task for Core 0 --------------------------
  xTaskCreatePinnedToCore(
    Relaycode, /* Function to implement the task */
    "Task1", /* Name of the task */
    10000,  /* Stack size in words */
    NULL,  /* Task input parameter */
    0,  /* Priority of the task */
    &Task1,  /* Task handle. */
    0); /* Core where the task should run */
  delay(500);  // needed to start-up task1

  //-------------- Initialize touch screen ------------------------
  Wire.begin();
  pinMode(TFT_DC, OUTPUT);
  // read diagnostics (optional but can help debug problems)
  //uint8_t x = gfx->readcommand8(ILI9341_RDMODE);
  delay(1000);
  // Touch screen start
  ts.begin(TS_I2C_ADDRESS);
  PrintCharTFT("Touch screen initiated", 20, 70, BLACK, WHITE, 1);

  //-------------- Define Relay pins as outputs ------------------------
  pinMode(RELAY1, OUTPUT);
  //  pinMode(RELAY2, OUTPUT);


  TimePressed = millis();


  // ----------- Initialize LittleFS --------------------------
  Serial.println("Testing Little FS");
  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
    return;
  }
  // ----------- Example Functions LittleFS --------------------------
  Serial.println("----list Dir LittleFS 1----");
  listDir(LittleFS, "/", 1);
  Serial.println( "LittleFS Test complete" );

  PrintCharTFT("Done testing files", 20, 80, BLACK, WHITE, 1);
  PrintCharTFT("Testing PNG images", 20, 90, BLACK, WHITE, 1);


  DrawPNG("/IQS60_40.png", 90, 160);
  PrintCharTFT("DONE!", 100, 260, BLACK, WHITE, 2);
  delay(5000);
  gfx->fillScreen(BLACK);
}

void loop()
{
  // ----------- Setup and main loop running in Core 1 --------------------------
  Serial.print("Loop() running on core ");
  Serial.println(xPortGetCoreID());

  TimeNow = millis();
  // Clear Screen
  gfx->fillScreen(BLACK);
  //Print IQS School of Engineering Logo on top of the screen
  DrawPNG("/IQSSE240_82.png", 0, 0);
  PrintCharTFT("Prof. Ruben Mercade Prieto", 80, 5, BLACK, WHITE, 1);
  PrintCharTFT("Electromagnet EL-D-100-40", 80, 15, BLACK, WHITE, 1);
  PrintCharTFT("ON-OFF Control with Relay", 80, 25, BLACK, WHITE, 1);
  PrintCharTFT("Microcontroller ESP32-EVB", 80, 35, BLACK, WHITE, 1);

  PrintCharTFT("Time Relay ON", 30, 90, WHITE, BLACK, 2);


  gfx->fillRoundRect(0, 110, 50, 50, 8, BLUE);
  gfx->fillTriangle(10, 120, 40, 120, 25, 150, WHITE);
  gfx->drawRoundRect(0, 110, 50, 50, 8, WHITE);

  PrintCharTFT(String(TimeON), 70, 120, WHITE, BLACK, 4);
  PrintCharTFT("s", 140, 120, WHITE, BLACK, 4);

  gfx->fillRoundRect(190, 110, 50, 50, 8, BLUE);
  gfx->fillTriangle(215, 120, 230, 150, 200, 150, WHITE);
  gfx->drawRoundRect(190, 110, 50, 50, 8, WHITE);

  PrintCharTFT("Time Relay OFF", 30, 167, WHITE, BLACK, 2);

  gfx->fillRoundRect(0, 190, 50, 50, 8, BLUE);
  gfx->fillTriangle(10, 200, 40, 200, 25, 230, WHITE);
  gfx->drawRoundRect(0, 190, 50, 50, 8, WHITE);

  PrintCharTFT(String(TimeOFF), 70, 200, WHITE, BLACK, 4);
  PrintCharTFT("s", 140, 200, WHITE, BLACK, 4);

  gfx->fillRoundRect(190, 190, 50, 50, 8, BLUE);
  gfx->fillTriangle(215, 200, 230, 230, 200, 230, WHITE);
  gfx->drawRoundRect(190, 190, 50, 50, 8, WHITE);


  // The appearance for the Relay buttons depends if the relays are activated or not
  xSemaphoreTake(Semaphore, portMAX_DELAY);
  if (Relay1ON == false) {
    xSemaphoreGive(Semaphore);
    gfx->fillRoundRect(0, 250, 120, 40, 8, RED);
    PrintCharTFT("START", 10, 260, WHITE, RED, 3);
    gfx->drawRoundRect(0, 250, 120, 40, 8, WHITE);
  }
  else if (Relay1ON == true) {
    xSemaphoreGive(Semaphore);
    PrintCharTFT("STOP", 20, 260, RED, BLACK, 3);
    gfx->drawRoundRect(0, 250, 120, 40, 8, RED);
  }

  //print the state of the relay
  PrintCharTFT("Relay State:", 160, 250, WHITE, BLACK, 1);
  if (digitalRead(RELAY1)) {
    PrintCharTFT("ON", 160, 265, GREEN, BLACK, 3);
  }
  else {
    PrintCharTFT("OFF", 160, 265, RED, BLACK, 3);
  }

  ButtonPressed = Get_Button();
  // Program will only continue to this point when the screen is touched. If not touched, it will stay within the function Get_button()
  // Define what to do if any buttons is pressed

  if (ButtonPressed == 1) {
    // Use semaphore to block access to variable Relay1ON
    xSemaphoreTake(Semaphore, portMAX_DELAY);
    if (Relay1ON == false) {
      Relay1ON = true;
      RelayCase = 1;
    }
    else {
      Relay1ON = false;
      RelayCase = 0;
      //      digitalWrite(RELAY1, LOW);  //force relay to be off, regardless of other core
    }
    Serial.println("Relay1ON :" + String(Relay1ON));
    xSemaphoreGive(Semaphore);
  }
  delay(10); // leave some time for other stuff?

}



// Function to control the touch screen and the buttons
int Get_Button(void) {
  int result = 0;   //value to be returned by the function

  TS_Point p;
  while (1) { //important while. meanwhile screen not touched, it will loop here indefinetelly
    delay(50);

    p = ts.getPoint();    //get data from the touch screen
    TimeNow = millis();

    if (TimeNow > (TimePressed + 250)) { //provide a time delay to check next touch, otherwise fake double activated button
      if (p.z < 10 || p.z > 140) {     // =0 should be sufficient for no touch. sometimes it give a signal p.z 255 when not touching
        //    Serial.println("No touch");
      }
      else if (p.z != 129) {    //for a first touch, p.z = 128. A value of 129 is for continuous touching
        // scale the touch screen coordinates for the TFT screen resolution
        p.x = map(p.x, TS_MINX, TS_MAXX, 0, gfx->width());
        p.y = map(p.y, TS_MINY, TS_MAXY, 0, gfx->height());
        p.y = 320 - p.y;

        //        gfx->fillCircle(p.x, p.y, 5, YELLOW);  //show in the screen where you touched

        // If list to identify the location of all the buttons
        if ((p.y > 250) && (p.y < 290) && (p.x > 20) && (p.x < 120)) {
          result = 1;     //Relay 1 - Start/Stop button
        }

        //Arrows for Time ON
        else if ((p.y > 110) && (p.y < 160) && (p.x > 0) && (p.x < 50)) {
          result = 10;    //DOWN
          TimeON = TimeON - 2;
          if (TimeON < TimeMin) {
            TimeON = TimeMin;
          }
          PrintCharTFT(String(TimeON), 70, 120, WHITE, BLACK, 4);
        }
        else if ((p.y > 110) && (p.y < 160) && (p.x > 190) && (p.x < 240)) {
          result = 11;    //UP
          TimeON = TimeON + 2;
          if (TimeON > TimeMax) {
            TimeON = TimeMax;
          }
          PrintCharTFT(String(TimeON), 70, 120, WHITE, BLACK, 4);
        }
        //Arrows for Time OFF
        else if ((p.y > 190) && (p.y < 240) && (p.x > 0) && (p.x < 50)) {
          result = 12;    //DOWN
          TimeOFF = TimeOFF - 2;
          if (TimeOFF < TimeMin) {
            TimeOFF = TimeMin;
          }
          PrintCharTFT(String(TimeOFF), 70, 200, WHITE, BLACK, 4);
        }
        else if ((p.y > 190) && (p.y < 240) && (p.x > 190) && (p.x < 240)) {
          result = 13;    //UP
          TimeOFF = TimeOFF + 2;
          if (TimeOFF > TimeMax) {
            TimeOFF = TimeMax;
          }
          PrintCharTFT(String(TimeOFF), 70, 200, WHITE, BLACK, 4);
        }

        else {
          result = 0;  //nothing happens
        }
        if (result != 0) {
          TimePressed = millis();
          //          Serial.println(TimePressed);
        }
        Serial.print("X = "); Serial.print(p.x);
        Serial.print("\tY = "); Serial.print(p.y);
        Serial.print("\tPressure = "); Serial.print(p.z);
        Serial.print("\tButton = "); Serial.println(result);
        if (result != 0 && result < 10) {
          return result;  //return which button has been pressed
        }
      }
    }
  }
}

//Relaycode: turns the relay 1 on and off at specified times in core 0
void Relaycode( void * pvParameters ) {
  Serial.print("Relaycode running on core ");
  Serial.println(xPortGetCoreID());

  for (;;) {
    //Make that only this task can read Relay1ON
    xSemaphoreTake(Semaphore, portMAX_DELAY);
    if (Relay1ON == true) {
      xSemaphoreGive(Semaphore);

      switch (RelayCase) {
        case 0:
          break;  //nothing happens
        case 1:
          TimeStart = millis();
          digitalWrite(RELAY1, HIGH);
          Serial.print("ON!");
          PrintCharTFT("ON", 160, 265, GREEN, BLACK, 3);
          RelayCase = 2;
          break;
        case 2:
          if (millis() - TimeStart > TimeON * 1000) { //finished time with relay on
            TimeLastChangedON = millis() - TimeStart;
            PrintCharTFT("Last real time ON (ms): " + String(TimeLastChangedON), 30, 300, WHITE, BLACK, 1);
            RelayCase = 3;
            Serial.println("... end ON!");
          }
          break;
        case 3:
          digitalWrite(RELAY1, LOW);
          PrintCharTFT("OFF", 160, 265, RED, BLACK, 3);
          RelayCase = 4;
          Serial.print("OFF!");
          break;
        case 4:
          if (millis() - TimeStart - TimeLastChangedON > TimeOFF * 1000) {
            Serial.println("... end OFF!");
            TimeLastChangedOFF = millis() - TimeStart - TimeLastChangedON;
            PrintCharTFT("Last real time OFF (ms): " + String(TimeLastChangedOFF), 30, 310, WHITE, BLACK, 1);
            RelayCase = 1;
          }
          break;
      }
    }
    else {
      xSemaphoreGive(Semaphore);
      digitalWrite(RELAY1, LOW);
      RelayCase = 0;
    }
    delay(10);
  }
}
