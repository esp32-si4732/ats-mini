#ifndef PAGE_TEMPLATE_H
#define PAGE_TEMPLATE_H

#include <Arduino.h>
#include <initializer_list>

struct PageValue
{
  const char *name;
  // Borrowed for the synchronous render call; never retain these references.
  const String &value;
};

// Values are escaped for HTML text and quoted attributes. Triple braces are
// reserved for trusted HTML fragments, never for unescaped user input.
static void pageAppend(String &out, const char *text, std::initializer_list<PageValue> values = {})
{
  size_t capacity = out.length() + strlen(text);
  for(const auto &item : values) capacity += item.value.length();
  out.reserve(capacity);

  while(*text)
  {
    const char *open = strstr(text, "{{");
    if(!open) { out += text; break; }
    out.concat(text, open - text);
    bool raw = open[2] == '{';
    const char *name = open + (raw? 3 : 2);
    const char *close = strstr(name, raw? "}}}" : "}}");
    if(!close) { out += open; break; }
    text = close + (raw? 3 : 2);

    const String *value = nullptr;
    for(const auto &item : values)
      if(strlen(item.name) == (size_t)(close - name) && !strncmp(item.name, name, close - name))
      {
        value = &item.value;
        break;
      }
    // Leave unknown placeholders visible to make template mistakes apparent.
    if(!value) { out.concat(open, text - open); continue; }
    if(raw) { out += *value; continue; }
    for(size_t i=0 ; i<value->length() ; i++)
    {
      switch((*value)[i])
      {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&#39;"; break;
        default: out += (*value)[i]; break;
      }
    }
  }
}

static String pageRender(const char *text, std::initializer_list<PageValue> values = {})
{
  String result;
  pageAppend(result, text, values);
  return result;
}

#endif // PAGE_TEMPLATE_H
