/*
 * ST7565.cpp
 *
 *  Created on: 21-06-2013
 *      Author: Wulfnor
 */

#include "ST7565.h"
#include "fonts/glcdfont.h"
#include "Kernel.h"
#include "platform_memory.h"
#include "Config.h"
#include "checksumm.h"
#include "StreamOutputPool.h"
#include "ConfigValue.h"

//definitions for lcd
#define FONT_SIZE_X 6
#define FONT_SIZE_Y 8
#define LCDPAGES(heigth) ((height+7)/8)
#define FB_SIZE(width, heigth) (width*LCDPAGES(heigth))

#define panel_checksum             CHECKSUM("panel")
#define power_en_pin_checksum      CHECKSUM("power_en_pin")
#define spi_channel_checksum       CHECKSUM("spi_channel")
#define spi_cs_pin_checksum        CHECKSUM("spi_cs_pin")
#define spi_frequency_checksum     CHECKSUM("spi_frequency")
#define encoder_a_pin_checksum     CHECKSUM("encoder_a_pin")
#define encoder_b_pin_checksum     CHECKSUM("encoder_b_pin")
#define click_button_pin_checksum  CHECKSUM("click_button_pin")
#define up_button_pin_checksum     CHECKSUM("up_button_pin")
#define down_button_pin_checksum   CHECKSUM("down_button_pin")
#define pause_button_pin_checksum  CHECKSUM("pause_button_pin")
#define back_button_pin_checksum   CHECKSUM("back_button_pin")
#define buzz_pin_checksum          CHECKSUM("buzz_pin")
#define buzz_type_simple_checksum  CHECKSUM("buzz_type_simple")
#define contrast_checksum          CHECKSUM("contrast")
#define reverse_checksum           CHECKSUM("reverse")
#define rst_pin_checksum           CHECKSUM("rst_pin")
#define a0_pin_checksum            CHECKSUM("a0_pin")
#define red_led_checksum           CHECKSUM("red_led_pin")
#define blue_led_checksum          CHECKSUM("blue_led_pin")

#define CLAMP(x, low, high) { if ( (x) < (low) ) x = (low); if ( (x) > (high) ) x = (high); } while (0);
#define swap(a, b) { uint8_t t = a; a = b; b = t; }

