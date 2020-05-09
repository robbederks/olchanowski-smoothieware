#include "StatusRGB.h"

#include "StreamOutputPool.h"
#include "modules/robot/Conveyor.h"
#include "SlowTicker.h"
#include "Config.h"
#include "checksumm.h"
#include "ConfigValue.h"

#define status_rgb_checksum        CHECKSUM("status_rgb")
#define enable_checksum            CHECKSUM("enable")
#define led_r_pin_checksum         CHECKSUM("led_r_pin")
#define led_g_pin_checksum         CHECKSUM("led_g_pin")
#define led_b_pin_checksum         CHECKSUM("led_b_pin")

StatusRGB::StatusRGB() {}

void StatusRGB::on_module_loaded()
{
    if(!THEKERNEL->config->value(status_rgb_checksum, enable_checksum)->by_default(false)->as_bool()) {
        delete this;
        return;
    }

    this->led_r.from_string(THEKERNEL->config->value(status_rgb_checksum, led_r_pin_checksum)->by_default("nc")->as_string())->as_output();
    this->led_g.from_string(THEKERNEL->config->value(status_rgb_checksum, led_g_pin_checksum)->by_default("nc")->as_string())->as_output();
    this->led_b.from_string(THEKERNEL->config->value(status_rgb_checksum, led_b_pin_checksum)->by_default("nc")->as_string())->as_output();

    THEKERNEL->slow_ticker->attach(4, this, &StatusRGB::tick);
}

// Not implemented PWM yet.
void StatusRGB::set_color(bool r, bool g, bool b){
    this->led_r.set(r);
    this->led_g.set(g);
    this->led_b.set(b);
}

// Check status of machine and set RGB accordingly
uint32_t StatusRGB::tick(uint32_t) {
    if(THEKERNEL->is_halted()){
        // Kernel is halted. Flash red quickly
        this->set_color((counter % 2), false, false);
    } else if(!THECONVEYOR->is_idle()){
        // Conveyor is not idle -> has queued up moves
        this->set_color(false, false, true);
    } else {
        this->set_color(false, true, false);
    }

    counter++;
    return 0;
}
