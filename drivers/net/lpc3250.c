/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <malloc.h>
#include <net.h>
#include <init.h>
#include <miiphy.h>
#include <driver.h>
#include <miiphy.h>
#include <errno.h>
#include <clock.h>
#include <xfuncs.h>

#include <asm/mmu.h>
#include <asm/io.h>

#include <mach/lpc3250.h>
#include <mach/lpc3250_net.h>

#define ENET_MAX_TX_PACKETS	16
#define ENET_MAX_RX_PACKETS	16
#define ENET_MAXF_SIZE		1536
#define ETH_BUFF_SIZE		0x10000

struct lpc3250_priv {
	struct lpc3250_ethernet_regs	*regs;
	struct miiphy_device		miiphy;
	unsigned long			flags;
	unsigned long			eth_bufs_phys;
	void				*eth_bufs_virt;
	void				*tx_bufs[ENET_MAX_TX_PACKETS];
	void				*rx_bufs[ENET_MAX_RX_PACKETS];
	struct txrx_desc		*tx_desc;
	unsigned long			*tx_status;
	struct txrx_desc		*rx_desc;
	struct rx_status		*rx_status;
};

/*
 * MII-interface related functions
 */
static int lpc3250_miiphy_read(struct miiphy_device *mdev, uint8_t phyAddr,
	u8 regAddr, u16 *data)
{
	struct eth_device *edev = mdev->edev;
	struct lpc3250_priv *lpc3250 = edev->priv;

	unsigned long mst = 250;
	int ret = 0;

	writel((phyAddr << 8) | regAddr, &lpc3250->regs->madr);
	writel(MCMD_READ, &lpc3250->regs->mcmd);

	while (mst--) {
		if ((lpc3250->regs->mind & MIND_BUSY) == 0) {
			*data = readl(&lpc3250->regs->mrdd);
			goto out;
		}
		mdelay(1);
	}

	ret = -ETIMEDOUT;
out:
	writel(0, &lpc3250->regs->mcmd);

	return ret;
}

static int lpc3250_miiphy_write(struct miiphy_device *mdev, u8 phyAddr,
	u8 regAddr, u16 data)
{
	struct eth_device *edev = mdev->edev;
	struct lpc3250_priv *lpc3250 = edev->priv;
	int mst = 250;

	/* Write value at PHY address and register */
	writel((phyAddr << 8) | regAddr, &lpc3250->regs->madr);
	writel(data, &lpc3250->regs->mwtd);

	while (mst--) {
		if ((readl(&lpc3250->regs->mind) & MIND_BUSY) == 0)
			return 0;
		mdelay(1);
	}

	return -ETIMEDOUT;
}


static int lpc3250_get_hwaddr(struct eth_device *edev, unsigned char *mac)
{
	/* no eeprom */
	return -1;
}

static int lpc3250_set_hwaddr(struct eth_device *edev, unsigned char *mac)
{
	struct lpc3250_priv *lpc3250 = edev->priv;

	writel(mac[0] | (mac[1] << 8), &lpc3250->regs->sa[2]);
	writel(mac[2] | (mac[3] << 8), &lpc3250->regs->sa[1]);
	writel(mac[4] | (mac[5] << 8), &lpc3250->regs->sa[0]);

	return 0;
}

