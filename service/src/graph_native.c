/* SPDX-License-Identifier: BSD-3-Clause */
#define LOG_TAG "AGM: graph_native"

#include <string.h>
#include <cutils/properties.h>
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
