#ifndef PAGE_PATCHES_H
#define PAGE_PATCHES_H

#include <Arduino.h>

static const char pagePatchesTitle[] PROGMEM = "ATS-Mini SI4732 Patches (Experimental)";

static const char pagePatches[] PROGMEM = R"HTML(<H1>{{title}}</H1>
{{{navigation}}}
{{{busy}}}
<FORM METHOD="POST" ACTION="/patches">
  <INPUT TYPE="HIDDEN" NAME="action" VALUE="select">
  <TABLE COLUMNS="2">
    <TR><TH COLSPAN="2" CLASS="HEADING">Active Patch Set</TH></TR>
    <TR><TD CLASS="LABEL"><LABEL FOR="patchslot">Patch Set</LABEL></TD><TD><SELECT ID="patchslot" NAME="slot" {{disabled}}>{{{options}}}</SELECT></TD></TR>
    <TR><TH COLSPAN="2" CLASS="HEADING"><INPUT TYPE="SUBMIT" VALUE="Apply" {{disabled}}></TH></TR>
  </TABLE>
</FORM>
{{{sets}}}
)HTML";

static const char pagePatchesBusy[] PROGMEM = R"HTML(<TABLE COLUMNS="1"><TR><TD CLASS="CENTER">A patch request is in progress.</TD></TR></TABLE>)HTML";

static const char pagePatchSet[] PROGMEM = R"HTML(<FORM METHOD="POST" ACTION="/patches" ENCTYPE="multipart/form-data">
  <!-- Slot and action must precede the file for upload callbacks. -->
  <INPUT TYPE="HIDDEN" NAME="slot" VALUE="{{slot}}">
  <INPUT TYPE="HIDDEN" NAME="action" VALUE="upload">
  <TABLE COLUMNS="2">
    <TR><TH COLSPAN="2" CLASS="HEADING">{{name}}</TH></TR>
    <TR><TD CLASS="LABEL">Installed Patches</TD><TD>{{installed}}</TD></TR>
    <TR><TD CLASS="LABEL"><LABEL FOR="patchmode{{slot}}">Mode</LABEL></TD><TD><SELECT ID="patchmode{{slot}}" NAME="mode" {{disabled}}>{{{options}}}</SELECT></TD></TR>
    <TR>
      <TD CLASS="LABEL"><LABEL FOR="patchfile{{slot}}">Upload Patch</LABEL></TD>
      <TD><INPUT TYPE="FILE" ID="patchfile{{slot}}" NAME="patch" ACCEPT=".bin" {{disabled}}><BR><SMALL>One .bin file at a time; maximum size: 32 KiB</SMALL></TD>
    </TR>
    {{{delete}}}
    <TR><TH COLSPAN="2" CLASS="HEADING"><INPUT TYPE="SUBMIT" VALUE="Save" {{disabled}}></TH></TR>
  </TABLE>
</FORM>
)HTML";

static const char pagePatchDelete[] PROGMEM = R"HTML(<TR><TD CLASS="LABEL"><LABEL FOR="patchdelete{{slot}}">Delete set</LABEL></TD><TD><INPUT TYPE="CHECKBOX" ID="patchdelete{{slot}}" NAME="delete" VALUE="on" {{disabled}}></TD></TR>)HTML";

static const char pagePatchOption[] PROGMEM = R"HTML(<OPTION VALUE="{{value}}" {{selected}}>{{label}}</OPTION>)HTML";

#endif // PAGE_PATCHES_H
