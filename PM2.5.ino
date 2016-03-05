#include "Arduino.h"
#include "serialReadPMValue.h"
#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility\debug.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define receiveDatIndex 32
#define WiDo_IRQ  7
#define WiDo_VBAT  5
#define WiDo_CS  10

#if defined(ARDUINO) && ARDUINO >= 100
#define printByte(args)  write(args);
#else
#define printByte(args)  print(args,BYTE);
#endif

LiquidCrystal_I2C lcd(0x20, 16, 2);

uint8_t receiveDat[receiveDatIndex];
uint32_t ip = 0;

Adafruit_CC3000 WiDo = Adafruit_CC3000(WiDo_CS, WiDo_IRQ, WiDo_VBAT, SPI_CLOCK_DIVIDER);

#define WLAN_SSID     "SSID_Name"  // use your own ssid name
#define WLAN_PASS     "Password"   // use your own password
#define WLAN_SECURITY WLAN_SEC_WPA2
#define TCP_TIMEOUT 3000
#define website       "www.lewei50.com"
#define userkey		  "UserKey"     // use your own userkey

void setup()
{
	Serial.begin(115200);
	Serial1.begin(9600);
	Serial.println(F("Hello, Wido!\n"));
	Serial.println(F("\nInitialising the CC3000 ..."));

	lcd.init();
	lcd.backlight();
	lcd.home();

	if (!WiDo.begin())
	{
		Serial.println(F("Unable to initialise the CC3000!"));
		while (1);
	}
	
	char *ssid = WLAN_SSID;
	Serial.print(F("\nAttempting to connect to "));
	Serial.println(ssid);

	if (!WiDo.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY))
	{
		Serial.println(F("Failed!"));
		while (1);
	}
	
	Serial.println(F("Connected!"));
	Serial.println(F("Request DHCP"));
	
	while (!WiDo.checkDHCP())
	{
		delay(100);
	}
}

        float PM01Value = 0;
        float PM2_5Value = 0;
        float PM10Value = 0;

void loop()
{
	static Adafruit_CC3000_Client WidoClient;
	static unsigned long RetryMillis = 0;
	static unsigned long uploadStamp = 0;
	static unsigned long sensorSamp = 0;
	
	int length = serialRead(Serial1, receiveDat, receiveDatIndex, 5);
	int checkSum = checkValue(receiveDat, receiveDatIndex);
	
	if (length&&checkSum);
	{
		PM01Value = transmitPM01(receiveDat);
		PM2_5Value = transmitPM2_5(receiveDat);
		PM10Value = transmitPM10(receiveDat);
	}
	static unsigned long OledTimer = millis();
	if (millis() - OledTimer >= 15000)
	{
		OledTimer = millis();
		
		Serial.print("PM1.0: ");
		Serial.print(PM01Value);
		Serial.println("  ug/m3");

		Serial.print("PM2.5: ");
		Serial.print(PM2_5Value);
		Serial.println("  ug/m3");

		Serial.print("PM10: ");
		Serial.print(PM10Value);
		Serial.println("  ug/m3");

		lcd.clear();
                lcd.print("PM1.0: ");
		lcd.print(PM01Value);
		lcd.setCursor(0, 1);
		lcd.print("PM2.5: ");
		lcd.print(PM2_5Value);
	}
	if (!WidoClient.connected() && millis() - RetryMillis > TCP_TIMEOUT)
	{
		RetryMillis = millis();
		Serial.println(F("Try to connect the cloud sever"));
		WidoClient.close();
		Serial.print(F("www.lewei50.com"));
		while (ip == 0)
		{
			if (!WiDo.getHostByName(website, &ip))
			{
				Serial.println(F("Couldn't resolve!"));
			}
			delay(500);
		}
		WiDo.printIPdotsRev(ip);
		Serial.println(F(""));
		WidoClient = WiDo.connectTCP(ip, 80);
	}
	
	if (WidoClient.connected() && millis() - uploadStamp > 15000)
	{
		uploadStamp = millis();
		int length = 0;
		char lengthstr[3] = "";
		char httpPackage[100] = "";
		
		strcat(httpPackage, "[{\"Name\":\"PM1\",");
		strcat(httpPackage, "\"Value\":\"");
		itoa(PM01Value, httpPackage + strlen(httpPackage), 10);
		strcat(httpPackage, "\"},{\"Name\":\"PM2.5\",");
		strcat(httpPackage, "\"Value\":\"");
		itoa(PM2_5Value, httpPackage + strlen(httpPackage), 10);
		strcat(httpPackage, "\"},{\"Name\":\"PM10\",");
		strcat(httpPackage, "\"Value\":\"");
		itoa(PM10Value, httpPackage + strlen(httpPackage), 10);
		strcat(httpPackage, "\"}]");
		length = strlen(httpPackage);
		itoa(length, lengthstr, 10);
		Serial.print(F("Length = "));
		Serial.println(length);

		Serial.println(F("Connected to Lewei sever. "));
		Serial.println(F("Sending headers"));
		WidoClient.fastrprint(F("POST /api/v1/gateway/updatesensors/"));
		WidoClient.fastrprint(F("01"));
		WidoClient.fastrprintln(F(" HTTP/1.1"));
		Serial.print(F("."));

		WidoClient.fastrprint(F("userkey: "));
		WidoClient.fastrprintln(userkey);
		Serial.print(F("."));
		WidoClient.fastrprintln(F("Host: open.lewei50.com"));
		Serial.print(F("."));

		WidoClient.fastrprint("Content-Length: ");
		WidoClient.fastrprintln(lengthstr);
		WidoClient.fastrprintln("");
		Serial.print(F("."));

		Serial.println(F(" done."));

		Serial.print(F("Sending data"));
		WidoClient.fastrprintln(httpPackage);

		Serial.println(F(" done."));

		unsigned long rTimer = millis();
		Serial.println(F("Reading Cloud Responose.\r\n"));
		while (millis() - rTimer < 10000)
		{
			while (WidoClient.connected() && WidoClient.available())
			{
				char c = WidoClient.read();
				Serial.print(c);
			}
		}
		delay(1000);
		WidoClient.close();
	}
}
