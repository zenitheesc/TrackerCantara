/* 
 * Software made to run in the TTGO TBeam boards, meant for tracking Zenith Aerospace Stratospheric sondes.
 *
 * This code was primarily made with caos and tears, during the 2022 Latin America Space Challenge, and then
 * gradually updated and improved. Many hands have typed lines in this crap, memorable mentions to Celente,
 * Matheus and Júlio. Hopefully this might be its last form.
 *
 * Júlio Calandrin - 22/11/2022 :)
 */

// ----------------- User Parameters ----------------- //

#define BLUETOOTH_DEVICE_NAME "TrackerCantara"
#define SERIAL_BAUD_RATE 115200
#define UTC_OFFSET 3


// ----------------- Includes ----------------- //

#include <SPI.h>
#include "SX127X.h"
#include <stdint.h>
#include "BluetoothSerial.h"
#include "AXP192.h"
#include "SPIMemory.h"


// ----------------- Bluetooth checks ----------------- //

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif
BluetoothSerial SerialBT;


// ----------------- Pinout ----------------- //

#define BUZZER 0
#define RED_LED 33
#define GREEN_LED 32
#define BLUE_LED 25
#define BUTTON 38

#define FLASH_SS 4
#define FLASH_MISO 14
#define FLASH_MOSI 2
#define FLASH_SCLK 13

#define RADIO_DIO0 26
#define RADIO_DIO1 39
#define RADIO_NRST 23
#define RADIO_SS 18
#define RADIO_MISO 19
#define RADIO_MOSI 27
#define RADIO_SCLK 5


// ----------------- Button Debounce ----------------- //

#define DebounceInterval 500
unsigned long LastButtonPress = 0;

// ----------------- Battery Management Object----------------- //

AXP192 TTGO_AXP192;


// ----------------- SPI Objects ----------------- //

// Uninitalised pointers to SPI objects
SPIClass *radio_spi = NULL;
SPIClass *flash_spi = NULL;


// ----------------- Radio Packets Structs ----------------- //

SX127X_t SX127X;

typedef struct {
  // Position
  float latitude, longitude, altitude;
  // GPS Time
  uint8_t seconds, minutes, hours;
  // Packet Identifier
  uint16_t packet_id;
} __attribute__((packed)) tracker_packet_values_t;

// Union of Packet data as struct and byte array
typedef union {
  tracker_packet_values_t values;
  uint8_t raw[sizeof(tracker_packet_values_t)];
} tracker_packet_t;

 
// ----------------- Flash Storage ----------------- //

typedef struct {
  uint32_t dead_beef;
  tracker_packet_values_t tracker_packet;
  uint8_t rssi;
} __attribute__((packed)) flash_packet_values_t;

typedef struct {
  tracker_packet_values_t tracker_packet;
  uint8_t rssi;
} __attribute__((packed)) flash_packet_no_deadbeaf_t;

typedef union {
  flash_packet_no_deadbeaf_t values;
  uint8_t raw[sizeof(flash_packet_no_deadbeaf_t)];
} __attribute__((packed)) flash_union_t;

#define ARDUINO_ARCH_ESP32
SPIFlash *flash = NULL;

// ----------------- Serial Command String ----------------- //

String serialCommand = "";
bool inCommand = false;
uint8_t charCounter = 0;

// ----------------- Task Configuration Structs ----------------- //

typedef struct {
  bool busy;
  int frequency;
  int beep_number;
  int on_time;
  int off_time;
  bool stop_please;
} buzzer_params_t;
volatile buzzer_params_t buzzer_params;

typedef struct {
  bool busy;
  uint32_t hex_value;
  int blink_number;
  int on_time;
  int off_time;
  bool stop_please;
} rgb_led_params_t;
volatile rgb_led_params_t rgb_led_params;
volatile uint32_t standbyColor = 0x111111;

// ----------------- Tasks declarations ----------------- //

static void dataHandlerTask(void *pvParameters);
static TaskHandle_t xDataHandlerTask = NULL;

static void softBlinkRGBTask(void *pvParameters);
static TaskHandle_t xSoftBlinkRGBTask = NULL;

static void beepsTask(void *pvParameters);
static TaskHandle_t xBeepsTask = NULL;

static void goodPacketWarning(void *pvParameters);
static TaskHandle_t xGoodPacketWarning = NULL;

static void badPacketWarning(void *pvParameters);
static TaskHandle_t xBadPacketWarning = NULL;

static void flashDumpTask(void *pvParameters);
static TaskHandle_t xFlashDumpTask = NULL;

static void commandHandlerTask(void *pvParameters);
static TaskHandle_t xCommandHandlerTask = NULL;

static void timerCallback(TimerHandle_t xTimer);
TimerHandle_t AutoReloadTimer;


// --------------------------------------------------------------------------- //
// ---------------------------------- Setup ---------------------------------- //
// --------------------------------------------------------------------------- //

