// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kstrtox.h>
#include <linux/leds.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/string.h>
#include <linux/uaccess.h>

/*
 * 麒麟LED控制驱动
 *
 * 使用misc子系统创建一个最小化的字符设备，演示Linux 5.10中的LED类接口。
 * 用户空间向/dev/kylin_led写入文本指令即可控制目标LED点亮或闪烁，同时驱动
 * 会暂时接管板载电源LED以确保指示灯反馈更加明显。
 */
#define KYLIN_LED_NAME "kylin:orange:skyswordx"
#define POWER_LED_NAME "firefly:blue:power"
#define KYLIN_DEV_NODE "kylin_led"
#define KYLIN_MAX_INPUT 32

/*
 * 文件操作之间共享的全局状态。互斥锁用于串行化LED访问，避免多个用户进程并发写入。
 */
static DEFINE_MUTEX(kylin_led_lock);
static struct led_classdev *kylin_led;
static struct led_classdev *power_led;
static int power_saved_brightness;
static bool power_saved_valid;

extern struct rw_semaphore leds_list_lock;
extern struct list_head leds_list;

/* 上述符号由drivers/leds/led-class.c导出。 */

/*
 * 根据名字解析LED类设备。Linux 5.10在leds_list链表中维护LED，同时也通过
 * 设备模型暴露它们。我们先在leds_list_lock保护下进行只读遍历，若未命中再
 * 回退到class_find_device_by_name()。调用者需要在使用完毕后调用put_device()。
 */
static struct led_classdev *kylin_led_get_by_name(const char *name)
{
	struct led_classdev *led_cdev;
	struct led_classdev *found = NULL;
	struct class *cls = NULL;
	struct device *dev;

	/* 遍历全局LED链表，尝试直接找到匹配的led_classdev。 */
	down_read(&leds_list_lock);
	list_for_each_entry(led_cdev, &leds_list, node) {
		if (!led_cdev->name)
			continue;

		/* 记录第一次遇到的class对象，便于后续退回到设备模型查询。 */
		if (!cls && led_cdev->dev)
			cls = led_cdev->dev->class;

		/* 名字完全匹配时提升引用计数并终止循环。 */
		if (!strcmp(led_cdev->name, name)) {
			get_device(led_cdev->dev);
			found = led_cdev;
			break;
		}
	}
	up_read(&leds_list_lock);

	/* 若列表查找失败且仍有class信息，则退回到设备模型继续查找。 */
	if (found || !cls)
		return found;

	/* class_find_device_by_name会遍历class下的设备，性能影响可以接受。 */
	dev = class_find_device_by_name(cls, name);
	if (!dev)
		return NULL;

	/* 获取设备的led_classdev指针，若失败别忘记归还引用。 */
	found = dev_get_drvdata(dev);
	if (!found) {
		put_device(dev);
		return NULL;
	}

	return found;
}

/*
 * 打印当前注册的LED名称，便于排查集成问题。当DTS中的LED标签与驱动预期不符时
 * 可以从日志中确认正确的名字。
 */
static void kylin_led_dump_names(void)
{
	struct led_classdev *led_cdev;

	/* 在读锁保护下遍历LED链表并输出每一个名称。 */
	down_read(&leds_list_lock);
	list_for_each_entry(led_cdev, &leds_list, node) {
		if (!led_cdev->name)
			continue;

		pr_info("%s: detected LED '%s'\n", KBUILD_MODNAME,
			led_cdev->name);
	}
	up_read(&leds_list_lock);
}

/*
 * 在操作状态LED时确保电源LED熄灭。先通过led_update_brightness()同步硬件状态，
 * 随后保存一次当前亮度，退出时再恢复。
 */
static void kylin_power_led_off_locked(void)
{
	if (!power_led)
		return;

	/* 只在首次关闭时保存当前亮度，避免重复读取。 */
	if (!power_saved_valid) {
		led_update_brightness(power_led);
		power_saved_brightness = power_led->brightness;
		power_saved_valid = true;
	}

	/* 通过LED类接口将电源灯熄灭。 */
	led_set_brightness(power_led, LED_OFF);
}

/*
 * 恢复先前保存的电源LED亮度。利用标记避免在电源LED不存在或尚未熄灭时误写。
 */
static void kylin_power_led_restore_locked(void)
{
	if (!power_led || !power_saved_valid)
		return;

	/* 恢复到最初保存的亮度，再清除标记防止重复恢复。 */
	led_set_brightness(power_led, power_saved_brightness);
	power_saved_valid = false;
}

/*
 * 通过软件在LED_FULL与LED_OFF之间切换以实现固定占空比的闪烁。
 * 简单实现即可适配没有硬件闪烁能力的LED，调用路径已持有kylin_led_lock。
 */
static int kylin_do_blink_locked(unsigned int count)
{
	unsigned int i;

	if (!count)
		return 0;

	/* 固定次数地交替点亮与熄灭，形成肉眼可见的闪烁。 */
	for (i = 0; i < count; i++) {
		led_set_brightness(kylin_led, LED_FULL);
		msleep(200);
		led_set_brightness(kylin_led, LED_OFF);
		msleep(200);
	}

	return 0;
}

