// Test E-Paper Display with full and partial refresh
#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <ExtendedChars.h>
#include <Fonts/Extended_ClassicFont7pt.h>
#include <Fonts/Extended_FreeSans9pt7b.h>
#include <SD.h>

// ESP32 Pinbelegung
#define EPD_CS    26
#define EPD_DC    25
#define EPD_RST   33
#define EPD_BUSY  27

#define EPD_CLK   18
#define EPD_DIN   23

#define SD_CS    5
#define SD_MISO  19

// Waveshare 1.54 Zoll E-Paper
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display( GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// maximale Breite und Höhe des Displays
int16_t displayWidth = display.width();
int16_t displayHeight = display.height();

// Bereich für teilweises Update
uint16_t partialWindowX;
uint16_t partialWindowY;
uint16_t partialWindowW;
uint16_t partialWindowH;

// Textboxes
int16_t textboxX;
int16_t textboxY;
uint16_t textboxW;
uint16_t textboxH;

//Cursor Position
int16_t cursorX;
int16_t cursorY;

//Singel Line Text für teilweise Update
String singleLine = "";

// Array für 5 Zeilen Text
const int numLines = 5; // Anzahl der Textzeilen
String lines[numLines] = {"", "", "", "", ""}; // Array für 5 Zeilen Text als String-Objekte
int currentLine = 0; // Aktuelle Zeile für das teilweise Update

// Zähler für teilweises Update
static int counter = 0;

void setup()
{
  Serial.begin(115200);
  delay(100);

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  // SD-Karte initialisieren
  if (!SD.begin(SD_CS)) {
    Serial.println("SD-Karte konnte nicht initialisiert werden!");
  } else {
    Serial.println("SD-Karte erfolgreich initialisiert.");
  }

  // read partition table and mount SD card
  if (!SD.exists("/")) {
    Serial.println("Partitionstabelle konnte nicht gelesen werden oder SD-Karte ist leer!");
  } else {
    Serial.println("Partitionstabelle erfolgreich gelesen und SD-Karte gemountet.");
  }
  // Warte kurz, damit die serielle Ausgabe vollständig angezeigt wird
  delay(100);
  // list files on SD card
  File root = SD.open("/");
  if (root) {
    File file = root.openNextFile();
    while (file) {
      Serial.print("Found file: ");
      Serial.println(file.name());
      file = root.openNextFile();
    }
  }



  Serial.println("Starte E-Paper...");

  // E-Paper Display initialisieren
  display.init(115200);

  // Hochformat
  display.setRotation(0);

  // Gesamten Bildschirm aktualisieren
  display.setFullWindow();

  display.firstPage();

  do
  {
    display.fillScreen(GxEPD_WHITE); // Löscht den gesamten Bildschirm im Puffer
   
    // Textbox Größe abfragen, gibt die Ecke links oben und Breite und Höhe zurück
    display.getTextBounds("E-Paper Test", 0, 0, &textboxX, &textboxY, &textboxW, &textboxH);

    // Textbox direkt unterhalb des Rahmens von 5 Pixeln setzen
    // wir brauchen hier links unten wegen externem Font-Offset, daher wird cursorY auf textboxY + textboxH + 2 gesetzt
    cursorX = textboxX + 5;
    cursorY = displayHeight - textboxH - 5;
    display.setCursor(cursorX, cursorY);
    display.setFont(&FreeSansBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.print("E-Paper Test"); // Dieser Befehl MUSS hier drin stehen!
    // rahmen 3 pixel ganz aussen
    display.drawRect(0, 0, displayWidth - 0, displayHeight - 0, GxEPD_BLACK); // erster Rahmen 0 Pixel innen
    display.drawRect(1, 1, displayWidth - 2, displayHeight - 2, GxEPD_BLACK); // zweiter Rahmen 1 Pixel innen
    display.drawRect(2, 2, displayWidth - 4, displayHeight - 4, GxEPD_BLACK); // dritter Rahmen 2 Pixel innen
  } while (display.nextPage()); // Erst hier wird der Puffer an die Hardware gesendet

  Serial.println("Display aktualisiert!");
  delay(1000); // Warte 10 Sekunden vor dem teilweisen Update
}

void loop()
{
  // teilweiser refresh
  // counter für teilweises Update, hier könnte man einen Zähler einfügen, um unterschiedliche Inhalte anzuzeigen
  counter++;
  // text definieren um die größen zu haben
  display.setTextColor(GxEPD_BLACK);
  display.fillScreen(GxEPD_WHITE);
  display.setFont(&FreeSansBold9pt7b);
  //get textbox für Update Count, Position egal, braucht nur die Größe, daher 0,0 als Referenzpunkt
  singleLine = "Update count: " + String(counter);
  display.getTextBounds(singleLine, 0, 0, &textboxX, &textboxY, &textboxW, &textboxH);

  // nur Höhe ist textabhängig, Breite kann fest sein, Unterlänge macht Unterschied zwischen textbox und cursor position
  partialWindowX = 20;
  partialWindowY = 10;
  partialWindowW = 160;
  partialWindowH = textboxH + 5;
  display.setPartialWindow(partialWindowX, partialWindowY, partialWindowW, partialWindowH); // Bereich für teilweises Update
  display.firstPage();
  do
  {
    cursorX = partialWindowX;
    cursorY = partialWindowY + textboxH;
    display.setCursor(cursorX, cursorY);
    display.print(singleLine);
  } while (display.nextPage());

  // partilaWindowY neu setzen
  partialWindowY = partialWindowY + partialWindowH; // neues Window unter dem alten
  // font wechseln
  display.setFont(&Extended_ClassicFont7pt);
  // neue textgröße bestimmen
  display.getTextBounds(singleLine, 0, 0, &textboxX, &textboxY, &textboxW, &textboxH);
  // neues partial update Window für numLines, um die bisherigen Updates anzuzeigen
  // höhe muss für numLines Zeilen ausreichen plus Raum für Unterlänge
  partialWindowH = textboxH * numLines + 5; // zusätzlicher Raum für Unterlänge
  display.setPartialWindow(partialWindowX, partialWindowY, partialWindowW, partialWindowH); // Bereich für teilweises Update
  display.firstPage();
  do
  {

    // add the update to the next line in the array
    if (currentLine < numLines)
    {
      lines[currentLine] = singleLine;
      currentLine++;
    } else {
      // schiebe alle eins nach oben, um Platz für die neue Zeile zu schaffen
      for (int i = 1; i < numLines; i++)
      {
        lines[i - 1] = lines[i];
      }
      lines[numLines - 1] = singleLine;
    }
    // display alle Zeilen im Array
    cursorX = partialWindowX;
    cursorY = partialWindowY + textboxH;
    for (int i = 0; i < numLines; i++)
    {
      // serial debug output
      Serial.print("Displaying line ");
      Serial.print(i);
      Serial.print(" at position ");
      Serial.print(partialWindowX);
      Serial.print(",");
      Serial.print(cursorY + (textboxH * i));
      Serial.print(": ");
      Serial.println(lines[i]);
      display.setCursor(partialWindowX, cursorY + (textboxH * i));
      display.print(lines[i]);
    }
  } while (display.nextPage());


  delay(1000); // Warte 1 Sekunde vor dem nächsten Update
}