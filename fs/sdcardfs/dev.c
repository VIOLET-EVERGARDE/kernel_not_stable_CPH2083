/************************************************************
 * Copyright 2017 OPPO Mobile Comm Corp., Ltd.
 * All rights reserved.
 *
 * Description     : record /sdcard/DCIM/Camera and Screenshots, and send uevent
 *
 *
 ** Version: 1
 ** Date created: 2016/01/06
 ** Author: Jiemin.Zhu@AD.Android.SdcardFs
 ** ------------------------------- Revision History: ---------------------------------------
 **        <author>      <data>           <desc>
 **      Jiemin.Zhu    2017/12/12    create this file
 **      Jiemin.Zhu    2018/08/08    modify for adding more protected directorys
 ************************************************************/
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/security.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>

#include "sdcardfs.h"
#include "dellog.h"

static DEFINE_MUTEX(dcim_mutex);
static struct kobject *dcim_kobject;
#define MAX_EVENT_PARAM    5
#define MAX_SKIPD_UID_NUMBER	20
#define MAX_RENAME_EVENT_PARAM	6

#define AID_APP_START 10000 /* first app user */

static int skipd_enable = 1;
module_param_named(skipd_enable, skipd_enable, int, S_IRUGO | S_IWUSR);

static int extra_enable = 1;
module_param_named(extra_enable, extra_enable, int, S_IRUGO | S_IWUSR);

static struct kmem_cache *sdcardfs_path_cachep;

struct uid_list_info {
	struct list_head list;
	uid_t uid;
};
struct list_head uid_head;
static struct proc_dir_entry *sdcardfs_procdir;

int dcim_delete_skip(void)
{
	struct uid_list_info *list;

	if (!skipd_enable)
		return 0;

	//system app, do skip
	if (current_uid().val < AID_APP_START)
		return 0;
	list_for_each_entry(list, &uid_head, list) {
		if (list->uid == current_uid().val) {
			return 0;
		}
	}

	return 1;
}

int dcim_delete_uevent(struct dentry *dentry)
{
	char *buf, *path;
	char *denied_param[MAX_EVENT_PARAM] = { "DELETE_STAT=DCIM", NULL };
	int i;
	struct sdcardfs_inode_info *info = SDCARDFS_I(d_inode(dentry));

	if (info->data->dcim_uid == from_kuid(&init_user_ns, current_uid())) {
		printk("sdcardfs: app[uid %u] delete it's own picture %s, do unlink directly\n",
			info->data->dcim_uid, dentry->d_name.name);
		return -1;
	}

	mutex_lock(&dcim_mutex);

	buf = kmem_cache_alloc(sdcardfs_path_cachep, GFP_KERNEL);
	if (buf == NULL) {
		mutex_unlock(&dcim_mutex);
		return -1;
	}
	path = dentry_path_raw(dentry, buf, PATH_MAX);
	if (IS_ERR(path)) {
		kmem_cache_free(sdcardfs_path_cachep, buf);
		mutex_unlock(&dcim_mutex);
		return -1;
	}

	for(i = 1; i < MAX_EVENT_PARAM - 1; i++) {
		denied_param[i] = kmem_cache_alloc(sdcardfs_path_cachep, GFP_KERNEL | __GFP_ZERO);
		if (!denied_param[i]) {
			goto free_memory;
		}
	}

	sprintf(denied_param[1], "UID=%u", from_kuid(&init_user_ns, current_uid()));
	sprintf(denied_param[2], "PATH=%s", path);
	sprintf(denied_param[3], "PID=%u", current->pid);
	//printk("sdcardfs: send delete dcim uevent %s %s\n", denied_param[1], denied_param[2]);

	if (dcim_kobject) {
		kobject_uevent_env(dcim_kobject, KOBJ_CHANGE, denied_param);
	}

free_memory:
    for(i--; i > 0; i--)
        kmem_cache_free(sdcardfs_path_cachep, denied_param[i]);

	kmem_cache_free(sdcardfs_path_cachep, buf);

	mutex_unlock(&dcim_mutex);

	return 0;
}

