#ifndef PAGE_STATUS_H
#define PAGE_STATUS_H

#include <Arduino.h>

static const char pageStatusTitle[] PROGMEM = "ATS-Mini Status";

static const char pageStatus[] PROGMEM = R"HTML(<H1>{{title}}</H1>
{{{navigation}}}
<TABLE COLUMNS="2">
  <TR><TD CLASS="LABEL">IP Address</TD><TD><A HREF="http://{{ip}}">{{ip}}</A> ({{ssid}})</TD></TR>
  <TR><TD CLASS="LABEL">MAC Address</TD><TD>{{mac}}</TD></TR>
  <TR><TD CLASS="LABEL">Firmware</TD><TD>{{version}}</TD></TR>
  <TR><TD CLASS="LABEL">Date/Time</TD><TD>{{time}}</TD></TR>
  <TR><TD CLASS="LABEL">Band</TD><TD>{{band}}</TD></TR>
  <TR><TD CLASS="LABEL">Frequency</TD><TD>{{frequency}}{{mode}}</TD></TR>
  <TR><TD CLASS="LABEL">Signal Strength</TD><TD>{{rssi}}dBuV</TD></TR>
  <TR><TD CLASS="LABEL">Signal to Noise</TD><TD>{{snr}}dB</TD></TR>
  <TR><TD CLASS="LABEL">Battery Voltage</TD><TD>{{battery}}V</TD></TR>
</TABLE>
)HTML";

#endif // PAGE_STATUS_H