/*
 * 处理针对/dev/kylin_led的写操作。misc子系统会生成权限为0220的字符设备，符合仅写接口。
 * 接受文本形式的整数指令，语义与常见sysfs LED类似：
 *   0 -> 关闭LED，并在结束时恢复电源LED
 *   1 -> 将LED设置为LED_FULL亮度，不调整电源LED
 *  >1 -> 按指定次数闪烁（为避免睡眠时间过长，上限100次）
 * 该函数运行在进程上下文中，因此使用msleep()是安全的。
 */
static ssize_t kylin_led_write(struct file *file, const char __user *buf,
			      size_t len, loff_t *ppos)
{
	char kbuf[KYLIN_MAX_INPUT];
	int value, ret;
	bool restore_power = false;

	if (len == 0)
		return 0;

	/* 拒绝超过缓冲区的输入，避免溢出。 */
	if (len >= sizeof(kbuf))
		return -EINVAL;

	/* 从用户空间拷贝指令并确保结尾有NUL字符。 */
	if (copy_from_user(kbuf, buf, len))
		return -EFAULT;

	kbuf[len] = '\0';

	ret = kstrtoint(strim(kbuf), 0, &value);
	if (ret)
		return ret;

	mutex_lock(&kylin_led_lock);

	if (!kylin_led) {
		mutex_unlock(&kylin_led_lock);
		return -ENODEV;
	}

	/* 在操作状态灯之前先熄灭电源灯。 */
	kylin_power_led_off_locked();

	if (value <= 0) {
		led_set_brightness(kylin_led, LED_OFF);
		restore_power = true;
	} else if (value == 1) {
		led_set_brightness(kylin_led, LED_FULL);
	} else {
		/* 限制闪烁次数，同时执行软件闪烁逻辑。 */
		if (value > 100)
			value = 100;
		kylin_do_blink_locked((unsigned int)value);
		restore_power = true;
	}

	/* 根据标记决定是否恢复电源灯。 */
	if (restore_power)
		kylin_power_led_restore_locked();

	mutex_unlock(&kylin_led_lock);

	return len;
}

/* 简要的misc设备文件操作表：仅支持write回调。 */
static const struct file_operations kylin_led_fops = {
	.owner = THIS_MODULE,
	.write = kylin_led_write,
};

/* 通过misc子系统注册设备，避免编写额外的主设备号管理逻辑。 */
static struct miscdevice kylin_led_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = KYLIN_DEV_NODE,
	.fops = &kylin_led_fops,
	.mode = 0220,
};

/* 模块初始化：获取目标LED、注册misc设备并输出状态信息。 */
static int __init kylin_led_init(void)
{
	int ret;

	/* 先尝试根据名字获取目标LED，若失败则打印列表帮助调试。 */
       kylin_led = kylin_led_get_by_name(KYLIN_LED_NAME);
	if (!kylin_led) {
		pr_err("%s: unable to locate LED %s\n", KBUILD_MODNAME,
			KYLIN_LED_NAME);
		kylin_led_dump_names();
		return -ENODEV;
	}

	/* 电源LED不是硬依赖，缺失时仅提示警告。 */
       power_led = kylin_led_get_by_name(POWER_LED_NAME);
	if (!power_led)
		pr_warn("%s: power LED %s not found, continuing\n", KBUILD_MODNAME,
			POWER_LED_NAME);

	/* 注册misc设备并在失败时回滚已获取的引用。 */
	ret = misc_register(&kylin_led_miscdev);
	if (ret) {
		pr_err("%s: failed to register misc device (%d)\n", KBUILD_MODNAME,
			ret);
		put_device(kylin_led->dev);
		if (power_led)
			put_device(power_led->dev);
		return ret;
	}

	/* 注册成功后给出可用提示。 */
		pr_info("%s: ready, use /dev/%s to control %s\n", KBUILD_MODNAME,
			kylin_led_miscdev.name, KYLIN_LED_NAME);

	return 0;
}

/* 模块退出与初始化对称：恢复LED状态并释放引用。 */
static void __exit kylin_led_exit(void)
{
	/* 在互斥锁保护下关闭状态灯并恢复电源灯。 */
	mutex_lock(&kylin_led_lock);
	if (kylin_led)
		led_set_brightness(kylin_led, LED_OFF);
	kylin_power_led_restore_locked();
	mutex_unlock(&kylin_led_lock);

	/* 注销misc设备节点。 */
	misc_deregister(&kylin_led_miscdev);

	/* 释放引用并清空指针，防止悬挂。 */
	if (kylin_led) {
		put_device(kylin_led->dev);
		kylin_led = NULL;
	}

	if (power_led) {
		put_device(power_led->dev);
		power_led = NULL;
	}

	/* 清除亮度缓存标记，以便下次加载模块重新获取。 */
       power_saved_valid = false;
}

module_init(kylin_led_init);
module_exit(kylin_led_exit);

MODULE_AUTHOR("skyswordx");
MODULE_DESCRIPTION("Kylin LED control driver");
MODULE_LICENSE("GPL");
