#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#include <PololuMaestro.h>
#include <DFMiniMp3.h>

#define LED_PIN 6
#define LED_PIN2 4
#define TOUCH_PIN 5
#define NUM_LEDS 56
#define NUM_LEDS2 12
#define METEOR_SIZE 10
#define METEOR_TRAIL_DECAY 64

class Mp3Notify;
SoftwareSerial secondarySerial(9, 8); // RX, TX
typedef DFMiniMp3<SoftwareSerial, Mp3Notify> DfMp3;
DfMp3 dfmp3(secondarySerial);

class Mp3Notify {
public:
  static void PrintlnSourceAction(DfMp3_PlaySources source, const char* action) {
    if (source & DfMp3_PlaySources_Sd) {
      Serial.print("SD Card, ");
    }
    if (source & DfMp3_PlaySources_Usb) {
      Serial.print("USB Disk, ");
    }
    if (source & DfMp3_PlaySources_Flash) {
      Serial.print("Flash, ");
    }
    Serial.println(action);
  }

  static void OnError([[maybe_unused]] DfMp3& mp3, uint16_t errorCode) {
    // see DfMp3_Error for code meaning
    Serial.println();
    Serial.print("Com Error ");
    Serial.println(errorCode);
  }

  static void OnPlayFinished([[maybe_unused]] DfMp3& mp3, [[maybe_unused]] DfMp3_PlaySources source, uint16_t track) {
    Serial.print("Play finished for #");
    Serial.println(track);
    // start next track
    track += 1;
    // this example will just start back over with 1 after track 3
    if (track > 3) {
      track = 1;
    }
    dfmp3.playMp3FolderTrack(track);  // sd:/mp3/0001.mp3, sd:/mp3/0002.mp3, sd:/mp3/0003.mp3
  }

  static void OnPlaySourceOnline([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source) {
    PrintlnSourceAction(source, "online");
  }

  static void OnPlaySourceInserted([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source) {
    PrintlnSourceAction(source, "inserted");
  }

  static void OnPlaySourceRemoved([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source) {
    PrintlnSourceAction(source, "removed");
  }
};

Adafruit_NeoPixel strip1 = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2 = Adafruit_NeoPixel(NUM_LEDS2, LED_PIN2, NEO_GRB + NEO_KHZ800);
SoftwareSerial maestroSerial(10, 11); // RX, TX pins for Pololu Maestro
MicroMaestro maestro(maestroSerial);

unsigned long previousMillis = 0;
unsigned long chasePreviousMillis = 0;
const long interval = 30; // Interval at which to update the meteor effect (milliseconds)
const long chaseInterval = 100; // Interval for updating the chasing light pattern
int meteorPos = 0;
bool isRunning = false;
int chaseIndex = 0;

void setup() {
  maestroSerial.begin(9600);
  Serial.begin(115200);
  strip1.begin();
  strip1.show(); // Initialize all pixels to 'off'
  strip2.begin();
  strip2.show(); // Initialize all pixels to 'off'
  pinMode(TOUCH_PIN, INPUT); // Set the touch sensor pin as input
  // Initialise Mp3 player
  secondarySerial.begin(9600);
  dfmp3.begin();
  Serial.println();
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));
  uint16_t volume = dfmp3.getVolume();
  dfmp3.setVolume(22);
  Serial.print("volume ");
  Serial.println(volume);
  randomSeed(analogRead(0)); // Make Random more random
  uint16_t count = dfmp3.getTotalTrackCount(DfMp3_PlaySource_Sd);
  Serial.print("files ");
  Serial.println(count);
}

void loop() {
  if (digitalRead(TOUCH_PIN) == HIGH && !isRunning) {
    isRunning = true;
    meteorPos = 0; // Reset the meteor position
    int randomScript = random(0, 4);
    maestro.restartScript(randomScript); // Trigger sequence 0 on the Maestro
    // Play a random MP3 track from 1 to 10 
    int randomTrack = random(0, 9);
    dfmp3.playMp3FolderTrack(randomTrack);
    Serial.println("looping");
  }

  if (isRunning) {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;

      // Update meteor effect
      updateMeteorEffect(strip1, strip1.Color(0, 0, 255));

      // Stop the effect after it completes one full loop
      if (meteorPos >= strip1.numPixels() * 3) { // Adjust as needed for complete loops
        fadeAllToBlack(strip1);
        fadeAllToBlack(strip2);
        isRunning = false;
      }
    }

    if (currentMillis - chasePreviousMillis >= chaseInterval) {
      chasePreviousMillis = currentMillis;
      updateChasingEffect(strip2, chaseIndex);
      chaseIndex = (chaseIndex - 1 + strip2.numPixels()) % strip2.numPixels();
    }
  }

  // Allow MP3 player notifications to be handled without interrupts
  //dfmp3.loop();
}

void updateMeteorEffect(Adafruit_NeoPixel &strip, uint32_t color) {
  int pos = meteorPos % strip.numPixels(); // Ensure the position loops back to 0 after the last LED
  for (int j = 0; j < strip.numPixels(); j++) {
    if (random(10) > 5) {
      fadeToBlack(j, METEOR_TRAIL_DECAY);
    }
  }
  for (int j = 0; j < METEOR_SIZE; j++) {
    int ledPos = (pos - j + strip.numPixels()) % strip.numPixels();
    if (ledPos >= 0 && ledPos < strip.numPixels()) {
      strip.setPixelColor(ledPos, color);
    }
  }
  strip.show();
  meteorPos++;
}

void updateChasingEffect(Adafruit_NeoPixel &strip, int index) {
  for (int i = 0; i < strip.numPixels(); i++) {
    if (i == index) {
      strip.setPixelColor(i, strip.Color(255, 255, 255)); // White color
    } else {
      strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
    }
  }
  strip.show();
}

void fadeToBlack(int ledNo, byte fadeValue) {
  uint32_t oldColor = strip1.getPixelColor(ledNo);
  uint8_t r = (oldColor & 0x00ff0000UL) >> 16;
  uint8_t g = (oldColor & 0x0000ff00UL) >> 8;
  uint8_t b = (oldColor & 0x000000ffUL);
  r = (r <= 10) ? 0 : (int) r - (r * fadeValue / 256);
  g = (g <= 10) ? 0 : (int) g - (g * fadeValue / 256);
  b = (b <= 10) ? 0 : (int) b - (b * fadeValue / 256);
  strip1.setPixelColor(ledNo, r, g, b);
}

void fadeAllToBlack(Adafruit_NeoPixel &strip) {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, 0); // Set all LEDs to black
  }
  strip.show();
}

void waitMilliseconds(uint16_t msWait) {
  uint32_t start = millis();
  
  while ((millis() - start) < msWait) {
    // calling mp3.loop() periodically allows for notifications to be handled without interrupts
    dfmp3.loop();
    delay(1);
  }
}
