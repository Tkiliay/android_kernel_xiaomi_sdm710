#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/swap.h>
#include <linux/gfp.h>

#define BUFF_SIZE           64
static DEFINE_SPINLOCK(mr_lock);
static DEFINE_SPINLOCK(ml_lock);
static char reclaim_buff[BUFF_SIZE];

static int global_reclaim_show(struct seq_file *seq, void *v)
{
	spin_lock(&ml_lock);

	seq_printf(seq, "%s", reclaim_buff);

	spin_unlock(&ml_lock);

	return 0;
}

static int global_reclaim_open(struct inode *inode, struct file *file)
{
	return single_open(file, global_reclaim_show, NULL);
}

static void global_reclaim_record(unsigned long nr_reclaim)
{
	if (!spin_trylock(&mr_lock))
		return;

	snprintf(reclaim_buff, BUFF_SIZE, "reclaim %lu pages", nr_reclaim);

	spin_unlock(&mr_lock);
}

static ssize_t global_reclaim_write(struct file *file, const char __user *userbuf,
		size_t count, loff_t *data)
{
	char buf[BUFF_SIZE] = {0};
	unsigned long reclaim_size = 0;
	unsigned long nr_reclaim = 0;
	int err = 0;

	if (count > BUFF_SIZE)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	err = kstrtoul(buf, 10, &reclaim_size);
	if (err != 0)
		return err;

	if(reclaim_size <= 0)
		return 0;

	nr_reclaim = try_to_free_mem_cgroup_pages(NULL, reclaim_size, GFP_KERNEL, true);
	global_reclaim_record(nr_reclaim);

	return count;
}

static const struct file_operations global_reclaim_ops = {
	.open           = global_reclaim_open,
	.read           = seq_read,
	.write          = global_reclaim_write,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static int __init mem_reclaim_node_init(void)
{
	struct proc_dir_entry *global_reclaim_entry;

	global_reclaim_entry = proc_create("global_reclaim", 0664, NULL, &global_reclaim_ops);
	if (!global_reclaim_entry)
		printk(KERN_ERR "%s: create global_reclaim node failed\n");

	return 0;
}

module_init(mem_reclaim_node_init);