void setup() {
  
  ImAliveBeeps();

  // ----- Serial port and Bluetooth Serial ----- //
  Serial.begin(SERIAL_BAUD_RATE);
  serialCommand.reserve(6);  // Reserves 6 bytes in memory for the string
  SerialBT.begin(BLUETOOTH_DEVICE_NAME);

  // ----- SPI initialization ----- //
  radio_spi = new SPIClass(VSPI);
  flash_spi = new SPIClass(HSPI);

  radio_spi->begin(RADIO_SCLK, RADIO_MISO, RADIO_MOSI, RADIO_SS);  //SCLK, MISO, MOSI, SS
  flash_spi->begin(FLASH_SCLK, FLASH_MISO, FLASH_MOSI, FLASH_SS);  //SCLK, MISO, MOSI, SS

  // Set up slave select pins as outputs as the Arduino API
  // Doesn't handle automatically pulling SS low, so each library (radio/flash) handles it idenpendetly
  pinMode(radio_spi->pinSS(), OUTPUT);  //VSPI SS
  pinMode(flash_spi->pinSS(), OUTPUT);  //HSPI SS

  // Serial.print("\n\nSCK:");
  // Serial.println(flash_spi->pinSCK());
  // Serial.print("\nMISO:");
  // Serial.println(flash_spi->pinMISO());
  // Serial.print("\nNOSI:");
  // Serial.println(flash_spi->pinMOSI());
  // Serial.print("\nSS:");
  // Serial.println(flash_spi->pinSS());

  // ----- Outputs ---- //
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(RADIO_NRST, OUTPUT);

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BLUE_LED, LOW);
  rgb_led_params.stop_please = false;  // Start it with false so it runs ok the fisrt time around.

  // ----- Inputs and Interrupts ----- //
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(RADIO_DIO0, INPUT);
  pinMode(RADIO_DIO1, INPUT);

  attachInterrupt(digitalPinToInterrupt(RADIO_DIO0), dealWithDIO0, RISING);
  attachInterrupt(digitalPinToInterrupt(BUTTON), dealWithButton, FALLING);

  // ----- Battery Management Settings ----- //
  TTGO_AXP192.begin(false, true, false, false, false, false);  // Turns off the GPS power supply.
  TTGO_AXP192.SetChargeCurrent(CURRENT_700MA);                 // 700 mA - I wrote down the values on the function.

  // ----- Flash Initialization ----- //
  flash = new SPIFlash(FLASH_SS, flash_spi);  //Use this constructor if using an SPI bus other than the default SPI. Only works with chips with more than one hardware SPI bus
  flash->begin();

  // ----- Tasks creation ----- //
  xTaskCreatePinnedToCore(dataHandlerTask, "dataHandlerTask", 20000, (void *)&SX127X, 3, &xDataHandlerTask, 0);              // Core 0
  xTaskCreatePinnedToCore(softBlinkRGBTask, "softBlinkRGBTask", 10000, (void *)&rgb_led_params, 2, &xSoftBlinkRGBTask, 0);      // Core 0
  xTaskCreatePinnedToCore(beepsTask, "beepsTask", 2000, (void *)&buzzer_params, 2, &xBeepsTask, 0);                             // Core 0
  xTaskCreatePinnedToCore(goodPacketWarning, "goodPacketWarning", 2000, NULL, 2, &xGoodPacketWarning, 0);                       // Core 0
  xTaskCreatePinnedToCore(badPacketWarning, "badPacketWarning", 2000, NULL, 2, &xBadPacketWarning, 0);                          // Core 0
  xTaskCreatePinnedToCore(flashDumpTask, "flashDumpTask", 5000, NULL, 2, &xFlashDumpTask, 0);                                   // Core 0
  xTaskCreatePinnedToCore(commandHandlerTask, "commandHandlerTask", 5000, (void *)&serialCommand, 2, &xCommandHandlerTask, 0);  // Core 0
  AutoReloadTimer = xTimerCreate("AutoReloadTimer", pdMS_TO_TICKS(2000), pdTRUE, (void *)1, &timerCallback);
  if (xTimerStart(AutoReloadTimer, 10) != pdPASS) Serial.printf("\nTimer start error");
}

void loop() {
}


static void timerCallback(TimerHandle_t xTimer) {

  float BatVoltage = TTGO_AXP192.GetBatVoltage();
  if (BatVoltage > 3.7) {
    standbyColor = 0x003300;
  } else if (BatVoltage < 3.7 && BatVoltage > 3.4) {
    standbyColor = 0x333300;

  } else {
    standbyColor = 0x330000;
  }

  if (!rgb_led_params.busy) {
    rgb_led_params.blink_number = 1;
    rgb_led_params.hex_value = standbyColor;
    rgb_led_params.on_time = 1000;
    rgb_led_params.off_time = 0;
    xTaskNotifyGive(xSoftBlinkRGBTask);
  }
}

void dealWithDIO0() {
  xTaskNotifyGive(xDataHandlerTask);
}

void dealWithButton() {

  if (millis() - LastButtonPress > DebounceInterval) {
    LastButtonPress = millis();

    if (rgb_led_params.busy) {
      rgb_led_params.stop_please = true;  // Stops it if it's already running
      xTaskAbortDelay(xSoftBlinkRGBTask);
    }
    rgb_led_params.blink_number = 1;
    rgb_led_params.hex_value = 0xFFFFFF;
    rgb_led_params.on_time = 1000;
    rgb_led_params.off_time = 100;
    xTaskNotifyGive(xSoftBlinkRGBTask);
  }

}


/*
  SerialEvent occurs whenever a new data comes in the hardware serial RX. This
  routine is run between each time loop() runs, so using delay inside loop can
  delay response. Multiple bytes of data may be available.
*/
void serialEvent() {
  while (Serial.available()) {

    char inChar = (char)Serial.read();

    if (inCommand == false) {
      if (inChar == '$') inCommand = true;

    } else {
      if (charCounter < 4) {
        serialCommand += inChar;
        charCounter++;
      } else {
        charCounter = 0;
        inCommand = false;
        xTaskNotifyGive(xCommandHandlerTask);
      }
    }
  }
}