ST7565::ST7565(uint8_t variant) {
    is_viki2 = false;
    is_mini_viki2 = false;
    is_ssd1306= false;
    is_sh1106= false;
    is_ssd1322= false;

    // set the variant
    switch(variant) {
        case 1:
            is_viki2 = true;
            this->reversed = true;
            this->contrast = 9;
            this->width = 128;
            this->height = 64;
            break;
        case 2:
            is_mini_viki2 = true;
            this->reversed = true;
            this->contrast = 18;
            this->width = 128;
            this->height = 64;
            break;
        case 3: // SSD1306 OLED
            // set default for sub variants
            is_ssd1306= true;
            this->reversed = false;
            this->contrast = 9;
            this->width = 128;
            this->height = 64;
            break;
        case 4: // SH1106 OLED
            // set default for sub variants
            is_sh1106= true;
            //is_ssd1306= true;
            this->reversed = false;
            this->contrast = 15;
            this->width = 132;
            this->height = 64;
            break;
        case 5: // SH1322 OLED
            // set default for sub variants
            is_ssd1322 = true;
            this->reversed = false;
            this->contrast = 255;
            this->width = 256;
            this->height = 64;
            break;
       default:
            // set default for sub variants
            this->reversed = false;
            this->contrast = 9;
            break;
    }

    // Power enable
    this->power_en_pin.from_string(THEKERNEL->config->value(panel_checksum, power_en_pin_checksum)->by_default("nc")->as_string())->as_output();
    if(this->power_en_pin.connected()) this->power_en_pin.set(this->power_en_pin.is_inverting() ? 0 : 1);
    if(this->power_en_pin.connected()) {
        if(this->power_en_pin.is_inverting()){
            THEKERNEL->streams->printf("Setting power pin to 0\n");
        } else {
            THEKERNEL->streams->printf("Setting power pin to 1\n");
        }
    } 

    // SPI com
    // select which SPI channel to use
    int spi_channel = THEKERNEL->config->value(panel_checksum, spi_channel_checksum)->by_default(0)->as_number();
    PinName mosi, miso, sclk;
    if(spi_channel == 0) {
        mosi = P0_18; miso = P0_17; sclk = P0_15;
    } else if(spi_channel == 1) {
        mosi = P0_9; miso = P0_8; sclk = P0_7;
    } else {
        mosi = P0_18; miso = P0_17; sclk = P0_15;
    }

    this->spi = new mbed::SPI(mosi, miso, sclk);
    this->spi->frequency(THEKERNEL->config->value(panel_checksum, spi_frequency_checksum)->by_default(1000000)->as_number()); //4Mhz freq, can try go a little lower

    //chip select
    this->cs.from_string(THEKERNEL->config->value( panel_checksum, spi_cs_pin_checksum)->by_default("0.16")->as_string())->as_output();
    cs.set(1);

    //lcd reset
    this->rst.from_string(THEKERNEL->config->value( panel_checksum, rst_pin_checksum)->by_default("nc")->as_string())->as_output();
    if(this->rst.connected()) rst.set(this->rst.is_inverting() ? 0 : 1);

    //a0
    this->a0.from_string(THEKERNEL->config->value( panel_checksum, a0_pin_checksum)->by_default("nc")->as_string())->as_output();
    if(a0.connected()) a0.set(this->a0.is_inverting() ? 0 : 1);

    if(!is_viki2 && !is_mini_viki2 && !is_ssd1306 && !is_ssd1322) {
        this->up_pin.from_string(THEKERNEL->config->value( panel_checksum, up_button_pin_checksum )->by_default("nc")->as_string())->as_input();
        this->down_pin.from_string(THEKERNEL->config->value( panel_checksum, down_button_pin_checksum )->by_default("nc")->as_string())->as_input();
    } else {
        this->up_pin.from_string("nc");
        this->down_pin.from_string("nc");
    }

    this->aux_pin.from_string("nc");
    if(is_viki2) {
        // the aux pin can be pause or back on a viki2
        string aux_but = THEKERNEL->config->value( panel_checksum, pause_button_pin_checksum )->by_default("nc")->as_string();
        if(aux_but != "nc") {
            this->aux_pin.from_string(aux_but)->as_input();
            this->use_pause = true;
            this->use_back = false;

        } else {
            aux_but = THEKERNEL->config->value( panel_checksum, back_button_pin_checksum )->by_default("nc")->as_string();
            if(aux_but != "nc") {
                this->aux_pin.from_string(aux_but)->as_input();
                this->use_back = true;
                this->use_pause = false;
            }
        }
    }

    this->click_pin.from_string(THEKERNEL->config->value( panel_checksum, click_button_pin_checksum )->by_default("nc")->as_string())->as_input();
    this->encoder_a_pin.from_string(THEKERNEL->config->value( panel_checksum, encoder_a_pin_checksum)->by_default("nc")->as_string())->as_input();
    this->encoder_b_pin.from_string(THEKERNEL->config->value( panel_checksum, encoder_b_pin_checksum)->by_default("nc")->as_string())->as_input();

    this->buzz_pin.from_string(THEKERNEL->config->value( panel_checksum, buzz_pin_checksum)->by_default("nc")->as_string())->as_output();
    this->buzz_type_simple = THEKERNEL->config->value( panel_checksum, buzz_type_simple_checksum)->by_default("false")->as_bool();
    if(this->buzz_pin.connected()) this->buzz_pin.set(this->buzz_pin.is_inverting() ? 1 : 0);

    if(is_viki2) {
        this->red_led.from_string(THEKERNEL->config->value( panel_checksum, red_led_checksum)->by_default("nc")->as_string())->as_output();
        this->blue_led.from_string(THEKERNEL->config->value( panel_checksum, blue_led_checksum)->by_default("nc")->as_string())->as_output();
        this->red_led.set(false);
        this->blue_led.set(true);
    }

    // contrast override
    this->contrast = THEKERNEL->config->value(panel_checksum, contrast_checksum)->by_default(this->contrast)->as_number();

    // reverse display
    this->reversed = THEKERNEL->config->value(panel_checksum, reverse_checksum)->by_default(this->reversed)->as_bool();

    framebuffer = (uint8_t *)AHB0.alloc(FB_SIZE(this->width, this->height)); // grab some memory from USB_RAM
    if(framebuffer == NULL) {
        THEKERNEL->streams->printf("Not enough memory available for frame buffer");
    }

}

