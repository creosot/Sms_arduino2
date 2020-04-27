//This file is generated by VisualGDB and is used to get accurate IntelliSense
#pragma once
#include <Arduino.h>

#define SYSPROGS_ARDUINO_EXPAND_GENERATED_PROTOTYPES \
	void setup(); \
	void loop(); \
	void checkSMS(); \
	bool deleteSMS(uint8_t index); \
	bool delete_all_SMS(); \
	bool readSMS(uint8_t index); \
	String tel_number_receiveSMS(String header); \
	uint16_t count_receiveSMS(); \
	void smsInit(); \
	bool checkNetwork(); \
	bool initModem(); \
	void testAtCommand(); \

