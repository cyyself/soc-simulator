#ifndef AUTO_SIG

#define AUTO_SIG(name, msb, lsb) \
    typename std::conditional <(msb-lsb+1) <=  8, CData, \
    typename std::conditional <(msb-lsb+1) <= 16, SData, \
    typename std::conditional <(msb-lsb+1) <= 32, IData, QData >::type >::type >::type name

#define AUTO_IN(name, msb, lsb)  AUTO_SIG(name, msb, lsb)
#define AUTO_OUT(name, msb, lsb) AUTO_SIG(name, msb, lsb)

#endif