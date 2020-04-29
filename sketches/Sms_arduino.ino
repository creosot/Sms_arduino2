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

#define LENGTH_SMS_BUFFER	10
#define ROTATE_ON	0 //inverted
#define ROTATE_OFF_AC_OFF	1
#define ROTATE_OFF_AC_ON	2

#include <avr/eeprom.h>
#include "TinyGsmClient.h"

bool initModem(void);
bool checkNetwork(void);
void smsInit(void);
bool readSMS(uint8_t);
uint16_t count_receiveSMS(void);
bool deleteSMS(uint8_t); 
bool delete_all_SMS(void);
String tel_number_receiveSMS(String);
void testAtCommand(void);

struct Provider
{
	const char *apn;
	const char *user;
	const char *pass;	
}
;
struct Provider beeline = { "internet.beeline.ru", "beeline", "beeline" };
struct Provider mts = { "internet.mts.ru", "mts", "mts" };
struct Provider megafon = { "internet", "", "" };

const char * apn;
const char * user;
const char * pass;

//const char server[] = "18.190.110.76"; //AWS
//const int  port = 8086;
const char server[] = "84.204.24.100";   //spb
const int  port = 53817;
uint8_t countCheckNetwork = 0;
int rotate = ROTATE_OFF_AC_ON;
String operator_apn;
String imei;
String gid;
String iccid;
String csq;
uint16_t sec;
uint8_t min;
uint16_t str_len;
uint8_t count_sms = 0;
int temp_indexoff = -1;
#define LENGHT_GID 16
uint8_t EEMEM flag_gid_eeprom;
uint8_t EEMEM gid_eeprom[LENGHT_GID];
char gid_array[LENGHT_GID] = { 0 };
const char* status_str[] = { "vraschenie", "net phase", "net vrascheniz" };
unsigned long measuring_timer = 0;

class MyTinyGsm : public TinyGsm
{
public:
	MyTinyGsm(Stream& stream)
		: TinyGsm(stream) {}
	~MyTinyGsm() {}
	/*
   * Basic functions
   */
	String getModemVersion()
	{
		sendAT(GF("+CGMR"));
		String res;
		if (waitResponse(1000L, res) != 1) {
			return "";
		}
		res.replace(GSM_NL "OK" GSM_NL, "");
		res.replace("_", " ");
		res.trim();
		return res;
	}
	
	String getSimCCID() {
		sendAT(GF("+CCID"));
		String res;
		if (waitResponse(1000L, res) != 1) {
			return "";
		}
		res.replace(GSM_NL "OK" GSM_NL, "");
		res.trim();
		return res;
	}
	
	bool sendSMS(const String& number, const String& text) {
		sendAT(GF("+CMGF=1"));
		waitResponse();
		//Set GSM 7 bit default alphabet (3GPP TS 23.038)
		sendAT(GF("+CSCS=\"GSM\""));
		waitResponse();
		sendAT(GF("+CMGS=\""), number, GF("\""));
		if (waitResponse(GF(">")) != 1) {
			SerialMon.println(F("ERROR: for print SMS"));
			return false;
		}
		SerialMon.print("Send SMS to: ");
		SerialMon.println(number);
		stream.print(text);
		stream.write((char)26);
		SerialMon.print("text: ");
		SerialMon.println(text);
		stream.flush();
		if (waitResponse(60000L) == 1)
		{
			SerialMon.println(F("Ok"));
			return true;
		}
		else
		{
			SerialMon.println(F("ERROR: for sending SMS"));
			return false;
		}
	}

private:
					
};

MyTinyGsm modem(SerialAT);
TinyGsmClient client(modem);

#define modem_on


void setup()
{
	// Set console baud rate
	SerialMon.begin(115200);
	delay(100);
	SerialMon.println("Wait...");
	delay(1000);
	//digitalWrite(PWR_KEY, LOW);
	// Set GSM module baud rate
	SerialAT.begin(115200);
	delay(3000);
	
#ifdef modem_on
	while (initModem() == false || checkNetwork() == false) {
		SerialMon.println();
		delay(3000);
	}
	
#if TINY_GSM_USE_GPRS
	// Unlock your SIM card with a PIN if needed
	if(GSM_PIN && modem.getSimStatus() != 3) {
		modem.simUnlock(GSM_PIN);
	}
#endif
#endif
	
#ifdef bat
	sensors.begin();
#endif
//	for (uint8_t i = 0; i < COUNT_ANALOG_PINS; i++) {
//		analog_measuring(i);
//	}
	smsInit();
	SerialMon.println("------------------------------------------------");
	SerialMon.println("30sec voltage, current, rotation, ACline measure");
	measuring_timer = millis();
}

