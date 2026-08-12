#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(modem_transport);

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/base64.h>

#include "transport/modem_transport.hpp"
#include "utils/system_clock.hpp"

extern "C" {
#include "private.h"   
#include "cJSON.h"
}

static constexpr int FULL_REQUEST_SIZE = 1024 * 15;
static constexpr int REQUEST_BODY_SIZE = 1024 * 12;
static constexpr int ACCESS_TOKEN_BUF = 1024 * 4;

#define CLOUD_REQUEST_TEMPLATE \
    "POST /v1/projects/" GOOGLE_CLOUD_PROJECT_ID "/topics/" GOOGLE_CLOUD_TOPIC_ID ":publish HTTP/1.1\r\n" \
    "Host: " CLOUD_HOST "\r\n" \
    "Content-Type: " CLOUD_CONTENT_TYPE "\r\n" \
    "Content-Length: %u\r\n" \
    "Authorization: " CLOUD_AUTHORIZATION " %s\r\n\r\n" \
    "%s\r\n"

#define CLOUD_REQUEST_BODY "{\"messages\":[{\"data\":\"%s\"}]}"


ModemTransport::ModemTransport()
    : modem_gpio_(GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), modem_pin_gpios)) , access_token_(nullptr)
{
}

int ModemTransport::init()
{
    if (!device_is_ready(modem_gpio_.port)) {
        LOG_ERR("Modem GPIO port not ready");
        return -EIO;
    }
    int rc = gpio_pin_configure_dt(&modem_gpio_, GPIO_OUTPUT_ACTIVE);
    if (rc != 0) {
        LOG_ERR("Modem GPIO configure failed: %d", rc);
        return rc;
    }
    return 0;
}


void ModemTransport::power_on()
{
    gpio_pin_set_dt(&modem_gpio_, 1);
    k_msleep(POWER_ON_DELAY_MS);
}

void ModemTransport::power_off()
{
    at("AT+CPOF", "OK", K_MSEC(BOOT_AT_TIMEOUT_MS));
    k_msleep(1000 * 10);
    gpio_pin_set_dt(&modem_gpio_, 0);
}


bool ModemTransport::at(const char *cmd, const char *expect, k_timeout_t timeout)
{
    return at_len(cmd, strlen(cmd), expect, timeout);
}

bool ModemTransport::at_len(const char *cmd, size_t len, const char *expect, k_timeout_t timeout)
{
    char response[AT_RESPONSE_SIZE];

    uart_.write(reinterpret_cast<const uint8_t *>(cmd), len);
    uart_.write(reinterpret_cast<const uint8_t *>("\r\n"), 2);

    while (uart_.read_line(response, sizeof(response), timeout)) {
        LOG_DBG("%s", response);
        if (strstr(response, expect)  != nullptr) return true;
        if (strstr(response, "ERROR") != nullptr) return false;
    }
    return false;
}

int ModemTransport::modem_init()
{
    if (!at("AT", "OK", K_MSEC(BOOT_AT_TIMEOUT_MS)))
        return -EIO;


    if (at("AT+CPIN?", "+CPIN: SIM PIN", K_MSEC(BOOT_AT_TIMEOUT_MS))) {
        if (!at("AT+CPIN=\"1234\"", "OK", K_MSEC(BOOT_AT_TIMEOUT_MS)))
            return -EACCES;
    }
    k_msleep(1000);
    return 0;
}

