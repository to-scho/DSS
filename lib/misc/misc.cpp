#include "misc.h"

String u20binstr(uint32_t val) {
  String binstr;
  binstr.reserve(32+3);
  binstr = u20binstr((uint8_t)((val>>24)&0xff)) + '.' + u20binstr((uint8_t)((val>>16)&0xff)) + '.' + u20binstr((uint8_t)((val>>8)&0xff)) + '.' + u20binstr((uint8_t)((val>>0)&0xff));
  return binstr;
}

String u20binstr(uint16_t val) {
  String binstr;
  binstr.reserve(16+1);
  binstr = u20binstr((uint8_t)((val>>8)&0xff)) + '.' + u20binstr((uint8_t)((val>>0)&0xff));
  return binstr;
}

String u20binstr(uint8_t val){
  String binstr;
  binstr.reserve(8);
  for(int idx=0; idx<8; idx++) {
    binstr += (val & (1<<7))?'1':'0';
    val <<= 1;
  }
  return binstr;
}