uint32_t rate = 0;

void loop()
{
	testAtCommand();
}

void checkSMS()
{
	uint16_t c = count_receiveSMS();
	if (!c) return;
	for (; c; --c)
	{
		readSMS(c);
		//deleteSMS(c);
	}
	delete_all_SMS();
}


bool deleteSMS(uint8_t index) 
{
	modem.sendAT(GF("+CMGD="), index, GF(","), 0);   // Delete SMS Message from <mem1> location
	return modem.waitResponse(5000L) == 1;
}

bool delete_all_SMS() 
{
	modem.sendAT(GF("+CMGDA=\"DEL ALL\""));     // Delete ALL SMS Message
	return modem.waitResponse(15000L) == 1;
}

bool readAllSMS()
{
	modem.sendAT(GF("+CMGF=1"));     //Text type messages instead of PDU
	modem.waitResponse();
	modem.sendAT(GF("+CNMI=2,0"));     //Disable messages about new SMS from the GSM module
	modem.waitResponse();
	String data;
	modem.sendAT(GF("+CMGL=\"ALL\""));
	modem.waitResponse(10000L, data, GF(GSM_NL "+CMGL:"));
	
//	for (uint8_t i = 0; i < LENGTH_SMS_BUFFER; i++)
//	{
//		
//		if (modem.waitResponse(10000L, GF(GSM_NL "+CMGL:")))
//		{
//			String header = modem.stream.readStringUntil('\n');
//			String body = modem.stream.readStringUntil('\n');
//			body.trim();
//			SerialMon.println();
//			SerialMon.println(F("Receive SMS:"));
//			SerialMon.print(F("Header: "));
//			SerialMon.println(header);
//			SerialMon.print(F("Body: "));
//			SerialMon.println(body);
//		
//		}
//		else
//		{
//			SerialMon.println(F("not receive SMS"));
//			return false;
//		}
//	}
	return false;
}

bool readSMS(uint8_t index)
{
	modem.sendAT(GF("+CMGF=1"));    //Text type messages instead of PDU
	modem.waitResponse();
	modem.sendAT(GF("+CNMI=2,0"));    //Disable messages about new SMS from the GSM module
	modem.waitResponse();
	modem.sendAT(GF("+CMGR="), index);
	if (modem.waitResponse(10000L, GF(GSM_NL "+CMGR:"))) 
	{
		String header = modem.stream.readStringUntil('\n');
		String body = modem.stream.readStringUntil('\n');
		body.trim();
		SerialMon.println();
		SerialMon.println(F("Receive SMS:"));
		SerialMon.print(F("Header: "));
		SerialMon.println(header);
		SerialMon.print(F("Body: "));
		SerialMon.println(body);
		temp_indexoff = body.indexOf("@");
		if (temp_indexoff != -1)
		{
			if (imei.equals(body.substring(0, temp_indexoff)))
			{
				SerialMon.println(F("imei Ok"));
				
				temp_indexoff = body.indexOf("status");  //imei@status
				if(temp_indexoff != -1)
				{
					SerialMon.println("receive status command");
					String tel = tel_number_receiveSMS(header);
					if (tel.length() > 7)
					{
						modem.sendSMS(tel, "GID=" + gid + "; imei=" + imei + "; status=" + status_str[rotate]);
						return true;
					}
					else
					{
						return false;
					}
				}
				
				temp_indexoff = body.indexOf("gid=");  //imei@gid=newgid
				if(temp_indexoff != -1)
				{
					uint8_t len = body.length() - (temp_indexoff + 4);  //+ 4(gid=)
					if(len > 3 && len < LENGHT_GID)
					{
						for (uint8_t i = 0; i < LENGHT_GID; i++)
						{
							gid_array[i] = 0;
						}
						gid = body.substring((temp_indexoff + 4), body.length());
						gid.toCharArray(gid_array, LENGHT_GID);
						for (uint8_t i = 0; i < LENGHT_GID; i++)
						{
							eeprom_write_byte((gid_eeprom + i), gid_array[i]);
						}
						eeprom_write_byte(&flag_gid_eeprom, 'r');
						SerialMon.print(F("new GID="));
						SerialMon.println(gid);
						String tel = tel_number_receiveSMS(header);
						if (tel.length() > 7)
						{
							modem.sendSMS(tel, "new GID=" + gid);
							return true;
						}
						else
						{
							return false;
						}
					}
					else
					{
						SerialMon.println(F("ERRROR: not valid size gid in SMS"));
						return false;
					}
				}
				else
				{
					SerialMon.println(F("ERRROR: not valid gid in SMS"));
					return false;
				}
			}
			else
			{
				SerialMon.println(F("ERRROR: not valid imei in SMS"));
				return false;
			}
		}
	}
	return true;
}

