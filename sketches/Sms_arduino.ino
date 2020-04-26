#define TINY_GSM_MODEM_SIM800
#define SerialMon Serial
#define SerialAT Serial1
#define TINY_GSM_RX_BUFFER 650
//#define DUMP_AT_COMMANDS
#define TINY_GSM_DEBUG SerialMon // Define the serial console for debug prints, if needed
//#define LOGGING  // <- Logging is for the HTTP library
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#define GSM_PIN ""
#include "TinyGsmClient.h"

void setup()
{
	pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
	digitalWrite(LED_BUILTIN, HIGH);
	delay(1000);
	digitalWrite(LED_BUILTIN, LOW);
	delay(1000);
}
