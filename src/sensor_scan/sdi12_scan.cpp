#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sdi12_scan);

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <array>
#include "sensor_scan/sdi12_scan.hpp"
#include "utils/app_events.hpp"

static constexpr int SDI12_STACKSIZE = 8192;
static constexpr int SDI12_THREAD_PRIORITY = 7;

K_THREAD_STACK_DEFINE(sdi12_stack, SDI12_STACKSIZE);

void SDI12Scanner::thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    static_cast<SDI12Scanner *>(p1)->run(); //pointer to thread function
}

static void read_teros_solyx_sn(char *dst, size_t size, const char *src)
{
    if (strlen(src) < 17) { *dst = '\0'; return; }
    strncpy(dst, src + 17, size);
    dst[size - 1] = '\0';
}

static void read_solinst_sn(char *dst, size_t size, const char *src)
{
    if (size < 4 || strlen(src) < 17) { *dst = '\0'; return; }
    strcpy(dst, "SOL");
    size -= 4;
    src  += 17;
    for (int i = 0; *src && i < 5; ++i) ++src;
    while (*src == ' ') ++src;
    strncat(dst, src, size);
}

SDI12Scanner::SDI12Scanner(SDI12Bus &bus, RuuviScanner &ruuvi, 
                            Topic<teros12_data> &teros_topic, 
                            Topic<solyx14_data> &solyx_topic, 
                            Topic<solinst_data> &solinst_topic) 
                            : bus_(bus), ruuvi_(ruuvi), 
                            pub_teros_(teros_topic), pub_solyx_(solyx_topic), 
                            pub_solinst_(solinst_topic)
{
    k_thread_create(&thread_, sdi12_stack, K_THREAD_STACK_SIZEOF(sdi12_stack),
                    thread_entry, this, nullptr, nullptr, SDI12_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&thread_, "sdi12_scan");
}

//thread run func
void SDI12Scanner::run()
{
    bus_.init();

    AppEvents::boot_wait();

    sensor_info sensors[MAX_SENSORS] = {};
    bus_.power_on();
    LOG_INF("SDI12 power on");
    int sensor_count = scan_sensors(sensors, MAX_SENSORS);
    bus_.power_off();
    LOG_INF("SDI12 power off");

    AppEvents::clock_wait();

    while (true) {
        AppEvents::wait(AppEvents::SDI12_READ, false, K_FOREVER);
        AppEvents::clear(AppEvents::SDI12_READ);

        bus_.power_on();
        LOG_INF("SDI12 power on");
        for (int i = 0; i < sensor_count; ++i) {
            switch (sensors[i].type) {
            case SensorType::Teros12: 
                read_teros12(sensors[i]); 
                break;
            case SensorType::Solyx14: 
                read_solyx14(sensors[i]); 
                break;
            case SensorType::Solinst: 
                read_solinst(sensors[i]); 
                break;
            default: 
                break;
            }
        }
        bus_.power_off();
        LOG_INF("SDI12 power off");
    }
}

int SDI12Scanner::scan_sensors(sensor_info *sensors, int max)
{
    struct known_sensor {
        const char *id;
        SensorType type;
        void (*sn_reader)(char *, size_t, const char *);
    };

    static constexpr std::array<known_sensor, 3> known = {{
        { "METER   TER12 ", SensorType::Teros12, read_teros_solyx_sn},
        { "METER   SLYX14", SensorType::Solyx14, read_teros_solyx_sn },
        { "SOLINST ", SensorType::Solinst, read_solinst_sn },
    }};

    int sid = 0;
    for (int i = 0; i <= 9 && sid < max; ++i) {
        char response[64];
        char cmd[] = "0I!";
        cmd[0] = '0' + i;
        bus_.cmd(cmd, true);

        if (bus_.wait_for(response, sizeof(response), nullptr) >= 25) {
            response[strcspn(response, "\r\n")] = '\0';

            for (const auto &kn : known) {
                const char *str = strstr(response, kn.id);
                if (str) {
                    sensors[sid].addr = cmd[0];
                    sensors[sid].type = kn.type;
                    kn.sn_reader(sensors[sid].id, sizeof(sensors[sid].id), str);
                    ++sid;
                    break;
                }
            }
        }
    }
    return sid;
}

