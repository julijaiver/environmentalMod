#include <zephyr/kernel.h>
#include "utils/app_events.hpp"

K_EVENT_DEFINE(envisens_events_);

void AppEvents::boot_halt()
{
    k_event_post(&envisens_events_, BOOT_HALT);
}

void AppEvents::boot_continue()
{
    k_event_post(&envisens_events_, BOOT_CONTINUE);
}

void AppEvents::boot_wait()
{
    k_event_wait(&envisens_events_, BOOT_CONTINUE, false, K_FOREVER);
}

void AppEvents::clock_wait()
{
    k_event_wait(&envisens_events_, CLOCK_SET, false, K_FOREVER);
}

void AppEvents::clock_synced()
{
    k_event_post(&envisens_events_, CLOCK_SET);
}

void AppEvents::take_sample_now()
{
    k_event_post(&envisens_events_, SDI12_READ | RUUVI_READ);
}

uint32_t AppEvents::wait(uint32_t events, bool reset, k_timeout_t timeout)
{
    return k_event_wait(&envisens_events_, events, reset, timeout);
}

void AppEvents::post(uint32_t events)
{
    k_event_post(&envisens_events_, events);
}

void AppEvents::clear(uint32_t events)
{
    k_event_clear(&envisens_events_, events);
}