ST7565::~ST7565()
{
    delete this->spi;
    AHB0.dealloc(framebuffer);
}

//send commands to lcd
void ST7565::send_commands(const unsigned char *buf, size_t size)
{
    cs.set(0);
    if(a0.connected()) a0.set(this->a0.is_inverting() ? 1 : 0);
    while(size-- > 0) {
        spi->write(*buf++);
    }
    cs.set(1);
}

void ST7565::send_command(const unsigned char cmd){
    cs.set(0);
    if(a0.connected()) a0.set(this->a0.is_inverting() ? 1 : 0);
    spi->write(cmd);
    cs.set(1);
}

//send data to lcd
void ST7565::send_data(const unsigned char *buf, size_t size)
{
    cs.set(0);
    if(a0.connected()) a0.set(this->a0.is_inverting() ? 0 : 1);
    while(size-- > 0) {
        spi->write(*buf++);
    }
    cs.set(1);
    if(a0.connected()) a0.set(this->a0.is_inverting() ? 1 : 0);
}

void ST7565::send_command_with_args(const unsigned char cmd, const unsigned char* args, size_t arg_size){
    cs.set(0);

    // command mode
    if(a0.connected()) a0.set(this->a0.is_inverting() ? 1 : 0);
    spi->write(cmd);

    // data mode
    if(a0.connected()) a0.set(this->a0.is_inverting() ? 0 : 1);
    while(arg_size-- > 0) {
        spi->write(*args++);
    }

    cs.set(1);
}

//clearing screen
void ST7565::clear()
{
    memset(framebuffer, 0, FB_SIZE(this->width, this->height));
    this->tx = 0;
    this->ty = 0;
    this->text_color = 1;
    this->text_background = true;
}

void ST7565::send_pic(const unsigned char *data)
{
    for (int i = 0; i < LCDPAGES(this->height); i++) {
        set_xy(0, i);
        send_data(data + i * this->width, this->width);
    }
}

// set column and page number
void ST7565::set_xy(int x, int y)
{
    CLAMP(x, 0, this->width - 1);
    CLAMP(y, 0, LCDPAGES(this->height) - 1);

    if(is_ssd1306) {
        unsigned char cmd[6];
        cmd[0] = 0x21;    // set column
        cmd[1] = x;       // start = col
        cmd[2] = 0x7F;    // end = col max
        cmd[3] = 0x22;    // set row
        cmd[4] = y;      // start = row
        cmd[5] = 0x07;    // end = row max
        send_commands(cmd, 6);
    } else if(is_ssd1322) {
        unsigned char args[2];

        // column
        args[0] = x + 28;                  // start = col + col_offset
        args[1] = this->width + 28;        // end = col max + col_offset
        send_command_with_args(0x15, args, 2);

        // page
        args[0] = y;                       // start = row
        args[1] = LCDPAGES(this->height);  // end = row max
        send_command_with_args(0x75, args, 2);

        // enable MCU to write data into RAM
        send_command(0x5C);
    } else {
        unsigned char cmd[3];
        cmd[0] = 0xb0 | (y & 0x07);
        cmd[1] = 0x10 | (x >> 4);
        cmd[2] = 0x00 | (x & 0x0f);
        send_commands(cmd, 3);
    }
}

void ST7565::setCursor(uint8_t col, uint8_t row)
{
    this->tx = col * 6;
    this->ty = row * 8;
}

void ST7565::setCursorPX(int x, int y)
{
    this->tx = x;
    this->ty = y;
}

void ST7565::setColor(int c)
{
    this->text_color = c;
}