static int txrx_setup(struct eth_device *edev)
{
	struct lpc3250_priv *lpc3250 = edev->priv;
	int idx;
	unsigned long *pTXStatusL, pbase1, pbase2, pbase3;
	struct txrx_desc *desc;
	struct rx_status *status;

	/* Get physical address and size of DMA buffers */
	lpc3250->eth_bufs_virt = dma_alloc_coherent(ETH_BUFF_SIZE);
	lpc3250->eth_bufs_phys = virt_to_phys(lpc3250->eth_bufs_virt);

	/* Setup base pointers */
	pbase1 = lpc3250->eth_bufs_phys; /* Start of descriptors */
	pbase2 = pbase1 + 256;  /* Start of statuses */
	pbase3 = pbase1 + 1024; /* Start of buffers */

	/* Setup pointers to TX structures */
	writel(pbase1, &lpc3250->regs->txdescriptor);
	writel(pbase2, &lpc3250->regs->txstatus);
	writel(ENET_MAX_TX_PACKETS - 1, &lpc3250->regs->txdescriptornumber);

	/* Save base address of TX descriptor table and TX status */
	desc = (struct txrx_desc *)pbase1;
	pTXStatusL = phys_to_virt(pbase2);
	lpc3250->tx_desc = phys_to_virt((unsigned long)desc);
	lpc3250->tx_status = pTXStatusL;

	/* Build TX descriptors */
	for (idx = 0; idx < ENET_MAX_TX_PACKETS; idx++) {
		writel(pbase3, &desc->packet);
		writel(0, &desc->control);
		writel(0, pTXStatusL);

		/* Save virtual address of buffer */
		lpc3250->tx_bufs[idx] = phys_to_virt((unsigned long)pbase3);

		/* Next descriptor and status */
		desc++;
		pTXStatusL++;
		pbase1 += sizeof (struct txrx_desc);
		pbase2 += sizeof (unsigned long);
		pbase3 += ENET_MAXF_SIZE;
	}

	/* Setup pointers to RX structures */
	writel(pbase1, &lpc3250->regs->rxdescriptor);
	writel(pbase2, &lpc3250->regs->rxstatus);
	writel(ENET_MAX_RX_PACKETS - 1, &lpc3250->regs->rxdescriptornumber);

	/* Save base address of RX descriptor table and RX status */
	lpc3250->rx_desc = phys_to_virt((unsigned long)desc);
	status = (struct rx_status *)pTXStatusL;
	lpc3250->rx_status = status;

	/* Build RX descriptors */
	for (idx = 0; idx < ENET_MAX_TX_PACKETS; idx++)	{
		writel(pbase3, &desc->packet);
		writel(0x80000000 | (ENET_MAXF_SIZE - 1), &desc->control);
		writel(0, &status->statusinfo);
		writel(0, &status->statushashcrc);

		/* Save virtual address of buffer */
		lpc3250->rx_bufs[idx] = phys_to_virt((unsigned long)pbase3);

		/* Next descriptor and status */
		desc++;
		status++;
		pbase1 += sizeof (struct txrx_desc);
		pbase2 += sizeof (unsigned long);
		pbase3 += ENET_MAXF_SIZE;
	}

	return 0;
}

static int lpc3250_init(struct eth_device *edev)
{
	struct lpc3250_priv *lpc3250 = edev->priv;
	u32 val;

	val = CLKPWR_MACCTRL_HRCCLK_EN | CLKPWR_MACCTRL_MMIOCLK_EN |
		CLKPWR_MACCTRL_DMACLK_EN;
	if (lpc3250->flags & LPC3250_NET_RMII)
		val |= CLKPWR_MACCTRL_USE_RMII_PINS;
	else
		val |= CLKPWR_MACCTRL_USE_MII_PINS;

	/* Enable MAC interface */
	writel(val, &CLKPWR->clkpwr_macclk_ctrl);

	/* Set RMII management clock rate. This clock should be slower
	 * than 12.5MHz (for NXP PHYs only). For a divider of 28, the
	 * clock rate when HCLK is 150MHz will be 5.4MHz
	 */
	writel(MCFG_CLOCK_SELECT(MCFG_CLOCK_HOST_DIV_28), &lpc3250->regs->mcfg);

	/* Reset all MAC logic */
	writel(MAC1_SOFT_RESET |
			MAC1_SIMULATION_RESET |
			MAC1_RESET_MCS_TX |
			MAC1_RESET_TX |
			MAC1_RESET_MCS_RX |
			MAC1_RESET_RX,
			&lpc3250->regs->mac1);

	writel(COMMAND_REG_RESET |
			COMMAND_TXRESET |
			COMMAND_RXRESET,
			&lpc3250->regs->command);
	mdelay(10);

	/* Initial MAC initialization */
	writel(MAC1_PASS_ALL_RX_FRAMES, &lpc3250->regs->mac1);
	writel(MAC2_PAD_CRC_ENABLE | MAC2_CRC_ENABLE, &lpc3250->regs->mac2);
	writel(1536, &lpc3250->regs->maxf);

	/* Maximum number of retries, 0x37 collision window, gap */
	writel(CLRT_LOAD_RETRY_MAX(0xF) |
			CLRT_LOAD_COLLISION_WINDOW(0x37),
			&lpc3250->regs->clrt);

	writel(IPGR_LOAD_PART2(0x12), &lpc3250->regs->ipgr);

	if (lpc3250->flags & LPC3250_NET_RMII) {
		writel(COMMAND_RMII | COMMAND_PASSRUNTFRAME, &lpc3250->regs->command);
		writel(SUPP_RESET_RMII, &lpc3250->regs->supp);

		mdelay(10);
	} else {
		writel(COMMAND_PASSRUNTFRAME, &lpc3250->regs->command);
	}

	txrx_setup(edev);

	miiphy_restart_aneg(&lpc3250->miiphy);

	return 0;
}

