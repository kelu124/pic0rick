//!
//! \file       wifi_server.c
//! \brief      Minimal HTTP server for RP2350 data-acquisition UI.
//!
//! Endpoints
//!   GET /              → HTML single-page app
//!   GET /api/pulse?p1=&p2=&p3=   → trigger acquisition (returns JSON)
//!   GET /api/status    → {"busy":false,"done":true}
//!   GET /api/data      → JSON array of 800 downsampled points
//!   GET /api/csv       → text/csv of all 8000 raw points
//!

//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------

#include "wifi_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"

//---------------------------------------------------------------------------
// Import ADC buffer from adc.c
//---------------------------------------------------------------------------

#define SAMPLE_COUNT 8000
extern uint16_t buffer[SAMPLE_COUNT];

//---------------------------------------------------------------------------
// Shared flags
//---------------------------------------------------------------------------

volatile bool     web_pulse_requested  = false;
volatile bool     web_pulse_busy       = false;
volatile bool     web_pulse_done       = false;
volatile uint32_t web_pulse_params[3]  = {1000, 1000, 1000};

//---------------------------------------------------------------------------
// Per-connection state (heap-allocated, freed on close/error)
//---------------------------------------------------------------------------

typedef struct http_conn {
    char   *resp;           // malloc'd full response (header+body)
    uint32_t resp_len;
    uint32_t resp_sent;
} http_conn_t;

//---------------------------------------------------------------------------
// EMBEDDED WEB PAGE
//---------------------------------------------------------------------------