void ST7565::setBackground(bool bg)
{
    this->text_background = bg;
}

void ST7565::home()
{
    this->tx = 0;
    this->ty = 0;
}

void ST7565::display()
{
    ///nothing
}

void ST7565::init()
{
    if(this->rst.connected()) {
        rst.set(this->rst.is_inverting() ? 1 : 0);
        wait_ms(20);
        rst.set(this->rst.is_inverting() ? 0 : 1);
    }

    if(is_ssd1306) {
        const unsigned char init_seq[] = {
            0xAE,  // display off
            0xD5,  // clock
            0x81,  // upper nibble is rate, lower nibble is divisor

            0xA8,  // mux ratio
            0x3F,  // rtfm

            0xD3,  // display offset
            0x00,  // rtfm
            0x00,

            0x8D,  // charge pump
            0x14,  // enable

            0x20,  // memory addr mode
            0x00,  // horizontal

            0xA1,  // segment remap

            0xA5,  // display on

            0xC8,  // com scan direction
            0xDA,  // com hardware cfg
            0x12,  // alt com cfg

            0x81,  // contrast aka current
            0x7F,  // 128 is midpoint

            0xD9,  // precharge
            0x11,  // rtfm

            0xDB,  // vcomh deselect level
            0x20,  // rtfm

            0xA6,  // non-inverted

            0xA4,  // display scan on
            0xAF,  // drivers on
        };
        send_commands(init_seq, sizeof(init_seq));
    } else if(is_ssd1322) {
        unsigned char args[2];

        // unlock IC
        args[0] = 0x12;
        send_command_with_args(0xFD, args, 1);

        // display off
        send_command(0xAE);

        // clock
        args[0] = 0x91;
        send_command_with_args(0xB3, args, 1);

        // mux ratio
        args[0] = 0x3F;
        send_command_with_args(0xCA, args, 1);

        // display offset
        args[0] = 0x00;
        send_command_with_args(0xA2, args, 1);

        // display start line
        args[0] = 0x00;
        send_command_with_args(0xA1, args, 1);

        // set remap & dual COM line
        args[0] = 0x14;
        args[1] = 0x11;
        send_command_with_args(0xA0, args, 2);

        // disable GPIO
        args[0] = 0x00;
        send_command_with_args(0xB5, args, 1);

        // use internal vdd
        args[0] = 0x01;
        send_command_with_args(0xAB, args, 1);

        // display enhancement A
        args[0] = 0xA0;
        args[1] = 0xFD;
        send_command_with_args(0xB4, args, 2);

        // normal contrast
        args[0] = 0x9F;
        send_command_with_args(0xC1, args, 1);

        // master contrast
        args[0] = 0x0F;
        send_command_with_args(0xC7, args, 1);

        // default greyscale table
        send_command(0xB9);

        // phase length
        args[0] = 0xE2;
        send_command_with_args(0xB1, args, 1);

        // display enhancement B (reset)
        args[0] = 0x82;
        args[1] = 0x20;
        send_command_with_args(0xD1, args, 2);

        // pre-charge voltage
        args[0] = 0x1F;
        send_command_with_args(0xBB, args, 1);

        // 2nd precharge period
        args[0] = 0x08;
        send_command_with_args(0xB6, args, 1);

        // set VcomH
        args[0] = 0x00;
        send_command_with_args(0xBE, args, 1);

        // normal display (reset)
        send_command(0xA6);

        // exit sleep
        send_command(0xAF);
    } else {
        const unsigned char init_seq[] = {
            0x40,    //Display start line 0
            (unsigned char)((reversed^is_sh1106) ? 0xa0 : 0xa1), // ADC
            (unsigned char)(reversed ? 0xc8 : 0xc0), // COM select
            0xa6,    //Display normal
            0xa2,    //Set Bias 1/9 (Duty 1/65)
            0x2f,    //Booster, Regulator and Follower On
            0xf8,    //Set internal Booster to 4x
            0x00,
            0x27,    //Contrast set
            0x81,
            this->contrast,    //contrast value
            0xac,    //No indicator
            0x00,
            0xaf,    //Display on
        };
        send_commands(init_seq, sizeof(init_seq));
    }

    clear();
}

