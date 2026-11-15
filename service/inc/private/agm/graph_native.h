/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef AGM_GRAPH_NATIVE_H
#define AGM_GRAPH_NATIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "gsl_intf.h"
#include <agm/agm_priv.h>
#include <agm/device.h>
#include <agm/session_obj.h>

static inline bool graph_native(void)
{
#ifdef AGM_NATIVE_TOPOLOGY
    return true;
#else
    return false;
#endif
}

int graph_native_card_name(char *name, size_t size);
bool graph_native_configures(uint32_t tag);
int graph_native_set_media(gsl_handle_t graph, struct session_obj *session);

#endif