static const char WEBPAGE[] =
"<!DOCTYPE html>\n"
"<html lang=\"en\"><head>\n"
"<meta charset=\"UTF-8\">\n"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
"<title>RP2350 DAQ</title>\n"
"<style>\n"
"*{box-sizing:border-box;margin:0;padding:0}\n"
"body{font-family:system-ui,sans-serif;background:#0f172a;color:#e2e8f0;padding:20px;max-width:960px;margin:0 auto}\n"
"h1{font-size:1.5rem;margin-bottom:16px;color:#38bdf8}\n"
".bar{display:flex;gap:8px;flex-wrap:wrap;align-items:end;margin-bottom:12px}\n"
".bar label{font-size:.8rem;color:#94a3b8}\n"
".bar input{width:80px;padding:6px;border:1px solid #334155;border-radius:6px;background:#1e293b;color:#e2e8f0;font-size:.9rem}\n"
"button{padding:10px 20px;border:none;border-radius:6px;cursor:pointer;font-size:.95rem;font-weight:600;transition:opacity .15s}\n"
"button:active{opacity:.7}\n"
".bp{background:#e11d48;color:#fff}\n"
".bd{background:#2563eb;color:#fff}\n"
".bc{background:#1e293b;color:#e2e8f0;border:1px solid #334155}\n"
"#st{margin:10px 0;padding:8px 12px;border-radius:6px;background:#1e293b;font-size:.85rem;min-height:1.6em}\n"
"canvas{width:100%;height:360px;background:#1e293b;border-radius:8px;margin-top:8px}\n"
"</style></head><body>\n"
"<h1>RP2350 Data Acquisition</h1>\n"
"<div class=\"bar\">\n"
" <div><label>Pulse 1 (ns)</label><br><input id=\"p1\" type=\"number\" value=\"1000\"></div>\n"
" <div><label>Pulse 2 (ns)</label><br><input id=\"p2\" type=\"number\" value=\"1000\"></div>\n"
" <div><label>Delay (ns)</label><br><input id=\"p3\" type=\"number\" value=\"1000\"></div>\n"
" <button class=\"bp\" onclick=\"doPulse()\">Pulse</button>\n"
" <button class=\"bd\" onclick=\"doDisplay()\">Display</button>\n"
" <button class=\"bc\" onclick=\"doCSV()\">Download CSV</button>\n"
"</div>\n"
"<div id=\"st\">Ready</div>\n"
"<canvas id=\"cv\"></canvas>\n"
"<script>\n"
"const st=document.getElementById('st'),cv=document.getElementById('cv'),cx=cv.getContext('2d');\n"
"function S(t,c){st.textContent=t;st.style.background=c||'#1e293b'}\n"
"async function doPulse(){\n"
" S('Acquiring...','#7c3aed');\n"
" let p1=document.getElementById('p1').value||1000;\n"
" let p2=document.getElementById('p2').value||1000;\n"
" let p3=document.getElementById('p3').value||1000;\n"
" try{\n"
"  await fetch('/api/pulse?p1='+p1+'&p2='+p2+'&p3='+p3);\n"
"  for(let i=0;i<60;i++){\n"
"   await new Promise(r=>setTimeout(r,100));\n"
"   let r=await(await fetch('/api/status')).json();\n"
"   if(!r.busy&&r.done){S('Acquisition complete ('+r.samples+' samples)','#065f46');return;}\n"
"  }\n"
"  S('Timeout waiting for acquisition','#991b1b');\n"
" }catch(e){S('Error: '+e,'#991b1b')}\n"
"}\n"
"async function doDisplay(){\n"
" S('Loading graph data...');\n"
" try{\n"
"  let d=await(await fetch('/api/data')).json();\n"
"  drawChart(d);\n"
"  S('Showing '+d.length+' points (10x downsampled)','#065f46');\n"
" }catch(e){S('Error: '+e,'#991b1b')}\n"
"}\n"
"function doCSV(){\n"
" S('Downloading CSV...');\n"
" let a=document.createElement('a');a.href='/api/csv';a.download='adc_data.csv';a.click();\n"
" S('Download started','#065f46');\n"
"}\n"
"function drawChart(d){\n"
" let W=cv.width=cv.clientWidth,H=cv.height=360,P=50;\n"
" cx.clearRect(0,0,W,H);cx.fillStyle='#1e293b';cx.fillRect(0,0,W,H);\n"
" let mn=Math.min(...d),mx=Math.max(...d),rg=mx-mn||1;\n"
" cx.strokeStyle='#334155';cx.lineWidth=1;cx.font='11px system-ui';cx.fillStyle='#64748b';\n"
" for(let i=0;i<=5;i++){\n"
"  let y=P+(H-2*P)*i/5;\n"
"  cx.beginPath();cx.moveTo(P,y);cx.lineTo(W-20,y);cx.stroke();\n"
"  cx.fillText(Math.round(mx-rg*i/5),4,y+4);\n"
" }\n"
" for(let i=0;i<=4;i++){\n"
"  let x=P+(W-P-20)*i/4;\n"
"  cx.fillText(Math.round(i*d.length/4*10),x-10,H-8);\n"
" }\n"
" cx.strokeStyle='#38bdf8';cx.lineWidth=1.5;cx.beginPath();\n"
" for(let i=0;i<d.length;i++){\n"
"  let x=P+(W-P-20)*i/(d.length-1),y=P+(H-2*P)*(1-(d[i]-mn)/rg);\n"
"  i?cx.lineTo(x,y):cx.moveTo(x,y);\n"
" }\n"
" cx.stroke();\n"
"}\n"
"</script></body></html>\n";

//---------------------------------------------------------------------------
// HELPERS
//---------------------------------------------------------------------------

// Simple query-string param extractor: finds ?…key=VALUE… and returns atoi
static uint32_t qs_uint(const char *req, const char *key, uint32_t def)
{
    char needle[32];
    snprintf(needle, sizeof(needle), "%s=", key);
    const char *p = strstr(req, needle);
    if (!p) return def;
    return (uint32_t)atoi(p + strlen(needle));
}

// Free connection state
static void conn_free(http_conn_t *c)
{
    if (!c) return;
    if (c->resp) free(c->resp);
    free(c);
}

//---------------------------------------------------------------------------
// TCP SEND ENGINE  – sends resp in chunks that fit tcp_sndbuf
//---------------------------------------------------------------------------

static void send_next_chunk(struct tcp_pcb *pcb, http_conn_t *c)
{
    if (!c || !c->resp) return;
    uint32_t remaining = c->resp_len - c->resp_sent;
    if (remaining == 0) return;

    u16_t space = tcp_sndbuf(pcb);
    u16_t to_send = (remaining < space) ? (u16_t)remaining : space;
    if (to_send == 0) return;

    uint8_t flags = TCP_WRITE_FLAG_COPY;
    if (to_send < remaining) flags |= TCP_WRITE_FLAG_MORE;

    err_t err = tcp_write(pcb, c->resp + c->resp_sent, to_send, flags);
    if (err == ERR_OK)
    {
        c->resp_sent += to_send;
        tcp_output(pcb);
    }
}