void extra_delete_uevent(struct dentry *dentry)
{
	char *buf, *path;
	char *denied_param[MAX_EVENT_PARAM] = { "DELETE_STAT=EXTRA", NULL };
	int i;

	if (!extra_enable) {
		return;
	}

	mutex_lock(&dcim_mutex);

	buf = kmem_cache_alloc(sdcardfs_path_cachep, GFP_KERNEL);
	if (buf == NULL) {
		mutex_unlock(&dcim_mutex);
		return ;
	}
	path = dentry_path_raw(dentry, buf, PATH_MAX);
	if (IS_ERR(path)) {
		kmem_cache_free(sdcardfs_path_cachep, buf);
		mutex_unlock(&dcim_mutex);
		return ;
	}

	for(i = 1; i < MAX_EVENT_PARAM - 1; i++) {
		denied_param[i] = kmem_cache_alloc(sdcardfs_path_cachep, GFP_KERNEL | __GFP_ZERO);
		if (!denied_param[i]) {
			goto free_memory;
		}
	}

	sprintf(denied_param[1], "UID=%u", from_kuid(&init_user_ns, current_uid()));
	sprintf(denied_param[2], "PATH=%s", path);
	sprintf(denied_param[3], "PID=%u", current->pid);
	//printk("sdcardfs: send delete extra uevent %s %s\n", denied_param[1], denied_param[2]);

	if (dcim_kobject) {
		kobject_uevent_env(dcim_kobject, KOBJ_CHANGE, denied_param);
	}

free_memory:
	for(i--; i > 0; i--) {
		kmem_cache_free(sdcardfs_path_cachep, denied_param[i]);
	}

	kmem_cache_free(sdcardfs_path_cachep, buf);

	mutex_unlock(&dcim_mutex);
}

static int sdcardfs_rename_uevent(char *old_path, char *new_path)
{
	char *denied_param[MAX_RENAME_EVENT_PARAM] = { "RENAME_STAT", NULL };
	int i;

	for(i = 1; i < MAX_RENAME_EVENT_PARAM - 1; i++) {
		denied_param[i] = kmem_cache_alloc(sdcardfs_path_cachep, GFP_KERNEL | __GFP_ZERO);
		if (!denied_param[i]) {
			goto free_memory;
		}
	}

	sprintf(denied_param[1], "UID=%u", from_kuid(&init_user_ns, current_uid()));
	sprintf(denied_param[2], "OLD_PATH=%s", old_path);
	sprintf(denied_param[3], "NEW_PATH=%s", new_path);
	sprintf(denied_param[4], "PID=%u", current->pid);

	if (dcim_kobject) {
		//printk("sdcardfs: send rename uevent %s %s\n", denied_param[2], denied_param[3]);
		kobject_uevent_env(dcim_kobject, KOBJ_CHANGE, denied_param);
	}

free_memory:
    for(i--; i > 0; i--)
        kmem_cache_free(sdcardfs_path_cachep, denied_param[i]);

	return 0;
}

