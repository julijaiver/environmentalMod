#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/uart_pipe.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/modem/backend/uart.h>
#include <zephyr/modem/pipe.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lora, CONFIG_MODEM_MODULES_LOG_LEVEL);

#include <stdio.h>
#include <string.h>
#include <time.h>


#define DEV_LORA DEVICE_DT_GET(DT_NODELABEL(uart0))

#define UART_RX_BUF_SIZE 700
#define UART_TX_BUF_SIZE 550

/* Buffers for modem backend */
struct modem_data
{
	struct
	{
		uint8_t uart_rx[UART_RX_BUF_SIZE];
		uint8_t uart_tx[UART_TX_BUF_SIZE];
	} buffers;

	struct modem_backend_uart mdm_uart_backend;
	struct modem_pipe *mdm_uart_pipe;

	struct k_event rcv_event;
};

static struct modem_data data;



typedef enum eventTypes { eEnter, eExit, eTick, eReceive } EventType;

typedef struct event_ {
	EventType ev; // event type (= what happened)
    int length;
	void *data;
    void (*handled)(void *); // pointer for callback
} event;

typedef struct smi_ smi;

typedef void (*smf)(smi *, const event *);  // prototype of state handler function pointer

#define SMI_BUFSIZE   UART_RX_BUF_SIZE  

struct smi_ {
	smf state;  // current state (function pointer)
	smf next_state; // next state (function pointer)
    int timer;
    int count;
    int pos;
    char buffer[SMI_BUFSIZE]; // buffer for received data
};

// macro for changing state. Correct operation requires that sm is a pointer to state machine instance (see state template)
#define TRAN(st) { sm->next_state = st; }

static const event evEnter = { eEnter, 0, NULL, NULL };
static const event evExit = { eExit, 0, NULL, NULL };
static const event evTick = { eTick, 0, NULL, NULL };

#if 0
/* this is state template */
void stStateTemplate(smi *sm, const event *e)
{
	switch(e->ev) {
	case eEnter:
		break;
	case eExit:
		break;
	case eTick:
		break;
	case eReceive:
		break;
	default:
		break;
	}
}
#endif


void stInit(smi *sm, const event *e);
void stMode(smi *sm, const event *e);
void stClass(smi *sm, const event *e);
void stDr(smi *sm, const event *e);
void stPort(smi *sm, const event *e);
void stJoin(smi *sm, const event *e);
void stConnected(smi *sm, const event *e);
void stClockSync(smi *sm, const event *e);

static int lora_set_system_time(const char *str);

void smClear(smi *sm) {
	sm->timer = 0;
	sm->count = 0;
	sm->pos = 0;
	sm->buffer[0] = '\0';
}

static void lora_tick(struct k_timer *timer);

K_MSGQ_DEFINE(lora_event_queue, sizeof(event), 20, 1);
K_TIMER_DEFINE(lora_tick_timer, lora_tick, NULL);

static void lora_tick(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	int rc = k_msgq_put(&lora_event_queue, &evTick, K_NO_WAIT);
}

/* Callback when modem pipe receives data */
static void modem_pipe_event_handler(struct modem_pipe *pipe, enum modem_pipe_event mdm_event,
									 void *user_data)
{
	event ev = { eReceive, 0, NULL, NULL };
	switch (mdm_event)
	{
	case MODEM_PIPE_EVENT_RECEIVE_READY:
		LOG_DBG("rcv ready");
		if(k_msgq_put(&lora_event_queue, &ev, K_NO_WAIT) < 0) {
			LOG_ERR("Event queue error");
		}
		break;

	case MODEM_PIPE_EVENT_TRANSMIT_IDLE:
		LOG_DBG("transmit idle");
		/* Can send more data if available */
		break;

	default:
		LOG_DBG("modem event: %d", mdm_event);
		break;
	}
}

static int init_modem_pipe(void)
{
	int ret;

	const struct modem_backend_uart_config uart_backend_config = {
		.uart = DEV_LORA,
		.receive_buf = data.buffers.uart_rx,
		.receive_buf_size = sizeof(data.buffers.uart_rx),
		.transmit_buf = data.buffers.uart_tx,
		.transmit_buf_size = sizeof(data.buffers.uart_tx),
	};
	k_event_init(&data.rcv_event);

	data.mdm_uart_pipe = modem_backend_uart_init(&data.mdm_uart_backend, &uart_backend_config);
	if (data.mdm_uart_pipe == NULL)
	{
		LOG_ERR("Failed to initialize modem backend");
		return -1;
	}

	modem_pipe_attach(data.mdm_uart_pipe, modem_pipe_event_handler, &data);

	ret = modem_pipe_open(data.mdm_uart_pipe, K_MSEC(100));
	if (ret < 0)
	{
		LOG_ERR("Failed to open modem pipe");
		return ret;
	}

	LOG_INF("Modem pipe initialized and opened");
	return 0;
}