//---------------------------------------------------------------------------
// TCP CALLBACKS
//---------------------------------------------------------------------------

static err_t on_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    http_conn_t *c = (http_conn_t *)arg;
    if (!c) return ERR_OK;

    if (c->resp_sent < c->resp_len)
    {
        send_next_chunk(pcb, c);
    }
    else
    {
        // Done – close
        tcp_arg(pcb, NULL);
        tcp_sent(pcb, NULL);
        tcp_recv(pcb, NULL);
        tcp_close(pcb);
        conn_free(c);
    }
    return ERR_OK;
}

static void on_err(void *arg, err_t err)
{
    (void)err;
    conn_free((http_conn_t *)arg);
}

//---------------------------------------------------------------------------
// Build full HTTP response (header + body) into a single malloc buffer
//---------------------------------------------------------------------------

static void send_response(struct tcp_pcb *pcb, http_conn_t *c,
                          const char *content_type,
                          const char *body, uint32_t body_len,
                          const char *extra_headers)
{
    char hdr[256];
    int hdr_len = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "%s"
        "\r\n",
        content_type, body_len,
        extra_headers ? extra_headers : "");

    uint32_t total = (uint32_t)hdr_len + body_len;
    c->resp = malloc(total);
    if (!c->resp)
    {
        tcp_abort(pcb);
        conn_free(c);
        return;
    }
    memcpy(c->resp, hdr, hdr_len);
    memcpy(c->resp + hdr_len, body, body_len);
    c->resp_len  = total;
    c->resp_sent = 0;

    send_next_chunk(pcb, c);
}

// Send a short static body (no extra alloc for body, only for final resp)
static void send_static(struct tcp_pcb *pcb, http_conn_t *c,
                        const char *ctype, const char *body)
{
    send_response(pcb, c, ctype, body, strlen(body), NULL);
}

//---------------------------------------------------------------------------
// ROUTE: GET /
//---------------------------------------------------------------------------

static void handle_index(struct tcp_pcb *pcb, http_conn_t *c)
{
    send_response(pcb, c, "text/html; charset=UTF-8",
                  WEBPAGE, sizeof(WEBPAGE) - 1, NULL);
}

//---------------------------------------------------------------------------
// ROUTE: GET /api/pulse?p1=&p2=&p3=
//---------------------------------------------------------------------------

static void handle_pulse(struct tcp_pcb *pcb, http_conn_t *c, const char *req)
{
    if (web_pulse_busy)
    {
        send_static(pcb, c, "application/json",
                    "{\"ok\":false,\"msg\":\"Acquisition already in progress\"}");
        return;
    }

    web_pulse_params[0] = qs_uint(req, "p1", 1000);
    web_pulse_params[1] = qs_uint(req, "p2", 1000);
    web_pulse_params[2] = qs_uint(req, "p3", 1000);
    web_pulse_done       = false;
    web_pulse_requested  = true;      // main loop picks this up

    send_static(pcb, c, "application/json", "{\"ok\":true,\"msg\":\"Acquisition started\"}");
}

//---------------------------------------------------------------------------
// ROUTE: GET /api/status
//---------------------------------------------------------------------------

static void handle_status(struct tcp_pcb *pcb, http_conn_t *c)
{
    char buf[96];
    snprintf(buf, sizeof(buf),
             "{\"busy\":%s,\"done\":%s,\"samples\":%d}",
             web_pulse_busy ? "true" : "false",
             web_pulse_done ? "true" : "false",
             SAMPLE_COUNT);
    send_static(pcb, c, "application/json", buf);
}

//---------------------------------------------------------------------------
// ROUTE: GET /api/data   (800 downsampled points as JSON)
//---------------------------------------------------------------------------

