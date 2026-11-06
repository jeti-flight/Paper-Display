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
#include <FS.h>
#include <vector>

#define DEBUG_SETUP
#define DEBUG_INIT_DISPLAY
#define DEBUG_SD

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

// Array für 20 Zeilen Text maximium
// 18 zeilen wenn 1 px mehr abstand zwischen den zeilen erforderlich ist
String lines[20] = {"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""}; // Array für 20 Zeilen Text als String-Objekte
int numLines = 20; // Anzahl der Textzeilen, wird neu berechnet
int currentLine = 0; // Aktuelle Zeile für das teilweise Update
int lineSpace = 1; // zusätzlicher Abstand zwischen den Zeilen

// on error go into deep sleep without wakeup
void onErrorDeepSleep() {
  display.powerOff();
  esp_deep_sleep_start();
} 

// simple Zeichenroutinen ohne Display on/off
// Rechteck zeichnen mit koordinierten Eckpunkten und Breite/Höhe, optional für Rahmen oder Textboxen
void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t thickness, uint16_t color)
{
  for (int i = 0; i < thickness; i++) {
    display.drawRect(x + i, y + i, w - 2 * i, h - 2 * i, color);
  }
}

// Line zeichnen mit Start- und Endkoordinaten, optional für dicke Linien
void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t thickness, uint16_t color)
{
  for (int i = 0; i < thickness; i++) {
    display.drawLine(x0, y0 + i, x1, y1 + i, color);
  }
}

// clear all lines in the text array
void clearLines() {
  for (int i = 0; i < numLines; i++) {
    lines[i] = "";
  }
  currentLine = 0;
}

// write a line into the array at position pos 
void writeLine(int pos, String text) {
  if (pos >= 0 && pos < numLines) {
    lines[pos] = text;
  }
}

// append a line to the array, automatically advancing the current line and rotate the array 
void appendLine(String text) {
  if (currentLine >= numLines) {
    // rotate the array
    for (int i = 1; i < numLines; i++) {
      lines[i - 1] = lines[i];
    }
    currentLine = numLines - 1;
  }
  writeLine(currentLine, text);
  currentLine++;
}

