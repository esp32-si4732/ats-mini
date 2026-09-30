#ifndef PAGE_CONFIG_H
#define PAGE_CONFIG_H

#include <Arduino.h>

static const char pageConfigTitle[] PROGMEM = "ATS-Mini Config";

static const char pageConfig[] PROGMEM = R"HTML(<H1>{{title}}</H1>
{{{navigation}}}
<FORM ACTION="/setconfig" METHOD="POST" ENCTYPE="multipart/form-data" ONSUBMIT="browserDateTime(true)">
  <TABLE COLUMNS="2">
    <TR><TH COLSPAN="2" CLASS="HEADING">WiFi Network 1</TH></TR>
    <TR><TD CLASS="LABEL">SSID</TD><TD><INPUT TYPE="TEXT" NAME="wifissid1" VALUE="{{ssid1}}"></TD></TR>
    <TR><TD CLASS="LABEL">Password</TD><TD><INPUT TYPE="PASSWORD" NAME="wifipass1" VALUE="{{pass1}}"></TD></TR>
    <TR><TH COLSPAN="2" CLASS="HEADING">WiFi Network 2</TH></TR>
    <TR><TD CLASS="LABEL">SSID</TD><TD><INPUT TYPE="TEXT" NAME="wifissid2" VALUE="{{ssid2}}"></TD></TR>
    <TR><TD CLASS="LABEL">Password</TD><TD><INPUT TYPE="PASSWORD" NAME="wifipass2" VALUE="{{pass2}}"></TD></TR>
    <TR><TH COLSPAN="2" CLASS="HEADING">WiFi Network 3</TH></TR>
    <TR><TD CLASS="LABEL">SSID</TD><TD><INPUT TYPE="TEXT" NAME="wifissid3" VALUE="{{ssid3}}"></TD></TR>
    <TR><TD CLASS="LABEL">Password</TD><TD><INPUT TYPE="PASSWORD" NAME="wifipass3" VALUE="{{pass3}}"></TD></TR>
    <TR><TH COLSPAN="2" CLASS="HEADING">This Web UI Login Credentials</TH></TR>
    <TR><TD CLASS="LABEL">Username</TD><TD><INPUT TYPE="TEXT" NAME="username" VALUE="{{username}}"></TD></TR>
    <TR><TD CLASS="LABEL">Password</TD><TD><INPUT TYPE="PASSWORD" NAME="password" VALUE="{{password}}"></TD></TR>
    <TR><TH COLSPAN="2" CLASS="HEADING">Settings</TH></TR>
    <TR><TD CLASS="LABEL">Scan Hidden SSIDs</TD><TD><INPUT TYPE="CHECKBOX" NAME="wifiscanhidden" VALUE="on" {{scan_hidden}}></TD></TR>
    <TR><TD CLASS="LABEL">Use Browser Date/Time</TD><TD><INPUT TYPE="CHECKBOX" ID="browserdatetime" ONCHANGE="browserDateTime()"></TD></TR>
    <TR><TD CLASS="LABEL">UTC Date/Time</TD><TD><INPUT TYPE="TEXT" ID="datetime" NAME="datetime" PLACEHOLDER="YYYY-mm-dd HH:MM:SS"></TD></TR>
    <TR><TD CLASS="LABEL">Time Zone</TD><TD><SELECT ID="utcoffset" NAME="utcoffset">{{{utc_options}}}</SELECT></TD></TR>
    <TR><TD CLASS="LABEL">Theme</TD><TD><SELECT NAME="theme">{{{theme_options}}}</SELECT></TD></TR>
    <TR><TD CLASS="LABEL">Reverse Scrolling</TD><TD><INPUT TYPE="CHECKBOX" NAME="scroll" VALUE="on" {{scroll}}></TD></TR>
    <TR><TD CLASS="LABEL">Half-step Encoder</TD><TD><INPUT TYPE="CHECKBOX" NAME="encoderhalfstep" VALUE="on" {{half_step}}></TD></TR>
    <TR><TD CLASS="LABEL">Zoomed Menu</TD><TD><INPUT TYPE="CHECKBOX" NAME="zoom" VALUE="on" {{zoom}}></TD></TR>
    <TR><TH COLSPAN="2" CLASS="HEADING">Splash Screen</TH></TR>
    <TR><TD CLASS="LABEL">Current Image</TD><TD>{{{splash}}}</TD></TR>
    <TR>
      <TD CLASS="LABEL">Upload PNG</TD>
      <TD><INPUT TYPE="FILE" NAME="splash" ACCEPT=".png"><BR><SMALL>Required resolution: {{resolution}} pixels; maximum size: 512 KB</SMALL></TD>
    </TR>
    <TR><TD CLASS="LABEL">Delete Image</TD><TD><INPUT TYPE="CHECKBOX" NAME="deletesplash" VALUE="on"></TD></TR>
    <TR><TH COLSPAN="2" CLASS="HEADING"><INPUT TYPE="SUBMIT" VALUE="Save"></TH></TR>
  </TABLE>
</FORM>
<SCRIPT>
function browserDateTime(submit) {
  const enabled = document.getElementById('browserdatetime').checked;
  const dateTime = document.getElementById('datetime');
  const utcOffset = document.getElementById('utcoffset');
  if(enabled) {
    const now = new Date();
    dateTime.value = now.toISOString().slice(0,19).replace('T', ' ');
    const minutes = -now.getTimezoneOffset();
    for(const option of utcOffset.options)
      if(Number(option.dataset.minutes) === minutes) utcOffset.value = option.value;
  }
  dateTime.disabled = utcOffset.disabled = enabled && !submit;
}
</SCRIPT>
)HTML";

static const char pageSplash[] PROGMEM = R"HTML(<IMG SRC="/splash.png?{{version}}" ALT="Current splash screen" STYLE="max-width:100%;height:auto;">)HTML";

static const char pageConfigThemeOption[] PROGMEM = R"HTML(<OPTION VALUE="{{value}}" {{selected}}>{{label}}</OPTION>)HTML";

static const char pageConfigUtcOption[] PROGMEM = R"HTML(<OPTION VALUE="{{value}}" DATA-MINUTES="{{minutes}}" {{selected}}>{{label}}</OPTION>)HTML";

#endif // PAGE_CONFIG_H
