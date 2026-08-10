#include <Wire.h>
#include <U8g2lib.h>
#include <7Semi_BMI270.h>
#include <BackgroundAudioSpeech.h>
#include <ESP32I2SAudio.h>
#include <libespeak-ng/voice/en.h>
#include <esp_sleep.h>
#include "FS.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>

// buttons
#define BUTTON_POWER D3
#define BUTTON_HOME D4

// i2c (for oled and imu)
#define SDA_PIN D5
#define SCL_PIN D6

// imu interrupt
#define IMU_INT1 D11

// i2s (for amplifier)
#define I2S_DOUT D12
#define I2S_BCLK D13
#define I2S_LRCLK D14

// for LittleFS
#define FORMAT_LITTLEFS_IF_FAILED true

bool initDirectories(fs::FS &fs){
  if (!fs.exists("/mcqs")){
    if (!fs.mkdir("/mcqs")){
      Serial.println("failed to create mcqs directory");
      return false;
    }
  };

  if (!fs.exists("/flashcards")){
    if (!fs.mkdir("/flashcards")){
      Serial.println("failed to create flashcards directory");
      return false;
    }
  };

  return true;
}

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
  U8G2_R0,
  U8X8_PIN_NONE
);

BMI270_7Semi imu;

ESP32I2SAudio audio(
  I2S_BCLK,
  I2S_LRCLK,
  I2S_DOUT
);

BackgroundAudioSpeech speech(audio);

enum Gesture {
  NONE,

  TILT_UP,
  TILT_DOWN,
  TILT_LEFT,
  TILT_RIGHT,

  FLICK_UP,
  FLICK_DOWN,
  FLICK_LEFT,
  FLICK_RIGHT,

  SPIN,
};

enum TILT_STATE {
  TILT_NEUTRAL,
  TILT_WAITING_RETURN_RIGHT,
  TILT_WAITING_RETURN_LEFT,
  TILT_WAITING_RETURN_UP,
  TILT_WAITING_RETURN_DOWN
};

enum FLICK_STATE {
  FLICK_NEUTRAL,
  FLICK_WAITING_RETURN_RIGHT,
  FLICK_WAITING_RETURN_LEFT,
  FLICK_WAITING_RETURN_UP,
  FLICK_WAITING_RETURN_DOWN,
};

enum FACE_STATE {
  FACE_UP,
  FACE_DOWN
};

enum DEVICE_STATE {
  HOME,
  MCQ_SET_MENU,
  FLASHCARD_SET_MENU,
  MCQ_ACTIVE,
  FLASHCARD_ACTIVE
};

const char* gestureToString(Gesture gesture);
Gesture detectTilt(float ax, float ay, float az);
Gesture detectFlick(float ax, float ay, float az);
Gesture detectSpin(float ax, float ay, float az);
Gesture detectGesture(float ax, float ay, float az);

TILT_STATE tiltState = TILT_NEUTRAL;
FLICK_STATE flickState = FLICK_NEUTRAL;
FACE_STATE faceState = FACE_UP;
DEVICE_STATE deviceState = HOME;

bool mcqSetsChanged = true;
bool flashcardSetsChanged = true;
String setSelected;

const char* gestureToString(Gesture gesture){
  switch (gesture){
    case TILT_UP: return "tilt up";
    case TILT_DOWN: return "tilt down";
    case TILT_LEFT: return "tilt left";
    case TILT_RIGHT: return "tilt right";

    case FLICK_UP: return "flick up";
    case FLICK_DOWN: return "flick down";
    case FLICK_LEFT: return "flick left";
    case FLICK_RIGHT: return "flick right";

    case SPIN: return "spin";

    default: return "none";
  }
}

