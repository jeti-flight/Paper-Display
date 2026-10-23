#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold9pt7b.h>

// ESP32 Pinbelegung
#define EPD_CS    26
#define EPD_DC    25
#define EPD_RST   33
#define EPD_BUSY  27

#define EPD_CLK   18
#define EPD_DIN   23

// Waveshare 1.54 Zoll E-Paper
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display( GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));



void setup()
{
  Serial.begin(115200);
  delay(100);

  Serial.println("Starte E-Paper...");

  // Display initialisieren
  display.init(115200);

  // Hochformat
  display.setRotation(0);

  // Gesamten Bildschirm aktualisieren
  display.setFullWindow();

  display.firstPage();

  do
  {
    display.fillScreen(GxEPD_WHITE); // Löscht das Rauschen im Puffer
    
    display.setFont(&FreeSansBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(20, 150);
    display.print("Test erfolgreich!"); // Dieser Befehl MUSS hier drin stehen!
    
  } while (display.nextPage()); // Erst hier wird der Puffer an die Hardware gesendet


  Serial.println("Display aktualisiert!");
}

void loop()
{
  delay(1000); // Warte 1 Sekunde
  // Keine wiederholte Aktualisierung
}