int SDI12Scanner::read_teros12(const sensor_info &s)
{
    char response[128];
    char cmd[]    = "0R0!";  cmd[0]    = s.addr;
    char expect[] = "0R0!0"; expect[0] = s.addr; expect[4] = s.addr;

    bus_.cmd(cmd, true);
    if (bus_.wait_for(response, sizeof(response), expect) <= 0) {
        LOG_ERR("teros12: response timeout");
        return -1;
    }

    const char *rsp = strstr(response, expect);
    if (!rsp) { LOG_ERR("teros12: no match"); return -1; }

    teros12_data data = {};
    strncpy(data.id, s.id, sizeof(data.id) - 1);
    int rv = sscanf(rsp + strlen(expect), "%f%f%f",
                    &data.vwc, &data.temp, &data.ec);
    if (rv != 3) { LOG_ERR("teros12: parse failed"); return -1; }

    data.timestamp = time(nullptr);
    pub_teros_.publish(data);
    return 0;
}

int SDI12Scanner::read_solyx14(const sensor_info &s)
{
    char response[128];
    char cmd[] = "0XR0!";  
    cmd[0] = s.addr;
    char expect[] = "0XR0!0"; 
    expect[0] = s.addr; 
    expect[5] = s.addr;

    bus_.cmd(cmd, true);
    if (bus_.wait_for(response, sizeof(response), expect) <= 0) {
        LOG_ERR("solyx14: response timeout");
        return -1;
    }

    const char *rsp = strstr(response, expect);
    if (!rsp) { LOG_ERR("solyx14: no match"); return -1; }

    solyx14_data data = {};
    strncpy(data.id, s.id, sizeof(data.id) - 1);
    int rv = sscanf(rsp + strlen(expect), "%f%f%f", &data.epsr, &data.temp, &data.bulk_ec);
    if (rv != 3) { LOG_ERR("solyx14: parse failed"); return -1; }

    data.timestamp = time(nullptr);
    float epsp = 80.3f - 0.37f * (data.temp - 20.0f);
    data.vwc = 0.0985f * sqrtf(data.epsr) - 0.159f;
    data.pw_ec = (epsp * data.bulk_ec) / (data.epsr - 4.1f);

    pub_solyx_.publish(data);
    return 0;
}

int SDI12Scanner::read_solinst(const sensor_info &s)
{
    char response[128];
    char cmd_m[] = "0M!";  
    cmd_m[0] = s.addr;
    char expect_m[] = "0M!0"; 
    expect_m[0] = s.addr; 
    expect_m[3] = s.addr;

    bus_.cmd(cmd_m, true);
    if (bus_.wait_for(response, sizeof(response), expect_m) <= 0) {
        LOG_ERR("solinst: aM! timeout");
        return -1;
    }

    const char *rsp = strstr(response, expect_m);
    if (!rsp) return -1;

    int wait = 0, val_num = 0;
    if (sscanf(rsp + strlen(expect_m), "%3d%1d", &wait, &val_num) != 2 || val_num < 2) {
        return -1;
    }

    k_sleep(K_SECONDS(wait + 1));

    char cmd_d[] = "0D0!"; 
    cmd_d[0] = s.addr;
    char expect_d[] = "0D0!0"; 
    expect_d[0] = s.addr; 
    expect_d[4] = s.addr;

    bus_.cmd(cmd_d, true);
    if (bus_.wait_for(response, sizeof(response), expect_d) <= 0) {
        LOG_ERR("solinst: aD0 timeout");
        return -1;
    }

    const char *rsp_d = strstr(response, expect_d);
    if (!rsp_d) return -1;

    solinst_data data = {};
    strncpy(data.id, s.id, sizeof(data.id) - 1);
    int rv = sscanf(rsp_d + strlen(expect_d), "%f%f", &data.temp, &data.level);
    if (rv != 2) { LOG_ERR("solinst: parse failed"); return -1; }

    data.timestamp = time(nullptr);
    float pressure_mbar_m = ruuvi_.get_last_pressure() * 0.0101972f;
    if (pressure_mbar_m == 0.0f) {
        LOG_WRN("No pressure from RuuviTag, compensated level not set");
        data.compensated_level = 0.0f;
    } else {
        data.compensated_level = data.level - pressure_mbar_m;
    }

    pub_solinst_.publish(data);
    return 0;
}