int ModemTransport::modem_get_time(char *buf, size_t size)
{
    char response[AT_RESPONSE_SIZE];
    bool time_ok = false;
    bool got_ok  = false;

    if (!buf || size == 0) return -EINVAL;
    buf[0] = '\0';

    uart_.write(reinterpret_cast<const uint8_t *>("AT+CCLK?\r\n"), sizeof("AT+CCLK?\r\n") - 1);

    while (uart_.read_line(response, sizeof(response), K_MSEC(BOOT_AT_TIMEOUT_MS))) {
        LOG_DBG("%s", response);

        if (strstr(response, "ERROR")) {
            snprintf(buf, size, "ERROR");
            return -EIO;
        }

        const char *start = strstr(response, "+CCLK: ");
        if (start) {
            const char *q1 = strchr(start, '"');
            if (q1) {
                const char *q2 = strchr(q1 + 1, '"');
                if (q2) {
                    size_t tlen = (size_t)(q2 - (q1 + 1));
                    if (tlen < size) {
                        memcpy(buf, q1 + 1, tlen);
                        buf[tlen] = '\0';
                        time_ok = true;
                    }
                }
            }
        }
        if (strstr(response, "OK")) { 
            got_ok = true; 
            break; 
        }
    }

    if (time_ok && got_ok) return 0;
    snprintf(buf, size, "ERROR");
    return -EIO;
}

int ModemTransport::boot_setup()
{
    if (uart_.init(DEVICE_DT_GET(DT_NODELABEL(uart0))) != 0) {
        LOG_ERR("UART init failed");
        return -EIO;
    }

    power_on();
    k_msleep(NET_SETTLE_DELAY_MS);

    // Initialise modem with retries
    int tries = 0;
    int rc    = modem_init();
    while (rc != 0 && tries < MAX_INIT_TRIES) {
        LOG_WRN("Modem init failed, retry %d/%d", tries + 1, MAX_INIT_TRIES);
        k_msleep(RETRY_DELAY_MS);
        rc = modem_init();
        ++tries;
    }
    if (rc != 0) {
        LOG_ERR("Modem init failed after %d tries", MAX_INIT_TRIES);
        power_off();
        return rc;
    }

    k_msleep(NET_SETTLE_DELAY_MS);

    char time_str[32];
    rc = modem_get_time(time_str, sizeof(time_str));
    if (rc == 0) {
        SystemClock::set(time_str);   // parses string, calls clock_settime + CLOCK_SYNCED
    } else {
        LOG_ERR("Failed to sync clock from modem");
    }

    power_off();
    return rc;
}

int ModemTransport::connect()
{
    power_on();
    k_msleep(STARTUP_DELAY_MS);

    int tries = 0;
    int rc = modem_init();
    while (rc != 0 && tries < MAX_INIT_TRIES) {
        k_msleep(RETRY_DELAY_MS);
        rc = modem_init();
        ++tries;
    }
    if (rc != 0) {
        LOG_ERR("Modem init failed");
        power_off();
        return rc;
    }

    rc = fetch_access_token();
    if (rc != 0) {
        LOG_ERR("Access token fetch failed: %d", rc);
        power_off();
        return rc;
    }
    return 0;
}

int ModemTransport::disconnect()
{
    if (access_token_) {
        k_free(access_token_);
        access_token_ = nullptr;
    }
    power_off();
}

int ModemTransport::max_payload()
{
    return REQUEST_BODY_SIZE / 2;
}

int ModemTransport::send_log(const uint8_t *buf, int len)
{
    return send(buf, len);
}

int ModemTransport::http_start()
{
    if (!at("AT+HTTPINIT", "OK", K_MSEC(BOOT_AT_TIMEOUT_MS)))
        return -EIO;
    return 0;
}

void ModemTransport::http_stop()
{
    at("AT+HTTPTERM", "OK", K_MSEC(BOOT_AT_TIMEOUT_MS));
}

int ModemTransport::http_post(const char *url, const char *content_type,
                               const char *body, size_t body_len)
{
    char cmd[1024 * 2];
    size_t cmd_len = sizeof(cmd);

    if (body_len > cmd_len - 64) return -E2BIG;

    snprintf(cmd, cmd_len, "AT+HTTPPARA=\"URL\", %s", url);
    if (!at(cmd, "OK", K_MSEC(BOOT_AT_TIMEOUT_MS))) return -EIO;

    snprintf(cmd, cmd_len, "AT+HTTPPARA=\"CONTENT\", %s", content_type);
    if (!at(cmd, "OK", K_MSEC(BOOT_AT_TIMEOUT_MS))) return -EIO;

    snprintf(cmd, cmd_len, "AT+HTTPDATA=%u, 10000", (unsigned)body_len);
    if (!at(cmd, "DOWNLOAD", K_MSEC(BOOT_AT_TIMEOUT_MS))) return -EIO;

    snprintf(cmd, cmd_len, "%s", body);
    if (!at(cmd, "OK", K_MSEC(BOOT_AT_TIMEOUT_MS))) return -EIO;

    if (!at("AT+HTTPACTION=1", "200", K_MSEC(BOOT_AT_TIMEOUT_MS * 6)))
        return -EIO;

    return 0;
}