void ST7565::setContrast(uint8_t c)
{
    if(is_ssd1322){
        unsigned char args[1] = {c};
        this->contrast = c;
        send_command_with_args(0xC1, args, 1);
    } else {
        const unsigned char contrast_seq[] = {
            0x27,    //Contrast set
            0x81,
            c    //contrast value
        };
        this->contrast = c;
        send_commands(contrast_seq, sizeof(contrast_seq));
    }
}

/**
* @brief Draws a character to the screen buffer
* @param x   X coordinate
* @param y   Y coordinate
* @param c   Character to print
* @param color Drawing mode for foreground.
* @param bg  True: Draw background, False: Transparent background)
*/
int ST7565::drawChar(int x, int y, unsigned char c, int color, bool bg)
{
    int retVal = -1;

    if (c == '\n') {
        this->ty += 8;
        retVal = -tx;
    } else if (c == '\r') {
        retVal = -tx;
    } else {
        for (uint8_t i = 0; i < 5; i++ ) {
            if (x < this->width) {     // Guard against drawing off screen
                // Character glyph may cross two screen pages
                int page = y / 8;
                // Draw the first byte
                if (page < LCDPAGES(this->height)) {
                    int screenIndex = page * this->width + x;
                    uint8_t fontByte = glcd_font[(c * 5) + i] << (y % 8);
                    if (bg) drawByte(screenIndex, 0xFF << (y % 8), !color);
                    drawByte(screenIndex, fontByte, color);
                }
                // Draw the second byte
                page++;
                if (page < LCDPAGES(this->height)) {
                    int screenIndex = page * this->width + x;
                    uint8_t fontByte = glcd_font[(c * 5) + i] >> (8 - (y % 8));
                    if (bg) drawByte(screenIndex, 0xFF >> (8 - (y % 8)), !color);
                    drawByte(screenIndex, fontByte, color);
                }
                x++;
            }
        }
        retVal = 6;
        this->tx += 6;
    }

    return retVal;
}

//write single char to screen
void ST7565::write_char(char value)
{
    drawChar(this->tx, this->ty, value, this->text_color, this->text_background);
}

void ST7565::write(const char *line, int len)
{
    for (int i = 0; i < len; ++i) {
        write_char(line[i]);
    }
}

//refreshing screen
void ST7565::on_refresh(bool now)
{
    static int refresh_counts = 0;
    refresh_counts++;
    // 10Hz refresh rate
    if(now || refresh_counts % 2 == 0 ) {
        send_pic(framebuffer);
    }
}

//reading button state
uint8_t ST7565::readButtons(void)
{
    uint8_t state = 0;
    state |= (this->click_pin.get() ? BUTTON_SELECT : 0);
    if(this->up_pin.connected()) {
        state |= (this->up_pin.get() ? BUTTON_UP : 0);
        state |= (this->down_pin.get() ? BUTTON_DOWN : 0);
    }
    if(this->aux_pin.connected() && this->aux_pin.get()) {
        if(this->use_pause) state |= BUTTON_PAUSE;
        else if(this->use_back) state |= BUTTON_LEFT;
    }

    return state;
}