static int lpc3250_send(struct eth_device *edev, void *eth_data,
		int data_length)
{
	struct lpc3250_priv *lpc3250 = edev->priv;
	unsigned long idx, cidx, fb;

	/* Determine number of free buffers and wait for a buffer if needed */
	do {
		idx = readl(&lpc3250->regs->txproduceindex);
		cidx = readl(&lpc3250->regs->txconsumeindex);

		if (idx == cidx)
			/* Producer and consumer are the same, all buffers are free */
			fb = ENET_MAX_TX_PACKETS;
		else if (cidx > idx)
			fb = (ENET_MAX_TX_PACKETS - 1) -
				((idx + ENET_MAX_TX_PACKETS) - cidx);
		else
			fb = (ENET_MAX_TX_PACKETS - 1) - (cidx - idx);
	} while (fb == 0);

	/* Update descriptor with new frame size */
	writel(data_length | 0x40000000, &lpc3250->tx_desc[idx].control);

	/* Move data to buffer */
	memcpy(lpc3250->tx_bufs[idx], eth_data, data_length);

	/* Get next index for transmit data DMA buffer and descriptor */
	idx++;
	if (idx >= ENET_MAX_TX_PACKETS)
		idx = 0;

	writel(idx, &lpc3250->regs->txproduceindex);

	return 0;
}

static int lpc3250_receive(struct eth_device *edev)
{
	struct lpc3250_priv *lpc3250 = edev->priv;
	unsigned long idx, len;

	/* Determine if a frame has been received */
	idx = readl(&lpc3250->regs->rxconsumeindex);

	if (readl(&lpc3250->regs->rxproduceindex) == idx)
		return 0;

	/* Clear interrupt */
	writel(MACINT_RXDONEINTEN, &lpc3250->regs->intclear);

	/* Frame received, get size of RX packet */
	len = readl(&lpc3250->rx_status[idx].statusinfo) & 0x7FF;

	/* Pass the packet up to the protocol layer */
	if (len > 0) {
	        memcpy(NetRxPackets[0], lpc3250->rx_bufs[idx], len);
		NetReceive(NetRxPackets[0], len);
	}

	/* Return DMA buffer */
	idx = (idx + 1) % ENET_MAX_TX_PACKETS;

	writel(idx, &lpc3250->regs->rxconsumeindex);

	return 0;
}

