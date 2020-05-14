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

#define LENGHT_GID 32
#define LENGTH_SMS_BUFFER	10
#define ROTATE_ON	0 //inverted
#define ROTATE_OFF_AC_OFF	1
#define ROTATE_OFF_AC_ON	2

#include <avr/eeprom.h>
#include "TinyGsmClient.h"

typedef struct sms_node {
	struct sms_node *next;
	uint8_t command;
	uint8_t mode;
	const char* data_str;
	const char* tel;
} sms_node_t;

typedef struct stack {
	int size;
	sms_node_t *head;
	sms_node_t *tail;
} stack_t;

stack_t sms_nodes = {0, NULL, NULL};
stack_t *ptr_sms_nodes = &sms_nodes;

const char *status = "status";

enum CmdStatus
{
	CMD_STATUS = 1,
	CMD_MODE = 2,
	CMD_GID2 = 3,
	CMD_GID = 4,
};

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
uint8_t gid_rotate_status = ROTATE_OFF_AC_ON;
uint8_t gid2_rotate_status = ROTATE_OFF_AC_ON;
String operator_apn;
String imei;
String gid;
String gid2;
String iccid;
String csq;
uint8_t mode = 1;
uint16_t sec;
uint8_t min;
uint16_t str_len;
uint8_t count_sms = 0;
int from_indexoff = -1;
int to_indexoff = -1;
uint8_t EEMEM mode_eeprom;
uint8_t EEMEM flag_gid_eeprom;
uint8_t EEMEM gid_eeprom[LENGHT_GID];
uint8_t EEMEM flag_gid2_eeprom;
uint8_t EEMEM gid2_eeprom[LENGHT_GID];
char gid_array[LENGHT_GID] = { 0 };
const char* rotate_status[] = { "vraschenie", "net phase", "net vrascheniz" };
unsigned long measuring_timer = 0;

bool initModem(void);
bool checkNetwork(void);
void smsInit(void);
bool readSMS(uint8_t);
uint16_t count_receiveSMS(void);
bool delete_SMS(uint8_t); 
bool delete_all_SMS(void);
String tel_number_receiveSMS(String);
bool read_all_SMS();
bool prepare_SMS(String header, String body);
void testAtCommand();
void node_push(stack_t *s, uint8_t cmd, int d_int, const char *d_str, const char *t);
void prepare_Stack_SMS(sms_node_t *node);
void write_GID(const char* gid_name);
void read_GID();
void write_GID2(const char* gid_name);
void read_GID2();
void read_Mode();
void write_Mode(uint8_t m);
void send_Status(const char* tel);

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
	
	bool sendSMS(const String &number, const String &text) {
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
	//testAtCommand();
	read_all_SMS();
	delay(30000);
	
}

bool delete_SMS(uint8_t index) 
{
	modem.sendAT(GF("+CMGD="), index, GF(","), 0);   // Delete SMS Message from <mem1> location
	return modem.waitResponse(5000L) == 1;
}

bool delete_all_SMS() 
{
	modem.sendAT(GF("+CMGDA=\"DEL ALL\""));     // Delete ALL SMS Message
	return modem.waitResponse(15000L) == 1;
}

bool read_all_SMS()
{
	modem.sendAT(GF("+CMGF=1"));     //Text type messages instead of PDU
	if(modem.waitResponse(10000L) != 1) {
		return false;
	}
	modem.sendAT(GF("+CNMI=2,0"));     //Disable messages about new SMS from the GSM module
	if(modem.waitResponse(10000L) != 1) {
		return false;
	}
	modem.sendAT(GF("+CMGL=\"ALL\""));
	for (uint8_t i = 0; i < LENGTH_SMS_BUFFER; i++)
	{
		uint8_t res = modem.waitResponse(10000L, GF("ERROR" GSM_NL), GF(GSM_NL "+CMGL:"), GF("OK" GSM_NL));
		if (!res)
		{
			SerialMon.println(F("ERROR: Read SMS unhandled data"));
			delete_all_SMS();
			return false;
		}
		if (res == 1)
		{
			SerialMon.println(F("ERROR: Read SMS"));
			delete_all_SMS();
			return false;
		}
		if (res == 2)
		{
			String header = modem.stream.readStringUntil('\n');
			String body = modem.stream.readStringUntil('\n');
			body.trim();
			SerialMon.println();
			SerialMon.print("count: ");
			SerialMon.println(i + 1);
			SerialMon.println(F("Receive SMS:"));
			SerialMon.print(F("Header: "));
			SerialMon.println(header);
			SerialMon.print(F("Body: "));
			SerialMon.println(body);
			prepare_SMS(header, body);
		}
		if (res == 3)
		{
			SerialMon.println();
			SerialMon.println(F("All receive SMS reading"));
			return true;
		}
	}
	return false;
}