void sdcardfs_rename_record(struct dentry *old_dentry, struct dentry *new_dentry)
{
	struct sdcardfs_inode_info *info = SDCARDFS_I(d_inode(old_dentry));
	char *old_buf, *old_path;
	char *new_buf, *new_path;

	if (!info->data->under_extra && !info->data->under_dcim)
		return;

	if (!extra_enable) {
		return;
	}

	old_buf = kmem_cache_alloc(sdcardfs_path_cachep, GFP_KERNEL);
	if (!old_buf) {
		return;
	}
	new_buf = kmem_cache_alloc(sdcardfs_path_cachep, GFP_KERNEL);
	if (!new_buf) {
		kmem_cache_free(sdcardfs_path_cachep, old_buf);
		return;
	}

	old_path = dentry_path_raw(old_dentry, old_buf, PATH_MAX);
	if (IS_ERR(old_path)) {
		kmem_cache_free(sdcardfs_path_cachep, old_buf);
		kmem_cache_free(sdcardfs_path_cachep, new_buf);
		return;
	}
	new_path = dentry_path_raw(new_dentry, new_buf, PATH_MAX);
	if (IS_ERR(new_path)) {
		kmem_cache_free(sdcardfs_path_cachep, old_buf);
		kmem_cache_free(sdcardfs_path_cachep, new_buf);
		return;
	}

	mutex_lock(&dcim_mutex);
	sdcardfs_rename_uevent(old_path, new_path);
	mutex_unlock(&dcim_mutex);

	DEL_LOG("[%u] rename from %s to %s", (unsigned int) current_uid().val,
			old_path, new_path);

	kmem_cache_free(sdcardfs_path_cachep, old_buf);
	kmem_cache_free(sdcardfs_path_cachep, new_buf);
}

static int proc_sdcardfs_skip_show(struct seq_file *m, void *v)
{
	struct uid_list_info *list;

	seq_printf(m, "skipd uid is: \n");
	list_for_each_entry(list, &uid_head, list) {
		seq_printf(m, "%u\n", list->uid);
	}

	return 0;
}

static int proc_sdcardfs_skip_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_sdcardfs_skip_show, NULL);
}

static ssize_t proc_sdcardfs_skip_write(struct file *file, const char __user *buffer,
				   size_t count, loff_t *offp)
{
	int uid, ret;
	char *tmp;
	struct uid_list_info *list;

	mutex_lock(&dcim_mutex);

	tmp = kzalloc((count + 1), GFP_KERNEL);
	if (!tmp) {
		mutex_unlock(&dcim_mutex);
		return -ENOMEM;
	}

	if(copy_from_user(tmp, buffer, count)) {
		kfree(tmp);
		mutex_unlock(&dcim_mutex);
		return -EINVAL;
	}

	ret = kstrtoint(tmp, 10, &uid);
	if (ret < 0) {
		kfree(tmp);
		mutex_unlock(&dcim_mutex);
		return ret;
	}

	if (uid < AID_APP_START) {
		kfree(tmp);
		mutex_unlock(&dcim_mutex);
		return -EINVAL;
	}

	list = kzalloc(sizeof(*list), GFP_KERNEL);
	if (!list) {
		kfree(tmp);
		mutex_unlock(&dcim_mutex);
		return -ENOMEM;
	}
	list->uid = uid;
	list_add_tail(&list->list, &uid_head);

	kfree(tmp);
	mutex_unlock(&dcim_mutex);

	return count;
}

static const struct file_operations proc_sdcardfs_skip_fops = {
	.open		= proc_sdcardfs_skip_open,
	.read		= seq_read,
	.write		= proc_sdcardfs_skip_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init dcim_event_init(void)
{
	struct proc_dir_entry *skipd_entry;

	INIT_LIST_HEAD(&uid_head);
	sdcardfs_path_cachep = kmem_cache_create("sdcardfs_path", PATH_MAX, 0, 0, NULL);
	if (!sdcardfs_path_cachep) {
		printk("sdcardfs: sdcardfs_path cache create failed\n");
		return -ENOMEM;
	}

	dcim_kobject = kset_find_obj(module_kset, "sdcardfs");
	if (dcim_kobject == NULL) {
		printk("sdcardfs: sdcardfs uevent kobject is null");
		return -1;
	}
	sdcardfs_procdir = proc_mkdir("fs/sdcardfs", NULL);
	skipd_entry = proc_create_data("skipd_delete", 664, sdcardfs_procdir,
				&proc_sdcardfs_skip_fops, NULL);

	return 0;
}

static void __exit dcim_event_exit(void)
{
}

module_init(dcim_event_init);
module_exit(dcim_event_exit);

MODULE_AUTHOR("jiemin.zhu@oppo.com");
MODULE_LICENSE("GPL");
