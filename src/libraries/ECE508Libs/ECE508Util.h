
#ifndef ECE508_Util
#define ECE508_Util

/**
 * Helper function for pretty printing mac addresses
 * */
String macToString(const unsigned char* mac) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}






#endif