/*
 * Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/debugfs.h>

#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of.h>

#if defined(CONFIG_DEBUG_FS)

static void print_device_tree_nodename(struct device_node *node, int depth) {
    int i = 0;
    struct device_node *child;
    char indent[255] = "";

    for(i = 0; i < depth * 3; i++) {
        indent[i] = ' ';
    }
    indent[i] = '\0';
    ++depth;

    for_each_child_of_node(node, child) 
	{
        pr_info("%s name = %s (%s)\n", indent, child->name, child->full_name);
        print_device_tree_nodename(child, depth);
    }    
}

static void print_child_node(struct device_node *node, int depth) {
    int i = 0;
    struct device_node *child;
    struct property    *properties;
    char indent[255] = "";

    for(i = 0; i < depth * 3; i++) {
        indent[i] = ' ';
    }
    indent[i] = '\0';
    ++depth;

    for_each_child_of_node(node, child) 
	{
        pr_info("%s{ name = %s (%s)\n", indent, child->name, child->full_name);
        pr_info("%s  type = %s\n", indent, child->type);

        for (properties = child->properties; properties != NULL; properties = properties->next) 
		{
            pr_info("%s  %s (%d)\n", indent, properties->name, properties->length);			
        }

        print_child_node(child, depth);
        pr_info("%s}\n", indent);
    }
}
static void print_device_tree_node(struct device_node *node, int depth) {
    struct property    *properties;
  
	pr_info("{ name = %s (%s)\n", node->name, node->full_name);
	pr_info("  type = %s\n", node->type);

	for (properties = node->properties; properties != NULL; properties = properties->next) 
	{
		pr_info("  %s (%d)\n", properties->name, properties->length);			
    }
	if(node->child != NULL)		
		print_child_node(node, 1);
	pr_info("}\n");    
}

static ssize_t dump_all_tree_name(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{	
	print_device_tree_nodename(of_find_node_by_path("/"), 0);
	return 0;
}

static ssize_t find_pro_by_name_write(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct device_node *test_node = NULL;
	char node_name[125];
	
	memcpy(&node_name, buf, count);
	node_name[count-1] = 0;

	test_node = of_find_node_by_name(of_find_node_by_path("/"), node_name);
	if(test_node != NULL)   
		print_device_tree_node(test_node, 0);
	else
		pr_info("not find\n");
		
	return count;
}

static ssize_t find_pro_by_path_write(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct device_node *test_node = NULL;
	char node_name[125];

	memcpy(&node_name, buf, count);
	node_name[count-1] = 0;
	
	test_node = of_find_node_by_path(node_name);
	if(test_node != NULL)
		print_device_tree_node(test_node, 0);
	else
		pr_info("not find\n");

	return count;
}

static ssize_t pp_of_u32_write(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct device_node *test_node = NULL;
	char node_name[125];
	char *cp;
	int size=0;
	int ret = 0;
	u32 tmp;
	
	memcpy(&node_name, buf, count);
	node_name[count-1] = 0;
	cp = strchr(node_name, ':');
	size = strlen(cp);
	node_name[count-size-1]=0;
	cp[size]=0;

	test_node = of_find_node_by_path(node_name);
	ret = of_property_read_u32(test_node, &(cp[1]), &tmp);
	if (ret)
		pr_info("Unable to read \n");  
	else
		pr_info("value = 0x%x \n", tmp); 

	return count;
}

static ssize_t pp_of_string_write(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct device_node *test_node = NULL;
	char node_name[125];
	char *cp;
	int size=0;
	const char *data;
	
	memcpy(&node_name, buf, count);
	node_name[count-1] = 0;
	cp = strchr(node_name, ':');
	size = strlen(cp);
	node_name[count-size-1]=0;
	cp[size]=0;

	test_node = of_find_node_by_path(node_name);
	data = of_get_property(test_node, &(cp[1]), NULL);

	pr_info("value = %s \n", data); 

	return count;
}
static ssize_t pp_of_gpio_write(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct device_node *test_node = NULL;
	char node_name[125];
	char *cp;
	int size=0;
	int data;
	
	memcpy(&node_name, buf, count);
	node_name[count-1] = 0;
	cp = strchr(node_name, ':');
	size = strlen(cp);
	node_name[count-size-1]=0;
	cp[size]=0;

	test_node = of_find_node_by_path(node_name);
	data = of_get_named_gpio(test_node, &(cp[1]), 0);

	pr_info("value = %d \n", data); 

	return count;
}

static ssize_t pp_of_array_write(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct device_node *test_node = NULL;
	char node_name[125];
	char *cp;
	int i=0, size=0, len;
	const char *data;
	
	memcpy(&node_name, buf, count);
	node_name[count-1] = 0;
	cp = strchr(node_name, ':');
	size = strlen(cp);
	node_name[count-size-1]=0;
	cp[size]=0;

	test_node = of_find_node_by_path(node_name);
	data = of_get_property(test_node, &(cp[1]), &len);
	if(data !=NULL)
	{
		pr_info("value =");
		for (i = 0; i < len; i++)
		{
			pr_info(" 0x%x",data[i]);
			if(i!=0 && i%12==0)
				pr_info("\n");
		}
		pr_info("\n");
	}
	return count;
}

static ssize_t pp_of_point_write(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct device_node *test_node = NULL;
	struct device_node *find_np = NULL;
	char node_name[125];
	char *cp;
	int size=0;

	memcpy(&node_name, buf, count);
	node_name[count-1] = 0;
	cp = strchr(node_name, ':');
	size = strlen(cp);
	node_name[count-size-1]=0;
	cp[size]=0;

	test_node = of_find_node_by_path(node_name);
	find_np = of_parse_phandle(test_node, &(cp[1]), 0);
	if (find_np != NULL) {
		print_device_tree_node(find_np, 0);
	}

	return count;
}


const struct file_operations dump_all_tree_ops = {
	.read = dump_all_tree_name,
};
const struct file_operations find_pro_by_name_ops = {
	.write = find_pro_by_name_write,
};
const struct file_operations find_pro_by_path_ops = {
	.write = find_pro_by_path_write,
};
const struct file_operations pp_of_u32_ops = {
	.write = pp_of_u32_write,
};
const struct file_operations pp_of_string_ops = {
	.write = pp_of_string_write,
};
const struct file_operations pp_of_gpio_ops = {
	.write = pp_of_gpio_write,
};
const struct file_operations pp_of_array_ops = {
	.write = pp_of_array_write,
};
const struct file_operations pp_of_point_ops = {
	.write = pp_of_point_write,
};

struct dentry *dt_ctrl_dent;
struct dentry *dt_ctrl_dfile;
static void device_tree_debugfs_init(void)
{
	dt_ctrl_dent = debugfs_create_dir("device_tree_info", 0);
	if (IS_ERR(dt_ctrl_dent))
		return;

	dt_ctrl_dfile = debugfs_create_file("dump_all_tree", S_IRUSR | S_IWUSR, dt_ctrl_dent, 0,
			&dump_all_tree_ops);
	if (!dt_ctrl_dfile || IS_ERR(dt_ctrl_dfile))
		debugfs_remove(dt_ctrl_dent);

	dt_ctrl_dfile = debugfs_create_file("find_pp_by_name", S_IRUSR | S_IWUSR, dt_ctrl_dent, 0,
			&find_pro_by_name_ops);
	if (!dt_ctrl_dfile || IS_ERR(dt_ctrl_dfile))
		debugfs_remove(dt_ctrl_dent);

	dt_ctrl_dfile = debugfs_create_file("find_pp_by_path", S_IRUSR | S_IWUSR, dt_ctrl_dent, 0,
			&find_pro_by_path_ops);
	if (!dt_ctrl_dfile || IS_ERR(dt_ctrl_dfile))
		debugfs_remove(dt_ctrl_dent);
	
	dt_ctrl_dfile = debugfs_create_file("pp_of_u32", S_IRUSR | S_IWUSR, dt_ctrl_dent, 0,
			&pp_of_u32_ops);
	if (!dt_ctrl_dfile || IS_ERR(dt_ctrl_dfile))
		debugfs_remove(dt_ctrl_dent);
	
	dt_ctrl_dfile = debugfs_create_file("pp_of_string", S_IRUSR | S_IWUSR, dt_ctrl_dent, 0,
			&pp_of_string_ops);
	if (!dt_ctrl_dfile || IS_ERR(dt_ctrl_dfile))
		debugfs_remove(dt_ctrl_dent);
	
	dt_ctrl_dfile = debugfs_create_file("pp_of_gpio", S_IRUSR | S_IWUSR, dt_ctrl_dent, 0,
			&pp_of_gpio_ops);
	if (!dt_ctrl_dfile || IS_ERR(dt_ctrl_dfile))
		debugfs_remove(dt_ctrl_dent);
	
	dt_ctrl_dfile = debugfs_create_file("pp_of_array", S_IRUSR | S_IWUSR, dt_ctrl_dent, 0,
			&pp_of_array_ops);
	if (!dt_ctrl_dfile || IS_ERR(dt_ctrl_dfile))
		debugfs_remove(dt_ctrl_dent);
	
	dt_ctrl_dfile = debugfs_create_file("pp_of_point", S_IRUSR | S_IWUSR, dt_ctrl_dent, 0,
			&pp_of_point_ops);
	if (!dt_ctrl_dfile || IS_ERR(dt_ctrl_dfile))
		debugfs_remove(dt_ctrl_dent);
}

static void device_tree_debugfs_exit(void)
{
	debugfs_remove(dt_ctrl_dfile);
	debugfs_remove(dt_ctrl_dent);
}

#else
static void device_tree_debugfs_init(void) { }
static void device_tree_debugfs_exit(void) { }
#endif

static int __init device_tree_info_init(void)
{
	device_tree_debugfs_init();

	return 0;
}
module_init(device_tree_info_init);

static void __exit device_tree_info_exit(void)
{
	device_tree_debugfs_exit();
}
module_exit(device_tree_info_exit);
MODULE_DESCRIPTION("smd control driver");
MODULE_LICENSE("GPL v2");
