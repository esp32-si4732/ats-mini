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

// A borrowed callback, invoked synchronously at a triple-brace placeholder.
// Like PageValue, use this only within the render call; it owns no captured data.
struct PageFragment
{
  const char *name;
  const void *context;
  void (*append)(String &, const void *);

  template<typename Render>
  PageFragment(const char *name, const Render &render) :
    name(name), context(&render), append([](String &out, const void *context) {
      (*static_cast<const Render *>(context))(out);
    }) {}
};

static void pageReserve(String &out, size_t additional)
{
  // Leave room for adjacent fragments instead of reallocating for every option.
  size_t capacity = out.length() + additional;
  out.reserve((capacity + 1023) & ~size_t(1023));
}

// Values are escaped for HTML text and quoted attributes. Triple braces are
// reserved for trusted HTML fragments, never for unescaped user input.
static void pageAppend(String &out, const char *text, std::initializer_list<PageValue> values = {},
                       std::initializer_list<PageFragment> fragments = {})
{
  size_t additional = strlen(text);
  for(const auto &item : values) additional += item.value.length();
  pageReserve(out, additional);

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

    auto matches = [&](const char *key) {
      return(strlen(key) == (size_t)(close - name) && !strncmp(key, name, close - name));
    };
    const PageFragment *fragment = nullptr;
    if(raw)
      for(const auto &item : fragments)
        if(matches(item.name)) { fragment = &item; break; }
    if(fragment) { fragment->append(out, fragment->context); continue; }

    const String *value = nullptr;
    for(const auto &item : values)
      if(matches(item.name))
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

#endif // PAGE_TEMPLATE_H
