/* drivers/android/ram_console.c
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/console.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/platform_data/ram_console.h>
#include <linux/cma.h>
#include <linux/seq_file.h>
#include <linux/kmsg_dump.h>
#include <linux/rtc.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <mach/regs-pmu.h>

#ifdef CONFIG_ANDROID_RAM_CONSOLE_ERROR_CORRECTION
#include <linux/rslib.h>
#endif

/* First/Fresh boot means a clean powered-off boot which may be triggered
 * by the following sources:
 *
 * 1. USB inserted power on: pin reset
 * 2. Power button short press from a previous shutdown: pin reset
 * 3. Power button long press from a dead: pin reset
 */

enum boot_reason {
	FRESH_BOOT,	/* First/Fresh boot */
	SW_RESTART,	/* Software reboot */
	OOPS_RESTART,
	PANIC_RESTART,
	HALT_RESTART,
	POWEROFF_RESTART,
	KEXEC_RESTART,
	EMERG_RESTART,
	WR_RESTART,	/* Hardware reset */
	WDT_RESTART,
	PIN_RESTART,
	UNKNOWN_REASON,
	END_REASON,	/* For end */
};

static char boot_reason_str[END_REASON][20] = {
	"fresh boot",
	"software reboot",
	"oops reboot",
	"panic reboot",
	"halt reboot",
	"poweroff reboot",
	"kexec reboot",
	"emerg reboot",
	"warm reset",
	"watchdog reset",
	"pin reset",
	"unknow reason",
};

struct boot_stat {
	int reason, new_reason;
	unsigned long long count[END_REASON];
};

struct time_info {
	char boot_time[20];
	char update_time[20];
};

struct ram_console_buffer {
	uint32_t    sig;
	uint32_t    start;
	uint32_t    size;
	struct boot_stat bs;
	struct time_info ti;
	uint8_t     data[0];
};

static int fresh_boot;		/* A normal boot or hardware restart */
struct time_info last_time_info;	/* For last boot */

#define RAM_CONSOLE_SIG (0x43474244) /* DBGC */

#ifdef CONFIG_ANDROID_RAM_CONSOLE_EARLY_INIT
static char __initdata
	ram_console_old_log_init_buffer[CONFIG_ANDROID_RAM_CONSOLE_EARLY_SIZE];
#endif
static char *ram_console_old_log;
static size_t ram_console_old_log_size;

static struct ram_console_buffer *ram_console_buffer;
static size_t ram_console_buffer_size;
#ifdef CONFIG_ANDROID_RAM_CONSOLE_ERROR_CORRECTION
static char *ram_console_par_buffer;
static struct rs_control *ram_console_rs_decoder;
static int ram_console_corrected_bytes;
static int ram_console_bad_blocks;
#define ECC_BLOCK_SIZE CONFIG_ANDROID_RAM_CONSOLE_ERROR_CORRECTION_DATA_SIZE
#define ECC_SIZE CONFIG_ANDROID_RAM_CONSOLE_ERROR_CORRECTION_ECC_SIZE
#define ECC_SYMSIZE CONFIG_ANDROID_RAM_CONSOLE_ERROR_CORRECTION_SYMBOL_SIZE
#define ECC_POLY CONFIG_ANDROID_RAM_CONSOLE_ERROR_CORRECTION_POLYNOMIAL
#endif

#define inc_boot_reason(r)			\
do {						\
	ram_console_buffer->bs.new_reason = r;	\
	ram_console_buffer->bs.count[r]++;	\
	pr_info("%s: %d, %s, %llu\n",		\
		__func__, r, boot_reason_str[r], ram_console_buffer->bs.count[r]); \
} while (0)

void record_boot_reason(enum kmsg_dump_reason reason)
{
	static int recorded;

	if (!ram_console_buffer || recorded)
		return;

	/* Record once */
	recorded = 1;

	switch (reason) {
	case KMSG_DUMP_OOPS:
		inc_boot_reason(OOPS_RESTART);
		break;
	case KMSG_DUMP_PANIC:
		inc_boot_reason(PANIC_RESTART);
		break;
	case KMSG_DUMP_RESTART:
		inc_boot_reason(SW_RESTART);
		break;
	case KMSG_DUMP_HALT:
		inc_boot_reason(HALT_RESTART);
		break;
	case KMSG_DUMP_POWEROFF:
		inc_boot_reason(POWEROFF_RESTART);
		break;
	case KMSG_DUMP_EMERG:
		inc_boot_reason(EMERG_RESTART);
		break;
	case KMSG_DUMP_KEXEC:
		inc_boot_reason(KEXEC_RESTART);
		break;
	default:
		pr_warning("%s: Don't support this boot reason\n", __func__);
		break;
	}
}