int lora_init(void)
{
	int ret;

	LOG_INF("LoRa starting...");

	/* Verify modem device is ready */
	if (!device_is_ready(DEV_LORA))
	{
		LOG_ERR("LORA UART device not ready");
		return -ENODEV;
	}

	/* Initialize modem pipe */
	ret = init_modem_pipe();
	if (ret < 0)
	{
		LOG_ERR("Failed to initialize modem pipe: %d", ret);
		return ret;
	}

	LOG_INF("SDI12 pipe initialized");

	return 0;
}

int lora_flush(smi *sm)
{
	int count = 0;
	int rv = 0;

	do
	{
		rv = modem_pipe_receive(data.mdm_uart_pipe, sm->buffer, sizeof(sm->buffer));
		if (rv > 0)
		{
			LOG_INF("flushed: %d", rv);
			count += rv;
		}
	} while (rv > 0);

	sm->buffer[0] = '\0';
	sm->pos = 0;

	return count;
}


int lora_read(smi *sm) {
	int ret = -1;

	/* Read from modem pipe */
	//	do	{
	int remain = sizeof(sm->buffer) - sm->pos - 1; // leave space for NUL
	if (remain < 1)
		return -2; // buffer full

	char *buf = sm->buffer + sm->pos;
	ret = modem_pipe_receive(data.mdm_uart_pipe, buf, remain);
	if (ret > 0)
	{
		buf[ret] = 0;
		sm->pos += ret;
		LOG_HEXDUMP_DBG(buf, ret, "RCV:");	
	}
	//	} while (ret > 0); // do we need to repeat - this gets typically called on receive event

	return ret;
}

int lora_write(smi *sm, const char *str)
{
	(void) sm; // not used here just for consistency of the API
	return modem_pipe_transmit(data.mdm_uart_pipe, (uint8_t *) str, strlen(str));
}


int lora_wait_for(smi *sm, const char *expect[])
{
	int ret = -1; // return code for incomplete line 

	lora_read(sm);

	int dist = strcspn(sm->buffer, "\n");		
	if (dist < sm->pos) // there is a linefeed 
	{
		sm->buffer[dist] = '\0'; // get rid of linefeed
		LOG_INF("Response: %s", sm->buffer);
		for(int i = 0; expect[i] != NULL; ++i) {
			// break if expected string was found 
			if (strstr(sm->buffer, expect[i])) {
				ret = i;
				break;
			}
		}
		if(ret < 0) ret = -2; // return code for full line but no match

		// todo: moving can't be done before returning - we must let user handle the data after returning from this function
		if(sm->pos - dist > 1) { // we have unhandled data after the linefeed move it to beginning of buffer
			memmove(sm->buffer, sm->buffer + dist + 1, sm->pos - dist);
			sm->pos -= dist + 1;
			LOG_INF("Pending: %s", sm->buffer);
		}
		else {
			// receive ended with line feed --> everything has been handled
			sm->pos = 0;
			// the data is still left in the buffer so that caller can handle the completed line.
		} 

	}

	LOG_INF("Ridx: %d", ret);

	return ret;
}


static void lora_thread(void *p1, void *p2, void *p3)
{
	event ev;
	smi *sm; // need to initialixze this with something
	k_timer_start(&lora_tick_timer, K_MSEC(1000), K_MSEC(1000));

	while (true)
	{
		if (k_msgq_get(&lora_event_queue, &ev, K_FOREVER) == 0)
		{
			// handle event
			sm->state(sm, &ev); // dispatch event to current state
			if(sm->state != sm->next_state) { // check if state was changed
				sm->state(sm, &evExit); // exit old state (cleanup)
				sm->state = sm->next_state; // change state
				sm->state(sm, &evEnter); // enter new state
			}

		}
		else
		{
			LOG_ERR("No message");
		}
	}
}



void stInit(smi *sm, const event *e)
{
	const char *expect[] = { "+AT: OK", NULL };

	switch(e->ev) {
	case eEnter:
		lora_flush(sm);
		smClear(sm);
		break;
	case eExit:
		break;
	case eTick:
		// send AT
		lora_write(sm, "AT\r\n");
		break;
	case eReceive:
		if(lora_wait_for(sm, expect)==0) {
			TRAN(stMode);
		}
		break;
	default:
		break;
	}
}

void stMode(smi *sm, const event *e)
{
	const char *expect[] = { "+MODE: LWOTAA", NULL };

	switch(e->ev) {
	case eEnter:
		lora_flush(sm);
		smClear(sm);
		lora_write(sm, "AT+MODE=LWOTAA\r\n");
		break;
	case eExit:
		break;
	case eTick:
		if(++sm->timer > 3) {
			TRAN(stInit);
		}
		break;
	case eReceive:
		if(lora_wait_for(sm, expect)==0) {
			TRAN(stMode);
		}
		break;
	default:
		break;
	}
}

