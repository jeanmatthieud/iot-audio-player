#include "ntp.h"
extern "C" {
  #include "sntp.h"
}

void updateNtp(String ntpServerName) {
  char* buffer = (char *)malloc((ntpServerName.length() + 1) * sizeof(char)); // - TODO : avoid malloc
  ntpServerName.toCharArray(buffer, ntpServerName.length() + 1);
  sntp_stop();
  sntp_set_timezone(0);
  sntp_setservername(0, buffer);
  sntp_init();
}

uint32 getCurrentTimestamp() {
  return sntp_get_current_timestamp();
}
