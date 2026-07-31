#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(binary_ser);

#include <stdio.h>
#include <string.h>

#include "serialization/binary_serializer.hpp"

extern "C" {
#include "sensors.pb.h"
#include "pb_encode.h"
}

int BinarySerializer::pack(uint8_t *buf, int max_len)
{
    SensorPayload msg = SensorPayload_init_zero;
    int total = 0;

    ruuvi_data r;
    while (msg.ruuvi_count < 4 && ruuvi_sub_.peek(r)) {
        RuuviData *rd = &msg.ruuvi[msg.ruuvi_count++];
        // parse MAC str
        unsigned int bytes[6] = {};
        sscanf(r.mac, "%x:%x:%x:%x:%x:%x",
               &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5]);
        for (int i = 0; i < 6; ++i) {
            rd->mac[i] = (uint8_t)bytes[i];
        }
        rd->timestamp = (uint32_t)r.timestamp;
        rd->temp = r.temperature;
        rd->humidity = r.humidity;
        rd->pressure = r.pressure;
        rd->bat_voltage = r.bat_voltage;

        size_t enc_size;
        if (!pb_get_encoded_size(&enc_size, SensorPayload_fields, &msg) || (int)enc_size > max_len) {
            msg.ruuvi_count--;
            break;
        }
        ruuvi_sub_.remove();
        ++total;
    }

    teros12_data t;
    while (msg.teros_count < 4 && teros_sub_.peek(t)) {
        Teros12Data *td = &msg.teros[msg.teros_count++];
        strncpy(td->id, t.id, sizeof(td->id) - 1);
        td->timestamp = (uint32_t)t.timestamp;
        td->vwc = t.vwc;
        td->temp = t.temp;
        td->ec = t.ec;

        size_t enc_size;
        if (!pb_get_encoded_size(&enc_size, SensorPayload_fields, &msg) || (int)enc_size > max_len) {
            msg.teros_count--;
            break;
        }
        teros_sub_.remove();
        ++total;
    }

    solyx14_data s;
    while (msg.solyx_count < 4 && solyx_sub_.peek(s)) {
        Solyx14Data *sd = &msg.solyx[msg.solyx_count++];
        strncpy(sd->id, s.id, sizeof(sd->id) - 1);
        sd->timestamp = (uint32_t)s.timestamp;
        sd->epsr = s.epsr;
        sd->temp = s.temp;
        sd->bulk_ec = s.bulk_ec;

        size_t enc_size;
        if (!pb_get_encoded_size(&enc_size, SensorPayload_fields, &msg) || (int)enc_size > max_len) {
            msg.solyx_count--;
            break;
        }
        solyx_sub_.remove();
        ++total;
    }

    solinst_data si;
    while (msg.solinst_count < 4 && solinst_sub_.peek(si)) {
        SolinstData *slst = &msg.solinst[msg.solinst_count++];
        strncpy(slst->id, si.id, sizeof(slst->id) - 1);
        slst->timestamp = (uint32_t)si.timestamp;
        slst->temp = si.temp;
        slst->level = si.level;
        slst->compensated_level = si.compensated_level;

        size_t enc_size;
        if (!pb_get_encoded_size(&enc_size, SensorPayload_fields, &msg) || (int)enc_size > max_len) {
            msg.solinst_count--;
            break;
        }
        solinst_sub_.remove();
        ++total;
    }

    if (total == 0) {
        return 0;
    }

    pb_ostream_t stream = pb_ostream_from_buffer(buf, max_len);
    if (!pb_encode(&stream, SensorPayload_fields, &msg)) {
        LOG_ERR("Encode failed: %s", PB_GET_ERROR(&stream));
        return -1;
    }

    LOG_INF("Packed %d items → %d bytes", total, (int)stream.bytes_written);
    return (int)stream.bytes_written;
}
