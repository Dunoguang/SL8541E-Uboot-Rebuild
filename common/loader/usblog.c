/*
 * [usblog] mirror U-Boot console output onto the u2s (USB CDC-ACM) channel.
 *
 * Historically this was done by binary-patching puts() at 0xE798 so it jumped
 * into an injected block.  In source it is just a call at the end of puts().
 *
 * Three things the old binary patch had to fight:
 *   1. usb_wait_trans_done() loops forever if the host never ACKs.  Here the
 *      wait is bounded, and we stop mirroring if the host is clearly absent.
 *   2. Sending more than one max-packet at a time used to stall the channel,
 *      so the string is chopped into USBLOG_CHUNK pieces.
 *   3. gs_write() can be completed from interrupt context, which printf()s on
 *      error.  That would re-enter usblog, hence the busy gate.
 */

#include <common.h>
#include "calibration_detect.h"

/* set once the host has configured the port and gs_open() succeeded */
int usblog_ready = 0;

#define USBLOG_CHUNK        64      /* one full-speed bulk max packet */
#define USBLOG_TX_TIMEOUT_MS 10     /* per chunk */
#define USBLOG_MAX_STRIKES 1000000 /* debug build: never self-disable */      /* consecutive timeouts before giving up */

static int usblog_busy = 0;
static int usblog_strikes = 0;

/* returns 1 if the chunk went out, 0 if the host did not take it */
static int usblog_tx_chunk(const unsigned char *buf, int len)
{
    unsigned long t0;
    int done = 0;

    gs_write(buf, len);

    t0 = get_timer(0);
    while (!(done = usb_is_trans_done(1))) {
        if (get_timer(t0) > USBLOG_TX_TIMEOUT_MS)
            break;
    }

    if (!done) {
        if (++usblog_strikes >= USBLOG_MAX_STRIKES)
            usblog_ready = 0;   /* nobody is listening, stop mirroring */
        return 0;
    }

    usblog_strikes = 0;
    return 1;
}

void usblog_puts(const char *s)
{
    int total, off;

    if (usblog_busy || !usblog_ready || !s)
        return;

    total = strlen(s);
    if (total <= 0)
        return;

    usblog_busy = 1;

    for (off = 0; off < total; off += USBLOG_CHUNK) {
        int len = total - off;

        if (len > USBLOG_CHUNK)
            len = USBLOG_CHUNK;
        if (!usblog_tx_chunk((const unsigned char *)s + off, len))
            break;
        if (!usblog_ready)
            break;
    }

    usblog_busy = 0;
}