// display all text lines in the partial window update area
// check if line is to long and cut it to fit the textbox width
void displayLines() {
  // power on the display before starting the partial window update without loosing data
  display.init(115200,false);
  display.setFont(&Extended_ClassicFont7pt);
  display.setTextColor(GxEPD_BLACK);
  display.setPartialWindow(partialWindowX, partialWindowY, partialWindowW, partialWindowH);
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    int y = partialWindowY + textboxH + lineSpace;
    for (int i = 0; i < numLines; i++) {
      if (lines[i] != "") {
        display.setCursor(partialWindowX, y);
        String lineToDisplay = lines[i];
        // check the exact length of the line and truncate if necessary to fit the textbox width
        display.getTextBounds(lineToDisplay, 0, 0, &textboxX, &textboxY, &textboxW, &textboxH);
        if (textboxW > partialWindowW) {
          // calculate the maximum number of characters that can fit test line within the textbox width
          for (int j = lineToDisplay.length(); j > 0; j--) {
            String testLine = lineToDisplay.substring(0, j);
            display.getTextBounds(testLine, 0, 0, &textboxX, &textboxY, &textboxW, &textboxH);
            if (textboxW <= partialWindowW) {
              // remove two more charakter and add ellipsis if needed
              if (j > 3) {
                lineToDisplay = testLine.substring(0, j - 3) + "...";
              } else {
                lineToDisplay = testLine;
              }
              break;
            }
          }
        }
        display.print(lineToDisplay);
        y += textboxH + lineSpace; // move to the next line
      }
    }
  } while (display.nextPage());
  // power off the display after updating the partial window to save power
  display.powerOff();
}
// Funktion zur Initialisierung des Displays und den Start-Screen
void initDisplay() {
  #ifdef DEBUG_INIT_DISPLAY
    Serial.println("Starte E-Paper...");
  #endif
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
    // rahmen 3 pixel ganz aussen
    drawRect(0, 0, displayWidth, displayHeight, 3, GxEPD_BLACK);
    // Textbox Größe abfragen, gibt die Ecke links oben und Breite und Höhe zurück
    display.setFont(&FreeSansBold9pt7b);
    display.getTextBounds("GPS-Tracker Test", 0, 0, &textboxX, &textboxY, &textboxW, &textboxH);
    // separator line unterhalb der Rahmen Abstang Textbox Höhe plus 10 Pixel, 3px breit
    drawLine(0, textboxH + 10, displayWidth, textboxH + 10, 3, GxEPD_BLACK);
    // set partial window size based on the boxes, one pixel inside the outer frame
    partialWindowX = 8;
    partialWindowY = textboxH + 16;
    partialWindowW = displayWidth - partialWindowX - 8;
    partialWindowH = displayHeight - partialWindowY - 8; 
    // Text in der Textbox zentrieren
    cursorX = (displayWidth - 6 - textboxW) / 2;
    // text über den separator line positionieren, daher cursorY auf textboxH + 5 setzen
    cursorY = textboxH + 5; // 5 Pixel Abstand von der oberen Linie des Textbereichs  
    display.setCursor(cursorX, cursorY);
    display.setTextColor(GxEPD_BLACK);
    display.print("GPS Tracker Test"); // Dieser Befehl MUSS hier drin stehen!
  } while (display.nextPage()); // Erst hier wird der Puffer an die Hardware gesendet

  // im loop nur ein kleiner Font verwenden, um die Updates anzuzeigen
  display.setFont(&Extended_ClassicFont7pt);
  display.setTextColor(GxEPD_BLACK);
  singleLine = "TestString";
  // Texthöhe ermitteln und die maximale Anzahl der Zeilen im partiellen Fenster berechnen
  display.getTextBounds(singleLine, 0, 0, &textboxX, &textboxY, &textboxW, &textboxH);
  numLines = round((partialWindowH + lineSpace) / (textboxH + lineSpace)); // Berechne die maximale Anzahl der Zeilen, die in das partielle Fenster passen

  #ifdef DEBUG_INIT_DISPLAY
    Serial.printf("Display initialisiert!\n");
    Serial.printf("Display width: %d, Display height: %d\n", displayWidth, displayHeight);
    Serial.printf("Partial window set to X: %d, Y: %d, W: %d, H: %d\n", partialWindowX, partialWindowY, partialWindowW, partialWindowH);
    Serial.printf("Initial text bounds for '%s': X: %d, Y: %d, W: %d, H: %d\n", singleLine.c_str(), textboxX, textboxY, textboxW, textboxH);
    Serial.printf("Calculated number of lines: %d\n", numLines);
  #endif
  // power off after each initialization
  display.powerOff();
}

// Zähler für teilweises Update
static int counter = 0;

// Filesystem related variables and functions
File root;
struct FileInfo {
  String pfad;
  uint64_t groese; // Größe in Bytes
  bool isDirectory; // Flag to indicate if the file is a directory
};
std::vector<FileInfo> fileInfoList;

// List all files in the root directory of the SD card and store them in the fileList vector
// makr directories as directories in the file list
// Funktion liest die SD-Karte aus und füllt das Array

void listFilesInternal(fs::FS &fs, const char * dirname) {
  File root = fs.open(dirname);
  if (!root || !root.isDirectory()) {
    Serial.println("Fehler beim Öffnen des Verzeichnisses!");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    
    // Ignoriere Mac-Systemeinträge, die mit "." beginnen
    String name = String(file.name());
    if (name.startsWith(".")) {
      file = root.openNextFile();
      continue; 
    }
    FileInfo info;
    info.pfad = String(file.path());
    info.isDirectory = file.isDirectory();

    if (info.isDirectory) {
      // Speichert den vollständigen Pfad (z.B. "/messung.csv") und die Größe im Array
      info.groese = 0;
      fileInfoList.push_back(info);
      listFilesInternal(fs, file.path());
    } else {
      info.pfad = String(file.path());
      info.groese = file.size();
      fileInfoList.push_back(info);
    }
    file = root.openNextFile();
  }
}

void listFiles(fs::FS &fs, const char * dirname) {
  fileInfoList.clear(); // Löscht die Liste GENAU EINMAL ganz am Anfang
  listFilesInternal(fs, dirname); // Startet das eigentliche Einlesen
  //sortieren pfade oben alphabetisch dann files nach name
  std::sort(fileInfoList.begin(), fileInfoList.end(), [](const FileInfo &a, const FileInfo &b) {
    if (a.isDirectory != b.isDirectory) {
      return a.isDirectory; // Directories come first
    }
    return a.pfad < b.pfad;
  });
}

