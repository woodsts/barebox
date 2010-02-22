/*
 * (c) 2004 Sascha Hauer <sascha@saschahauer.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <common.h>
#include <driver.h>
#include <init.h>
#include <malloc.h>
#include <errno.h>
#include <asm/io.h>
#include <mach/lpc3250.h>

struct lpc_serial_priv {
	struct console_device cdev;
	int baudrate;
	int no;
};

static int abs(int v1, int v2)
{
	if (v1 > v2)
		return v1 - v2;

	return v2 - v1;
}

/*
 * Find the best UART clock divider to get the desired port rate
 */
static void serial_getdiv(u32 baudrate,
			unsigned int *xdiv,
			unsigned int *ydiv)
{
	unsigned int clkrate, savedclkrate, diff, basepclk;
	int idxx, idyy;

	/* Get the clock rate for the UART block */
	basepclk = sys_get_rate(CLKPWR_PERIPH_CLK) >> 4;

	/* Find the best divider */
	*xdiv = *ydiv = 0;
	savedclkrate = 0;
	diff = 0xffffffff;

	for (idxx = 1; idxx < 0xff; idxx++) {
		for (idyy = idxx; idyy < 0xff; idyy++) {
			clkrate = (basepclk * idxx) / idyy;
			if (abs(clkrate, baudrate) < diff) {
				diff = abs(clkrate, baudrate);
				savedclkrate = clkrate;
				*xdiv = idxx;
				*ydiv = idyy;
			}
		}
	}
}

/*
 * Initialise the serial port with the given baudrate. The settings
 * are always 8 data bits, no parity, 1 stop bit, no start bits.
 *
 */
static int lpc_serial_init_port(struct console_device *cdev)
{
	u32 tmp;
	struct device_d *dev = cdev->dev;
	struct lpc_serial_priv *priv = container_of(cdev,
                                        struct lpc_serial_priv, cdev);
	struct uart_regs *puregs = (struct uart_regs *)dev->map_base;

	/* Place UART in autoclock mode */
	tmp = readl(&UARTCNTL->clkmode);
	tmp &= ~UART_CLKMODE_MASK(priv->no);
	tmp |= UART_CLKMODE_LOAD(UART_CLKMODE_AUTO, (priv->no));
	writel(tmp, &UARTCNTL->clkmode);

	writel(1, &puregs->dll_fifo);
	writel(0, &puregs->dlm_ier);

	/* Setup default UART state for N81 with FIFO mode */
	writel(UART_LCR_WLEN_8BITS, &puregs->lcr);

	/* Clear FIFOs and set FIFO level */
	writel(UART_FCR_RXFIFO_TL16 |
			UART_FCR_TXFIFO_TL0 |
			UART_FCR_FIFO_CTRL |
			UART_FCR_FIFO_EN |
			UART_FCR_TXFIFO_FLUSH |
			UART_FCR_RXFIFO_FLUSH,
			&puregs->iir_fcr);

	readl(&puregs->iir_fcr);
	readl(&puregs->lsr);

	return 0;
}

static void lpc_serial_putc(struct console_device *cdev, char c)
{
	struct device_d *dev = cdev->dev;
	unsigned long base = dev->map_base;
	struct uart_regs *puregs = (struct uart_regs *)base;

	/* Wait for FIFO to become empty */
	while ((readl(&puregs->lsr) & UART_LSR_THRE) == 0);

	writel(c, &puregs->dll_fifo);
}

static int lpc_serial_tstc(struct console_device *cdev)
{
	struct device_d *dev = cdev->dev;
	unsigned long base = dev->map_base;
	struct uart_regs *puregs = (struct uart_regs *)base;

	/* Wait for a character from the UART */
	if ((readl(&puregs->lsr) & UART_LSR_RDR) == 0)
		return 0;

	return 1;
}

static int lpc_serial_getc(struct console_device *cdev)
{
	struct device_d *dev = cdev->dev;
	unsigned long base = dev->map_base;
	struct uart_regs *puregs = (struct uart_regs *)base;

	/* Wait for a character from the UART */
	while ((readl(&puregs->lsr) & UART_LSR_RDR) == 0);

	return readl(&puregs->dll_fifo) & 0xFF;
}

static void lpc_serial_flush(struct console_device *cdev)
{
}

static int lpc_serial_setbaudrate(struct console_device *cdev, int baudrate)
{
	struct lpc_serial_priv *priv = container_of(cdev,
                                        struct lpc_serial_priv, cdev);
	unsigned int xdiv, ydiv;
	void *clk_ctrl = (void *)&CLKPWR->clkpwr_uart3_clk_ctrl;

	clk_ctrl += (priv->no - 3) * 4;

	serial_getdiv(baudrate, &xdiv, &ydiv);

	writel(CLKPWR_UART_X_DIV(xdiv) |
			CLKPWR_UART_Y_DIV(ydiv),
			clk_ctrl);

	return 0;
}

static int lpc_serial_get_uart_number(unsigned long base)
{
	switch (base) {
	case UART3_BASE:
		return 3;
	case UART4_BASE:
		return 4;
	case UART5_BASE:
		return 5;
	case UART6_BASE:
		return 6;
	default:
		return -EINVAL;
	}
}

static int lpc_serial_probe(struct device_d *dev)
{
	struct console_device *cdev;
	struct lpc_serial_priv *priv;
	int uart_number;

	uart_number = lpc_serial_get_uart_number(dev->map_base);
	if (uart_number < 0)
		return uart_number;

	priv = xzalloc(sizeof(*priv));
	cdev = &priv->cdev;

	priv->no = uart_number;
	dev->type_data = cdev;
	cdev->dev = dev;
	cdev->f_caps = CONSOLE_STDIN | CONSOLE_STDOUT | CONSOLE_STDERR;
	cdev->tstc = lpc_serial_tstc;
	cdev->putc = lpc_serial_putc;
	cdev->getc = lpc_serial_getc;
	cdev->flush = lpc_serial_flush;
	cdev->setbrg = lpc_serial_setbaudrate;

	lpc_serial_init_port(cdev);
	lpc_serial_setbaudrate(cdev, 115200);

	console_register(cdev);

	return 0;
}

static void lpc_serial_remove(struct device_d *dev)
{
	struct console_device *cdev = dev->type_data;

	lpc_serial_flush(cdev);
}

static struct driver_d lpc_serial_driver = {
        .name   = "lpc_serial",
        .probe  = lpc_serial_probe,
	.remove = lpc_serial_remove,
};

static int lpc_serial_init(void)
{
	register_driver(&lpc_serial_driver);
	return 0;
}

console_initcall(lpc_serial_init);