int ST7565::readEncoderDelta()
{
    static int8_t enc_states[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    static uint8_t old_AB = 0;
    if(this->encoder_a_pin.connected()) {
        // mviki
        old_AB <<= 2;                   //remember previous state
        old_AB |= ( this->encoder_a_pin.get() + ( this->encoder_b_pin.get() * 2 ) );  //add current state
        return  enc_states[(old_AB & 0x0f)];

    } else {
        return 0;
    }
}

void ST7565::bltGlyph(int x, int y, int w, int h, const uint8_t *glyph, int span, int x_offset, int y_offset)
{
    if(x_offset == 0 && y_offset == 0 && span == 0) {
        // blt the whole thing
        renderGlyph(x, y, glyph, w, h);

    } else {
        // copy portion of glyph into g where x_offset is left byte aligned
        // Note currently the x_offset must be byte aligned
        int n = w / 8; // bytes per line to copy
        if(w % 8 != 0) n++; // round up to next byte
        uint8_t g[n * h];
        uint8_t *dst = g;
        const uint8_t *src = &glyph[y_offset * span + x_offset / 8];
        for (int i = 0; i < h; ++i) {
            memcpy(dst, src, n);
            dst += n;
            src += span;
        }
        renderGlyph(x, y, g, w, h);
    }
}

void ST7565::renderGlyph(int x, int y, const uint8_t *g, int w, int h)
{
    CLAMP(x, 0, this->width - 1);
    CLAMP(y, 0, this->height - 1);
    CLAMP(w, 0, this->width - x);
    CLAMP(h, 0, this->height - y);

    for(int i = 0; i < w; i++) {
        for(int j = 0; j < h; j++) {
            pixel(x + i, y + j, g[(i / 8) + j * ((w - 1) / 8 + 1)] & (1 << (7 - i % 8)));
        }
    }
}

void ST7565::drawByte(int index, uint8_t mask, int color)
{
    int shift = (is_sh1106)? 2 : 0; // 2 pixels as border on wide OLED
    index += shift;
    if (color == 1) {
        framebuffer[index] |= mask;
    } else if (color == 0) {
        framebuffer[index] &= ~mask;
    } else {
        framebuffer[index] ^= mask;
    }
}

void ST7565::pixel(int x, int y, int color)
{
    int page = y / 8;
    unsigned char mask = 1 << (y % 8);
    drawByte(page * this->width + x, mask, color);
}

void ST7565::drawHLine(int x, int y, int w, int color)
{
    int page = y / 8;
    uint8_t mask = 1 << (y % 8);
    for (int i = 0; i < w; i++) {
        drawByte(page * this->width + x + i, mask, color);
    }
}

void ST7565::drawVLine(int x, int y, int h, int color){
    int page = y / 8;
    if (page >= LCDPAGES(this->height)) return;
    // First byte. Start with all on and shift to turn of the
    // bits before the start of the line
    int startbit = y % 8;
    uint8_t mask = 0xff << startbit;
    // Account for when the start and end of the line fall on the
    // same byte
    if (h < 8) {
        mask &= 0xff >> (8 - (startbit + h));
    }
    drawByte(page * this->width + x, mask, color);
    h -= 8 - (y % 8);
    // Draw any completely filled bytes along the line
    while (h > 8) {
        page++;
        if (page >= LCDPAGES(this->height)) return;
        mask = 0xff;
        drawByte(page * this->width + x, mask, color);
        h -= 8;
    }
    page++;
    if (page >= LCDPAGES(this->height)) return;
    // Last byte. Start filled and shift by 8 - number of pixels remaining
    mask = 0xff >> (8 - h);
    drawByte(page * this->width + x, mask, color);
}

void ST7565::drawBox(int x, int y, int w, int h, int color) {
    for (int i = 0; i < w; i++) {
        drawVLine(x + i, y, h, color);
    }
}

// cycle the buzzer pin at a certain frequency (hz) for a certain duration (ms)
// if buzz_type_simple is true, we don't modulate the pin
void ST7565::buzz(long duration, uint16_t freq)
{
    if(!this->buzz_pin.connected()) return;

    long elapsed_time = 0;
    if(this->buzz_type_simple) {
        this->buzz_pin.set(this->buzz_pin.is_inverting() ? 0 : 1);
        wait_ms(duration);
        this->buzz_pin.set(this->buzz_pin.is_inverting() ? 1 : 0);
    } else {
        long period = 1000000 / freq; // period in us        
        duration *= 1000; //convert from ms to us
        while (elapsed_time < duration) {
            this->buzz_pin.set(1);
            wait_us(period / 2);
            this->buzz_pin.set(0);
            wait_us(period / 2);
            elapsed_time += (period);
        }
    }
}

void ST7565::setLed(int led, bool onoff)
{
    if(!is_viki2) return;

    if(led == LED_HOT) {
        if(onoff)  {
            red_led.set(true);
            blue_led.set(false);
        } else {
            red_led.set(false);
            blue_led.set(true);
        }
    }
}