#define get_current_boot_reason() (ram_console_buffer->bs.reason)
#define get_boot_reason_str(r)	(boot_reason_str[r])
#define get_boot_count(r) (ram_console_buffer->bs.count[r])

static inline char *get_current_boot_reason_str(void)
{
	return get_boot_reason_str(get_current_boot_reason());
}

static inline unsigned long long get_current_boot_count(void)
{
	return get_boot_count(get_current_boot_reason());
}

/* Hardware specific part */
#define SWRESET		(1 << 29)
#define WREST		(1 << 28)
#define SYS_WDTRESET	(1 << 20)
#define PINRESET	(1 << 16)

static void check_hardware_reset_reason(void)
{
	int reset_reason;

	reset_reason = __raw_readl(S5P_RST_STAT);
	pr_debug("%s: reset_reason = 0x%x\n", __func__, reset_reason);

	if (reset_reason & SWRESET) {
		pr_debug("%s: Boot reason from RST_STAT: SWRESET\n", __func__);
		inc_boot_reason(SW_RESTART);
	} else if (reset_reason & WREST) {
		pr_debug("%s: Boot reason from RST_STAT: WRESET\n", __func__);
		inc_boot_reason(WR_RESTART);
	} else if (reset_reason & SYS_WDTRESET) {
		pr_debug("%s: Boot reason from RST_STAT: WDT_RESET\n", __func__);
		inc_boot_reason(WDT_RESTART);
	} else if (reset_reason & PINRESET) {
		pr_debug("%s: Boot reason from RST_STAT: PINRESET\n", __func__);
		inc_boot_reason(PIN_RESTART);
	} else {
		pr_debug("%s: Boot reason from RST_STAT: the others\n", __func__);
		inc_boot_reason(UNKNOWN_REASON);
	}
}

static void init_boot_reason(void)
{
	int reason, i;

	if (fresh_boot) {
		memset(&ram_console_buffer->bs, 0, sizeof(struct boot_stat));
		inc_boot_reason(FRESH_BOOT);
	}

	/* Get the software reason */
	reason = ram_console_buffer->bs.new_reason;
	/* The reason coutent may include shit, clear it if so... */
	if (reason < FRESH_BOOT || reason >= END_REASON) {
		pr_err("%s: The boot_stat info is broken, clear them\n", __func__);
		memset(&ram_console_buffer->bs, 0, sizeof(struct boot_stat));
		reason = UNKNOWN_REASON;
		inc_boot_reason(reason);
	}
	/* If the reason is the one we know, show it
	 * Note: The FRESH_BOOT is not precise, so, check if a hard reset happens.
	 */
	if (reason != UNKNOWN_REASON && reason != FRESH_BOOT)
		goto out;

	/* Check the hardware reason */
	check_hardware_reset_reason();
out:
	/* Save the new reason to reason
	 *
	 * Note: The *current* related functions can only be called after the following line.
	 */
	ram_console_buffer->bs.reason = ram_console_buffer->bs.new_reason;
	ram_console_buffer->bs.new_reason = UNKNOWN_REASON;

	/* Show boot stat */
	pr_info(BOOT_FROM_LABEL "%d, %s, %llu\n", get_current_boot_reason(), get_current_boot_reason_str(), get_current_boot_count());
	pr_info(BOOT_STAT_LABEL);
	for (i = FRESH_BOOT; i < END_REASON; i++)
		pr_info("%d, %s, %llu\n", i, get_boot_reason_str(i), get_boot_count(i));
}

