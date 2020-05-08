/*
 * SSD1322.cpp
 *
 *  Created on: 05-05-2020
 *      Author: Robbe Derks
 */

#include "SSD1322.h"
#include "fonts/ssd1322font.h"
#include "Kernel.h"
#include "platform_memory.h"
#include "Config.h"
#include "checksumm.h"
#include "StreamOutputPool.h"
#include "ConfigValue.h"

//definitions for lcd
#define FONT_SIZE_X 8
#define FONT_SIZE_Y 8
#define FB_SIZE(width, heigth) (width*heigth/2) // 4 bits per pixel

#define panel_checksum             CHECKSUM("panel")
#define power_en_pin_checksum      CHECKSUM("power_en_pin")
#define spi_channel_checksum       CHECKSUM("spi_channel")
#define spi_cs_pin_checksum        CHECKSUM("spi_cs_pin")
#define spi_frequency_checksum     CHECKSUM("spi_frequency")
#define encoder_a_pin_checksum     CHECKSUM("encoder_a_pin")
#define encoder_b_pin_checksum     CHECKSUM("encoder_b_pin")
#define click_button_pin_checksum  CHECKSUM("click_button_pin")
#define back_button_pin_checksum   CHECKSUM("back_button_pin")
#define buzz_pin_checksum          CHECKSUM("buzz_pin")
#define buzz_type_simple_checksum  CHECKSUM("buzz_type_simple")
#define contrast_checksum          CHECKSUM("contrast")
#define reverse_checksum           CHECKSUM("reverse")
#define rst_pin_checksum           CHECKSUM("rst_pin")
#define a0_pin_checksum            CHECKSUM("a0_pin")

#define CLAMP(x, low, high) { if ( (x) < (low) ) x = (low); if ( (x) > (high) ) x = (high); } while (0);
#define swap(a, b) { uint8_t t = a; a = b; b = t; }

SSD1322::SSD1322() {
    this->reversed = false;
    this->contrast = 255;
    this->width = 256;
    this->height = 64;

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

    this->click_pin.from_string(THEKERNEL->config->value( panel_checksum, click_button_pin_checksum )->by_default("nc")->as_string())->as_input();
    this->encoder_a_pin.from_string(THEKERNEL->config->value( panel_checksum, encoder_a_pin_checksum)->by_default("nc")->as_string())->as_input();
    this->encoder_b_pin.from_string(THEKERNEL->config->value( panel_checksum, encoder_b_pin_checksum)->by_default("nc")->as_string())->as_input();

    this->buzz_pin.from_string(THEKERNEL->config->value( panel_checksum, buzz_pin_checksum)->by_default("nc")->as_string())->as_output();
    this->buzz_type_simple = THEKERNEL->config->value( panel_checksum, buzz_type_simple_checksum)->by_default("false")->as_bool();
    if(this->buzz_pin.connected()) this->buzz_pin.set(this->buzz_pin.is_inverting() ? 1 : 0);

    // contrast override
    this->contrast = THEKERNEL->config->value(panel_checksum, contrast_checksum)->by_default(this->contrast)->as_number();

    // reverse display
    this->reversed = THEKERNEL->config->value(panel_checksum, reverse_checksum)->by_default(this->reversed)->as_bool();

    framebuffer = (uint8_t *)AHB0.alloc(FB_SIZE(this->width, this->height)); // grab some memory from USB_RAM
    if(framebuffer == NULL) {
        THEKERNEL->streams->printf("Not enough memory available for frame buffer");
    }

}

SSD1322::~SSD1322()
{
    delete this->spi;
    AHB0.dealloc(framebuffer);
}

void SSD1322::send_command(const unsigned char cmd){
    cs.set(0);
    if(a0.connected()) a0.set(this->a0.is_inverting() ? 1 : 0);
    spi->write(cmd);
    cs.set(1);
}