bool initSDCard() {
  // SD-Karte initialisieren und Dateien auflisten
  // all name capital letters for consistency on the SD card
  clearLines();
  appendLine("Initialisiere SD-Karte...");
  displayLines();
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  // SD-Karte initialisieren
  if (!SD.begin(SD_CS)) {
    Serial.println("SD-Karte konnte nicht initialisiert werden!");
    appendLine("SD-Karte konnte nicht initialisiert werden!");
  } else {
    Serial.println("SD-Karte erfolgreich initialisiert.");
    appendLine("SD-Karte erfolgreich initialisiert.");
  }
  displayLines();

  // read partition table and mount SD card
  if (!SD.exists("/")) {
    Serial.println("Partitionstabelle konnte nicht gelesen werden oder SD-Karte ist leer!");
    appendLine("Partitionstabelle konnte nicht gelesen werden oder SD-Karte ist leer!");
  } else {
    Serial.println("Partitionstabelle erfolgreich gelesen und SD-Karte gemountet.");
    appendLine("Partitionstabelle erfolgreich gelesen und SD-Karte gemountet.");
  }
  displayLines();
  // warte kurz, damit der Benutzer die Statusmeldungen auf dem Display sehen kann
  delay(2000); // 2 Sekunden warten
  return SD.exists("/"); // Return true if SD card is successfully initialized and mounted
}

// list files in / (root directory) and display them on the serial monitor and e-paper display
void listRootFiles() {
  listFiles(SD, "/");
  for (size_t i = 0; i < fileInfoList.size(); i++) {
    if (fileInfoList[i].isDirectory) {
      Serial.printf("(Dir) %s\n", fileInfoList[i].pfad.c_str());
      appendLine("(Dir) " + String(fileInfoList[i].pfad));
    } else {
      Serial.printf("(File) %s\n", fileInfoList[i].pfad.c_str());
      appendLine("(File) " + String(fileInfoList[i].pfad));
      appendLine(String("-->Size: ") + String(fileInfoList[i].groese) + " bytes");
    }
  }

  if (fileInfoList.empty()) {
    Serial.println("No files found on SD card.");
    appendLine("Keine Dateien auf der SD-Karte gefunden.");
  }
  displayLines(); // Zeige die gefundenen Dateien auf dem Display an
}



