#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(json_ser);

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "serialization/json_serializer.hpp"

int JsonSerializer::pack(uint8_t *buf, int max_len)
{
    static constexpr int ELEMENT_MAX = 256;
    static constexpr int MARGIN = 16;

    char *out = reinterpret_cast<char *>(buf);
    int pos = 0;
    int count = 0;

    pos += snprintf(out + pos, max_len - pos, "{\"measurements\":[");

    ruuvi_data r;
    while (ruuvi_sub_.peek(r) && pos + ELEMENT_MAX + MARGIN < max_len) {
        if (count > 0) { 
            out[pos++] = ','; 
        }
        pos += snprintf(out + pos, max_len - pos,
                        "{\"mac\":\"%s\",\"ts\":%ld,\"t\":%.2f,"
                        "\"h\":%.2f,\"p\":%.2f,\"bv\":%.2f}",
                        r.mac, (long int)r.timestamp,
                        (double)r.temperature, (double)r.humidity,
                        (double)r.pressure,    (double)r.bat_voltage);
        ruuvi_sub_.remove();
        ++count;
    }

    teros12_data t;
    while (teros_sub_.peek(t) && pos + ELEMENT_MAX + MARGIN < max_len) {
        if (count > 0) { 
            out[pos++] = ','; 
        }
        pos += snprintf(out + pos, max_len - pos,
                        "{\"id\":\"%s\",\"ts\":%ld,\"vwc\":%.2f,"
                        "\"t\":%.2f,\"ec\":%.2f}",
                        t.id, (long int)t.timestamp,
                        (double)t.vwc, (double)t.temp, (double)t.ec);
        teros_sub_.remove();
        ++count;
    }

    solyx14_data s;
    while (solyx_sub_.peek(s) && pos + ELEMENT_MAX + MARGIN < max_len) {
        if (count > 0) { 
            out[pos++] = ','; 
        }
        pos += snprintf(out + pos, max_len - pos,
                        "{\"id\":\"%s\",\"ts\":%ld,\"perm\":%.2f,"
                        "\"t\":%.2f,\"ec\":%.2f}",
                        s.id, (long int)s.timestamp,
                        (double)s.epsr, (double)s.temp, (double)s.bulk_ec);
        solyx_sub_.remove();
        ++count;
    }

    solinst_data si;
    while (solinst_sub_.peek(si) && pos + ELEMENT_MAX + MARGIN < max_len) {
        if (count > 0) { 
            out[pos++] = ','; 
        }
        pos += snprintf(out + pos, max_len - pos,
                        "{\"id\":\"%s\",\"ts\":%ld,\"t\":%.2f,"
                        "\"lvl\":%.2f,\"clvl\":%.2f}",
                        si.id, (long int)si.timestamp,
                        (double)si.temp, (double)si.level,
                        (double)si.compensated_level);
        solinst_sub_.remove();
        ++count;
    }

    pos += snprintf(out + pos, max_len - pos, "]}");

    if (count == 0) {
        return 0;
    }

    LOG_INF("JSON: %d items, %d bytes", count, pos);
    return pos;
}
