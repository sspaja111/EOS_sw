#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
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

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif


static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x37a0cba, "kfree" },
	{ 0x619cb7dd, "simple_read_from_buffer" },
	{ 0xfb578fc5, "memset" },
	{ 0x40a9b349, "vzalloc" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0xeea3c1d8, "__class_create" },
	{ 0x6c333d48, "device_create" },
	{ 0x4c260c62, "cdev_alloc" },
	{ 0xff0d65c6, "cdev_add" },
	{ 0x7affd727, "kmalloc_caches" },
	{ 0x3d650a07, "kmalloc_trace" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0x96848186, "scnprintf" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
	{ 0xa648e561, "__ubsan_handle_shift_out_of_bounds" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0xbcab6ee6, "sscanf" },
	{ 0xc07351b3, "__SCT__cond_resched" },
	{ 0xa19b956, "__stack_chk_fail" },
	{ 0x43d34239, "cdev_del" },
	{ 0x3e2bc4cf, "device_destroy" },
	{ 0x6a2dd7c, "class_destroy" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x999e8297, "vfree" },
	{ 0x122c3a7e, "_printk" },
	{ 0x453e7dc, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "1FFDC3BC17311232418D9D2");
