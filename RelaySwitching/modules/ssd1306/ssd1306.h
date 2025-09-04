#ifndef __SSD1306_H__
#define __SSD1306_H__


#include <zephyr/sys/timeutil.h>
#include <time.h>

#include "ssd1306_conf.h"


#ifdef SSD1306_X_OFFSET
#define SSD1306_X_OFFSET_LOWER (SSD1306_X_OFFSET & 0x0F)
#define SSD1306_X_OFFSET_UPPER ((SSD1306_X_OFFSET >> 4) & 0x07)
#else
#define SSD1306_X_OFFSET_LOWER 0
#define SSD1306_X_OFFSET_UPPER 0
#endif

// SSD1306 OLED height in pixels
#ifndef SSD1306_HEIGHT
#define SSD1306_HEIGHT          64
#endif

// SSD1306 width in pixels
#ifndef SSD1306_WIDTH
#define SSD1306_WIDTH           128
#endif

#ifndef SSD1306_BUFFER_SIZE
#define SSD1306_BUFFER_SIZE   SSD1306_WIDTH * SSD1306_HEIGHT / 8
#endif

// Enumeration for screen colors
typedef enum {
    Black = 0x00, // Black color, no pixel
    White = 0x01  // Pixel is set. Color depends on OLED
} SSD1306_COLOR;

typedef enum {
    SSD1306_OK = 0x00,
    SSD1306_ERR = 0x01  // Generic error.
} SSD1306_Error_t;

// Struct to store transformations
typedef struct {
    uint16_t CurrentX;
    uint16_t CurrentY;
    uint8_t Initialized;
    uint8_t DisplayOn;
} SSD1306_t;

typedef struct {
    uint8_t x;
    uint8_t y;
} SSD1306_VERTEX;

/** Font */
typedef struct {
	const uint8_t width;                /**< Font width in pixels */
	const uint8_t height;               /**< Font height in pixels */
	const uint16_t *const data;         /**< Pointer to font data array */
    const uint8_t *const char_width;    /**< Proportional character width in pixels (NULL for monospaced) */
} SSD1306_Font_t;

// Procedure definitions

int ssd1306_Fill(const struct device *dev,SSD1306_COLOR color);
int ssd1306_UpdateScreen(const struct device *dev);
int ssd1306_DrawPixel(const struct device *dev,uint8_t x, uint8_t y, SSD1306_COLOR color);
int ssd1306_WriteChar(const struct device *dev,char ch, SSD1306_Font_t Font, SSD1306_COLOR color);
int ssd1306_WriteString(const struct device *dev,char* str, SSD1306_Font_t Font, SSD1306_COLOR color);
int ssd1306_SetCursor(const struct device *dev,uint8_t x, uint8_t y);
int ssd1306_Line(const struct device *dev,uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color);
int ssd1306_DrawArc(const struct device *dev,uint8_t x, uint8_t y, uint8_t radius, uint16_t start_angle, uint16_t sweep, SSD1306_COLOR color);
int ssd1306_DrawArcWithRadiusLine(const struct device *dev,uint8_t x, uint8_t y, uint8_t radius, uint16_t start_angle, uint16_t sweep, SSD1306_COLOR color);
int ssd1306_DrawCircle(const struct device *dev,uint8_t par_x, uint8_t par_y, uint8_t par_r, SSD1306_COLOR color);
int ssd1306_FillCircle(const struct device *dev,uint8_t par_x,uint8_t par_y,uint8_t par_r,SSD1306_COLOR par_color);
int ssd1306_Polyline(const struct device *dev,const SSD1306_VERTEX *par_vertex, uint16_t par_size, SSD1306_COLOR color);
int ssd1306_DrawRectangle(const struct device *dev,uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color);
int ssd1306_FillRectangle(const struct device *dev,uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color);

/**
 * @brief Invert color of pixels in rectangle (include border)
 * 
 * @param x1 X Coordinate of top left corner
 * @param y1 Y Coordinate of top left corner
 * @param x2 X Coordinate of bottom right corner
 * @param y2 Y Coordinate of bottom right corner
 * @return SSD1306_Error_t status
 */
SSD1306_Error_t ssd1306_InvertRectangle(const struct device *dev,uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

int ssd1306_DrawBitmap(const struct device *dev,uint8_t x, uint8_t y, const unsigned char* bitmap, uint8_t w, uint8_t h, SSD1306_COLOR color);

/**
 * @brief Sets the contrast of the display.
 * @param[in] value contrast to set.
 * @note Contrast increases as the value increases.
 * @note RESET = 7Fh.
 */
int ssd1306_SetContrast(const struct device *dev,const uint8_t value);

/**
 * @brief Set Display ON/OFF.
 * @param[in] on 0 for OFF, any for ON.
 */
int ssd1306_SetDisplayOn(const struct device *dev,const uint8_t on);

/**
 * @brief Reads DisplayOn state.
 * @return  0: OFF.
 *          1: ON.
 */
int ssd1306_GetDisplayOn(const struct device *dev);

// Low-level procedures
int ssd1306_Reset(const struct device *dev);
int ssd1306_WriteCommand(const struct device *dev,uint8_t byte);
int ssd1306_WriteData(const struct device *dev,uint8_t* buffer, size_t buff_size);
SSD1306_Error_t ssd1306_FillBuffer(const struct device *dev,uint8_t* buf, uint32_t len);




#endif // __SSD1306_H__