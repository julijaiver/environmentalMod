#ifndef COMMON_H_INCLUDED_
#define COMMON_H_INCLUDED_

extern struct k_event envisens_events;

#define BOOT_HALT_EVENT      1
#define BOOT_CONTINUE_EVENT  2
#define CLOCK_SET_EVENT      4
#define TAKE_SAMPLE_EVENT    8

#define BOOT_WAIT()   { k_event_wait(&envisens_events, BOOT_CONTINUE_EVENT, false, K_FOREVER); }
#define CLOCK_WAIT()   { k_event_wait(&envisens_events, CLOCK_SET_EVENT, false, K_FOREVER); }
#define CLOCK_SYNCED() { k_event_post(&envisens_events, CLOCK_SET_EVENT); }
#define TAKE_SAMPLE_NOW() { k_event_post(&envisens_events, TAKE_SAMPLE_EVENT); }

void boot_halt(void);
void boot_continue(void);

#define MEASURE_CYCLE_MINUTES   10

#endif

