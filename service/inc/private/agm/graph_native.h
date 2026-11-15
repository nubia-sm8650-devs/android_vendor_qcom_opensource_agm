/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef AGM_GRAPH_NATIVE_H
#define AGM_GRAPH_NATIVE_H

#include <stdbool.h>
#include <stddef.h>

static inline bool graph_native(void)
{
#ifdef AGM_NATIVE_TOPOLOGY
    return true;
#else
    return false;
#endif
}

int graph_native_card_name(char *name, size_t size);

#endif
