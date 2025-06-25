// Install these libraries:
// - "OneWire" by Jim Studt, Tom Pollard, Robin James...
// - "DallasTemperature" by Miles Burton, Tim Newsome, ...
// - "Seeed_Arduino_CAN": https://github.com/Seeed-Studio/Seeed_Arduino_CAN

#include <SPI.h>
#include "mcp2515_can.h"

#include "OneWire.h"
#include "DallasTemperature.h"

#define CAN_BAUDRATE CAN_500KBPS
#define SERIAL_BAUDRATE 115200
#define SENDING_CAN_PERIOD 1000 // Sending to CAN bus every 1s
#define BOARD_ID 0x10C

#define SENSOR_1_ONEWIRE_BUS 3
#define SENSOR_2_ONEWIRE_BUS 4

// Comment this to disable SERIAL logging
// Must be commented when using for production, otherwise, the Arduino will keep waiting for Serial forever
#define USE_SERIAL

#ifdef USE_SERIAL
#define DBG(str) SERIAL_PORT_MONITOR.println("[DBG] " str)
#define ERR(str) SERIAL_PORT_MONITOR.println("[ERR] " str)
#define WARN(str) SERIAL_PORT_MONITOR.println("[ERR] " str)
#define PRINTLN(str) SERIAL_PORT_MONITOR.println(str)
#define PRINT(str) SERIAL_PORT_MONITOR.print(str)
#else
#define DBG(str)
#define ERR(str)
#define WARN(str)
#define PRINTLN(str)
#define PRINT(str)
#endif

typedef union {
  float f;
  uint8_t bytes[4];
} FloatBytes;

// For Arduino MCP2515 Hat:
// the cs pin of the version after v1.1 is default to D9
// v0.9b and v1.0 is default D10
const int SPI_CS_PIN = 9;
const int CAN_INT_PIN = 2;

const int ONE_WIRE_BUS = 3;

mcp2515_can CAN(SPI_CS_PIN);  // Set CS pin

// Create a new instance of the oneWire class to communicate with any OneWire device:
OneWire sensor1OneWire(SENSOR_1_ONEWIRE_BUS);
OneWire sensor2OneWire(SENSOR_2_ONEWIRE_BUS);
OneWire oneWire(ONE_WIRE_BUS);

// Pass the oneWire reference to DallasTemperature library:
DallasTemperature sensorsGroup1(&sensor1OneWire);
DallasTemperature sensorsGroup2(&sensor2OneWire);
DallasTemperature sensors(&oneWire);

// CAN message
unsigned char stmp[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float tempGroup1 = -1000.0f; // in C
float tempGroup2 = -1000.0f; // in C

// Perform reading temperatures from sensors and assign reading data to variables
void perform_read_temperature();

// Send temp data to CAN bus
void send_temp_to_can();

void setup() {
#ifdef USE_SERIAL
  // put your setup code here, to run once:
  SERIAL_PORT_MONITOR.begin(SERIAL_BAUDRATE);

  while (!Serial) {};

  DBG("Serial init ok! Please disable this feature when in production");
#endif

  // Init CAN
  while (CAN_OK != CAN.begin(CAN_BAUDRATE)) {  // init can bus : baudrate = 500k
    DBG("CAN init fail, retry...");
    delay(100);
  }

  DBG("CAN init ok!");

  // Start up the library:
  sensorsGroup1.begin();
  sensorsGroup2.begin();

  DBG("Temperature Groups started!");
}

void loop() {
  perform_read_temperature();

  send_temp_to_can();

  delay(SENDING_CAN_PERIOD);
}

void perform_read_temperature() {
  // Send the command for all devices on the bus to perform a temperature conversion:
  sensorsGroup1.requestTemperatures();
  sensorsGroup2.requestTemperatures();

  // Fetch the temperature in degrees Celsius
  tempGroup1 = sensorsGroup1.getTempCByIndex(0); // the index 0 refers to the first device // for now we only have one device
  tempGroup2 = sensorsGroup2.getTempCByIndex(0); // the index 0 refers to the first device // for now we only have one device

  // Show results to serial
  PRINT("[DBG] [TEMPERATURE GROUP #1]: ");
  PRINT(tempGroup1);
  PRINT(" \xC2\xB0");  // shows degree symbol
  PRINT("C\n");

  PRINT("[DBG] [TEMPERATURE GROUP #2]: ");
  PRINT(tempGroup2);
  PRINT(" \xC2\xB0");  // shows degree symbol
  PRINT("C\n");
}

void send_temp_to_can() {
  // Pack float data to the 4 bytes
  FloatBytes temp1, temp2;

  temp1.f = tempGroup1;
  temp2.f = tempGroup2;

  // Send data of the two sensors
  // Data of the first sensor will be in the first 4 bytes
  // Data of the second sensor will be in the last 4 bytes
  // Assign the float to the next 4 bytes of the CAN message
  for (int i = 0; i < 4; ++i) {
    stmp[i] = temp1.bytes[i];
  }

  for (int i = 4; i < 8; ++i) {
    stmp[i] = temp2.bytes[i - 4];
  }

  // send data
  CAN.sendMsgBuf(BOARD_ID, 0, 8, stmp);

  DBG("[CAN BUS] Send sensor ok!");
}