bool prepare_SMS(String header, String body)
{
	//header
	header.trim();
	from_indexoff = header.indexOf(F(",\"REC"));
	if (from_indexoff == -1 || from_indexoff == 0)
	{
		return false;
	}
	int index = header.substring(0, from_indexoff).toInt();
	SerialMon.println();
	SerialMon.print("sms index: ");
	SerialMon.println(index);
	from_indexoff = header.indexOf(F("READ\",\""));
	if (from_indexoff == -1)
	{
		return false;
	}
	from_indexoff += 7; //length READ","
	to_indexoff = header.indexOf(F("\",\""), from_indexoff);
	if (to_indexoff == -1)
	{
		return false;
	}
	//tel
	String tel = header.substring(from_indexoff, to_indexoff);
	SerialMon.print("tel: ");
	SerialMon.println(tel);
	//body
	body.trim();
	//find imei
	to_indexoff = body.indexOf("@");
	if (imei.equals(body.substring(0, to_indexoff)) == false)
	{
		SerialMon.println(F("ERRROR: not valid imei, SMS delete"));
		goto exit;
	}
	SerialMon.println(F("imei Ok"));
	//find command
	from_indexoff = to_indexoff + 1; //@
	//status
	to_indexoff = body.indexOf("status", from_indexoff);
	if (to_indexoff != -1)
	{
		SerialMon.println(F("receive command status"));
		node_push(ptr_sms_nodes, CMD_STATUS, 0, NULL, tel.c_str());
		return true;
	}
	//mode
	to_indexoff = body.indexOf("mode=", from_indexoff);
	if (to_indexoff != -1)
	{
		int len_data = body.length() - (to_indexoff + 5);  //legth mode=
		if(len_data == 0 || len_data > 1)
		{
			SerialMon.println(F("ERROR: mode command not valid length"));
			goto exit;
		}
		char ch = body.charAt(from_indexoff + 5);
		if (ch == '1' || ch == '2')
		{
			SerialMon.print(F("receive command mode="));
			SerialMon.println(ch);
			uint8_t mode_number = ch - '0';
			node_push(ptr_sms_nodes, CMD_MODE, mode_number, NULL, tel.c_str());
			return true;
		}
		else
		{
			SerialMon.println(F("ERROR: mode command not valid data"));
			goto exit;
		}
	}
	//gid2
	to_indexoff = body.indexOf("gid2=", from_indexoff);
	if (to_indexoff != -1)
	{
		SerialMon.println(F("receive command gid2="));
		from_indexoff = to_indexoff + 5; //legth gid2=
		String gid2 = body.substring(from_indexoff, body.length());
		node_push(ptr_sms_nodes, CMD_GID2, 0, gid2.c_str(), tel.c_str());
		return true;
	}
	//gid
	to_indexoff = body.indexOf("gid=", from_indexoff);
	if (to_indexoff != -1)
	{
		SerialMon.println(F("receive command gid="));
		from_indexoff = to_indexoff + 4;  //legth gid=
		String gid = body.substring(from_indexoff, body.length());
		node_push(ptr_sms_nodes, CMD_GID, 0, gid.c_str(), tel.c_str());
		return true;
	}
	SerialMon.println(F("ERROR: command not found, SMS delete"));
exit:
	delete_SMS(index);
	return false;
}

void node_push(stack_t *s, uint8_t cmd, int d_int, const char *d_str, const char *t)
{
	sms_node_t *node = (sms_node_t *) malloc(sizeof(sms_node_t));
	if (node == NULL)
	{
		return;
	}
	node->command = cmd;
	node->mode = d_int;
	node->data_str = d_str;
	node->tel = t;
	node->next = s->head;
	s->head = node;
	s->size += 1;
}

