#ifndef DISPLAY_H
#define DISPLAY_H

#include <LovyanGFX.hpp>

#if !defined(LILYGO_SI473X)
#include <esp_rom_gpio.h>
#include <hal/gpio_ll.h>
#endif

#define LCD_ID_ORIGINAL   0x8181B3
#define LCD_ID_HIGH_GAMMA 0x858552
#define LCD_ID_MIRRORED   0x009307

static constexpr uint8_t LCD_CMD_RDDID = 0x04;

#if !defined(LILYGO_SI473X)
// LovyanGFX leaves the ESP32-S3 LCD_CAM peripheral in control of the data
// pins' output enables while reading. Override that handoff so its
// normal Panel_LCD::readCommand() path can safely sample the parallel bus.
class Bus_Parallel8_ATSMini : public lgfx::Bus_Parallel8
{
  bool initialized = false;

public:
  using lgfx::Bus_Parallel8::beginRead;

  bool init(void) override
  {
    // ID detection and panel initialization share the same bus allocation.
    if(!initialized) initialized = lgfx::Bus_Parallel8::init();
    return initialized;
  }

  void release(void) override
  {
    if(!initialized) return;
    lgfx::Bus_Parallel8::release();
    initialized = false;
  }

  void beginRead(void) override
  {
    wait();
    const auto& cfg = config();
    for(int8_t pin : cfg.pin_data)
    {
      // LCD_CAM's OEN ignores GPIO_ENABLE until control is returned to GPIO.
      gpio_ll_set_output_enable_ctrl(&GPIO, pin, false, false);
      gpio_ll_output_disable(&GPIO, pin);
    }

    gpio_ll_set_level(&GPIO, cfg.pin_rs, 1);
    esp_rom_gpio_connect_out_signal(cfg.pin_rs, SIG_GPIO_OUT_IDX, false, false);
    gpio_ll_set_level(&GPIO, cfg.pin_rd, 0);
  }
};
#endif

// ST7789 variant using the TFT_eSPI "INIT_SEQUENCE_3" table (JLX240),
// which the previous driver setup used because it improves the display image.
class Panel_ST7789_ATSMini : public lgfx::Panel_ST7789
{
public:
  bool highGammaDisplay = false;

  Panel_ST7789_ATSMini()
  {
    _nop_closing = false; // Both supported boards have a dedicated CS pin.
  }

protected:
  uint8_t getMadCtl(uint8_t rotation) const override
  {
    static constexpr uint8_t values[] =
    {
      0,
      MAD_MX | MAD_MV,
      MAD_MX | MAD_MY,
      MAD_MV | MAD_MY,
    };
    return values[rotation & 3];
  }

  const uint8_t* getInitCommands(uint8_t listno) const override
  {
    static constexpr uint8_t list0[] =
    {
      CMD_SLPOUT   , CMD_INIT_DELAY, 120,
      CMD_NORON    , 0,
      CMD_PORCTRL  , 5, 0x0B, 0x0B, 0x00, 0x33, 0x33,
      CMD_GCTRL    , 1, 0x75,
      CMD_VCOMS    , 1, 0x28,
      CMD_LCMCTRL  , 1, 0x2C,
      CMD_VDVVRHEN , 1, 0x01,
      CMD_VRHS     , 1, 0x1F,
      CMD_FRCTR2   , 1, 0x13,
      CMD_PWCTRL1  , 1, 0xA7,
      CMD_PWCTRL1  , 2, 0xA4, 0xA1,
      0xD6         , 1, 0xA1,
      CMD_PVGAMCTRL, 14, 0xF0, 0x05, 0x0A, 0x06, 0x06, 0x03, 0x2B,
                         0x32, 0x43, 0x36, 0x11, 0x10, 0x2B, 0x32,
      CMD_NVGAMCTRL, 14, 0xF0, 0x08, 0x0C, 0x0B, 0x09, 0x24, 0x2B,
                         0x22, 0x43, 0x38, 0x15, 0x16, 0x2F, 0x37,
      CMD_DISPON   , CMD_INIT_DELAY, 120,
      0xFF, 0xFF,
    };
    static constexpr uint8_t highGamma[] =
    {
      0x26, 1, 0x08, // GAMSET: Gamma Curve 3
      0x55, 1, 0xB1, // WRCACE: High enhancement, UI mode
      0xFF, 0xFF,
    };
    if(listno == 0) return list0;
    if(listno == 1 && highGammaDisplay) return highGamma;
    return nullptr;
  }
};