static int lpc3250_open(struct eth_device *edev)
{
	struct lpc3250_priv *lpc3250 = (struct lpc3250_priv *)edev->priv;
	int ret;
	u32 tmp = 0;

	/* Enable broadcast and matching address packets */
	writel(RXFLTRW_ACCEPTUBROADCAST | RXFLTRW_ACCEPTPERFECT,
			&lpc3250->regs->rxfliterctrl);

	/* Clear and enable interrupts */
	writel(0xffff, &lpc3250->regs->intclear);
	writel(0, &lpc3250->regs->intenable);

	/* Enable receive and transmit mode of MAC ethernet core */
	tmp = readl(&lpc3250->regs->command);
	tmp |= COMMAND_RXENABLE | COMMAND_TXENABLE;
	writel(tmp, &lpc3250->regs->command);

	tmp = readl(&lpc3250->regs->mac1);
	tmp |= MAC1_RECV_ENABLE;
	writel(tmp, &lpc3250->regs->mac1);

	ret = miiphy_wait_aneg(&lpc3250->miiphy);
        if (ret)
		return ret;
	miiphy_print_status(&lpc3250->miiphy);

	/* Perform a 'dummy' send of the first ethernet frame with a size of 0
	 * to 'prime' the MAC. The first packet after a reset seems to wait
	 * until at least 2 packets are ready to go.
	 */
	lpc3250_send(edev, &tmp, 4);

	return 0;
}

static void lpc3250_shutdown(struct eth_device *edev)
{
	struct lpc3250_priv *lpc3250 = (struct lpc3250_priv *)edev->priv;

	/* Reset all MAC logic */
	writel(MAC1_SOFT_RESET |
			MAC1_SIMULATION_RESET |
			MAC1_RESET_MCS_TX |
			MAC1_RESET_TX |
			MAC1_RESET_MCS_RX |
			MAC1_RESET_RX,
			&lpc3250->regs->mac1);

	writel(COMMAND_REG_RESET |
			COMMAND_TXRESET |
			COMMAND_RXRESET,
			&lpc3250->regs->command);

	mdelay(2);

	/* Disable MAC clocks, but keep MAC interface active */
	if (lpc3250->flags & LPC3250_NET_RMII)
		writel(CLKPWR_MACCTRL_USE_RMII_PINS,
				&CLKPWR->clkpwr_macclk_ctrl);
	else
		writel(CLKPWR_MACCTRL_USE_MII_PINS,
				&CLKPWR->clkpwr_macclk_ctrl);
}

static void lpc3250_halt(struct eth_device *edev)
{
}

static int lpc3250_probe(struct device_d *dev)
{
        struct lpc3250_platform_data *pdata = dev->platform_data;
        struct eth_device *edev;
	struct lpc3250_priv *lpc3250;

	edev = (struct eth_device *)xzalloc(sizeof(struct eth_device));
        dev->type_data = edev;
	lpc3250 = (struct lpc3250_priv *)malloc(sizeof(*lpc3250));

        edev->priv = lpc3250;
	edev->open = lpc3250_open;
	edev->init = lpc3250_init;
	edev->send = lpc3250_send;
	edev->recv = lpc3250_receive;
	edev->halt = lpc3250_halt;
	edev->get_ethaddr = lpc3250_get_hwaddr;
	edev->set_ethaddr = lpc3250_set_hwaddr;
	lpc3250->regs = (void *)dev->map_base;

	if (pdata)
		lpc3250->flags = pdata->flags;

	lpc3250->regs = (void *)dev->map_base;

	lpc3250->miiphy.read = lpc3250_miiphy_read;
	lpc3250->miiphy.write = lpc3250_miiphy_write;
	lpc3250->miiphy.address = 1;
	lpc3250->miiphy.flags = 0;
	lpc3250->miiphy.edev = edev;

	miiphy_register(&lpc3250->miiphy);

	eth_register(edev);

	return 0;
}

static void lpc3250_remove(struct device_d *dev)
{
	struct eth_device *edev = dev->type_data;

	lpc3250_shutdown(edev);
}

/**
 * Driver description for registering
 */
static struct driver_d lpc3250_driver = {
        .name   = "lpc3250_net",
        .probe  = lpc3250_probe,
	.remove = lpc3250_remove,
};

static int lpc3250_register(void)
{
        register_driver(&lpc3250_driver);
        return 0;
}

device_initcall(lpc3250_register);