static void get_std_time(char time[])
{
	struct timespec ts;
	struct rtc_time tm;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	sprintf(time, "%d/%02d/%02d-%02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour + 8, tm.tm_min, tm.tm_sec);
}

static void init_time_info(void)
{
	if (!ram_console_buffer)
		return;
	if (fresh_boot)
		memset(&ram_console_buffer->ti, ' ', sizeof(struct time_info));
	memcpy(&last_time_info, &ram_console_buffer->ti, sizeof(struct time_info));

	/* Reset the time info for the new boot */
	memset(&ram_console_buffer->ti, ' ', sizeof(struct time_info));
}

static struct console ram_console;

void ram_console_update_time_info(void)
{
	static int init;

	if (!ram_console_buffer)
		return;

	get_std_time(ram_console_buffer->ti.update_time);
	if (init == 0) {
		memcpy(&ram_console_buffer->ti.boot_time, &ram_console_buffer->ti.update_time, 20);
		init = 1;
	}
}

static int ram_console_update_thread(void *data)
{
	while (1) {
		msleep_interruptible(10 * 1000);

		if (ram_console.flags & CON_ENABLED)
			ram_console_update_time_info();
	}

	return 0;
}

static int __init start_update_thread(void)
{
	struct task_struct *update_task;

	pr_info("%s: Start ram console update daemon\n", __func__);
	update_task = kthread_run(ram_console_update_thread, NULL, "ram_console_update/daemon");
	if (IS_ERR(update_task)) {
		pr_err("%s: Fail to create ram console update thread\n", __func__);
		return -EFAULT;
	}
	return 0;
}

#ifdef CONFIG_ANDROID_RAM_CONSOLE_ERROR_CORRECTION
static void ram_console_encode_rs8(uint8_t *data, size_t len, uint8_t *ecc)
{
	int i;
	uint16_t par[ECC_SIZE];
	/* Initialize the parity buffer */
	memset(par, 0, sizeof(par));
	encode_rs8(ram_console_rs_decoder, data, len, par, 0);
	for (i = 0; i < ECC_SIZE; i++)
		ecc[i] = par[i];
}

static int ram_console_decode_rs8(void *data, size_t len, uint8_t *ecc)
{
	int i;
	uint16_t par[ECC_SIZE];
	for (i = 0; i < ECC_SIZE; i++)
		par[i] = ecc[i];
	return decode_rs8(ram_console_rs_decoder, data, par, len,
				NULL, 0, NULL, 0, NULL);
}
#endif

static void ram_console_update(const char *s, unsigned int count)
{
	struct ram_console_buffer *buffer = ram_console_buffer;
#ifdef CONFIG_ANDROID_RAM_CONSOLE_ERROR_CORRECTION
	uint8_t *buffer_end = buffer->data + ram_console_buffer_size;
	uint8_t *block;
	uint8_t *par;
	int size = ECC_BLOCK_SIZE;
#endif
	memcpy(buffer->data + buffer->start, s, count);
#ifdef CONFIG_ANDROID_RAM_CONSOLE_ERROR_CORRECTION
	block = buffer->data + (buffer->start & ~(ECC_BLOCK_SIZE - 1));
	par = ram_console_par_buffer +
	      (buffer->start / ECC_BLOCK_SIZE) * ECC_SIZE;
	do {
		if (block + ECC_BLOCK_SIZE > buffer_end)
			size = buffer_end - block;
		ram_console_encode_rs8(block, size, par);
		block += ECC_BLOCK_SIZE;
		par += ECC_SIZE;
	} while (block < buffer->data + buffer->start + count);
#endif
}

static void ram_console_update_header(void)
{
#ifdef CONFIG_ANDROID_RAM_CONSOLE_ERROR_CORRECTION
	struct ram_console_buffer *buffer = ram_console_buffer;
	uint8_t *par;
	par = ram_console_par_buffer +
	      DIV_ROUND_UP(ram_console_buffer_size, ECC_BLOCK_SIZE) * ECC_SIZE;
	ram_console_encode_rs8((uint8_t *)buffer, sizeof(*buffer), par);
#endif
}

static void
ram_console_write(struct console *console, const char *s, unsigned int count)
{
	int rem;
	struct ram_console_buffer *buffer = ram_console_buffer;

	if (count > ram_console_buffer_size) {
		s += count - ram_console_buffer_size;
		count = ram_console_buffer_size;
	}
	rem = ram_console_buffer_size - buffer->start;
	if (rem < count) {
		ram_console_update(s, rem);
		s += rem;
		count -= rem;
		buffer->start = 0;
		buffer->size = ram_console_buffer_size;
	}
	ram_console_update(s, count);

	buffer->start += count;
	if (buffer->size < ram_console_buffer_size)
		buffer->size += count;
	ram_console_update_header();
}

static struct console ram_console = {
	.name	= "ram",
	.write	= ram_console_write,
	.flags	= CON_PRINTBUFFER | CON_ENABLED | CON_ANYTIME,
	.index	= -1,
};

void ram_console_enable_console(int enabled)
{
	if (enabled)
		ram_console.flags |= CON_ENABLED;
	else
		ram_console.flags &= ~CON_ENABLED;
}

static void __init
ram_console_save_old(struct ram_console_buffer *buffer, const char *bootinfo,
	char *dest)
{
	size_t old_log_size = buffer->size;
	size_t bootinfo_size = 0;
	size_t total_size = old_log_size;
	char *ptr;
	const char *bootinfo_label = BOOT_INFO_LABEL;

#ifdef CONFIG_ANDROID_RAM_CONSOLE_ERROR_CORRECTION
	uint8_t *block;
	uint8_t *par;
	char strbuf[80];
	int strbuf_len = 0;

	block = buffer->data;
	par = ram_console_par_buffer;
	while (block < buffer->data + buffer->size) {
		int numerr;
		int size = ECC_BLOCK_SIZE;
		if (block + size > buffer->data + ram_console_buffer_size)
			size = buffer->data + ram_console_buffer_size - block;
		numerr = ram_console_decode_rs8(block, size, par);
		if (numerr > 0) {
#if 0
			printk(KERN_INFO "ram_console: error in block %p, %d\n",
			       block, numerr);
#endif
			ram_console_corrected_bytes += numerr;
		} else if (numerr < 0) {
#if 0
			printk(KERN_INFO "ram_console: uncorrectable error in "
			       "block %p\n", block);
#endif
			ram_console_bad_blocks++;
		}
		block += ECC_BLOCK_SIZE;
		par += ECC_SIZE;
	}
	if (ram_console_corrected_bytes || ram_console_bad_blocks)
		strbuf_len = snprintf(strbuf, sizeof(strbuf),
			"\n%d Corrected bytes, %d unrecoverable blocks\n",
			ram_console_corrected_bytes, ram_console_bad_blocks);
	else
		strbuf_len = snprintf(strbuf, sizeof(strbuf),
				      "\nNo errors detected\n");
	if (strbuf_len >= sizeof(strbuf))
		strbuf_len = sizeof(strbuf) - 1;
	total_size += strbuf_len;
#endif

	if (bootinfo)
		bootinfo_size = strlen(bootinfo) + strlen(bootinfo_label);
	total_size += bootinfo_size;

	if (dest == NULL) {
		dest = kmalloc(total_size, GFP_KERNEL);
		if (dest == NULL) {
			printk(KERN_ERR
			       "ram_console: failed to allocate buffer\n");
			return;
		}
	}

	ram_console_old_log = dest;
	ram_console_old_log_size = total_size;
	memcpy(ram_console_old_log,
	       &buffer->data[buffer->start], buffer->size - buffer->start);
	memcpy(ram_console_old_log + buffer->size - buffer->start,
	       &buffer->data[0], buffer->start);
	ptr = ram_console_old_log + old_log_size;
#ifdef CONFIG_ANDROID_RAM_CONSOLE_ERROR_CORRECTION
	memcpy(ptr, strbuf, strbuf_len);
	ptr += strbuf_len;
#endif
	if (bootinfo) {
		memcpy(ptr, bootinfo_label, strlen(bootinfo_label));
		ptr += strlen(bootinfo_label);
		memcpy(ptr, bootinfo, bootinfo_size);
		ptr += bootinfo_size;
	}
}

static int __init ram_console_init(struct ram_console_buffer *buffer,
				   size_t buffer_size, const char *bootinfo,
				   char *old_buf)
{
#ifdef CONFIG_ANDROID_RAM_CONSOLE_ERROR_CORRECTION
	int numerr;
	uint8_t *par;
#endif
	ram_console_buffer = buffer;
	ram_console_buffer_size =
		buffer_size - sizeof(struct ram_console_buffer);

	if (ram_console_buffer_size > buffer_size) {
		pr_err("ram_console: buffer %p, invalid size %zu, "
		       "datasize %zu\n", buffer, buffer_size,
		       ram_console_buffer_size);
		return 0;
	}

#ifdef CONFIG_ANDROID_RAM_CONSOLE_ERROR_CORRECTION
	ram_console_buffer_size -= (DIV_ROUND_UP(ram_console_buffer_size,
						ECC_BLOCK_SIZE) + 1) * ECC_SIZE;

	if (ram_console_buffer_size > buffer_size) {
		pr_err("ram_console: buffer %p, invalid size %zu, "
		       "non-ecc datasize %zu\n",
		       buffer, buffer_size, ram_console_buffer_size);
		return 0;
	}

	ram_console_par_buffer = buffer->data + ram_console_buffer_size;


	/* first consecutive root is 0
	 * primitive element to generate roots = 1
	 */
	ram_console_rs_decoder = init_rs(ECC_SYMSIZE, ECC_POLY, 0, 1, ECC_SIZE);
	if (ram_console_rs_decoder == NULL) {
		printk(KERN_INFO "ram_console: init_rs failed\n");
		return 0;
	}

	ram_console_corrected_bytes = 0;
	ram_console_bad_blocks = 0;

	par = ram_console_par_buffer +
	      DIV_ROUND_UP(ram_console_buffer_size, ECC_BLOCK_SIZE) * ECC_SIZE;

	numerr = ram_console_decode_rs8(buffer, sizeof(*buffer), par);
	if (numerr > 0) {
		printk(KERN_INFO "ram_console: error in header, %d\n", numerr);
		ram_console_corrected_bytes += numerr;
	} else if (numerr < 0) {
		printk(KERN_INFO
		       "ram_console: uncorrectable error in header\n");
		ram_console_bad_blocks++;
	}
#endif

	if (buffer->sig == RAM_CONSOLE_SIG) {
		if (buffer->size > ram_console_buffer_size
		    || buffer->start > buffer->size)
			printk(KERN_INFO "ram_console: found existing invalid "
			       "buffer, size %d, start %d\n",
			       buffer->size, buffer->start);
		else {
			printk(KERN_INFO "ram_console: found existing buffer, "
			       "size %d, start %d\n",
			       buffer->size, buffer->start);
			ram_console_save_old(buffer, bootinfo, old_buf);
		}
	} else {
		printk(KERN_INFO "ram_console: no valid data in buffer "
		       "(sig = 0x%08x)\n", buffer->sig);
		fresh_boot = 1;
	}

	buffer->sig = RAM_CONSOLE_SIG;
	buffer->start = 0;
	buffer->size = 0;

	register_console(&ram_console);
#ifdef CONFIG_ANDROID_RAM_CONSOLE_ENABLE_VERBOSE
	console_verbose();
#endif

	init_boot_reason();

	return 0;
}

#ifdef CONFIG_ANDROID_RAM_CONSOLE_EARLY_INIT
static int __init ram_console_early_init(void)
{
	return ram_console_init((struct ram_console_buffer *)
		CONFIG_ANDROID_RAM_CONSOLE_EARLY_ADDR,
		CONFIG_ANDROID_RAM_CONSOLE_EARLY_SIZE,
		NULL,
		ram_console_old_log_init_buffer);
}
#else
static int ram_console_driver_probe(struct platform_device *pdev)
{
	size_t start;
	size_t buffer_size;
	void *buffer;
	const char *bootinfo = NULL;
	struct ram_console_platform_data *pdata = pdev->dev.platform_data;

	buffer_size = CONFIG_ANDROID_RAM_CONSOLE_MEMSIZE * SZ_1K - PAGE_SIZE;

	start = cma_alloc(&pdev->dev, "ram_console", buffer_size, 0);
	if (IS_ERR_VALUE(start)) {
		pr_err("%s: CMA Alloc Error!!!", __func__);
		return start;
	}
	printk(KERN_INFO "ram_console: got buffer at %zx, size %zx\n",
	       start, buffer_size);

	buffer = cma_get_virt(start, buffer_size, 1);

	if (pdata)
		bootinfo = pdata->bootinfo;

	return ram_console_init(buffer, buffer_size, bootinfo, NULL/* allocate */);
}

static struct platform_driver ram_console_driver = {
	.probe = ram_console_driver_probe,
	.driver		= {
		.name	= "ram_console",
	},
};

static int __init ram_console_module_init(void)
{
	int err = 0;

	pr_info("%s\n", __func__);
	err = platform_driver_register(&ram_console_driver);
	if (err)
		pr_err("%s: platform_driver_register fail\n", __func__);

	return err;
}
#endif

#ifdef CONFIG_DUMP_LAST_KMSG

#include <linux/bootmode.h>
#include <linux/syscalls.h>

/* Check if current boot is from a previous crash */
int boot_from_crash(void)
{
	int reason = get_current_boot_reason();

	return (reason == OOPS_RESTART || reason == PANIC_RESTART || reason == WDT_RESTART);
}
EXPORT_SYMBOL(boot_from_crash);

/*
 * dump_last_kmsg(): allow to dump the /proc/last_kmsg to a file on disk for late acessing
 *
 * @log_file: The file name with absolute path used to save the dumped file.
 *
 * Note: Please create the directory for the log file, this function will not care about it.
 */
int dump_last_kmsg(char *log_file)
{
	struct file *fp_in, *fp_out;
	ssize_t max_size, cur_size, nread, nwrite;
	mm_segment_t old_fs;
	int err = 0;
	char buf[512];

	if (ram_console_old_log == NULL || ram_console_old_log_size == 0) {
		pr_err("%s: No invalid existing last kernel log saved\n", __func__);
		return -EFAULT;
	}

	pr_info("%s: open /proc/last_kmsg\n", __func__);
	fp_in = filp_open("/proc/last_kmsg", O_RDONLY, 0);
	if (IS_ERR(fp_in)) {
		pr_err("open /proc/last_kmsg error! err = %ld!\n", PTR_ERR(fp_in));
		err = PTR_ERR(fp_in);
		goto out;
	}
	pr_info("%s: open %s\n", __func__, log_file);
	fp_out = filp_open(log_file, O_RDWR | O_CREAT | O_APPEND, 0777);
	if (IS_ERR(fp_out)) {
		pr_err("open %s error! err = %ld!\n", log_file, PTR_ERR(fp_out));
		filp_close(fp_in, NULL);
		err = PTR_ERR(fp_out);
		goto out;
	}

	/* Get file length */
	max_size = CONFIG_LAST_KMSG_LOG_FILE_MAX_SIZE * 1024;
	old_fs = get_fs();
	set_fs(get_ds());
	cur_size = fp_out->f_op->llseek(fp_out, 0, SEEK_END);
	set_fs(old_fs);
	pr_info("%s: max_size = %d, current size = %d\n", __func__, max_size, cur_size);
	if (cur_size >= max_size) {
		/* Trucate it to zero length and copy /proc/last_kmsg to it. */
		do_truncate(fp_out->f_dentry, 0, 0, NULL);
	}

	while (1) {
		memset(buf, '\0', sizeof(buf));
		old_fs = get_fs();
		set_fs(get_ds());
		nread = fp_in->f_op->read(fp_in, buf, sizeof(buf), &fp_in->f_pos);
		set_fs(old_fs);
		if (nread == 0)
			break;
		if (nread < 0) {
			err = nread;
			pr_err("%s: read /proc/last_kmsg error, err = %d\n\n", __func__, err);
			break;
		}

		pr_debug("%s: write %d message to %s\n", __func__, nread, log_file);
		old_fs = get_fs();
		set_fs(get_ds());
		nwrite = fp_out->f_op->write(fp_out, buf, nread, &fp_out->f_pos);
		set_fs(old_fs);
		if (nwrite < 0) {
			err = nwrite;
			pr_err("%s: write %s error, err = %d\n\n", __func__, log_file, err);
		}
	}
	filp_close(fp_in, NULL);
	old_fs = get_fs();
	set_fs(get_ds());
	err = fp_out->f_op->fsync(fp_out, 1);
	set_fs(old_fs);
	filp_close(fp_out, NULL);

out:
	return err;
}

#endif

static int boot_stat_show(struct seq_file *m, void *v)
{
	int reason, i;
	unsigned long long count;
	char *reason_str;

	reason = get_current_boot_reason();

	if (reason < FRESH_BOOT || reason >= END_REASON) {
		pr_err("%s: The boot_stat info is broken, clear them\n", __func__);
		memset(&ram_console_buffer->bs, 0, sizeof(struct boot_stat));
		reason = UNKNOWN_REASON;
		inc_boot_reason(reason);
		return -EFAULT;
	}

	reason_str = get_current_boot_reason_str();
	count = get_current_boot_count();
	pr_debug(BOOT_FROM_LABEL "%d, %s, %llu\n", reason, reason_str, count);
	seq_printf(m, BOOT_FROM_LABEL "%d, %s, %llu\n", reason, reason_str, count);
	seq_printf(m, BOOT_STAT_LABEL);
	for (i = FRESH_BOOT; i < END_REASON; i++) {
		pr_debug("%d, %s, %llu\n", i, get_boot_reason_str(i), get_boot_count(i));
		seq_printf(m, "%d, %s, %llu\n", i, get_boot_reason_str(i), get_boot_count(i));
	}

	return 0;
}

static void *boot_stat_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

static void *boot_stat_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void boot_stat_stop(struct seq_file *m, void *v)
{
}

const struct seq_operations boot_stat_op = {
	.start	= boot_stat_start,
	.next	= boot_stat_next,
	.stop	= boot_stat_stop,
	.show	= boot_stat_show
};

static int proc_boot_stat_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &boot_stat_op);
}