int ModemTransport::http_read_access_token(char *out, size_t out_size)
{
    char response[1024 * 2];
    char cmd[50];
    char *full = (char *)k_malloc(1024 * 2);
    if (!full) return -ENOMEM;
    full[0] = '\0';

    int response_len = 0;

    uart_.write(reinterpret_cast<const uint8_t *>("AT+HTTPREAD?\r\n"),sizeof("AT+HTTPREAD?\r\n") - 1);
    while (uart_.read_line(response, sizeof(response), K_MSEC(BOOT_AT_TIMEOUT_MS))) {
        if (strstr(response, "ERROR")) { k_free(full); return -EIO; }
        if (strstr(response, "+HTTPREAD:")) {
            sscanf(response, "+HTTPREAD: LEN,%d", &response_len);
        }
    }

    snprintf(cmd, sizeof(cmd), "AT+HTTPREAD=0,%d", response_len);
    if (!at(cmd, "OK", K_MSEC(BOOT_AT_TIMEOUT_MS))) { k_free(full); return -EIO; }

    while (uart_.read_line(response, sizeof(response), K_MSEC(BOOT_AT_TIMEOUT_MS))) {
        if (strstr(response, "ERROR")) { k_free(full); return -EIO; }
        if (!strstr(response, "+HTTPREAD:")) strcat(full, response);
    }

    delete_char(full, '\n');
    cJSON *json = cJSON_Parse(full);
    k_free(full);
    if (!json) return -EBADMSG;

    cJSON *token = cJSON_GetObjectItemCaseSensitive(json, "access_token");
    if (!token) { cJSON_Delete(json); return -EBADMSG; }

    snprintf(out, out_size, "%s", token->valuestring);
    cJSON_Delete(json);
    return 0;
}


int ModemTransport::fetch_access_token()
{
    char *jwt = jwt_.build();
    if (!jwt) return -EIO;

    char *body = (char *)k_malloc(ACCESS_TOKEN_BUF);
    if (!body) { 
        k_free(jwt); 
        return -ENOMEM; 
    }

    snprintf(body, ACCESS_TOKEN_BUF,
             "{\"grant_type\": \"urn:ietf:params:oauth:grant-type:jwt-bearer\","
             " \"assertion\": \"%s\"}", jwt);
    k_free(jwt);

    size_t body_len = strlen(body);

    if (http_start() != 0) { 
        k_free(body); 
        return -EIO; 
    }

    int rc = http_post(ACCESS_TOKEN_URL, ACCESS_TOKEN_CONTENT_TYPE, body, body_len);
    k_free(body);

    if (rc != 0) { 
        http_stop(); 
        return rc; 
    }

    access_token_ = (char *)k_malloc(ACCESS_TOKEN_BUF);
    if (!access_token_) { 
        http_stop(); 
        return -ENOMEM; 
    }

    rc = http_read_access_token(access_token_, ACCESS_TOKEN_BUF);
    http_stop();

    if (rc != 0) {
        k_free(access_token_);
        access_token_ = nullptr;
    }
    return rc;
}

