#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <mach/regs-pmu.h>
#include <linux/io.h>

static void *boot_info_reg = S5P_INFORM4;

static u32 bootinfo_val = 0;
static int charging_mode = 0;
static int display_logo = 0;

static int inform_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	*eof = 1;
	len = snprintf(page, count, "0x%08x\n", bootinfo_val);

	return len;
}

static int init_inform_file(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *pfile;
	pfile = create_proc_entry("bootinfo", 0, parent);
	pfile->read_proc = inform_proc_read;
	pfile->write_proc = NULL;
	return 0;
}

static int __init inform_proc_init(void)
{
	struct proc_dir_entry *inform_dir = proc_mkdir("inform", NULL);
	init_inform_file(inform_dir);
	return 0;
}

late_initcall(inform_proc_init);

static int __init s5p_inform_val_init(void)
{
	bootinfo_val = __raw_readl(boot_info_reg);
	__raw_writel(0x01000000, S5P_INFORM4);

	charging_mode =  (bootinfo_val == 0xA) || (bootinfo_val == 0x6);
	display_logo = !(charging_mode || (bootinfo_val == 0x1));

	return 0;
}
early_initcall(s5p_inform_val_init);