Gesture detectTilt(float ax, float ay, float az){
  const float tiltThreshold = 0.7;
  const float neutralThreshold = 0.25;

  if (tiltState == TILT_NEUTRAL){
    if (ax > tiltThreshold){
      tiltState = TILT_WAITING_RETURN_RIGHT;
    }
    else if (ax < (-1 * tiltThreshold)){
      tiltState = TILT_WAITING_RETURN_LEFT;
    }
    else if (ay > tiltThreshold){
      tiltState = TILT_WAITING_RETURN_UP;
    }
    else if (ay < (-1 * tiltThreshold)){
      tiltState = TILT_WAITING_RETURN_DOWN;
    }
  }
  
  if (tiltState == TILT_WAITING_RETURN_DOWN){
    if (abs(ax) < neutralThreshold && abs(ay) < neutralThreshold){
      tiltState = TILT_NEUTRAL;
      return TILT_DOWN;
    }
  }
  if (tiltState == TILT_WAITING_RETURN_UP) {
    if (abs(ax) < neutralThreshold && abs(ay) < neutralThreshold){
      tiltState = TILT_NEUTRAL;
      return TILT_UP;
    }
  }

  if (tiltState == TILT_WAITING_RETURN_LEFT){
    if (abs(ax) < neutralThreshold && abs(ay) < neutralThreshold){
      tiltState = TILT_NEUTRAL;
      return TILT_LEFT;
    }
  }

  if (tiltState == TILT_WAITING_RETURN_RIGHT){
    if (abs(ax) < neutralThreshold && abs(ay) < neutralThreshold){
      tiltState = TILT_NEUTRAL;
      return TILT_RIGHT;
    }
  }
  return NONE;
}

Gesture detectFlick(float ax, float ay, float az){
  const float flickThreshold = 1.5;
  const float neutralThreshold = 0.25;

  if (flickState == FLICK_NEUTRAL){
    if (ax > flickThreshold){
      flickState = FLICK_WAITING_RETURN_RIGHT;
    }
    else if (ax < (-1 * flickThreshold)){
      flickState = FLICK_WAITING_RETURN_LEFT;
    }
    else if (ay > flickThreshold){
      flickState = FLICK_WAITING_RETURN_UP;
    }
    else if (ay < (-1 * flickThreshold)){
      flickState = FLICK_WAITING_RETURN_DOWN;
    }
  }
  
  if (flickState == FLICK_WAITING_RETURN_DOWN){
    if (abs(ax) < neutralThreshold && abs(ay) < neutralThreshold){
      flickState = FLICK_NEUTRAL;
      return FLICK_DOWN;
    }
  }
  if (flickState == FLICK_WAITING_RETURN_UP) {
    if (abs(ax) < neutralThreshold && abs(ay) < neutralThreshold){
      flickState = FLICK_NEUTRAL;
      return FLICK_UP;
    }
  }

  if (flickState == FLICK_WAITING_RETURN_LEFT){
    if (abs(ax) < neutralThreshold && abs(ay) < neutralThreshold){
      flickState = FLICK_NEUTRAL;
      return FLICK_LEFT;
    }
  }

  if (flickState == FLICK_WAITING_RETURN_RIGHT){
    if (abs(ax) < neutralThreshold && abs(ay) < neutralThreshold){
      flickState = FLICK_NEUTRAL;
      return FLICK_RIGHT;
    }
  }
  return NONE;
}

Gesture detectSpin(float ax, float ay, float az){
  const float faceDownThreshold = -0.8;
  const float faceUpThreshold = 0.8;

  if (faceState == FACE_UP && az < faceDownThreshold){
    faceState = FACE_DOWN;
  }
  
  if (faceState == FACE_DOWN && az > faceUpThreshold){
    faceState = FACE_UP;
    return SPIN;
  }

  return NONE;
}

Gesture detectGesture(float ax, float ay, float az){
  Gesture gesture;

  if (flickState != FLICK_NEUTRAL){
    return detectFlick(ax, ay, az);
  }

  if (tiltState != TILT_NEUTRAL){
    return detectTilt(ax, ay, az);
  }

  if (faceState != FACE_UP){
    return detectSpin(ax, ay, az);
  }

  gesture = detectFlick(ax, ay, az);
  if (flickState != FLICK_NEUTRAL){
    return gesture;
  }

  gesture = detectTilt(ax, ay, az);
  if (tiltState != TILT_NEUTRAL){
    return gesture;
  }

  gesture = detectSpin(ax, ay, az);
  return gesture;
}

