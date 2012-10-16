/* linux/drivers/usb/gadget/exynos_ss_udc.h
 *
 * Copyright 2011 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * EXYNOS SuperSpeed USB 3.0 Device Controlle driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#ifndef __EXYNOS_SS_UDC_H__
#define __EXYNOS_SS_UDC_H__

#define DMA_ADDR_INVALID (~((dma_addr_t)0))

/* Maximum packet size for different speeds */
#define EP0_LS_MPS	8
#define EP_LS_MPS	8

#define EP0_FS_MPS	64
#define EP_FS_MPS	64

#define EP0_HS_MPS	64
#define EP_HS_MPS	512

#define EP0_SS_MPS	512
#define EP_SS_MPS	1024

#define EXYNOS_USB3_EPS	(9)

/* Has to be multiple of four */
#define EXYNOS_USB3_EVENT_BUFF_WSIZE	256
#define EXYNOS_USB3_EVENT_BUFF_BSIZE	(EXYNOS_USB3_EVENT_BUFF_WSIZE << 2)

#define EXYNOS_USB3_CTRL_BUFF_SIZE	8
#define EXYNOS_USB3_EP0_BUFF_SIZE	512

#define call_gadget(_udc, _entry)				\
	if ((_udc)->gadget.speed != USB_SPEED_UNKNOWN &&	\
	    (_udc)->driver && (_udc)->driver->_entry)		\
		(_udc)->driver->_entry(&(_udc)->gadget);

/**
 * States of EP0
 */
enum ctrl_ep_state {
	EP0_UNCONNECTED,
	EP0_SETUP_PHASE,
	EP0_DATA_PHASE,
	EP0_WAIT_NRDY,
	EP0_STATUS_PHASE_2,
	EP0_STATUS_PHASE_3,
	EP0_STALL,
};

/**
 * Types of TRB
 */
enum trb_control {
	NORMAL = 1,
	CONTROL_SETUP,
	CONTROL_STATUS_2,
	CONTROL_STATUS_3,
	CONTROL_DATA,
	ISOCHRONOUS_FIRST,
	ISOCHRONOUS,
	LINK_TRB,
};

/**
 * struct exynos_ss_udc_trb - transfer request block (TRB)
 * @buff_ptr_low: Buffer pointer low.
 * @buff_ptr_high: Buffer pointer high.
 * @param1: TRB parameter 1.
 * @param2: TRB parameter 2.
 */
struct exynos_ss_udc_trb {
	u32 buff_ptr_low;
	u32 buff_ptr_high;
	u32 param1;
	u32 param2;
};

/**
 * struct exynos_ss_udc_ep - driver endpoint definition.
 * @ep: The gadget layer representation of the endpoint.
 * @queue: Queue of requests for this endpoint.
 * @cmd_queue: Queue of commands for this endpoint.
 * @parent: Reference back to the parent device structure.
 * @req: The current request that the endpoint is processing. This is
 *       used to indicate an request has been loaded onto the endpoint
 *       and has yet to be completed (maybe due to data move, or simply
 *	 awaiting an ack from the core all the data has been completed).
 * @lock: State lock to protect contents of endpoint.
 * @trb: Transfer Request Block.
 * @trb_dma: Transfer Request Block DMA address.
 * @tri: Transfer resource index.
 * @epnum: The USB endpoint number.
 * @type: The endpoint type.
 * @dir_in: Set to true if this endpoint is of the IN direction, which
 *	    means that it is sending data to the Host.
 * @halted: Set if the endpoint has been halted.
 * @not_ready: Set to true if a command for the endpoint hasn't completed
 *	       during timeout interval.
 * @name: The driver generated name for the endpoint.
 *
 * This is the driver's state for each registered enpoint, allowing it
 * to keep track of transactions that need doing. Each endpoint has a
 * lock to protect the state, to try and avoid using an overall lock
 * for the host controller as much as possible.
 */
struct exynos_ss_udc_ep {
	struct usb_ep		ep;
	struct list_head	queue;
	struct list_head	cmd_queue;
	struct exynos_ss_udc	*parent;
	struct exynos_ss_udc_req	*req;
	spinlock_t		lock;

