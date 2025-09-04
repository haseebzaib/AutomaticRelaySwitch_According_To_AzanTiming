#define DT_DRV_COMPAT zephyr_ssd1306

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <ssd1306.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/timeutil.h>
#include <zephyr/sys/util.h>
#include <time.h>


LOG_MODULE_REGISTER(SSD1306, CONFIG_CUSTOM_SSD1306_LOG_LEVEL);


// Screenbuffer
static uint8_t SSD1306_Buffer[SSD1306_BUFFER_SIZE];
// Screen object
static SSD1306_t SSD1306;

struct ssd1306_config {
	struct i2c_dt_spec i2c;   
};
struct ssd1306_data
{
	const struct device *ssd1306;
	struct k_sem lock;
};

static int ssd1306_check_device_exists(const struct device *dev)
{
	
   const struct ssd1306_config *cfg = dev->config;
    uint8_t dummy_data = 0;
    int ret = i2c_write_dt(&cfg->i2c, dummy_data, sizeof(dummy_data));
	return ret;
}

/* Convert Degrees to Radians */
static float ssd1306_DegToRad(float par_deg) {
    return par_deg * (3.14f / 180.0f);
}

/* Normalize degree to [0;360] */
static uint16_t ssd1306_NormalizeTo0_360(uint16_t par_deg) {
    uint16_t loc_angle;
    if(par_deg <= 360) {
        loc_angle = par_deg;
    } else {
        loc_angle = par_deg % 360;
        loc_angle = (loc_angle ? loc_angle : 360);
    }
    return loc_angle;
}