void powerOff(){
  display.clearBuffer();
  display.sendBuffer();

  speech.flush();

  delay(100);

  esp_sleep_enable_ext0_wakeup(
    (gpio_num_t)BUTTON_POWER,
    0
  );

  esp_deep_sleep_start();
}

bool validateMCQJSON(const String& json){
  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json);

  if (error){
    Serial.println("invalid json");
    return false;
  }

  if (!doc["name"].is<const char*>()){
    Serial.println("missing/invalid name");
    return false;
  }

  if (!doc["questions"].is<JsonArray>()){
    Serial.println("missing questions array");
    return false;
  }

  JsonArray questions = doc["questions"];

  if (questions.size() == 0){
    Serial.println("no questions");
    return false;
  }

  for (JsonObject question: questions){
    if (!question["question"].is<const char*>()){
      Serial.println("invalid question");
      return false;
    }

    if (!question["options"].is<JsonArray>()){
      Serial.println("ERROR: missing/invalid options");
      return false;
    }

    JsonArray options = question["options"];

    if (options.size() < 2 || options.size() > 4){
      Serial.println("mcq must have between 2 and 4 options");
      return false;
    }

    for (JsonVariant option: options){
      if (!option.is<const char*>()){
        Serial.println("option must be text");
        return false;
      }
    }

    if (!question["answer"].is<int>()){
      Serial.println("invalid answer");
      return false;
    }

    int answer = question["answer"];
    if (answer < 0 || answer >= options.size()){
      Serial.print("answer must be between 0-");
      Serial.println(options.size()-1);
      return false;
    }
  }

  return true;
}

bool validateFlashcardJSON(const String& json){
  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json);

  if (error){
    Serial.println("ERROR: invalid json");
    return false;
  }

  if (!doc["name"].is<const char*>()){
    Serial.println("ERROR: missing/invalid json");
    return false;
  }

  if (!doc["cards"].is<JsonArray>()){
    Serial.println("ERROR: missing cards array");
    return false;
  }

  JsonArray cards = doc["cards"];

  if (cards.size() == 0){
    Serial.println("ERROR: no flashcards");
    return false;
  }

  for (JsonObject card: cards){
    if (!card["front"].is<const char*>()){
      Serial.println("ERROR: invalid front");
      return false;
    }

    if (!card["back"].is<const char*>()){
      Serial.println("ERROR: invalid back");
      return false;
    }
  }

  return true;
}

bool validateSet(const String& type, const String& json){
  if (type == "MCQ"){
    return validateMCQJSON(json);
  }

  if (type == "FLASHCARD"){
    return validateFlashcardJSON(json);
  }

  return false;
}

bool validateTempFile(const String& type){
  File file = LittleFS.open("/tmp.json", FILE_READ);

  if (!file){
    Serial.println("ERROR: failed to open temp file");
    return false;
  }

  String json = file.readString();
  file.close();

  return validateSet(type, json);
}

bool moveTempFile(const String& finalPath){
  if (LittleFS.exists(finalPath)){
    LittleFS.remove(finalPath);
  }

  if (!LittleFS.rename("/tmp.json", finalPath)){
    Serial.println("ERROR: failed to move temp file");
    return false;
  }

  return true;
}