//send data to lcd
void SSD1322::send_data(const unsigned char *buf, size_t size)
{
    cs.set(0);
    if(a0.connected()) a0.set(this->a0.is_inverting() ? 0 : 1);
    while(size-- > 0) {
        spi->write(*buf++);
    }
    cs.set(1);
    if(a0.connected()) a0.set(this->a0.is_inverting() ? 1 : 0);
}

void SSD1322::send_command_with_args(const unsigned char cmd, const unsigned char* args, size_t arg_size){
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
void SSD1322::clear()
{
    memset(framebuffer, 0, FB_SIZE(this->width, this->height));
    this->tx = 0;
    this->ty = 0;
    this->text_color = 255;
    this->text_background = true;
}

void SSD1322::send_pic(const unsigned char *data)
{
    for (int i = 0; i < this->height; i++) {
        set_xy(0, i);
        send_data(data + (i * this->width / 2), this->width / 2);
    }
}


#define BUFFER_OFFSET_X 112
#define BUFFER_OFFSET_Y 0

// Set column and page number
// Note that they will be capped to the col/seg, not the actual pixel!
void SSD1322::set_xy(int x, int y) {
    // Clamp x and y to the valid range
    CLAMP(x, 0, this->width - 1);
    CLAMP(y, 0, this->height - 1);

    // Add offsets
    x += BUFFER_OFFSET_X;
    y += BUFFER_OFFSET_Y;

    unsigned char args[2];

    // column
    args[0] = x / 4;
    args[1] = (this->width + BUFFER_OFFSET_X) / 4;
    send_command_with_args(0x15, args, 2);

    // row
    args[0] = y;
    args[1] = this->height;
    send_command_with_args(0x75, args, 2);

    // enable MCU to write data into RAM
    send_command(0x5C);
}

void SSD1322::setCursor(uint8_t col, uint8_t row)
{
    this->tx = col * FONT_SIZE_X;
    this->ty = row * FONT_SIZE_Y;
}

void SSD1322::setCursorPX(int x, int y)
{
    this->tx = x;
    this->ty = y;
}

void SSD1322::setColor(int c)
{
    this->text_color = c > 0 ? 255 : 0;
}

void SSD1322::setBackground(bool bg)
{
    this->text_background = bg;
}

void SSD1322::home()
{
    this->tx = 0;
    this->ty = 0;
}

void SSD1322::display()
{
    ///nothing
}

void SSD1322::init()
{
    if(this->rst.connected()) {
        rst.set(this->rst.is_inverting() ? 1 : 0);
        wait_ms(20);
        rst.set(this->rst.is_inverting() ? 0 : 1);
    }

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

    clear();
}

void SSD1322::setContrast(uint8_t c) {
    unsigned char args[1] = {c};
    this->contrast = c;
    send_command_with_args(0xC1, args, 1);
}

/**
* @brief Draws a character to the screen buffer
* @param x   X coordinate
* @param y   Y coordinate
* @param c   Character to print
* @param color Drawing mode for foreground.
* @param bg  True: Draw background, False: Transparent background)
*/
int SSD1322::drawChar(int x, int y, unsigned char c, int color, bool bg)
{
    int retVal = -1;

    if (c == '\n') {
        this->ty += FONT_SIZE_Y;
        retVal = -(this->tx);
    } else if (c == '\r') {
        retVal = -(this->tx);
    } else {
        int char_start = ((c - 32) * FONT_SIZE_X);
        int index = (y * this->width / 2) + (x / 2);
        bool first_pixel = ((x % 2) == 0);
        uint8_t mask = first_pixel ? 0xF0 : 0x0F;

        for(uint8_t i=0; i<FONT_SIZE_X; i++){
            for(uint8_t j=0; j<FONT_SIZE_Y; j++){
                if(ssd1322_font[char_start + i] & (1 << j)){
                    framebuffer[index + (j * this->width / 2)] &= ~(mask);
                    framebuffer[index + (j * this->width / 2)] |= (mask & (first_pixel ? (color << 4) : color));
                } else if(bg){
                    framebuffer[index + (j * this->width / 2)] &= ~(mask);
                }
            }
            first_pixel = !first_pixel;
            mask = first_pixel ? 0xF0 : 0x0F;
            if((x+i)%2)
                index++;
        }

        retVal = FONT_SIZE_X;
        this->tx += FONT_SIZE_X;
    }

    return retVal;
}

//write single char to screen
void SSD1322::write_char(char value)
{
    drawChar(this->tx, this->ty, value, this->text_color, this->text_background);
}

void SSD1322::write(const char *line, int len)
{
    for (int i = 0; i < len; ++i) {
        write_char(line[i]);
    }
}

//refreshing screen
void SSD1322::on_refresh(bool now)
{
    static int refresh_counts = 0;
    refresh_counts++;
    // 10Hz refresh rate
    if(now || refresh_counts % 2 == 0 ) {
        send_pic(framebuffer);
    }
}

//reading button state
uint8_t SSD1322::readButtons(void)
{
    uint8_t state = 0;
    state |= (this->click_pin.get() ? BUTTON_SELECT : 0);
    return state;
}

int SSD1322::readEncoderDelta()
{
    static int8_t enc_states[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    static uint8_t old_AB = 0;
    if(this->encoder_a_pin.connected()) {
        old_AB <<= 2;                   //remember previous state
        old_AB |= ( this->encoder_a_pin.get() + ( this->encoder_b_pin.get() * 2 ) );  //add current state
        return  enc_states[(old_AB & 0x0f)];

    } else {
        return 0;
    }
}

void SSD1322::bltGlyph(int x, int y, int w, int h, const uint8_t *glyph, int span, int x_offset, int y_offset)
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

void SSD1322::renderGlyph(int x, int y, const uint8_t *g, int w, int h)
{
    CLAMP(x, 0, this->width - 1);
    CLAMP(y, 0, this->height - 1);
    CLAMP(w, 0, this->width - x);
    CLAMP(h, 0, this->height - y);

    int graphics_byte_index = 0;
    for(int i = 0; i < w; i++) {
        for(int j = 0; j < h; j++) {
            graphics_byte_index = (i/8) + (j * (w/8));
            pixel(x + i, y + j, g[graphics_byte_index] & (1 << (i%8)) ? text_color : text_background );
        }
    }
}

void SSD1322::pixel(int x, int y, int color) {
    int index = (y * this->width / 2) + (x / 2);
    bool first_pixel = ((x % 2) == 0);
    uint8_t mask = first_pixel ? 0xF0 : 0x0F;
    framebuffer[index] &= ~(mask);
    framebuffer[index] |= (mask & (first_pixel ? (color << 4) : color));
}

void SSD1322::drawHLine(int x, int y, int w, int color)
{
    CLAMP(w, 0, this->width - x);
    color &= 0x0F;

    int index = (y * this->width / 2) + (x / 2);
    if((x % 2) == 1){
        framebuffer[index] &= 0x0F;
        framebuffer[index] |= color;
        index++;
    }

    for (int i = 0; i < (w/2); i++) {
        framebuffer[index + i] = (color << 4) & color;
    }

    if(((x+w) % 2) == 1){
        framebuffer[index + (x+w)/2] &= 0xF0;
        framebuffer[index + (x+w)/2] |= (color << 4);
    }
}

void SSD1322::drawVLine(int x, int y, int h, int color){
    CLAMP(h, 0, this->height - y);

    int index = (y * this->width / 2) + (x / 2);
    bool first_pixel = ((x % 2) == 0);
    uint8_t mask = first_pixel ? 0xF0 : 0x0F;
    for (int i = 0; i < h; i++) {
        framebuffer[index] &= ~(mask);
        framebuffer[index] |= (mask & (first_pixel ? (color << 4) : color));
        index += (this->width / 2);
    }
}

void SSD1322::drawBox(int x, int y, int w, int h, int color) {
    for (int i = 0; i < w; i++) {
        drawVLine(x + i, y, h, color);
    }
}

// cycle the buzzer pin at a certain frequency (hz) for a certain duration (ms)
// if buzz_type_simple is true, we don't modulate the pin
void SSD1322::buzz(long duration, uint16_t freq)
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

