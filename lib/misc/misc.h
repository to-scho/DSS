#ifndef _MISC
#define _MISC

#include <Arduino.h>

#define NUMEL(_x)  (sizeof(_x) / sizeof((_x)[0]))

#define __TO_STRING(__name) #__name
#define TO_STRING(__name) __TO_STRING(__name)

/* Number of bits in inttype_MAX, or in any (1<<k)-1 where 0 <= k < 2040 */
#define IMAX_BITS(_m) ((_m)/((_m)%255+1) / 255%255*8 + 7-86/((_m)%255+12))

#define UMAXVALTYPE(_x) ((__typeof__(_x))(~(__typeof__(_x))(0)))
#define UMINVALTYPE(_x) (0)

#define INCSAT(_x) { __typeof__(_x) _t = (_x)+1; _x = (_t > (_x)) ? _t : (_x); }
#define DECSAT(_x) { __typeof__(_x) _t = (_x)-1; _x = (_t < (_x)) ? _t : (_x); }

#define UADDSAT(_x,_y) { __typeof__(_x) _t = (_x)+(_y); _x = (_t >= (_x)) ? _t : UMAXVALTYPE(_x); }
#define USUBSAT(_x,_y) { __typeof__(_x) _t = (_x)-(_y); _x = (_t <= (_x)) ? _t : UMINVALTYPE(_x); }

#define IF_ELSE(condition) ___IF_ELSE(__BOOL(condition))
#define __SECOND(a, b, ...) b
#define __IS_PROBE(...) __SECOND(__VA_ARGS__, 0)
#define __PROBE() ~, 1
#define __CAT(a,b) a ## b
#define __NOT(x) __IS_PROBE(__CAT(___NOT_, x))
#define ___NOT_0 __PROBE()
#define ___NOT_ __PROBE()
#define __BOOL(x) __NOT(__NOT(x))
#define ___IF_ELSE(condition) __CAT(___IF_, condition)
#define ___IF_1(...) __VA_ARGS__ ___IF_1_ELSE
#define ___IF_0(...)             ___IF_0_ELSE
#define ___IF_1_ELSE(...)
#define ___IF_0_ELSE(...) __VA_ARGS__

#define IS_char(_type) __IS_PROBE(__CAT(___IS_char_, _type))
#define ___IS_char_char __PROBE()

#define BUILD_BUG_ON(condition) extern char _CHECK_[1 - 2*!!(condition)];

String u20binstr(uint32_t val);
String u20binstr(uint16_t val);
String u20binstr(uint8_t val);

#endif
