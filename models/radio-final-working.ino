
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// ========================================
// DFPLAYER MINI - ORIGINAL KICAD WIRING
// ========================================

const int DF_RX = 4;
const int DF_TX = 5;
const int DF_BUSY = 7;  // NEW: DFPlayer BUSY -> Arduino D7

// ========================================
// ROTARY ENCODER
// ========================================

const int encoderCLK = 2;
const int encoderDT = 3;
const int encoderSW = 6;

// Volume: 0 to 30
const int speakerVolume = 30;

SoftwareSerial dfSerial(DF_RX, DF_TX);
DFRobotDFPlayerMini dfPlayer;

// ========================================
// TRACK VARIABLES
// ========================================

int totalTracks = 0;
int currentTrack = 0;

bool isPlaying = false;
bool isPaused = false;
bool waitingForPlayback = false;
bool playbackStarted = false;

unsigned long trackCommandTime = 0;

// ========================================
// ENCODER TRACKING
// ========================================

int lastCLK;
int encoderSteps = 0;

const int transitionsPerClick = 4;

// ========================================
// BUTTON DEBOUNCE
// ========================================

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 40;

// ========================================
// PLAY SELECTED RECORDING
// ========================================

void playSelectedTrack() {

  if (totalTracks <= 0) return;

  if (currentTrack < 1 || currentTrack > totalTracks) return;

  dfPlayer.playFolder(1, currentTrack);

  isPlaying = true;
  isPaused = false;

  waitingForPlayback = true;
  playbackStarted = false;
  trackCommandTime = millis();

  Serial.print("Playing recording ");
  Serial.println(currentTrack);
}

// ========================================
// SETUP
// ========================================

void setup() {

  Serial.begin(9600);

  pinMode(encoderCLK, INPUT_PULLUP);
  pinMode(encoderDT, INPUT_PULLUP);
  pinMode(encoderSW, INPUT_PULLUP);

  // BUSY is normally LOW during playback
  pinMode(DF_BUSY, INPUT_PULLUP);

  lastCLK = digitalRead(encoderCLK);

  dfSerial.begin(9600);

  if (!dfPlayer.begin(dfSerial)) {

    Serial.println("DFPlayer not detected. Check wiring.");

    while (true) {
      delay(100);
    }
  }

  dfPlayer.volume(speakerVolume);

  delay(500);

  // Keep the original three attempts
  for (int attempt = 0; attempt < 3; attempt++) {

    int count = dfPlayer.readFileCountsInFolder(1);

    if (count > 0) {
      totalTracks = count;
      break;
    }

    delay(500);
  }

  Serial.print("Recordings found: ");
  Serial.println(totalTracks);

  if (totalTracks <= 0) {
    Serial.println("No recordings found in folder 01.");
  }

  // Silent on startup
  dfPlayer.stop();

  Serial.println("Radio ready!");
  Serial.println("Turn knob or press to start.");
}

// ========================================
// MAIN LOOP
// ========================================

void loop() {

  // ----------------------------------------
  // ROTARY ENCODER - NEXT / PREVIOUS
  // ----------------------------------------

  int currentCLK = digitalRead(encoderCLK);

  if (currentCLK != lastCLK) {

    if (digitalRead(encoderDT) != currentCLK) {
      encoderSteps++;
    } else {
      encoderSteps--;
    }

    if (abs(encoderSteps) >= transitionsPerClick) {

      int direction = (encoderSteps > 0) ? 1 : -1;
      encoderSteps = 0;

      if (totalTracks > 0) {

        if (currentTrack == 0) {
          currentTrack = (direction > 0) ? 1 : totalTracks;
        } else {
          currentTrack += direction;

          // Manual skipping wraps around
          if (currentTrack > totalTracks) {
            currentTrack = 1;
          }

          if (currentTrack < 1) {
            currentTrack = totalTracks;
          }
        }

        // Interrupt current recording
        playSelectedTrack();
      }
    }
  }

  lastCLK = currentCLK;

  // ----------------------------------------
  // PUSH BUTTON - START / PAUSE / RESUME
  // ----------------------------------------

  // ----------------------------------------
  // PUSH BUTTON - START / PAUSE / RESUME
  // ----------------------------------------

  bool buttonReading = digitalRead(encoderSW);

  if (buttonReading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {

    if (buttonReading != stableButtonState) {

      stableButtonState = buttonReading;

      if (stableButtonState == LOW && totalTracks > 0) {

        if (isPlaying && !isPaused) {

          dfPlayer.pause();
          isPaused = true;

          Serial.println("Paused");

        } else if (isPlaying && isPaused) {

          dfPlayer.start();
          isPaused = false;

          waitingForPlayback = true;
          playbackStarted = false;

          Serial.println("Resumed");

        } else {

          currentTrack = 1;
          playSelectedTrack();
        }
      }
    }
  }

  lastButtonReading = buttonReading;


  // ----------------------------------------
  // AUTOMATIC PLAYBACK USING BUSY PIN
  // ----------------------------------------

  if (isPlaying && !isPaused) {

    bool busy = (digitalRead(DF_BUSY) == LOW);

    // Wait until the DFPlayer actually starts
    if (waitingForPlayback) {

      if (busy) {
        playbackStarted = true;
        waitingForPlayback = false;
      }

    } else if (playbackStarted && !busy) {

      // Allow brief transitions to settle
      delay(100);

      if (digitalRead(DF_BUSY) == HIGH) {

        playbackStarted = false;

        Serial.print("Finished recording ");
        Serial.println(currentTrack);

        if (currentTrack < totalTracks) {

          // Automatically play next recording
          currentTrack++;
          playSelectedTrack();

        } else {

          // Last recording: STOP
          dfPlayer.stop();

          isPlaying = false;
          isPaused = false;
          waitingForPlayback = false;

          Serial.println("All recordings finished!");
        }
      }
    }
  }
}
