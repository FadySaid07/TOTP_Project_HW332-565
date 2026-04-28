#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include "mbedtls/md.h"
#include "esp_sleep.h"


const char* WIFI_SSID     = "";
const char* WIFI_PASSWORD = "";
const char* TOTP_SECRET   = "JBSWY3DPEHPK3PXP"; // Seceret Key

// ==========================================
// 2. HARDWARE DEFINITIONS
// ==========================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
#define WAKEUP_PIN   GPIO_NUM_4 // The pin connected to your button

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==========================================
// 3. MAIN SETUP LOGIC (Runs once per wake)
// ==========================================
void setup() {
  Serial.begin(115200);

  pinMode(WAKEUP_PIN, INPUT_PULLUP);
  esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, 0);


  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); 
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  connectWiFi();
  syncTime();

  String otpCode = generateTOTP(TOTP_SECRET);
  displayCode(otpCode);
  delay(10000);
  goToDeepSleep();
}

void loop() {
}


// NETWORK & TIME FUNCTIONS

void connectWiFi() {
  display.clearDisplay();
  display.setCursor(0, 10);
  display.setTextSize(1);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    display.println("WiFi Connected!");
    display.display();
  } else {
    display.println("WiFi Failed!");
    display.display();
    delay(2000);
    goToDeepSleep(); 
  }
}

void syncTime() {
  display.println("Syncing Time...");
  display.display();

  
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  time_t now = time(nullptr);
  while (now < 100000) {
    delay(500);
    now = time(nullptr);
  }
}


// CRYPTOGRAPHY


int decodeBase32(const char *base32, uint8_t *output) {
  int len = strlen(base32);
  int buffer = 0, bitsLeft = 0, count = 0;

  for (int i = 0; i < len; i++) {
    uint8_t val = 0;
    char c = base32[i];

    if (c >= 'A' && c <= 'Z') val = c - 'A';
    else if (c >= '2' && c <= '7') val = c - '2' + 26;
    else continue; 

    buffer = (buffer << 5) | val;
    bitsLeft += 5;

    if (bitsLeft >= 8) {
      output[count++] = (buffer >> (bitsLeft - 8)) & 0xFF;
      bitsLeft -= 8;
    }
  }

  return count; 
}

String generateTOTP(const char* base32Secret) {
  uint8_t secretBytes[32];
  int secretLen = decodeBase32(base32Secret, secretBytes);

  unsigned long currentTime = time(nullptr);
  unsigned long counter = currentTime / 30;

  uint8_t counterBytes[8];
  for (int i = 7; i >= 0; i--) {
    counterBytes[i] = counter & 0xFF;
    counter >>= 8;
  }

  uint8_t hmacResult[20];
  mbedtls_md_context_t ctx;

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), 1);
  mbedtls_md_hmac_starts(&ctx, secretBytes, secretLen);
  mbedtls_md_hmac_update(&ctx, counterBytes, 8);
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);

  int offset = hmacResult[19] & 0x0F;

  unsigned long truncatedHash =
      ((hmacResult[offset] & 0x7F) << 24) |
      ((hmacResult[offset + 1] & 0xFF) << 16) |
      ((hmacResult[offset + 2] & 0xFF) << 8) |
      (hmacResult[offset + 3] & 0xFF);

  char codeString[7];
  sprintf(codeString, "%06lu", truncatedHash % 1000000);

  return String(codeString);
}

// DISPLAY & POWER MANAGEMENT

void displayCode(String code) {
  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(20, 20);
  display.print(code.substring(0, 3) + " " + code.substring(3, 6));

  unsigned long remainingSeconds = 30 - (time(nullptr) % 30);

  display.setTextSize(1);
  display.setCursor(20, 50);
  display.print("Valid for: ");
  display.print(remainingSeconds);
  display.print("s");

  display.display();
}

void goToDeepSleep() {
  Serial.println("Going to sleep now...");

  display.clearDisplay();
  display.display();

  display.ssd1306_command(SSD1306_DISPLAYOFF);

  esp_deep_sleep_start();
}