static const struct file_operations proc_boot_stat_operations = {
	.open		= proc_boot_stat_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int last_kmsg_show(struct seq_file *m, void *v)
{
	char *p;
	char dump_time[20];

	get_std_time(dump_time);
	seq_printf(m, BOOT_TIME_LABEL "Boot@%s; Update@%s; Dump@%s\n",
		last_time_info.boot_time,
		last_time_info.update_time,
		dump_time);
	seq_printf(m, BOOT_INFO_LABEL "%s", RAM_CONSOLE_BOOT_INFO);
	seq_printf(m, BOOT_MACH_LABEL "%s, %04x, %08x%08x\n", machine_name, system_rev,
		   system_serial_high, system_serial_low);
	seq_printf(m, BOOT_PARM_LABEL "%s\n", saved_command_line);
	boot_stat_show(m, v);
	seq_printf(m, BOOT_KMSG_LABEL);
	/* Filter the space symbol from the old meesage log */
	p = ram_console_old_log;
	while (*p == ' ')
		p++;
	seq_printf(m, "%s", p);

	return 0;
}

static void *last_kmsg_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

static void *last_kmsg_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void last_kmsg_stop(struct seq_file *m, void *v)
{
}

const struct seq_operations last_kmsg_op = {
	.start	= last_kmsg_start,
	.next	= last_kmsg_next,
	.stop	= last_kmsg_stop,
	.show	= last_kmsg_show
};

static int proc_last_kmsg_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &last_kmsg_op);
}

