//!
//! \file       wifi_server.h
//! \brief      Lightweight HTTP server for RP2350 + CYW43 WiFi
//!

#ifndef WIFI_SERVER_H
#define WIFI_SERVER_H

#include <stdint.h>
#include <stdbool.h>

//---------------------------------------------------------------------------
// Shared flags between web server (IRQ context) and main loop
//---------------------------------------------------------------------------

extern volatile bool     web_pulse_requested;
extern volatile bool     web_pulse_busy;
extern volatile bool     web_pulse_done;
extern volatile uint32_t web_pulse_params[3];   // timing params in ns

//---------------------------------------------------------------------------
// API
//---------------------------------------------------------------------------

void http_server_init(void);

#endif // WIFI_SERVER_H