// Setup function for initializing serial communication, SD card, and E-Paper display
void setup()
{
  #ifdef DEBUG_SETUP
    Serial.begin(115200);
    delay(100);
    Serial.println("Serielle Schnittstelle initialisiert.");
    Serial.println("Setup gestartet...");
  #endif

  // init display
  initDisplay(); // Initialize the e-paper display

  // init SD card, no output on success, display error on failure done in initSDCard()
  // just stop working here if SD card initialization fails
  if (!initSDCard()) {
    appendLine(" "); // Leerzeile vor der Fehlermeldung einfügen
    appendLine("Fehler beim Initialisieren der SD-Karte.");
    appendLine("Bitte überprüfen Sie die SD-Karte und starten Sie das Gerät neu.");
    displayLines(); // Zeige die Fehlermeldung auf dem Display an
    //hold the program here indefinitely if SD card initialization fails
    onErrorDeepSleep(); // Gehe in den Tiefschlaf bei Fehlern beim Initialisieren der SD-Karte
  }

  // test if directory gpstracks exists on the SD card
  // if not try to create it
  if (SD.exists("/GPSTRK")) {
    appendLine(" "); // Leerzeile vor der Meldung einfügen
    appendLine("Verzeichnis /GPSTRK existiert");
    displayLines();
  } else {
    if (!SD.mkdir("/GPSTRK")) {
      appendLine(" "); // Leerzeile vor der Fehlermeldung einfügen
      appendLine("Fehler: Verzeichnis /GPSTRK konnte nicht erstellt werden.");
      appendLine("Bitte überprüfen Sie die SD-Karte und starten Sie das Gerät neu.");
      displayLines();
      onErrorDeepSleep(); // Gehe in den Tiefschlaf bei Fehlern beim Erstellen des Verzeichnisses /gpstracks
    }
  }

  //read the next trackfile number from file, 0 if not existing
  int lastTrackFileNumber = 0;
  if (SD.exists("/GPSTRK/TRKNUM.TXT")) {
    File numberFile = SD.open("/GPSTRK/TRKNUM.TXT", FILE_READ);
    if (numberFile) {
      lastTrackFileNumber = numberFile.parseInt();
      numberFile.close();
    }
  }
  #ifdef DEBUG_SD
    Serial.println("Last track file number: " + String(lastTrackFileNumber));
  #endif

  //find the next possible trackname, we will have folders 100TRACK, 101TRACK, with max 9999 tracks in each folder with the name TR and the next free number will be determined accordingly
  // list all existing track folders in /gpstracks and find the one with the highest number
  // make sure the 100TRACK exists, as it is the starting point for track folders
  if (!SD.exists("/GPSTRK/100TRACK")) {
    if (!SD.mkdir("/GPSTRK/100TRACK")) {
      appendLine(" "); // Leerzeile vor der Fehlermeldung einfügen
      appendLine("Fehler: Verzeichnis /GPSTRK/100TRACK konnte nicht erstellt werden.");
      appendLine("Bitte überprüfen Sie die SD-Karte und starten Sie das Gerät neu.");
      displayLines();
      onErrorDeepSleep(); // Gehe in den Tiefschlaf bei Fehlern beim Erstellen des Verzeichnisses /gpstracks/100TRACK
    }
  }

  int highestTrackFolder = 100;
  File gpstracksDir = SD.open("/GPSTRK");
  if (gpstracksDir) {
    File entry;
    while (entry = gpstracksDir.openNextFile()) {
      if (entry.isDirectory()) {
        String folderName = entry.name();
        if (folderName.endsWith("TRACK")) {
          int folderNumber = folderName.substring(0, folderName.indexOf("TRACK")).toInt();
          if (folderNumber > highestTrackFolder) {
            highestTrackFolder = folderNumber;
          }
        }
      }
    }
    gpstracksDir.close();
  }

  String highestTrackFolderName = "/GPSTRK/" + String(highestTrackFolder) + "TRACK";

  #ifdef DEBUG_SD
    Serial.println("highestTrackFolder: " + String(highestTrackFolder) + "TRACK");
  #endif
  appendLine(" "); // Leerzeile vor der Meldung einfügen
  appendLine("Höchstes Track-Verzeichnis: ");
  appendLine("  " + String(highestTrackFolder) + "TRACK");
  displayLines();

  // the next track file number will be highestTrackFileNumber + 1
  int nextTrackFileNumber = lastTrackFileNumber + 1;
  // Filename with leading zeros for the track file number
  String nextTrackFileNumberStr = String(nextTrackFileNumber);
  while (nextTrackFileNumberStr.length() < 6) {
    nextTrackFileNumberStr = "0" + nextTrackFileNumberStr;
  }
  String nextTrackFileName = highestTrackFolderName + "/TR" + nextTrackFileNumberStr + ".TXT";

  // if the number modulo 1000 we nee the next directory ans save the file thereA
  // based on last track not next track
  if (lastTrackFileNumber % 10 == 0 && lastTrackFileNumber != 0) {
    highestTrackFolder++;
    highestTrackFolderName = "/GPSTRK/" + String(highestTrackFolder) + "TRACK";
    #ifdef DEBUG_SD
      Serial.println("Erstelle neues Track-Verzeichnis: " + highestTrackFolderName);
    #endif
    appendLine("  ");
    appendLine("Erstelle neues Track-Verzeichnis: ");
    appendLine("  " + highestTrackFolderName);
    displayLines();
    SD.mkdir(highestTrackFolderName);
    nextTrackFileName = highestTrackFolderName + "/TR" + nextTrackFileNumberStr + ".TXT";
  } 
  #ifdef DEBUG_SD
    Serial.println("Nächste Track-Datei: " + nextTrackFileName);
  #endif
  appendLine(" "); // Leerzeile vor der Meldung einfügen
  appendLine("Nächste Track-Datei: ");
  appendLine("  " + nextTrackFileName);
  displayLines();

  //create the next track file if it doesn't exist yet
  if (!SD.exists(nextTrackFileName)) {
    File nextTrackFile = SD.open(nextTrackFileName, FILE_WRITE);
    if (nextTrackFile) {
      nextTrackFile.close();
      #ifdef DEBUG_SD
        Serial.println("Erstelle neue Track-Datei: " + nextTrackFileName);
      #endif
      appendLine(" "); // Leerzeile vor der Meldung einfügen
      appendLine("Erstelle neue Track-Datei: ");
      appendLine("  " + nextTrackFileName);
      displayLines();
    } else {
      #ifdef DEBUG_SD
        Serial.println("Fehler beim Erstellen der Track-Datei: " + nextTrackFileName);
      #endif
      appendLine(" "); // Leerzeile vor der Meldung einfügen
      appendLine("Fehler beim Erstellen der Track-Datei: ");
      appendLine("  " + nextTrackFileName);
      displayLines();
      onErrorDeepSleep();
    }
  }
  #ifdef DEBUG_SD
    Serial.println("Track-Datei bereit: " + nextTrackFileName);
  #endif
  appendLine("Track-Datei bereit: ");
  appendLine("  " + nextTrackFileName);
  displayLines();

  // add dummy text and close the file
  File nextTrackFile = SD.open(nextTrackFileName, FILE_WRITE);
  if (nextTrackFile) {
    nextTrackFile.println("Dummy text for testing purposes.");
    nextTrackFile.close();
    #ifdef DEBUG_SD
      Serial.println("Dummy text hinzugefügt und Track-Datei geschlossen: " + nextTrackFileName);
    #endif
    appendLine("Dummy text hinzugefügt und Track-Datei geschlossen: ");
    appendLine("  " + nextTrackFileName);
    displayLines();
  } else {
    #ifdef DEBUG_SD
      Serial.println("Fehler beim Öffnen der Track-Datei zum Hinzufügen von Dummy-Text: " + nextTrackFileName);
    #endif
    appendLine("Fehler beim Öffnen der Track-Datei zum Hinzufügen von Dummy-Text: ");
    appendLine("  " + nextTrackFileName);
    displayLines();
  } 
  #ifdef DEBUG_SD
    Serial.println("Fertig mit dem Hinzufügen von Dummy-Text zur Track-Datei: " + nextTrackFileName);
  #endif
  appendLine("Fertig mit dem Hinzufügen von Dummy-Text zur Track-Datei: ");
  appendLine("  " + nextTrackFileName); 
  displayLines();

  //update last track file number
  lastTrackFileNumber = nextTrackFileNumber;
  // save the last track file number to a file for persistence
  File numberFile = SD.open("/GPSTRK/TRKNUM.TXT", FILE_WRITE);
  if (numberFile) {
    numberFile.println(lastTrackFileNumber);
    numberFile.close();
  }
  #ifdef DEBUG_SD
    Serial.println("Letzte Track-Datei-Nummer gespeichert: " + String(lastTrackFileNumber));
  #endif
  appendLine("Letzte Track-Datei-Nummer gespeichert: ");
  appendLine("  " + String(lastTrackFileNumber));
  displayLines();


  // list files in / on SD card
  // appendLine(" "); // Leerzeile vor der Auflistung einfügen
  // appendLine("Dateien im Wurzelverzeichnis der SD-Karte:");
  // listRootFiles();
  delay(10000); // Warte 10 Sekunden vor dem teilweisen Update
}

