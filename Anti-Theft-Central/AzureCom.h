#pragma once
#include <azureiot/iothubtransportmqtt.h>
#include <applibs/networking.h>

bool AzureIoT_SetupClient(void);
void AzureIoT_DoPeriodicTasks(void);
typedef void (*MessageReceivedFnType)(const char* payload);
void sendTelemetry(const unsigned char* key, const unsigned char* value);

#define MY_CONNECTION_STRING "HostName=****"