void node_pop(stack *s)
{
	if (s->size == 0)
	{
		return;
	}
	sms_node_t *node = s->head;
	s->head = node->next;
	prepare_Stack_SMS(node);
	free(node);
	s->size -= 1;
	if (s->size == 0)
	{
		s->head = NULL;
		s->tail = NULL;
	}
}

void prepare_Stack_SMS(sms_node_t *node)
{
	switch (node->command)
	{
	case CMD_STATUS:
		send_Status(node->tel);
		break;
	case CMD_MODE:
		write_Mode(node->mode);
		send_Status(node->tel);
		break;
	case CMD_GID2:
		write_GID2(node->data_str);
		send_Status(node->tel);
		break;
	case CMD_GID:
		write_GID(node->data_str);
		send_Status(node->tel);
		break;
	default:
		break;
	}
}

void send_Status(const char* tel)
{
	if (mode == 1)
	{
		modem.sendSMS(tel, "gid=" + gid + "; status=" + rotate_status[gid_rotate_status] + "; mode=" + mode + "; imei=" + imei); 
	}
	else
	{
		modem.sendSMS(tel, "gid=" + gid + "; status=" + rotate_status[gid_rotate_status] + \
			"gid2=" + gid2 + "; status=" + rotate_status[gid2_rotate_status] + \
			"; mode=" + mode + "; imei=" + imei);
	}
}

void read_Mode()
{
	uint8_t mode_number = eeprom_read_byte(&mode_eeprom);
	if (mode_number == 1 || mode_number == 2)
	{
		mode = mode_number; 
	}
	else
	{
		write_Mode(1);
	}
}

void write_Mode(uint8_t mode_number)
{
	mode = mode_number;
	eeprom_write_byte(&mode_eeprom, mode_number);
}

void read_GID()
{
	byte flg = eeprom_read_byte(&flag_gid_eeprom);
	if (flg == 'w')
	{
		for (uint8_t i = 0; i < LENGHT_GID; i++)
		{
			gid_array[i] = eeprom_read_byte(gid_eeprom + i);
			if (gid_array[i] == '\0')
			{
				break;
			}
		}
		gid_array[LENGHT_GID - 1] = '\0';
		gid = String(gid_array);
		SerialMon.print(F("GID(over SMS): "));
	}
	else
	{
		SerialMon.print(F("no GID. set default GID: "));
		gid = imei;
	}
}

void read_GID2()
{
	uint8_t flg = eeprom_read_byte(&flag_gid2_eeprom);
	if (flg == 'w')
	{
		for (uint8_t i = 0; i < LENGHT_GID; i++)
		{
			gid_array[i] = eeprom_read_byte(gid2_eeprom + i);
			if (gid_array[i] == '\0')
			{
				break;
			}
		}
		gid_array[LENGHT_GID - 1] = '\0';
		gid2 = String(gid_array);
		SerialMon.print(F("GID2(over SMS): "));
	}
	else
	{
		SerialMon.print(F("no GID2. set default GID2: "));
		gid2 = imei;
	}
}

void write_GID(const char* gid_name)
{
	uint8_t i = 0;
	for (; i < strlen(gid_name); i++)
	{
		if (i == (LENGHT_GID - 1)) //31
		{
			break;
		}
		eeprom_write_byte((gid_eeprom + i), gid_name[i]);
	}
	eeprom_write_byte((gid_eeprom + i), '\0');
	eeprom_write_byte(&flag_gid_eeprom, 'w');
	gid = String(gid_name);
	SerialMon.print(F("new GID="));
	SerialMon.println(gid);
}

void write_GID2(const char* gid_name)
{
	uint8_t i = 0;
	for (; i < strlen(gid_name); i++)
	{
		if (i == (LENGHT_GID - 1)) //31
			{
				break;
			}
		eeprom_write_byte((gid2_eeprom + i), gid_name[i]);
	}
	eeprom_write_byte((gid2_eeprom + i), '\0');
	eeprom_write_byte(&flag_gid2_eeprom, 'w');
	gid2 = String(gid_name);
	SerialMon.print(F("new GID2="));
	SerialMon.println(gid2);
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
			read_GID();
			read_GID2();
			read_Mode();
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