String tel_number_receiveSMS(String header)
{
	String tel;
	temp_indexoff = header.indexOf("\",\"+");
	if (temp_indexoff != -1)
	{
		tel = header.substring(temp_indexoff + 3, header.length());  //start to +
		temp_indexoff = tel.indexOf("\",\"");
		if (temp_indexoff != -1)
		{
			return tel.substring(0, temp_indexoff);
		}
		else
		{
			SerialMon.print(F("ERROR: not valid lenght number telefon"));
			return tel = "";
		}
	}
	else
	{
		return tel = "";
	}
}

uint16_t count_receiveSMS()
{
	uint16_t c = 0;
	String res;
	res.reserve(50);
	modem.waitResponse();
	modem.sendAT(GF("+CPMS?"));
	if (modem.waitResponse(10000L, GF("+CPMS:"))) 
	{
		res = modem.stream.readStringUntil('\n');
		temp_indexoff = res.indexOf("SM_P");
		if (temp_indexoff != -1)
		{
			c = res.substring(temp_indexoff + 6, temp_indexoff + 8).toInt();
		}
	}
	modem.waitResponse();
	return c;
}

void smsInit()
{
	modem.sendAT(GF("+CPMS=SM,SM,SM"));   //Storage to store SMS
	modem.waitResponse();
	modem.sendAT(GF("+CMGF=1"));   //Text type messages instead of PDU
	modem.waitResponse();
	modem.sendAT(GF("+CNMI=2,0"));   //Disable messages about new SMS from the GSM module
	modem.waitResponse();
	modem.sendAT(GF("+CSCS=\"GSM\""));  //GSM 7 bit default alphabet
	modem.waitResponse();
	modem.sendAT(GF("+CMGDA=\"DEL ALL\""));  //delete all sms
	modem.waitResponse();
	//modem.sendSMS("+79037450251", "init");
}

bool checkNetwork() {
	while (uint8_t status = modem.getRegistrationStatus() != REG_OK_HOME)
	{
		switch (status)
		{
		case REG_OK_ROAMING: SerialMon.println(F("REGISTRATION STATUS: Registered, roaming"));
			break;
		case REG_UNKNOWN: SerialMon.println(F("REGISTRATION STATUS: Network unknown"));
			break;
		case REG_DENIED: SerialMon.println(F("REGISTRATION STATUS: Registration denied"));
			break;
		case REG_SEARCHING: SerialMon.println(F("REGISTRATION STATUS: Not registered"));
			break;
		default: SerialMon.println(F("REGISTRATION STATUS: Network Registration"));
			break;
		}
		++countCheckNetwork;
		SerialMon.println(countCheckNetwork);
		if (countCheckNetwork == 25)
		{
			SerialMon.println(F("ERROR: Network Registration failed"));
			countCheckNetwork = 0;
			return false;
		}
		delay(2000);
	}
	SerialMon.println(F("REGISTERED, Home network"));
	SerialMon.print(F("OPERATOR APN: "));
	operator_apn = modem.getOperator();
	SerialMon.println(operator_apn);
	operator_apn.replace(" ", "");
	operator_apn.toLowerCase();
	if (operator_apn.indexOf("beeline") != -1)
	{
		apn  = beeline.apn;
		user = beeline.user;
		pass = beeline.pass;
	} 
	else if (operator_apn.indexOf("mts") != -1)
	{
		apn  = mts.apn;
		user = mts.pass;
		pass = mts.user;
	}
	else if (operator_apn.indexOf("megafon") != -1)
	{
		apn  = megafon.apn;
		user = megafon.pass;
		pass = megafon.user;
	}
	else
	{
		SerialMon.println(F("ERROR: OPERATOR APN not valid"));
		return false;
	}
	SerialMon.print(F("apn: "));
	SerialMon.println(apn);
	SerialMon.print(F("user: "));
	SerialMon.println(user);
	SerialMon.print(F("pass: "));
	SerialMon.println(pass);
	csq = modem.getSignalQuality();
	SerialMon.print("Signal Quality: ");
	SerialMon.println(csq);
	return true;
}

