#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xe8f81d87, "nonseekable_open" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x83d9de5c, "cdev_init" },
	{ 0xb2730d06, "cdev_add" },
	{ 0x6c66ac6d, "class_create" },
	{ 0x434998eb, "device_create" },
	{ 0xf57a62f1, "class_destroy" },
	{ 0x92997ed8, "_printk" },
	{ 0xf6d47c40, "cdev_del" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x89940875, "mutex_lock_interruptible" },
	{ 0x619cb7dd, "simple_read_from_buffer" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x24021414, "device_destroy" },
	{ 0xdcb764ad, "memset" },
	{ 0x12a4e128, "__arch_copy_from_user" },
	{ 0x4829a47e, "memcpy" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x150028cc, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "2BA48D4D47752011BA681D5");