void checkUSB(){
  if (Serial.available()){
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "PING"){
      Serial.println("PONG");
      return;
    }

    if (command == "UPLOAD"){
      String type = Serial.readStringUntil('\n');
      String name = Serial.readStringUntil('\n');
      String sizeString = Serial.readStringUntil('\n');

      type.trim();
      name.trim();
      sizeString.trim();

      size_t fileSize = sizeString.toInt();

      if (name.length() == 0 ||
          name.indexOf('/') != -1 ||
          name.indexOf('\\') != -1 ||
          !name.endsWith(".json")){
        Serial.println("ERROR: invalid filename");
        return;
      }

      String finalPath;

      if (type == "MCQ"){
        finalPath = "/mcqs/" + name;
      } else if (type == "FLASHCARD"){
        finalPath = "/flashcards/" + name;
      } else {
        Serial.println("ERROR: invalid type");
        return;
      }

      if (LittleFS.exists("/tmp.json")){
        LittleFS.remove("/tmp.json");
      }

      File tempFile = LittleFS.open("/tmp.json", FILE_WRITE);
      if (!tempFile){
        Serial.println("ERROR: failed to open temp file");
        return;
      }

      size_t received = 0;

      while (received < fileSize){
        if (Serial.available()){
          uint8_t byte = Serial.read();
          tempFile.write(byte);
          received++;
        }
      }

      tempFile.close();

      if (!validateTempFile(type)){
        Serial.println("ERROR: invalid set");
        LittleFS.remove("/tmp.json");
        return;
      }

      if (!moveTempFile(finalPath)){
        Serial.println("ERROR: could not copy file into directory");
        LittleFS.remove("/tmp.json");
        return;
      }

      if (type == "MCQ"){
        mcqSetsChanged = true;
      } else if (type == "FLASHCARD"){
        flashcardSetsChanged = true;
      } 

      Serial.println("OK");
    }
  }
}

void checkPowerButton(){
  static unsigned long pressedAt = 0;
  static bool holding = false;

  bool pressed = digitalRead(BUTTON_POWER) == LOW;

  if (pressed && !holding){
    holding = true;
    pressedAt = millis();
  }

  if (!pressed){
    holding = false;
  }

  if (holding && millis() - pressedAt >= 2000){
    powerOff();
  }
}

void checkHomeButton(){
  static bool wasPressed = false;

  bool pressed = digitalRead(BUTTON_HOME) == LOW;

  if (pressed && !wasPressed){
    display.clearBuffer();
    display.sendBuffer();

    speech.flush();
    deviceState = HOME;
  }

  wasPressed = pressed;
}

std::vector<String> getAllFlashcardSets(fs::FS &fs){
  std::vector<String> sets;
  File root = fs.open("/flashcards");
  
  File file = root.openNextFile();

  if (!root || !root.isDirectory()){
    Serial.println("failed to open flashcards directory");
    return sets;
  }

  while (file){
    if (!file.isDirectory()){
      sets.push_back(String(file.name()));
    }

    file = root.openNextFile();
  }

  return sets;
}

std::vector<String> getAllMCQSets(fs::FS &fs){
  std::vector<String> sets;
  File root = fs.open("/mcqs");

  if (!root || !root.isDirectory()){
    Serial.println("failed to open mcqs directory");
    return sets;
  }

  File file = root.openNextFile();
  while (file){
    if (!file.isDirectory()){
      sets.push_back(String(file.name()));
    }

    file = root.openNextFile();
  }

  return sets;
}

void updateHome(){
  static bool willNeedRender = true;
  static int menuOptionSelected = 0;

  float ax, ay, az;
  if (imu.readAccel(ax, ay, az)) {
    Gesture gesture = detectGesture(ax, ay, az);
    if (gesture == TILT_UP){
      if (menuOptionSelected == 1){
        menuOptionSelected = 0;
        willNeedRender = true;
      }
    }
    else if (gesture == TILT_DOWN){
      if (menuOptionSelected == 0){
        menuOptionSelected = 1;
        willNeedRender = true;
      }
    }
    else if (gesture == TILT_RIGHT){
      if (menuOptionSelected == 0){
        deviceState = MCQ_SET_MENU;
      } else {
        deviceState = FLASHCARD_SET_MENU;
      }
      willNeedRender = true;
      return;
    }
  }

  if (willNeedRender){
    display.clearBuffer();
    display.setFont(u8g2_font_6x12_tf);
    display.setCursor(0, 10);
    display.print("lethe");

    display.setCursor(0, 20);
    if (menuOptionSelected == 0){
      display.print("> MCQs");
    } else {
      display.print(" MCQs");
    }

    display.setCursor(0, 30);
    if (menuOptionSelected == 1){
      display.print("> Flashcards");
    } else {
      display.print("Flashcards");
    }

    display.sendBuffer();
    willNeedRender = false;
  }
}