static const struct file_operations proc_last_kmsg_operations = {
	.open		= proc_last_kmsg_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int __init ram_console_late_init(void)
{
	struct proc_dir_entry *entry;

	/* Whenever the old ram buffer exist, provide the boot_stat interface */
	proc_create("boot_stat", S_IFREG | S_IRUGO, NULL, &proc_boot_stat_operations);

	if (ram_console_old_log == NULL)
		return 0;
#ifdef CONFIG_ANDROID_RAM_CONSOLE_EARLY_INIT
	ram_console_old_log = kmalloc(ram_console_old_log_size, GFP_KERNEL);
	if (ram_console_old_log == NULL) {
		printk(KERN_ERR
		       "ram_console: failed to allocate buffer for old log\n");
		ram_console_old_log_size = 0;
		return 0;
	}
	memcpy(ram_console_old_log,
	       ram_console_old_log_init_buffer, ram_console_old_log_size);
#endif
	entry = proc_create("last_kmsg", S_IFREG | S_IRUGO, NULL, &proc_last_kmsg_operations);
	if (!entry) {
		printk(KERN_ERR "ram_console: failed to create proc entry of last_kmsg\n");
		kfree(ram_console_old_log);
		ram_console_old_log = NULL;
		return 0;
	}

	/* Must save the old time info before starting the update daemon */
	init_time_info();
	start_update_thread();

	return 0;
}

#ifdef CONFIG_ANDROID_RAM_CONSOLE_EARLY_INIT
console_initcall(ram_console_early_init);
#else
postcore_initcall(ram_console_module_init);
#endif
late_initcall(ram_console_late_init);