static void handle_data(struct tcp_pcb *pcb, http_conn_t *c)
{
    // 800 points × max 5 chars + commas + brackets ≈ 5 KB
    const int DS = 10;                        // downsample factor
    const int NPTS = SAMPLE_COUNT / DS;       // 800
    const int BUFSZ = NPTS * 6 + 16;

    char *body = malloc(BUFSZ);
    if (!body) { tcp_abort(pcb); conn_free(c); return; }

    int pos = 0;
    body[pos++] = '[';
    for (int i = 0; i < SAMPLE_COUNT; i += DS)
    {
        uint16_t val = (buffer[i] >> 1) & 0x3FF;
        if (i > 0) body[pos++] = ',';
        pos += snprintf(body + pos, BUFSZ - pos, "%u", val);
    }
    body[pos++] = ']';
    body[pos]   = '\0';

    send_response(pcb, c, "application/json", body, (uint32_t)pos, NULL);
    free(body);
}

//---------------------------------------------------------------------------
// ROUTE: GET /api/csv    (all 8000 points)
//---------------------------------------------------------------------------

static void handle_csv(struct tcp_pcb *pcb, http_conn_t *c)
{
    // 8000 × ~6 chars ≈ 48 KB
    const int BUFSZ = SAMPLE_COUNT * 7 + 64;

    char *body = malloc(BUFSZ);
    if (!body) { tcp_abort(pcb); conn_free(c); return; }

    int pos = 0;
    pos += snprintf(body + pos, BUFSZ - pos, "index,value\r\n");
    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        uint16_t val = (buffer[i] >> 1) & 0x3FF;
        pos += snprintf(body + pos, BUFSZ - pos, "%d,%u\r\n", i, val);
    }

    send_response(pcb, c, "text/csv", body, (uint32_t)pos,
                  "Content-Disposition: attachment; filename=\"adc_data.csv\"\r\n");
    free(body);
}

//---------------------------------------------------------------------------
// REQUEST ROUTER
//---------------------------------------------------------------------------

static void route_request(struct tcp_pcb *pcb, http_conn_t *c, char *req)
{
    // req starts with "GET /path ..."
    if (strncmp(req, "GET /api/pulse", 14) == 0)
        handle_pulse(pcb, c, req);
    else if (strncmp(req, "GET /api/status", 15) == 0)
        handle_status(pcb, c);
    else if (strncmp(req, "GET /api/data", 13) == 0)
        handle_data(pcb, c);
    else if (strncmp(req, "GET /api/csv", 12) == 0)
        handle_csv(pcb, c);
    else if (strncmp(req, "GET / ", 6) == 0 ||
             strncmp(req, "GET /index", 10) == 0)
        handle_index(pcb, c);
    else if (strncmp(req, "GET /favicon", 12) == 0)
        send_static(pcb, c, "text/plain", "");    // silence favicon 404
    else
        send_static(pcb, c, "text/plain", "404 Not Found");
}

//---------------------------------------------------------------------------
// TCP RECV – fires when data arrives on an accepted connection
//---------------------------------------------------------------------------

static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    http_conn_t *c = (http_conn_t *)arg;

    if (!p)
    {
        // Client closed connection
        tcp_arg(pcb, NULL);
        tcp_close(pcb);
        conn_free(c);
        return ERR_OK;
    }

    tcp_recved(pcb, p->tot_len);

    // Copy first chunk (enough for the request line)
    char req[256];
    uint16_t copy_len = p->tot_len < sizeof(req) - 1 ? p->tot_len : sizeof(req) - 1;
    pbuf_copy_partial(p, req, copy_len, 0);
    req[copy_len] = '\0';
    pbuf_free(p);

    route_request(pcb, c, req);
    return ERR_OK;
}

//---------------------------------------------------------------------------
// TCP ACCEPT – fires for each new client connection
//---------------------------------------------------------------------------

static err_t on_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
    (void)arg; (void)err;

    http_conn_t *c = calloc(1, sizeof(http_conn_t));
    if (!c)
    {
        tcp_abort(pcb);
        return ERR_MEM;
    }

    tcp_arg(pcb, c);
    tcp_recv(pcb, on_recv);
    tcp_sent(pcb, on_sent);
    tcp_err(pcb, on_err);

    return ERR_OK;
}

//---------------------------------------------------------------------------
// PUBLIC: start listening
//---------------------------------------------------------------------------

void http_server_init(void)
{
    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb) return;

    err_t err = tcp_bind(pcb, IP_ANY_TYPE, 80);
    if (err != ERR_OK) return;

    pcb = tcp_listen_with_backlog(pcb, 4);
    if (!pcb) return;

    tcp_accept(pcb, on_accept);
}