bool initModem() {
	SerialMon.println(F("Initializing modem..."));
	modem.restart();
	// modem.init();

	String modemInfo = modem.getModemInfo();
	SerialMon.print(F("Modem Name: "));
	SerialMon.println(modemInfo);
	SerialMon.print(F("Modem "));
	SerialMon.println(modem.getModemVersion());
	imei = modem.getIMEI();
	gid = imei;
	SerialMon.print(F("Modem IMEI: "));
	SerialMon.println(imei);
	if (5 < imei.length() && imei.length() < (LENGHT_GID)) //864626047424541
		{
			for (uint8_t i = 0; i < imei.length(); i++)
			{
				if (isdigit(imei[i]) == false)
				{
					SerialMon.println(F("ERROR: Modem IMEI not valid"));
					return false;
				}
			}
		
			byte flg = eeprom_read_byte(&flag_gid_eeprom);
			if (flg == 'r')
			{
				for (uint8_t i = 0; i < LENGHT_GID; i++)
				{
					gid_array[i] = 0;
				}
				for (uint8_t i = 0; i < LENGHT_GID; i++)
				{
					gid_array[i] = eeprom_read_byte(gid_eeprom + i);
				}
				gid = String(gid_array);
				SerialMon.print(F("GID(over SMS): "));
			}
			else if (flg == 'd')
			{
				SerialMon.print(F("default GID: "));
				gid = imei;
			}
			else
			{
				eeprom_write_byte(&flag_gid_eeprom, 'd');
				SerialMon.print(F("no GID. set default GID: "));
				gid = imei;
			}
			SerialMon.println(gid);
		} 
	else
	{
		SerialMon.println(F("ERROR: Modem IMEI not valid"));
		return false;
	}
	
	switch (modem.getSimStatus())
	{
		//case 3:
		case SIM_LOCKED : SerialMon.println(F("ERROR: insert unlocked SIM Card"));
		return false;
	case SIM_READY:
		break;
	case SIM_ERROR:
	default: SerialMon.println(F("ERROR: no SIM Card"));
		return false;
	}
	SerialMon.println(F("SIM Card: READY"));
	iccid = modem.getSimCCID();
	if (iccid.length() == 0)
	{
		iccid = "non";
	}
	SerialMon.print(F("SIM Card ICCID: "));
	SerialMon.println(iccid);
	return true;
}

void testAtCommand() {
	if (!rate) {
		rate = TinyGsmAutoBaud(SerialAT);
	}
	
	if (!rate) {
		SerialMon.println(F("***********************************************************"));
		SerialMon.println(F(" Module does not respond!"));
		SerialMon.println(F("   Check your Serial wiring"));
		SerialMon.println(F("   Check the module is correctly powered and turned on"));
		SerialMon.println(F("***********************************************************"));
		delay(30000L);
		return;
	}
	
	SerialAT.begin(rate);
	
	// Access AT commands from Serial Monitor
	SerialMon.println(F("***********************************************************"));
	SerialMon.println(F(" You can now send AT commands"));
	SerialMon.println(F(" Enter \"AT\" (without quotes), and you should see \"OK\""));
	SerialMon.println(F(" If it doesn't work, select \"Both NL & CR\" in Serial Monitor"));
	SerialMon.println(F("***********************************************************"));
	
	while (true) {
		if (SerialAT.available()) {
			SerialMon.write(SerialAT.read());
		}
		if (SerialMon.available()) {
			SerialAT.write(SerialMon.read());
		}
		delay(0);
	}
}