	struct exynos_ss_udc_trb *trb;
	dma_addr_t		trb_dma;

	u8			tri;

	unsigned char		epnum;
	unsigned int		type;
	unsigned int		dir_in:1;

	unsigned int		halted:1;

	bool			not_ready;

	char			name[10];
};

/**
 * struct exynos_ss_udc_req - data transfer request
 * @req: The USB gadget request.
 * @queue: The list of requests for the endpoint this is queued for.
 * @mapped: DMA buffer for this request has been mapped via dma_map_single().
 */
struct exynos_ss_udc_req {
	struct usb_request	req;
	struct list_head	queue;
	unsigned char		mapped;
};

/**
 * struct exynos_ss_udc_ep_command - endpoint command.
 * @queue: The list of commands for the endpoint.
 * @ep: physical endpoint number.
 * @param0: Command parameter 0.
 * @param1: Command parameter 1.
 * @param2: Command parameter 2.
 * @cmdtype: Command to issue.
 * @cmdflags: Command flags.
 */
struct exynos_ss_udc_ep_command {
	struct list_head	queue;
	int ep;
	u32 param0;
	u32 param1;
	u32 param2;
	u32 cmdtyp;
	u32 cmdflags;
};

/**
 * struct exynos_ss_udc - driver state.
 * @dev: The parent device supplied to the probe function
 * @driver: USB gadget driver
 * @plat: The platform specific configuration data.
 * @regs: The memory area mapped for accessing registers.
 * @regs_res: The resource that was allocated when claiming register space.
 * @irq: The IRQ number we are using.
 * @clk: The clock we are using.
 * @event_buff: Event buffer.
 * @event_buff_dma: Event buffer DMA address.
 * @event_indx: Event buffer index.
 * @eps_enabled: Set if new configuration for physical endpoints > 1 started.
 * @ep0_state: State of EP0.
 * @ep0_three_stage: Set if control transfer has three stages.
 * @ep0_buff: Buffer for EP0 data.
 * @ep0_buff_dma: EP0 data buffer DMA address.
 * @ctrl_buff: Buffer for EP0 control requests.
 * @ctrl_buff_dma: EP0 control request buffer DMA address.
 * @ctrl_req: Request for EP0 control packets.
 * @gadget: Represents USB slave device.
 * @eps: The endpoints being supplied to the gadget framework
 */
struct exynos_ss_udc {
	struct device		 *dev;
	struct usb_gadget_driver *driver;
	struct exynos_ss_udc_plat	 *plat;

	void __iomem		*regs;
	struct resource		*regs_res;
	int			irq;
	struct clk		*clk;

	u32			*event_buff;
	dma_addr_t		event_buff_dma;
	u32			event_indx;

	bool			eps_enabled;
	enum ctrl_ep_state	ep0_state;
	int			ep0_three_stage;
	u8			*ep0_buff;
	dma_addr_t		ep0_buff_dma;
	u8			*ctrl_buff;
	dma_addr_t		ctrl_buff_dma;
	struct usb_request	*ctrl_req;

	struct usb_gadget	gadget;
	struct exynos_ss_udc_ep	eps[EXYNOS_USB3_EPS];
};

/* conversion functions */
static inline struct exynos_ss_udc_req *our_req(struct usb_request *req)
{
	return container_of(req, struct exynos_ss_udc_req, req);
}

static inline struct exynos_ss_udc_ep *our_ep(struct usb_ep *ep)
{
	return container_of(ep, struct exynos_ss_udc_ep, ep);
}

static inline void __orr32(void __iomem *ptr, u32 val)
{
	writel(readl(ptr) | val, ptr);
}

static inline void __bic32(void __iomem *ptr, u32 val)
{
	writel(readl(ptr) & ~val, ptr);
}

static inline int get_phys_epnum(struct exynos_ss_udc_ep *udc_ep)
{
	return udc_ep->epnum * 2 + udc_ep->dir_in;
}

static inline int get_usb_epnum(int index)
{
	return index >> 1;
}

#endif /* __EXYNOS_SS_UDC_H__ */
