#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <plat/cpu.h>

static int uboot_version_proc_show(struct seq_file *m, void *v)
{
	if (soc_is_exynos4210())
		seq_printf(m, "%s\n", saved_uboot_version);
	else
		seq_printf(m, "%s\n", bootinfo.uboot_release_version);

	return 0;
}

static int uboot_version_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, uboot_version_proc_show, NULL);
}

static const struct file_operations uboot_version_proc_fops = {
	.open		= uboot_version_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_uboot_version_init(void)
{
	proc_create("uboot_version", 0, NULL, &uboot_version_proc_fops);
	return 0;
}
module_init(proc_uboot_version_init);