class Panel_GC9307_ATSMini : public lgfx::Panel_GC9307
{
public:
  void setWindow(uint_fast16_t xs, uint_fast16_t ys, uint_fast16_t xe, uint_fast16_t ye) override
  {
    // Use configured offsets instead of Panel_GC9307's hardcoded 34 pixels.
    lgfx::Panel_GC9xxx::setWindow(xs, ys, xe, ye);
  }
};

class LGFX : public lgfx::LGFX_Device
{
  uint32_t displayId = 0;
  Panel_ST7789_ATSMini displayPanel;
  Panel_GC9307_ATSMini gc9307Panel;
#if defined(LILYGO_SI473X)
  lgfx::Bus_SPI bus;
#else
  Bus_Parallel8_ATSMini bus;
#endif

public:
  LGFX()
  {
#if defined(LILYGO_SI473X)
    {
      auto config = bus.config();
      config.spi_host = SPI3_HOST;
      config.freq_write = 40000000;
      config.freq_read = 10000000;
      config.spi_3wire = true;
      config.pin_sclk = 12;
      config.pin_mosi = 11;
      config.pin_miso = -1;
      config.pin_dc = 13;
      bus.config(config);
      displayPanel.setBus(&bus);
    }
#else
    {
      auto config = bus.config();
      config.freq_write = 20000000;
      config.pin_wr = 8;
      config.pin_rd = 9;
      config.pin_rs = 7;
      config.pin_d0 = 39;
      config.pin_d1 = 40;
      config.pin_d2 = 41;
      config.pin_d3 = 42;
      config.pin_d4 = 45;
      config.pin_d5 = 46;
      config.pin_d6 = 47;
      config.pin_d7 = 48;
      bus.config(config);
      displayPanel.setBus(&bus);
    }
#endif

    {
      auto config = displayPanel.config();
#if defined(LILYGO_SI473X)
      config.pin_cs = 10;
      config.pin_rst = 9;
#else
      config.pin_cs = 6;
      config.pin_rst = 5;
#endif
      // The init table sets LCMCTRL=0x2C (XBGR=1), so MADCTL must be RGB.
      config.rgb_order = true;
      config.panel_width = 170;
      config.offset_x = 35;
      config.dummy_read_pixel = 8;
      config.invert = true;
      config.bus_shared = false;
      displayPanel.config(config);

      auto gcConfig = gc9307Panel.config();
      gcConfig.pin_cs = config.pin_cs;
      gcConfig.pin_rst = config.pin_rst;
      gcConfig.panel_width = 170;
      gcConfig.offset_x = 35;
      gcConfig.bus_shared = false;
      gc9307Panel.config(gcConfig);
      gc9307Panel.setBus(&bus);
    }

    setPanel(&displayPanel);
  }

protected:
  bool init_impl(bool use_reset, bool use_clear) override
  {
    // Prepare the bus/reset pins without sending either panel's init table.
    if(!displayPanel.lgfx::Panel_Device::init(use_reset)) return false;
    displayId = displayPanel.isReadable()
      ? __builtin_bswap32(displayPanel.readCommand(LCD_CMD_RDDID, 0, 3)) >> 8
      : 0;
    displayPanel.highGammaDisplay = displayId == LCD_ID_HIGH_GAMMA;
    if(displayId == LCD_ID_MIRRORED)
      setPanel(&gc9307Panel);
    else
      setPanel(&displayPanel);
    return lgfx::LGFX_Device::init_impl(false, use_clear);
  }

public:
  uint32_t getDisplayId() const { return displayId; }

};

#endif
