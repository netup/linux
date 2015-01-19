/*
 * netup_unidvb.h
 *
 * Data type definitions for NetUP Universal Dual DVB-CI
 *
 * Copyright (C) 2014 NetUP Inc.
 * Copyright (C) 2014 Sergey Kozlov <serjk@netup.ru>
 * Copyright (C) 2014 Abylay Ospan <aospan@netup.ru>
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
 */

#include <linux/pci.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/videobuf-dma-sg.h>
#include <media/videobuf-dvb.h>
#include <dvb_ca_en50221.h>

#define NETUP_UNIDVB_NAME	"netup_unidvb"
#define NETUP_UNIDVB_VERSION	"0.0.1"
#define NETUP_VENDOR_ID		0x1b55
#define NETUP_PCI_DEV_REVISION  0x2

/* Avalon-MM PCI-E registers */
#define	AVL_PCIE_IENR		0x50
#define AVL_PCIE_ISR		0x40
#define AVL_IRQ_ENABLE		0x80
#define AVL_IRQ_ASSERTED	0x80

/* IRQ-related regisers */
#define REG_ISR			0x4890
#define REG_ISR_MASKED		0x4892
#define REG_IMASK_SET		0x4894
#define REG_IMASK_CLEAR		0x4896

#define NETUP_UNIDVB_IRQ_SPI	(1 << 0)
#define NETUP_UNIDVB_IRQ_I2C0	(1 << 1)
#define NETUP_UNIDVB_IRQ_I2C1	(1 << 2)
#define NETUP_UNIDVB_IRQ_FRA0	(1 << 4)	/* FrontendA IRQ0 */
#define NETUP_UNIDVB_IRQ_FRA1	(1 << 5)	/* FrontendA IRQ1 */
#define NETUP_UNIDVB_IRQ_FRB0	(1 << 6)	/* FrontendB IRQ0 */
#define NETUP_UNIDVB_IRQ_FRB1	(1 << 7)	/* FrontendB IRQ1 */
#define NETUP_UNIDVB_IRQ_DMA1	(1 << 8)	/* DMA1 */
#define NETUP_UNIDVB_IRQ_DMA2	(1 << 9)	/* DMA2 */
#define NETUP_UNIDVB_IRQ_CI	(1 << 10)	/* CI MAIN MODULE */
#define NETUP_UNIDVB_IRQ_CAM0	(1 << 11)	/* CI CAM0 */
#define NETUP_UNIDVB_IRQ_CAM1	(1 << 12)	/* CI CAM1 */

/* GPIO registers */
#define GPIO_REG_IO		0x4880
#define GPIO_REG_IO_TOGGLE	0x4882
#define GPIO_REG_IO_SET		0x4884
#define GPIO_REG_IO_CLEAR	0x4886
/* GPIO pins */
#define GPIO_FEA_RESET		(1 << 0)
#define GPIO_FEB_RESET		(1 << 1)
#define GPIO_RFA_CTL		(1 << 2)
#define GPIO_RFB_CTL		(1 << 3)
#define GPIO_FEA_TU_RESET	(1 << 4)
#define GPIO_FEB_TU_RESET	(1 << 5)

/* DMA section */
#define NETUP_DMA0_ADDR		0x4900
#define NETUP_DMA1_ADDR		0x4940
/* DMA part. 8 blocks for 120Mbps per port is ok */
#define NETUP_DMA_BLOCKS_COUNT	8
#define NETUP_DMA_PACKETS_COUNT	128
/* DMA block status */
#define NETUP_DMA_UNK		1
#define NETUP_DMA_DONE		2
#define BIT_DMA_RUN		1
#define BIT_DMA_ERROR		2
#define BIT_DMA_IRQ		0x200

struct netup_dma_regs {
	/* see BIT_DMA_* defines;
	 * [24-31] - dma_current_length;
	 * [16-23] - dma_current_block_cnt */
	u32	ctrlstat_set;
	u32	ctrlstat_clear;
	/* dma start address, low */
	u32	start_addr_lo;
	/* dma start address, high */
	u32	start_addr_hi;
	/* [0-7] dma_packet_size (188);
	 * [16-23] packets count in block (128);
	 * [24-31] dma_length - blocks count in dma */
	u32	size;
	/* 8ns intervals;
	 * for example 3sec => timeout = 375 000 000 */
	u32	timeout;
	/* dma current address, low */
	u32	curr_addr_lo;
	/* dma current address, high */
	u32	curr_addr_hi;
	u32	stat_pkt_received;
	u32	stat_pkt_accepted;
	u32	stat_pkt_overruns;
	u32	stat_pkt_underruns;
	u32	stat_fifo_overruns;
} __packed __aligned(1);

