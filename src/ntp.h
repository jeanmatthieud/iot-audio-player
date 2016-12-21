#ifndef _NTP_H_
#define _NTP_H_

#include <Arduino.h>

void updateNtp(String ntpServerName);
uint32 getCurrentTimestamp();

#endif
