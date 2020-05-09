#ifndef _STATUS_RGB_H
#define _STATUS_RGB_H

#include "libs/Kernel.h"
#include "libs/Pin.h"


class StatusRGB : public Module {
public:
    StatusRGB();

    void on_module_loaded(void);

private:
    uint32_t tick(uint32_t);
    void set_color(bool r, bool g, bool b);
    uint32_t counter;
    Pin  led_r;
    Pin  led_g;
    Pin  led_b;
};

#endif /* _STATUS_RGB_H */
