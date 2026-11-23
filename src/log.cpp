// Debug log. 0x218A0: vsprintf into one shared buffer, then OutputDebugStringA.
// With EVOX_UART_LOG the text is also written to the COM1 UART (0x3F8): the kernel sends
// OutputDebugString as KD packets, so under xemu (-serial stdio) only the UART shows it as text.
#include <xtl.h>
#include <stdarg.h>
#include <stdio.h>
#include "evox/log.h"

static char g_LogBuffer[0x480]; // 0x57E130 (up to the next global at 0x57E5B0)

#ifdef EVOX_UART_LOG
static void UartWrite(const char *s)
{
    for (; *s; s++) {
        unsigned char lsr;
        do // wait for the transmit holding register to empty
            __asm__ volatile("inb %1, %0" : "=a"(lsr) : "Nd"((unsigned short)0x3FD));
        while (!(lsr & 0x20));
        __asm__ volatile("outb %0, %1" : : "a"((unsigned char)*s), "Nd"((unsigned short)0x3F8));
    }
}
#endif

void Log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsprintf(g_LogBuffer, fmt, ap);
    va_end(ap);
    OutputDebugStringA(g_LogBuffer);
#ifdef EVOX_UART_LOG
    UartWrite(g_LogBuffer);
#endif
}