void stClass(smi *sm, const event *e)
{
	const char *expect[] = { "+CLASS: A", NULL };

	switch(e->ev) {
	case eEnter:
		lora_flush(sm);
		smClear(sm);
		lora_write(sm, "AT+CLASS=A\r\n");
		break;
	case eExit:
		break;
	case eTick:
		if(++sm->timer > 3) {
			TRAN(stInit);
		}
		break;
	case eReceive:
		if(lora_wait_for(sm, expect)==0) {
			TRAN(stDr);
		}
		break;
	default:
		break;
	}
}


void stDr(smi *sm, const event *e)
{
	const char *expect[] = { "+DR: DR5", NULL };

	switch(e->ev) {
	case eEnter:
		lora_flush(sm);
		smClear(sm);
		lora_write(sm, "AT+DR=5\r\n");
		break;
	case eExit:
		break;
	case eTick:
		if(++sm->timer > 3) {
			TRAN(stInit);
		}
		break;
	case eReceive:
		if(lora_wait_for(sm, expect)==0) {
			TRAN(stPort);
		}
		break;
	default:
		break;
	}
}



void stPort(smi *sm, const event *e)
{
	const char *expect[] = { "+PORT: 1", NULL };

	switch(e->ev) {
	case eEnter:
		lora_flush(sm);
		smClear(sm);
		lora_write(sm, "AT+PORT=1\r\n");
		break;
	case eExit:
		break;
	case eTick:
		if(++sm->timer > 3) {
			TRAN(stInit);
		}
		break;
	case eReceive:
		if(lora_wait_for(sm, expect)==0) {
			TRAN(stJoin);
		}
		break;
	default:
		break;
	}
}

void stJoin(smi *sm, const event *e)
{
	const char *expect[] = { "+JOIN: Join failed", "+JOIN: LoRaWAN modem is busy", "+JOIN: Done", "+JOIN: Joined already", NULL };
	int res = -1;

	switch(e->ev) {
	case eEnter:
		lora_flush(sm);
		smClear(sm);
		lora_write(sm, "AT+JOIN\r\n");
		break;
	case eExit:
		break;
	case eTick:
		if(++sm->timer == 30) {
			// try again
			lora_write(sm, "AT+JOIN\r\n");
			sm->timer = 0;
		}
		break;
	case eReceive:
		res = lora_wait_for(sm, expect);
		if(res == 0) {
			// failed
		}
		else if(res == 1) {
			// modem is busy
		}
		else if(res == 2 || res == 3) {
			TRAN(stClockSync);
		}
		break;
	default:
		break;
	}
}

void stClockSync(smi *sm, const event *e)
{
	const char *expect[] = { "+LW: DTR", "+CMSGHEX: Done", "+RTC:", NULL };
	int res = -1;

	switch(e->ev) {
	case eEnter:
		lora_flush(sm);
		smClear(sm);
		lora_write(sm, "AT+LW=DTR\r\n");
		break;
	case eExit:
		break;
	case eTick:
		if(++sm->timer > 20) {
			lora_write(sm, "AT+LW=DTR\r\n");
			smClear(sm);
		}
		break;
	case eReceive:
		res = lora_wait_for(sm, expect);
		if(res == 0) {
			lora_write(sm, "AT+CMSGHEX\r\n"); // send dummy message to activate downlink
		}
		else if(res == 1) {
			lora_write(sm, "AT+RTC=UTCMS\r\n");
		}
		else if(res == 2) {
			char *tstr = strstr(sm->buffer, "+RTC:") + 5;
			if(sscanf(tstr, "%d", &res) == 1 && res >= 2026) {
				if(lora_set_system_time(tstr)==0) 
				TRAN(stConnected);
			}
		}
		break;
	default:
		break;
	}

}

void stConnected(smi *sm, const event *e)
{

	switch(e->ev) {
	case eEnter:
		lora_flush(sm);
		smClear(sm);
		LOG_INF("Lora connected");
		break;
	case eExit:
		break;
	case eTick:
		break;
	case eReceive:
		break;
	default:
		break;
	}
}



static int lora_set_system_time(const char *str)
{
	struct tm now;
	if(!str) return -1;
 
    int year, month, day, hour, min, sec, msec;
	//Format:  " 2026-04-30 12:30:49.465"
    int parsed = sscanf(str, "%2d-%2d-%2d %2d:%2d:%2d.%d", &year, &month, &day, &hour, &min, &sec, &msec);

    if(parsed != 7) {
        LOG_ERR("Invalid time format");
        return -2;
    }

    now.tm_year = year - 1900; // RTC uses years sence 1900
    now.tm_mon = month - 1; // starts from 0
    now.tm_mday = day;
    now.tm_hour = hour;
    now.tm_min = min;
    now.tm_sec = sec;
    now.tm_isdst = 0;

    time_t epoch = mktime(&now);

    if(epoch == (time_t)-1){
        LOG_ERR("Invalid time struct");
        return -3;
    }

    struct timespec ts = {
        .tv_sec = epoch,
        .tv_nsec = (long) msec * 1000000L
    };

    if(clock_settime(CLOCK_REALTIME, &ts) != 0){
        LOG_ERR("Failed to set system time");
		return -4;
    } else {
        LOG_INF("system time set successfully");
    }

	return 0;
}