void loop()
{
  // teilweiser refresh
  // next line to display in the partial window
  // randow string 50 mchar long for testing purposes
  singleLine = "";
  for (int i = 0; i < 50; i++) {
    singleLine += char(random(32, 127)); // generate random printable ASCII character
  }
  singleLine = String(counter) + " --- " + singleLine;
  appendLine(singleLine);
  counter ++;
  // every 7 iterations write one line randomly into the buffer or display
  if (counter % 7 == 0) {
    // randomly select a line from the existing lines array
    int randomIndex = random(0, numLines);
    writeLine(randomIndex, "<----" + String(randomIndex) + "---->");
  }

  displayLines();

  // every 20 iterations list files again on the SD card
  if (counter % 20 == 0) {
    listRootFiles(); // List files in / on SD card every 20 iterations
  }

  // full refresh of the display after partial updates after 10 cycles
  if (counter % 50 == 0) { // Perform full refresh every 10 cycles
    display.setFullWindow(); // Trigger a full refresh of the e-paper display
    display.refresh(); // Perform the full refresh of the e-paper display
  }

  // power on/off managed in every display routine, no need to handle it separately here
  // but to be safe, ensure that the display is powered off at the end of each loop iteration if no further updates are expected immediately.
  display.powerOff(); // Turn off the e-paper display to save power
  delay(1000); // Warte 1 Sekunde vor dem nächsten Update
}