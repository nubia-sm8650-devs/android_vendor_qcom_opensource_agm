/* SPDX-License-Identifier: BSD-3-Clause */
#define LOG_TAG "AGM: graph_native"

#include <string.h>
#include <cutils/properties.h>
#include "gsl_intf.h"
#include <agm/graph.h>
#include <agm/graph_module.h>
#include <agm/graph_native.h>

#define NATIVE_CARD_PROPERTY "ro.vendor.audio.native_topology.alias"

int graph_native_card_name(char *name, size_t size)
{
    char value[PROPERTY_VALUE_MAX] = {0};
    int length = property_get(NATIVE_CARD_PROPERTY, value, "");

    if (length <= 0 || (size_t)length >= size)
        return 0;

    memcpy(name, value, length + 1);

    return length;
}

bool graph_native_configures(uint32_t tag)
{
    switch (tag) {
    case DEVICE_HW_ENDPOINT_RX:
    case DEVICE_HW_ENDPOINT_TX:
    case STREAM_INPUT_MEDIA_FORMAT:
    case RD_SHMEM_ENDPOINT:
    case STREAM_PCM_ENCODER:
    case STREAM_PCM_DECODER:
    case STREAM_PCM_CONVERTER:
    case TAG_STREAM_PLACEHOLDER_ENCODER:
    case TAG_STREAM_PLACEHOLDER_DECODER:
    case MODULE_GAPLESS:
        return true;
    default:
        return false;
    }
}
