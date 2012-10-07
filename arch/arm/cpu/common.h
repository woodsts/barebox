
#include <io.h>

#define URTX0		0x40		/* Transmitter Register */

#define UCR1		0x80		/* Control Register 1 */
#define UCR1_UARTEN	(1 << 0)	/* UART enabled */

#define USR2		0x98		/* Status Register 2 */
#define USR2_TXDC	(1 << 3)	/* Transmitter complete */

#define IMX51_UART_BASE 0x73fbc000
#define IMX27_UART_BASE 0x1000a000
#define IMX31_UART_BASE 0x43f90000

#define UART_BASE IMX31_UART_BASE

#define IMX_PUTC(c) ({						\
		if ((readl(UART_BASE + UCR1) & UCR1_UARTEN)) {		\
			while (!(readl(UART_BASE + USR2) & USR2_TXDC));	\
			writel(c, UART_BASE + URTX0);			\
		}	\
		})

# define PUTHEX_LL(value)  ({ unsigned long v = (unsigned long) (value); \
			     int i; unsigned char ch; \
			     for (i = 8; i--; ) {\
			     ch = ((v >> (i*4)) & 0xf);\
			     ch += (ch >= 10) ? 'a' - 10 : '0';\
			     IMX_PUTC (ch); }})


static __inline__ void PUTS_LL(char * str)
{
	while (*str) {
		if (*str == '\n') {
			IMX_PUTC('\r');
		}
		IMX_PUTC(*str);
		str++;
	}
}

void relocate(void);

void relocate_binary(unsigned long target);