int ModemTransport::tcp_open()
{
    if (!at("AT+NETOPEN?", "+NETOPEN: 1", K_MSEC(BOOT_AT_TIMEOUT_MS))) {
        if (!at("AT+NETOPEN", "OK", K_MSEC(BOOT_AT_TIMEOUT_MS)))
            return -EIO;
    }
    if (!at("AT+CSSLCFG=\"sslversion\", 0,3", "OK", K_MSEC(BOOT_AT_TIMEOUT_MS))) return -EIO;
    if (!at("AT+CSSLCFG=\"authmode\",0,0", "OK", K_MSEC(BOOT_AT_TIMEOUT_MS))) return -EIO;
    if (!at("AT+CCHSET=1", "OK", K_MSEC(BOOT_AT_TIMEOUT_MS))) return -EIO;
    if (!at("AT+CCHSTART", "OK", K_MSEC(BOOT_AT_TIMEOUT_MS))) return -EIO;
    if (!at("AT+CCHSSLCFG=0,0",  "OK", K_MSEC(BOOT_AT_TIMEOUT_MS))) return -EIO;
    if (!at("AT+CCHOPEN=0,\"pubsub.googleapis.com\",443,2", "OK", K_MSEC(BOOT_AT_TIMEOUT_MS * 6))) return -EIO;
    if (!at("AT+CCHOPEN?", "OK", K_MSEC(BOOT_AT_TIMEOUT_MS))) return -EIO;
    return 0;
}

void ModemTransport::tcp_close()
{
    at("AT+CCHCLOSE=0", "OK", K_MSEC(BOOT_AT_TIMEOUT_MS));
    at("AT+CCHSTOP", "OK", K_MSEC(BOOT_AT_TIMEOUT_MS));
    at("AT+NETCLOSE", "OK", K_MSEC(BOOT_AT_TIMEOUT_MS));
}

void ModemTransport::tcp_send_chunked(const char *buf, size_t len)
{
    char cmd[64];
    for (size_t i = 0; i < len; i += 2048) {
        size_t chunk = (len - i >= 2048) ? 2048 : (len - i);
        snprintf(cmd, sizeof(cmd), "AT+CCHSEND=0, %u", (unsigned)chunk);
        at(cmd, ">", K_MSEC(BOOT_AT_TIMEOUT_MS));
        k_msleep(100);
        at_len(buf + i, chunk, "OK", K_MSEC(BOOT_AT_TIMEOUT_MS));
    }
}

int ModemTransport::send(const uint8_t *buf, int len)
{
    if (!access_token_) return -ENOENT;

    char *encoded = (char *)k_calloc(FULL_REQUEST_SIZE, 1);
    char *body    = (char *)k_calloc(REQUEST_BODY_SIZE,  1);
    if (!encoded || !body) {
        k_free(encoded);
        k_free(body);
        return -ENOMEM;
    }

    // base64-encode payload into encoded (use as temp buffer)
    size_t enc_len = 0;
    if (base64_encode((uint8_t *)encoded, (size_t)FULL_REQUEST_SIZE, &enc_len,
                      buf, (size_t)len)) {
        k_free(encoded); k_free(body);
        return -ENOMEM;
    }
    encoded[enc_len] = '\0';

    // Form Pub/Sub JSON body  {\"messages\":[{\"data\":\"<b64>\"}]}
    snprintf(body, (size_t)REQUEST_BODY_SIZE, CLOUD_REQUEST_BODY, encoded);
    size_t body_len = strlen(body);

    // Form full HTTP/1.1 POST request (reuse encoded buffer)
    snprintf(encoded, (size_t)FULL_REQUEST_SIZE,
             CLOUD_REQUEST_TEMPLATE, body_len, access_token_, body);
    size_t req_len = strlen(encoded);
    k_free(body);

    int rc = tcp_open();
    if (rc == 0) {
        tcp_send_chunked(encoded, req_len);
        tcp_close();
    }
    k_free(encoded);
    return rc;
}

char *ModemTransport::delete_char(char *s, char ch)
{
    int i = 0, j = 0;
    int len = (int)strlen(s);
    for (; i < len; ++i) {
        if (s[i] != ch) {
            s[j++] = s[i];
        }
    }
    s[j] = '\0';
    return s;
}
