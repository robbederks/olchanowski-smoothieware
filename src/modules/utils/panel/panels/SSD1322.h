/*
 * SSD1322.h
 *
 *  Created on: 05-05-2020
 *      Author: Robbe Derks
 */

#ifndef SSD1322_H_
#define SSD1322_H_

#include "LcdBase.h"
#include "mbed.h"
#include "libs/Pin.h"

class SSD1322: public LcdBase {
public:
    SSD1322();
    virtual ~SSD1322();
    void home();
    void clear();
    void display();
    void setCursor(uint8_t col, uint8_t row);
    void init();
    void write_char(char value);
    void write(const char* line, int len);

    void on_refresh(bool now=false);
    //encoder which dosent exist :/
    uint8_t readButtons();
    int readEncoderDelta();
    int getEncoderResolution() { return 2; }
    uint16_t get_screen_lines() { return 7; }
    bool hasGraphics() { return true; }

    void send_command(const unsigned char cmd);
    void send_command_with_args(const unsigned char cmd, const unsigned char* args, size_t arg_size);
    void send_data(const unsigned char* buf, size_t size);
    // set column and page number
    void set_xy(int x, int y);
    //send pic to whole screen
    void send_pic(const unsigned char* data);
    //drawing char
    int drawChar(int x, int y, unsigned char c, int color, bool bg);
    // blit a glyph of w pixels wide and h pixels high to x, y. offset pixel position in glyph by x_offset, y_offset.
    // span is the width in bytes of the src bitmap
    // The glyph bytes will be 8 bits of X pixels, msbit->lsbit from top left to bottom right
    void bltGlyph(int x, int y, int w, int h, const uint8_t *glyph, int span= 0, int x_offset=0, int y_offset=0);
    void renderGlyph(int x, int y, const uint8_t *g, int pixelWidth, int pixelHeight);
    void drawByte(int index, uint8_t mask, int color);
    void pixel(int x, int y, int color);
    void setCursorPX(int x, int y);
    void setColor(int c);
    void setBackground(bool bg);
    void drawHLine(int x, int y, int w, int color);
    void drawVLine(int x, int y, int h, int color);
    void drawBox(int x, int y, int w, int h, int color);

    uint8_t getContrast() { return contrast; }
    void setContrast(uint8_t c);

    void buzz(long duration, uint16_t freq);

private:
    unsigned char *framebuffer;
    mbed::SPI* spi;
    Pin power_en_pin;
    Pin cs;
    Pin rst;
    Pin a0;
    Pin click_pin;
    Pin buzz_pin;
    bool buzz_type_simple;
    Pin encoder_a_pin;
    Pin encoder_b_pin;
    uint8_t tx, ty;
    uint8_t text_color = 0xF;
    bool text_background = true;
    uint8_t contrast;
    uint16_t width;
    uint16_t height;
    bool reversed = false;
};

#endif /* SSD1322_H_ */
