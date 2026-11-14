/**
 * @file sl_clap_entry.cpp Studio Link CLAP entry point
 *
 * clap-wrapper compiles this file into every format (CLAP, VST3, AUv2), the
 * implementation is in the static library (sl_clap.c).
 *
 * Copyright (C) 2026 studio-link.de
 */
#include <clap/clap.h>
#include "sl_clap.h"

extern "C" {
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

/* explicit extern: a const in a braced extern "C" block still has internal
 * linkage in C++, MinGW rejects dllexport on it */
extern const CLAP_EXPORT struct clap_plugin_entry clap_entry = {
	CLAP_VERSION_INIT,
	sl_clap_entry_init,
	sl_clap_entry_deinit,
	sl_clap_entry_get_factory,
};

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}
