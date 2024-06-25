#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xf8cdd757, "module_layout" },
	{ 0xd7617cd1, "usb_deregister" },
	{ 0x129ac5e6, "usb_register_driver" },
	{ 0x9202ba1c, "current_task" },
	{ 0x362ef408, "_copy_from_user" },
	{ 0x432f28e5, "usb_control_msg" },
	{ 0xa6093a32, "mutex_unlock" },
	{ 0x41aed6e7, "mutex_lock" },
	{ 0xa202a8e5, "kmalloc_order_trace" },
	{ 0xd090ac19, "usb_register_dev" },
	{ 0xf86c8d03, "kmem_cache_alloc_trace" },
	{ 0xf4b9b193, "kmalloc_caches" },
	{ 0x3926a2c3, "usb_deregister_dev" },
	{ 0x37a0cba, "kfree" },
	{ 0xdb7305a1, "__stack_chk_fail" },
	{ 0x27e1a049, "printk" },
	{ 0xa1c76e0a, "_cond_resched" },
	{ 0xc1fbc747, "usb_clear_halt" },
	{ 0xb44ad4b3, "_copy_to_user" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x198dc2be, "usb_bulk_msg" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x9a76f11f, "__mutex_init" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0xcf2a6966, "up" },
	{ 0x6626afca, "down" },
	{ 0xbdfb6dbb, "__fentry__" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("usb:v0547p1002d*dc*dsc*dp*ic*isc*ip*in*");
MODULE_ALIAS("usb:v21E1p0000d*dc*dsc*dp*ic*isc*ip*in*");
MODULE_ALIAS("usb:v21E1p0001d*dc*dsc*dp*ic*isc*ip*in*");
MODULE_ALIAS("usb:v21E1p0011d*dc*dsc*dp*ic*isc*ip*in*");
MODULE_ALIAS("usb:v21E1p0019d*dc*dsc*dp*ic*isc*ip*in*");
MODULE_ALIAS("usb:v21E1p0015d*dc*dsc*dp*ic*isc*ip*in*");

MODULE_INFO(srcversion, "98E0D1917D3AFB6814A11DA");
MODULE_INFO(rhelversion, "8.7");
