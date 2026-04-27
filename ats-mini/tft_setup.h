#if defined(LILYGO_SI473X)

// LilyGo T-Embed S3 ST7789 using SPI
#define USER_SETUP_ID 210

// Workaround for https://github.com/Bodmer/TFT_eSPI/issues/3329
#define USE_HSPI_PORT

#define ST7789_DRIVER

#define TFT_WIDTH 170
#define TFT_HEIGHT 320

#define TFT_RGB_ORDER TFT_BGR

#define TFT_INVERSION_ON
#define TFT_BACKLIGHT_ON HIGH

// #define TFT_BL     15   // LED back-light
#define TFT_MISO   -1   // Not connected
#define TFT_MOSI   11
#define TFT_SCLK   12
#define TFT_CS     10
#define TFT_DC     13
#define TFT_RST    9 // Connect reset to ensure display initialises

#define SPI_FREQUENCY 40000000
#define SPI_READ_FREQUENCY 20000000
#define SPI_TOUCH_FREQUENCY 2500000

#else

// ST7789 using 8-bit Parallel
#define USER_SETUP_ID 206

#define ST7789_DRIVER
#define INIT_SEQUENCE_3 // Using this initialisation sequence improves the display image

#define CGRAM_OFFSET
#define TFT_RGB_ORDER TFT_RGB  // Colour order Red-Green-Blue
//#define TFT_RGB_ORDER TFT_BGR // Colour order Blue-Green-Red

#define TFT_INVERSION_ON
// #define TFT_INVERSION_OFF

#define TFT_PARALLEL_8_BIT

#define TFT_WIDTH 170
#define TFT_HEIGHT 320

#define TFT_CS  6
#define TFT_DC  7
#define TFT_RST 5

#define TFT_WR 8
#define TFT_RD 9

#define TFT_D0 39
#define TFT_D1 40
#define TFT_D2 41
#define TFT_D3 42
#define TFT_D4 45
#define TFT_D5 46
#define TFT_D6 47
#define TFT_D7 48

// Disable to prevent turning the backlight on too early
// #define TFT_BL 38
#define TFT_BACKLIGHT_ON HIGH

#endif

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT
