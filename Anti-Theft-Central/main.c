#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <applibs/log.h>
#include <applibs/gpio.h>
#include "mt3620.h"
#include "mfrc522.h"
#include "applibs_versions.h"
#include "AzureCom.h"

// Define functions
void btox(char* xp, const char* bb, int n);
bool readCard();
void sendValues();

// Define variables
const struct timespec sleepTimeShort = { 4, 0 }; // Main loop delay
const struct timespec sleepTimeLong = { 10, 0 }; // Enough time to move away from central after activation
int AlEnabled = 0; // Alarm activation variable. Define if the central is ready to detect thieves
int AlTimer = 0; // Variable that counts the number of loop cycles in alarm
char hexstr[8]; // Temporary variable that stored read tag value
int readPIR = 0;
int readCONTACT = 0;

int main(void)
{
	Log_Debug("Starting...\n");
	AzureIoT_SetupClient();

	// Set-up GPIO
	int ledEN = GPIO_OpenAsOutput(26, GPIO_OutputMode_PushPull, GPIO_Value_High); // AlEnabled LED
	int ledRD = GPIO_OpenAsOutput(8, GPIO_OutputMode_PushPull, GPIO_Value_High); // Red LED
	int ledBL = GPIO_OpenAsOutput(10, GPIO_OutputMode_PushPull, GPIO_Value_High); // Blue LED
	int buzzer = GPIO_OpenAsOutput(28, GPIO_OutputMode_PushPull, GPIO_Value_High); // Alarm buzzer
	int pir = GPIO_OpenAsInput(0); // PIR
	int cont = GPIO_OpenAsInput(2); // LIMIT SWITCH
	GPIO_SetValue(ledBL, GPIO_Value_High);
	GPIO_SetValue(ledRD, GPIO_Value_High);
	GPIO_SetValue(ledBL, GPIO_Value_High);
	GPIO_SetValue(ledEN, GPIO_Value_Low);
	GPIO_SetValue(buzzer, GPIO_Value_Low);

	// Start the RFID Scanner
	mfrc522_init();

	// Main loop
    while (true) {
		// Check RFID tag
		if (readCard()) {
			// Check if readed tag is the same hardcoded below
			if (strcmp(hexstr, "B673BF32") == 0) {  // Edit this string to your current RFID tag
				GPIO_SetValue(ledBL, GPIO_Value_Low);
				if (AlEnabled == 0) {
					// Enable alarm
					AlEnabled = 1;
					GPIO_SetValue(ledEN, GPIO_Value_High);
				}
				else {
					// Disable alarm and reset alarm signaling
					AlEnabled = 0;
					AlTimer = 0;
					GPIO_SetValue(ledEN, GPIO_Value_Low);
					GPIO_SetValue(ledRD, GPIO_Value_High);
					GPIO_SetValue(buzzer, GPIO_Value_Low);
				}
				nanosleep(&sleepTimeLong, NULL);
				GPIO_SetValue(ledBL, GPIO_Value_High);
			}
		}
        // Check PIR/CONTACT
		GPIO_GetValue(pir, &readPIR);
		GPIO_GetValue(cont, &readCONTACT);
		if (AlEnabled && ((readPIR == 1) || (readCONTACT == 1))) {
			AlTimer++;
		}
		// If central is not deactivated after 3 loop of alarm enable red led and buzzer
		if (AlTimer > 3) {
			GPIO_SetValue(ledRD, GPIO_Value_Low);
			GPIO_SetValue(buzzer, GPIO_Value_High);
		}
		// Send everything to Azure IoTCentral, if possible
		sendValues();
		AzureIoT_DoPeriodicTasks();
		//
		nanosleep(&sleepTimeShort, NULL);
    }
}

bool readCard() {
	uint8_t str[MAX_LEN];
	uint8_t byte = mfrc522_request(PICC_REQALL, str);
	if (byte == CARD_FOUND){
		Log_Debug("[MFRC522][INFO] Found a card: %x\n", byte);
		byte = mfrc522_get_card_serial(str);
		if (byte == CARD_FOUND)
		{
			btox(hexstr, str, 8);
			hexstr[8] = 0;
			Log_Debug("[MFRC522][INFO] Serial: %s\n", hexstr);
			return true;
		}
	}
	return false;
}

void btox(char* xp, const char* bb, int n)
{
	const char xx[] = "0123456789ABCDEF";
	while (--n >= 0) xp[n] = xx[(bb[n >> 1] >> ((1 - (n & 1)) << 2)) & 0xF];
}

void sendValues() {
	// Prepare data for transfer
	char bufPIR[20];
	snprintf(bufPIR, 20, "%d", readPIR);
	char bufCONTACT[20];
	snprintf(bufCONTACT, 20, "%d", readCONTACT);
	char bufALARM[20];
	snprintf(bufALARM, 20, "%d", AlEnabled);
	char bufALARMTIM[20];
	snprintf(bufALARMTIM, 20, "%d", AlTimer);
	// Send everything to Azure IoTCentral
	sendTelemetry("pir", bufPIR);
	sendTelemetry("cont", bufCONTACT);
	sendTelemetry("AlEnabled", bufALARM);
	sendTelemetry("AlTimer", bufALARMTIM);
}