struct netup_unidvb_buffer {
	struct videobuf_buffer	vb;
	u32			size;
};

struct netup_dma {
	u8			num;
	spinlock_t		lock;
	struct netup_unidvb_dev	*ndev;
	struct netup_dma_regs	*regs;
	u32			ring_buffer_size;
	u8			*addr_virt;
	dma_addr_t		addr_phys;
	u64			addr_last;
	u32			high_addr;
	u32			data_offset;
	u32			data_size;
	struct list_head	free_buffers;
	struct work_struct	work;
	struct timer_list	timeout;
};

/* I2C bus section */
#define NETUP_I2C_BUS0_ADDR		0x4800
#define NETUP_I2C_BUS1_ADDR		0x4840
#define NETUP_I2C_TIMEOUT		HZ

struct netup_i2c_fifo_regs {
	union {
		u8	data8;
		u16	data16;
		u32	data32;
	};
	u8		padding[4];
	u16		stat_ctrl;
} __packed __aligned(1);

struct netup_i2c_regs {
	u16				clkdiv;
	u16				twi_ctrl0_stat;
	u16				twi_addr_ctrl1;
	u16				length;
	u8				padding1[8];
	struct netup_i2c_fifo_regs	tx_fifo;
	u8				padding2[6];
	struct netup_i2c_fifo_regs	rx_fifo;
} __packed __aligned(1);

enum netup_i2c_state {
	STATE_DONE,
	STATE_WAIT,
	STATE_WANT_READ,
	STATE_WANT_WRITE,
	STATE_ERROR
};

struct netup_i2c {
	spinlock_t			lock;
	wait_queue_head_t		wq;
	struct i2c_adapter		adap;
	struct netup_unidvb_dev		*dev;
	struct netup_i2c_regs		*regs;
	struct i2c_msg			*msg;
	enum netup_i2c_state		state;
	u32				xmit_size;
};

/* CI section */
struct netup_ci_state {
	struct dvb_ca_en50221		ca;
	u8 __iomem			*membase8_config;
	u8 __iomem			*membase8_io;
	struct netup_unidvb_dev		*dev;
	int status;
	int nr;
};

#define CAM_CTRLSTAT_READ_SET		0x4980
#define CAM_CTRLSTAT_CLR		0x4982

/* SPI flash section */
struct netup_spi;

/* main device context */

struct netup_unidvb_dev {
	struct pci_dev			*pci_dev;
	int				pci_bus;
	int				pci_slot;
	int				pci_func;
	int				board_num;
	int				old_fw;
	/* MMIO */
	u32 __iomem			*lmmio0;
	u8 __iomem			*bmmio0;
	u32 __iomem			*lmmio1;
	u8 __iomem			*bmmio1;
	struct videobuf_dvb_frontends	frontends[2];
	/* I2C buses */
	struct netup_i2c	i2c[2];
	/* DMA */
	struct workqueue_struct	*wq;
	struct netup_dma	dma[2];
	/* CI slots */
	struct netup_ci_state	ci[2];
	/* SPI */
	struct netup_spi	*spi;
};

int netup_i2c_register(struct netup_unidvb_dev *ndev);
void netup_i2c_unregister(struct netup_unidvb_dev *ndev);
irqreturn_t netup_i2c_interrupt(struct netup_i2c *i2c);
irqreturn_t netup_spi_interrupt(struct netup_spi *spi);
irqreturn_t netup_dma_interrupt(struct netup_dma *dma);
int netup_unidvb_ci_register(
	struct netup_unidvb_dev *dev, int num, struct pci_dev *pci_dev);
void netup_unidvb_ci_unregister(struct netup_unidvb_dev *dev, int num);
int netup_spi_init(struct netup_unidvb_dev *ndev);
void netup_spi_release(struct netup_unidvb_dev *ndev);