void updateMCQMenu(){
  static bool willNeedRender = true;
  static int menuOptionSelected = 0;
  static std::vector<String> sets = getAllMCQSets(LittleFS);

  if (mcqSetsChanged){
    sets = getAllMCQSets(LittleFS);
    mcqSetsChanged = false;

    menuOptionSelected = 0;
    willNeedRender = true;
  }

  float ax, ay, az;
  if (imu.readAccel(ax, ay, az)) {
    Gesture gesture = detectGesture(ax, ay, az);
    if (gesture == TILT_UP){
      if (!sets.empty() && menuOptionSelected != 0){
        menuOptionSelected -= 1;
        willNeedRender = true;
      }
    }
    else if (gesture == TILT_DOWN){
      if (!sets.empty() && menuOptionSelected != sets.size() - 1){
        menuOptionSelected += 1;
        willNeedRender = true;
      }
    }
    else if (gesture == TILT_RIGHT){
      if (!sets.empty()){
        deviceState = MCQ_ACTIVE;
        setSelected = sets[menuOptionSelected];
        willNeedRender = true;
        return;
      }
    }
    else if (gesture == TILT_LEFT){
      deviceState = HOME;
      willNeedRender = true;
    }
  }

  if (willNeedRender){
    display.clearBuffer();
    display.setFont(u8g2_font_6x12_tf);
    if (sets.empty()){
      display.setCursor(0, 10);
      display.print("no mcq sets");
    } else {
      int y = 10;
      for (int i=menuOptionSelected; i < menuOptionSelected+4; i++) {
        display.setCursor(0, y);
        if (i < sets.size()){
          if (i == menuOptionSelected){
            display.print("> ");
          } else {
            display.print("  ");
          }
          display.print(sets[i]);
        }
        y += 10;
      }
    }
    display.sendBuffer();
    willNeedRender = false;
  }

  // up and down to move between the sets
  // tilt right for okay to select that set
}

void updateFlashcardMenu(){
  // up and down to move between the sets
  // tilt right for okay to select the set
}

void updateMCQ(){
  //  play question
  //  tilt for each answer (four directions or less)
  //  spin to finish the quiz (if finish then say score and go back to select set menu)
  //  on answer, say answer (right/wrong), then next question
  //  keep a record of which questions were answered correctly/wrong and will keep on going until all questions were answered correctly
}

void updateFlashcard(){
  //  tilt forward to flip to back then say
  //  tilt left/right to say its right/wrong
  //  spin to finish
  //  keep showing wrong ones until finished
}

void setup() {
  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);

  BMI270_7Semi::Config cfg;

  cfg.bus = BMI270_7Semi::Bus::I2C;
  cfg.addr = 0x69;
  cfg.sda = SDA_PIN;
  cfg.scl = SCL_PIN;
  cfg.i2cHz = 400000;

  if (!imu.begin(cfg)){
    Serial.println("BMI270 failed");
    return;
  }

  imu.setAccelConfig(
    BMI2_ACC_ODR_100HZ,
    BMI2_ACC_RANGE_2G,
    BMI2_ACC_NORMAL_AVG4,
    BMI2_PERF_OPT_MODE
  );

  display.begin();

  pinMode(BUTTON_POWER, INPUT_PULLUP);
  pinMode(BUTTON_HOME, INPUT_PULLUP);

  speech.setVoice(voice_en);

  if (!speech.begin()){
    Serial.println("speech failed");
    return;
  }
  speech.speak("hello lethe");

  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
    Serial.println("LittleFS Mount Failed");
    return;
  }
  
  if (!initDirectories(LittleFS)){
    return;
  }
}

void loop() {
  checkPowerButton();
  checkHomeButton();
  checkUSB();

  switch (deviceState) {
    case HOME:
      updateHome();
      break;

    case MCQ_SET_MENU:
      updateMCQMenu();
      break;

    case FLASHCARD_SET_MENU:
      updateFlashcardMenu();
      break;

    case MCQ_ACTIVE:
      updateMCQ();
      break;

    case FLASHCARD_ACTIVE:
      updateFlashcard();
      break;
  }
  
  delay(20);
}
