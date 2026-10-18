#ifndef PAGE_CONFIG_H
#define PAGE_CONFIG_H

#include <Arduino.h>

static const char pageConfigTitle[] PROGMEM = "ATS-Mini Config";

static const char pageConfig[] PROGMEM = R"HTML(<H1>{{title}}</H1>
{{{navigation}}}
<FORM ACTION="/setconfig" METHOD="POST" ENCTYPE="multipart/form-data" ONSUBMIT="browserDateTime(true)">
  <TABLE COLUMNS="2">
    <TR><TH COLSPAN="2" CLASS="HEADING">Date/Time</TH></TR>
    <TR><TD CLASS="LABEL">Use Browser Date/Time</TD><TD><INPUT TYPE="CHECKBOX" ID="browserdatetime" ONCHANGE="browserDateTime()"></TD></TR>
    <TR><TD CLASS="LABEL">UTC Date/Time</TD><TD><INPUT TYPE="TEXT" ID="datetime" NAME="datetime" PLACEHOLDER="YYYY-mm-dd HH:MM:SS"></TD></TR>
    <TR><TD CLASS="LABEL">Time Zone</TD><TD><SELECT ID="utcoffset" NAME="utcoffset">{{{utc_options}}}</SELECT></TD></TR>
    <TR><TH COLSPAN="2" CLASS="HEADING">UI</TH></TR>
    <TR><TD CLASS="LABEL">UI Layout</TD><TD><SELECT NAME="uilayout">{{{layout_options}}}</SELECT></TD></TR>
    <TR><TD CLASS="LABEL">Theme</TD><TD><SELECT NAME="theme">{{{theme_options}}}</SELECT></TD></TR>
    <TR><TD CLASS="LABEL">Reverse Scrolling</TD><TD><INPUT TYPE="CHECKBOX" NAME="scroll" VALUE="on" {{scroll}}></TD></TR>
    <TR><TD CLASS="LABEL">Zoomed Menu</TD><TD><INPUT TYPE="CHECKBOX" NAME="zoom" VALUE="on" {{zoom}}></TD></TR>
    <TR><TH COLSPAN="2" CLASS="HEADING">Radio</TH></TR>
    <TR><TD CLASS="LABEL">RDS</TD><TD><SELECT NAME="rds">{{{rds_options}}}</SELECT></TD></TR>
    <TR><TD CLASS="LABEL">FM Region</TD><TD><SELECT NAME="fmregion">{{{region_options}}}</SELECT></TD></TR>
    <TR><TD CLASS="LABEL">FM Stereo</TD><TD><SELECT NAME="fmstereo">{{{stereo_options}}}</SELECT></TD></TR>
    <TR><TH COLSPAN="2" CLASS="HEADING">Power</TH></TR>
    <TR><TD CLASS="LABEL">Brightness</TD><TD><INPUT TYPE="NUMBER" NAME="brightness" MIN="10" MAX="255" STEP="5" VALUE="{{brightness}}" REQUIRED></TD></TR>
    <TR><TD CLASS="LABEL">Sleep Timer</TD><TD><INPUT TYPE="NUMBER" NAME="sleeptimer" MIN="0" MAX="255" STEP="5" VALUE="{{sleep_timer}}" REQUIRED><BR><SMALL>Seconds; 0 disables sleep</SMALL></TD></TR>
    <TR><TD CLASS="LABEL">Sleep Mode</TD><TD><SELECT NAME="sleepmode">{{{sleep_options}}}</SELECT></TD></TR>
    <TR><TD CLASS="LABEL">Off Timer</TD><TD><INPUT TYPE="NUMBER" NAME="offtimer" MIN="0" MAX="240" STEP="5" VALUE="{{off_timer}}" REQUIRED><BR><SMALL>Minutes; 0 disables shutdown. Changing the duration restarts the timer.</SMALL></TD></TR>
    <TR><TH COLSPAN="2" CLASS="HEADING">Connectivity</TH></TR>
    <TR><TD CLASS="LABEL">USB Mode</TD><TD><SELECT NAME="usbmode">{{{usb_options}}}</SELECT></TD></TR>
    <TR><TD CLASS="LABEL">TCP Mode</TD><TD><SELECT NAME="tcpmode">{{{tcp_options}}}</SELECT><BR><SMALL>Listens on port 60000; requires Wi-Fi</SMALL></TD></TR>
    <TR><TD CLASS="LABEL">Bluetooth Mode</TD><TD><SELECT NAME="blemode">{{{ble_options}}}</SELECT></TD></TR>
    <TR><TH COLSPAN="2" CLASS="HEADING">Hardware</TH></TR>
    <TR><TD CLASS="LABEL">Audio Clamp</TD><TD><INPUT TYPE="CHECKBOX" NAME="audioclamp" VALUE="on" {{audio_clamp}}><BR><SMALL>Enable if the receiver has an audio clamp circuit</SMALL></TD></TR>
    <TR><TD CLASS="LABEL">Half-step Encoder</TD><TD><INPUT TYPE="CHECKBOX" NAME="encoderhalfstep" VALUE="on" {{half_step}}></TD></TR>
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
