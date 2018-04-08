#ifndef FW_VERSION_H
#define FW_VERSION_H

#define _MY_CONCAT5(a, b, c, d, e) (_MY_CONCAT4(a,b,c,d)*1000) + ((unsigned long)e##.0)
#define _MY_CONCAT4(a, b, c, d) (_MY_CONCAT3(a,b,c)*1000) + ((unsigned long)d##.0)
#define _MY_CONCAT3(a, b, c) (_MY_CONCAT2(a,b)*1000) + ((unsigned long)c##.0)
#define _MY_CONCAT2(a, b) ((_MY_CONCAT1(a)*1000) + ((unsigned long)b##.0))
#define _MY_CONCAT1(a) ((unsigned long)a##.0)
#define _GET_OVERRIDE(_1, _2, _3, _4, _5, NAME, ...) NAME
#define MY_CONCAT(...) _GET_OVERRIDE(__VA_ARGS__, _MY_CONCAT5, _MY_CONCAT4, _MY_CONCAT3, _MY_CONCAT2, _MY_CONCAT1)(__VA_ARGS__)

extern const unsigned long fw_version;
char* fw_version_ul2char(unsigned long fw_version, int len);
unsigned long fw_version_char2ul(const char* fw_version_str);

#endif
