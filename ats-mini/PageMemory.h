#ifndef PAGE_MEMORY_H
#define PAGE_MEMORY_H

#include <Arduino.h>

static const char pageMemory[] PROGMEM = R"HTML(<H1>ATS-Mini Pocket Receiver Memory</H1>
{{{navigation}}}
<TABLE COLUMNS="2">
{{{rows}}}
</TABLE>
)HTML";

static const char pageMemoryRow[] PROGMEM = R"HTML(<TR><TD CLASS="LABEL" WIDTH="10%">{{slot}}</TD><TD>{{frequency}}{{mode}}</TD></TR>
)HTML";

static const char pageMemoryEmpty[] PROGMEM = R"HTML(<TR><TD CLASS="LABEL" WIDTH="10%">{{slot}}</TD><TD>&nbsp;---&nbsp;</TD></TR>
)HTML";

#endif // PAGE_MEMORY_H