int ssd1306_Fill(const struct device *dev,SSD1306_COLOR color)
{
  memset(SSD1306_Buffer, (color == Black) ? 0x00 : 0xFF, sizeof(SSD1306_Buffer));
}
int ssd1306_UpdateScreen(const struct device *dev)
{
    // Write data to each page of RAM. Number of pages
    // depends on the screen height:
    //
    //  * 32px   ==  4 pages
    //  * 64px   ==  8 pages
    //  * 128px  ==  16 pages
    for(uint8_t i = 0; i < SSD1306_HEIGHT/8; i++) {
        ssd1306_WriteCommand(dev,0xB0 + i); // Set the current RAM page address.
        ssd1306_WriteCommand(dev,0x00 + SSD1306_X_OFFSET_LOWER);
        ssd1306_WriteCommand(dev,0x10 + SSD1306_X_OFFSET_UPPER);
        ssd1306_WriteData(dev,&SSD1306_Buffer[SSD1306_WIDTH*i],SSD1306_WIDTH);
    }

    return 0;
}
int ssd1306_DrawPixel(const struct device *dev,uint8_t x, uint8_t y, SSD1306_COLOR color)
{
    if(x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) {
        // Don't write outside the buffer
        return;
    }
   
    // Draw in the right color
    if(color == White) {
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y % 8);
    } else { 
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
    }
}
int ssd1306_WriteChar(const struct device *dev,char ch, SSD1306_Font_t Font, SSD1306_COLOR color)
{
 uint32_t i, b, j;
    
    // Check if character is valid
    if (ch < 32 || ch > 126)
        return 0;
    
    // Char width is not equal to font width for proportional font
    const uint8_t char_width = Font.char_width ? Font.char_width[ch-32] : Font.width;
    // Check remaining space on current line
    if (SSD1306_WIDTH < (SSD1306.CurrentX + char_width) ||
        SSD1306_HEIGHT < (SSD1306.CurrentY + Font.height))
    {
        // Not enough space on current line
        return 0;
    }
    
    // Use the font to write
    for(i = 0; i < Font.height; i++) {
        b = Font.data[(ch - 32) * Font.height + i];
        for(j = 0; j < char_width; j++) {
            if((b << j) & 0x8000)  {
                ssd1306_DrawPixel(dev,SSD1306.CurrentX + j, (SSD1306.CurrentY + i), (SSD1306_COLOR) color);
            } else {
                ssd1306_DrawPixel(dev,SSD1306.CurrentX + j, (SSD1306.CurrentY + i), (SSD1306_COLOR)!color);
            }
        }
    }
    
    // The current space is now taken
    SSD1306.CurrentX += char_width;
    
    // Return written char for validation
    return ch;
}
int ssd1306_WriteString(const struct device *dev,char* str, SSD1306_Font_t Font, SSD1306_COLOR color)
{
    while (*str) {
        if (ssd1306_WriteChar(dev,*str, Font, color) != *str) {
            // Char could not be written
            return *str;
        }
        str++;
    }
    
    // Everything ok
    return *str;
}
int ssd1306_SetCursor(const struct device *dev,uint8_t x, uint8_t y)
{
    SSD1306.CurrentX = x;
    SSD1306.CurrentY = y;
}
int ssd1306_Line(const struct device *dev,uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color)
{
    int32_t deltaX = abs(x2 - x1);
    int32_t deltaY = abs(y2 - y1);
    int32_t signX = ((x1 < x2) ? 1 : -1);
    int32_t signY = ((y1 < y2) ? 1 : -1);
    int32_t error = deltaX - deltaY;
    int32_t error2;
    
    ssd1306_DrawPixel(dev,x2, y2, color);

    while((x1 != x2) || (y1 != y2)) {
        ssd1306_DrawPixel(dev,x1, y1, color);
        error2 = error * 2;
        if(error2 > -deltaY) {
            error -= deltaY;
            x1 += signX;
        }
        
        if(error2 < deltaX) {
            error += deltaX;
            y1 += signY;
        }
    }
    return 0;
}
int ssd1306_DrawArc(const struct device *dev,uint8_t x, uint8_t y, uint8_t radius, uint16_t start_angle, uint16_t sweep, SSD1306_COLOR color)
{
static const uint8_t CIRCLE_APPROXIMATION_SEGMENTS = 36;
    float approx_degree;
    uint32_t approx_segments;
    uint8_t xp1,xp2;
    uint8_t yp1,yp2;
    uint32_t count;
    uint32_t loc_sweep;
    float rad;
    
    loc_sweep = ssd1306_NormalizeTo0_360(sweep);
    
    count = (ssd1306_NormalizeTo0_360(start_angle) * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
    approx_segments = (loc_sweep * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
    approx_degree = loc_sweep / (float)approx_segments;
    while(count < approx_segments)
    {
        rad = ssd1306_DegToRad(count*approx_degree);
        xp1 = x + (int8_t)(sinf(rad)*radius);
        yp1 = y + (int8_t)(cosf(rad)*radius);    
        count++;
        if(count != approx_segments) {
            rad = ssd1306_DegToRad(count*approx_degree);
        } else {
            rad = ssd1306_DegToRad(loc_sweep);
        }
        xp2 = x + (int8_t)(sinf(rad)*radius);
        yp2 = y + (int8_t)(cosf(rad)*radius);    
        ssd1306_Line(dev,xp1,yp1,xp2,yp2,color);
    }
    
    return 0;
}
int ssd1306_DrawArcWithRadiusLine(const struct device *dev,uint8_t x, uint8_t y, uint8_t radius, uint16_t start_angle, uint16_t sweep, SSD1306_COLOR color){
const uint32_t CIRCLE_APPROXIMATION_SEGMENTS = 36;
    float approx_degree;
    uint32_t approx_segments;
    uint8_t xp1;
    uint8_t xp2 = 0;
    uint8_t yp1;
    uint8_t yp2 = 0;
    uint32_t count;
    uint32_t loc_sweep;
    float rad;
    
    loc_sweep = ssd1306_NormalizeTo0_360(sweep);
    
    count = (ssd1306_NormalizeTo0_360(start_angle) * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
    approx_segments = (loc_sweep * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
    approx_degree = loc_sweep / (float)approx_segments;

    rad = ssd1306_DegToRad(count*approx_degree);
    uint8_t first_point_x = x + (int8_t)(sinf(rad)*radius);
    uint8_t first_point_y = y + (int8_t)(cosf(rad)*radius);   
    while (count < approx_segments) {
        rad = ssd1306_DegToRad(count*approx_degree);
        xp1 = x + (int8_t)(sinf(rad)*radius);
        yp1 = y + (int8_t)(cosf(rad)*radius);    
        count++;
        if (count != approx_segments) {
            rad = ssd1306_DegToRad(count*approx_degree);
        } else {
            rad = ssd1306_DegToRad(loc_sweep);
        }
        xp2 = x + (int8_t)(sinf(rad)*radius);
        yp2 = y + (int8_t)(cosf(rad)*radius);    
        ssd1306_Line(dev,xp1,yp1,xp2,yp2,color);
    }
    
    // Radius line
    ssd1306_Line(dev,x,y,first_point_x,first_point_y,color);
    ssd1306_Line(dev,x,y,xp2,yp2,color);
    return 0;
}
int ssd1306_DrawCircle(const struct device *dev,uint8_t par_x, uint8_t par_y, uint8_t par_r, SSD1306_COLOR par_color)
{
    int32_t x = -par_r;
    int32_t y = 0;
    int32_t err = 2 - 2 * par_r;
    int32_t e2;

    if (par_x >= SSD1306_WIDTH || par_y >= SSD1306_HEIGHT) {
        return;
    }

    do {
        ssd1306_DrawPixel(dev,par_x - x, par_y + y, par_color);
        ssd1306_DrawPixel(dev,par_x + x, par_y + y, par_color);
        ssd1306_DrawPixel(dev,par_x + x, par_y - y, par_color);
        ssd1306_DrawPixel(dev,par_x - x, par_y - y, par_color);
        e2 = err;

        if (e2 <= y) {
            y++;
            err = err + (y * 2 + 1);
            if(-x == y && e2 <= x) {
                e2 = 0;
            }
        }

        if (e2 > x) {
            x++;
            err = err + (x * 2 + 1);
        }
    } while (x <= 0);

    return 0;
}
int ssd1306_FillCircle(const struct device *dev,uint8_t par_x,uint8_t par_y,uint8_t par_r,SSD1306_COLOR par_color)
{
 int32_t x = -par_r;
    int32_t y = 0;
    int32_t err = 2 - 2 * par_r;
    int32_t e2;

    if (par_x >= SSD1306_WIDTH || par_y >= SSD1306_HEIGHT) {
        return;
    }

    do {
        for (uint8_t _y = (par_y + y); _y >= (par_y - y); _y--) {
            for (uint8_t _x = (par_x - x); _x >= (par_x + x); _x--) {
                ssd1306_DrawPixel(dev,_x, _y, par_color);
            }
        }

        e2 = err;
        if (e2 <= y) {
            y++;
            err = err + (y * 2 + 1);
            if (-x == y && e2 <= x) {
                e2 = 0;
            }
        }

        if (e2 > x) {
            x++;
            err = err + (x * 2 + 1);
        }
    } while (x <= 0);

    return 0;
}
int ssd1306_Polyline(const struct device *dev,const SSD1306_VERTEX *par_vertex, uint16_t par_size, SSD1306_COLOR color)
{
    uint16_t i;
    if(par_vertex == NULL) {
        return 1;
    }

    for(i = 1; i < par_size; i++) {
        ssd1306_Line(dev,par_vertex[i - 1].x, par_vertex[i - 1].y, par_vertex[i].x, par_vertex[i].y, color);
    }

    return 0;
}




int ssd1306_DrawRectangle(const struct device *dev,uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color)
{
    ssd1306_Line(dev,x1,y1,x2,y1,color);
    ssd1306_Line(dev,x2,y1,x2,y2,color);
    ssd1306_Line(dev,x2,y2,x1,y2,color);
    ssd1306_Line(dev,x1,y2,x1,y1,color);

    return 0;
}
int ssd1306_FillRectangle(const struct device *dev,uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color)
{
   uint8_t x_start = ((x1<=x2) ? x1 : x2);
    uint8_t x_end   = ((x1<=x2) ? x2 : x1);
    uint8_t y_start = ((y1<=y2) ? y1 : y2);
    uint8_t y_end   = ((y1<=y2) ? y2 : y1);

    for (uint8_t y= y_start; (y<= y_end)&&(y<SSD1306_HEIGHT); y++) {
        for (uint8_t x= x_start; (x<= x_end)&&(x<SSD1306_WIDTH); x++) {
            ssd1306_DrawPixel(dev,x, y, color);
        }
    }
    return 0;
}

/**
 * @brief Invert color of pixels in rectangle (include border)
 * 
 * @param x1 X Coordinate of top left corner
 * @param y1 Y Coordinate of top left corner
 * @param x2 X Coordinate of bottom right corner
 * @param y2 Y Coordinate of bottom right corner
 * @return SSD1306_Error_t status
 */
SSD1306_Error_t ssd1306_InvertRectangle(const struct device *dev,uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
  if ((x2 >= SSD1306_WIDTH) || (y2 >= SSD1306_HEIGHT)) {
    return SSD1306_ERR;
  }
  if ((x1 > x2) || (y1 > y2)) {
    return SSD1306_ERR;
  }
  uint32_t i;
  if ((y1 / 8) != (y2 / 8)) {
    /* if rectangle doesn't lie on one 8px row */
    for (uint32_t x = x1; x <= x2; x++) {
      i = x + (y1 / 8) * SSD1306_WIDTH;
      SSD1306_Buffer[i] ^= 0xFF << (y1 % 8);
      i += SSD1306_WIDTH;
      for (; i < x + (y2 / 8) * SSD1306_WIDTH; i += SSD1306_WIDTH) {
        SSD1306_Buffer[i] ^= 0xFF;
      }
      SSD1306_Buffer[i] ^= 0xFF >> (7 - (y2 % 8));
    }
  } else {
    /* if rectangle lies on one 8px row */
    const uint8_t mask = (0xFF << (y1 % 8)) & (0xFF >> (7 - (y2 % 8)));
    for (i = x1 + (y1 / 8) * SSD1306_WIDTH;
         i <= (uint32_t)x2 + (y2 / 8) * SSD1306_WIDTH; i++) {
      SSD1306_Buffer[i] ^= mask;
    }
  }
  return SSD1306_OK;
}

int ssd1306_DrawBitmap(const struct device *dev,uint8_t x, uint8_t y, const unsigned char* bitmap, uint8_t w, uint8_t h, SSD1306_COLOR color)
{
    int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
    uint8_t byte = 0;

    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) {
        return;
    }

    for (uint8_t j = 0; j < h; j++, y++) {
        for (uint8_t i = 0; i < w; i++) {
            if (i & 7) {
                byte <<= 1;
            } else {
                byte = (*(const unsigned char *)(&bitmap[j * byteWidth + i / 8]));
            }

            if (byte & 0x80) {
                ssd1306_DrawPixel(dev,x + i, y, color);
            }
        }
    }
    return 0;
}

/**
 * @brief Sets the contrast of the display.
 * @param[in] value contrast to set.
 * @note Contrast increases as the value increases.
 * @note RESET = 7Fh.
 */
int ssd1306_SetContrast(const struct device *dev,const uint8_t value)
{
     int rt = 0;
    const uint8_t kSetContrastControlRegister = 0x81;
    rt = ssd1306_WriteCommand(dev,kSetContrastControlRegister);
    rt = ssd1306_WriteCommand(dev,value);

    return rt;
}

/**
 * @brief Set Display ON/OFF.
 * @param[in] on 0 for OFF, any for ON.
 */
int ssd1306_SetDisplayOn(const struct device *dev,const uint8_t on)
{
    uint8_t value;
    if (on) {
        value = 0xAF;   // Display on
        SSD1306.DisplayOn = 1;
    } else {
        value = 0xAE;   // Display off
        SSD1306.DisplayOn = 0;
    }
    int rt = ssd1306_WriteCommand(dev,value);

    return rt;
}

/**
 * @brief Reads DisplayOn state.
 * @return  0: OFF.
 *          1: ON.
 */
int ssd1306_GetDisplayOn(const struct device *dev)
{
 return SSD1306.DisplayOn;
}

// Low-level procedures
int ssd1306_Reset(const struct device *dev)
{

}
int ssd1306_WriteCommand(const struct device *dev,uint8_t byte)
{
    const struct ssd1306_config *cfg = dev->config;
	int ret = i2c_burst_write_dt(&cfg->i2c,0x00,&byte,1);
	return ret;
}
int ssd1306_WriteData(const struct device *dev,uint8_t* buffer, size_t buff_size)
{
   const struct ssd1306_config *cfg = dev->config;
	int ret = i2c_burst_write_dt(&cfg->i2c,0x40,buffer,buff_size);
	return ret;
}
SSD1306_Error_t ssd1306_FillBuffer(const struct device *dev,uint8_t* buf, uint32_t len)
{
    SSD1306_Error_t ret = SSD1306_ERR;
    if (len <= SSD1306_BUFFER_SIZE) {
        memcpy(SSD1306_Buffer,buf,len);
        ret = SSD1306_OK;
    }
    return ret;
}





static int ssd1306_Init(const struct device *dev)
{
int stat = 0;
	if(ssd1306_check_device_exists(dev))
	{
		stat = 1;
		goto err;
	}


    // Reset OLED
    ssd1306_Reset(dev);

    // Wait for the screen to boot
    k_msleep(100);

    // Init OLED
    ssd1306_SetDisplayOn(dev,0); //display off

    ssd1306_WriteCommand(dev,0x20); //Set Memory Addressing Mode
    ssd1306_WriteCommand(dev,0x00); // 00b,Horizontal Addressing Mode; 01b,Vertical Addressing Mode;
                                // 10b,Page Addressing Mode (RESET); 11b,Invalid

    ssd1306_WriteCommand(dev,0xB0); //Set Page Start Address for Page Addressing Mode,0-7

#ifdef SSD1306_MIRROR_VERT
    ssd1306_WriteCommand(dev,0xC0); // Mirror vertically
#else
    ssd1306_WriteCommand(dev,0xC8); //Set COM Output Scan Direction
#endif

    ssd1306_WriteCommand(dev,0x00); //---set low column address
    ssd1306_WriteCommand(dev,0x10); //---set high column address

    ssd1306_WriteCommand(dev,0x40); //--set start line address - CHECK

    ssd1306_SetContrast(dev,0xFF);

#ifdef SSD1306_MIRROR_HORIZ
    ssd1306_WriteCommand(dev,0xA0); // Mirror horizontally
#else
    ssd1306_WriteCommand(dev,0xA1); //--set segment re-map 0 to 127 - CHECK
#endif

#ifdef SSD1306_INVERSE_COLOR
    ssd1306_WriteCommand(dev,0xA7); //--set inverse color
#else
    ssd1306_WriteCommand(dev,0xA6); //--set normal color
#endif

// Set multiplex ratio.
#if (SSD1306_HEIGHT == 128)
    // Found in the Luma Python lib for SH1106.
    ssd1306_WriteCommand(dev,0xFF);
#else
    ssd1306_WriteCommand(dev,0xA8); //--set multiplex ratio(1 to 64) - CHECK
#endif

#if (SSD1306_HEIGHT == 32)
    ssd1306_WriteCommand(dev,0x1F); //
#elif (SSD1306_HEIGHT == 64)
    ssd1306_WriteCommand(dev,0x3F); //
#elif (SSD1306_HEIGHT == 128)
    ssd1306_WriteCommand(dev,0x3F); // Seems to work for 128px high displays too.
#else
#error "Only 32, 64, or 128 lines of height are supported!"
#endif

    ssd1306_WriteCommand(dev,0xA4); //0xa4,Output follows RAM content;0xa5,Output ignores RAM content

    ssd1306_WriteCommand(dev,0xD3); //-set display offset - CHECK
    ssd1306_WriteCommand(dev,0x00); //-not offset

    ssd1306_WriteCommand(dev,0xD5); //--set display clock divide ratio/oscillator frequency
    ssd1306_WriteCommand(dev,0xF0); //--set divide ratio

    ssd1306_WriteCommand(dev,0xD9); //--set pre-charge period
    ssd1306_WriteCommand(dev,0x22); //

    ssd1306_WriteCommand(dev,0xDA); //--set com pins hardware configuration - CHECK
#if (SSD1306_HEIGHT == 32)
    ssd1306_WriteCommand(dev,0x02);
#elif (SSD1306_HEIGHT == 64)
    ssd1306_WriteCommand(dev,0x12);
#elif (SSD1306_HEIGHT == 128)
    ssd1306_WriteCommand(dev,0x12);
#else
#error "Only 32, 64, or 128 lines of height are supported!"
#endif

    ssd1306_WriteCommand(dev,0xDB); //--set vcomh
    ssd1306_WriteCommand(dev,0x20); //0x20,0.77xVcc

    ssd1306_WriteCommand(dev,0x8D); //--set DC-DC enable
    ssd1306_WriteCommand(dev,0x14); //
    ssd1306_SetDisplayOn(dev,1); //--turn on SSD1306 panel
    k_msleep(100);
    // Clear screen
    ssd1306_Fill(dev,Black);
    
    // Flush buffer to screen
    ssd1306_UpdateScreen(dev);
    
    // Set default values for screen object
    SSD1306.CurrentX = 0;
    SSD1306.CurrentY = 0;
    
    SSD1306.Initialized = 1;

    
err:
	return stat;
}


#define INST_DT_SSD1306(index)                                                         \
    static struct ssd1306_data ssd1306_data_##index;                     \
	static const struct ssd1306_config ssd1306_config_##index = {			\
		.i2c = I2C_DT_SPEC_INST_GET(index),					\
	};										\
	DEVICE_DT_INST_DEFINE(index, ssd1306_Init, NULL, 				\
		    &ssd1306_data_##index,  \
		    &ssd1306_config_##index,						\
		    POST_KERNEL,							\
		    CONFIG_CUSTOM_SSD1306_INIT_PRIORITY,					\
		    NULL);

DT_INST_FOREACH_STATUS_OKAY(INST_DT_SSD1306);