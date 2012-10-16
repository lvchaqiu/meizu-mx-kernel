#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <linux/mx_akm8975.h>
#include <linux/earlysuspend.h>
 
#define AKM8975_DEBUG		0
#define AKM8975_DEBUG_MSG	1
#define AKM8975_DEBUG_FUNC	0
#define AKM8975_DEBUG_DATA	0
#define MAX_FAILURE_COUNT	3
#define I2C_RETRY_DELAY		2
#define I2C_RETRIES		10
#define AKM8975_DEFAULT_DELAY	100

#if AKM8975_DEBUG_MSG
#define AKMDBG(format, ...)	printk(KERN_INFO "AKM8975 " format "\n", ## __VA_ARGS__)
#else
#define AKMDBG(format, ...)
#endif

#if AKM8975_DEBUG_FUNC
#define AKMFUNC(func) printk(KERN_INFO "AKM8975 " func " is called\n")
#else
#define AKMFUNC(func)
#endif

static struct i2c_client *this_client;

struct akm8975_data {
	struct input_dev *input_dev;
	struct early_suspend akm_early_suspend;
	struct mutex akm_aot_ioctl_lock;
	struct mutex akmd_ioctl_lock;
};

/* Addresses to scan -- protected by sense_data_mutex */
static struct mutex sense_data_mutex;
static DECLARE_WAIT_QUEUE_HEAD(open_wq);

static atomic_t open_flag;
static atomic_t reserve_open_flag;

static atomic_t m_flag;
static atomic_t a_flag;
static atomic_t mv_flag;

static short akmd_delay = AKM8975_DEFAULT_DELAY;

static atomic_t suspend_flag = ATOMIC_INIT(0);

static int AKI2C_RxData(char *rxData, int length)
{
	uint8_t loop_i;
	struct i2c_msg msgs[] = {
		{
			.addr = this_client->addr,
			.flags = 0,
			.len = 1,
			.buf = rxData,
		},
		{
			.addr = this_client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = rxData,
		},
	};
#if AKM8975_DEBUG_DATA
	int i;
	char addr = rxData[0];
#endif
#ifdef AKM8975_DEBUG
	/* Caller should check parameter validity.*/
	if ((rxData == NULL) || (length < 1)) {
		return -EINVAL;
	}
#endif
	for (loop_i = 0; loop_i < I2C_RETRIES; loop_i++) {
		if (i2c_transfer(this_client->adapter, msgs, 2) > 0) {
			break;
		}
		pr_err("%s: i2c_transfer fail, retry %d\n", __func__, loop_i + 1);
		msleep_interruptible(I2C_RETRY_DELAY);
	}
	
	if (loop_i >= I2C_RETRIES) {
		pr_err("%s retry over %d\n", __func__, I2C_RETRIES);
		return -EIO;
	}
#if AKM8975_DEBUG_DATA
	printk(KERN_INFO "RxData: len=%02x, addr=%02x\n  data=", length, addr);
	for (i = 0; i < length; i++) {
		printk(KERN_INFO " %02x", rxData[i]);
	}
    printk(KERN_INFO "\n");
#endif
	return 0;
}

static int AKI2C_TxData(char *txData, int length)
{
	uint8_t loop_i;
	struct i2c_msg msg[] = {
		{
			.addr = this_client->addr,
			.flags = 0,
			.len = length,
			.buf = txData,
		},
	};
#if AKM8975_DEBUG_DATA
	int i;
#endif
#ifdef AKM8975_DEBUG
	/* Caller should check parameter validity.*/
	if ((txData == NULL) || (length < 2)) {
		return -EINVAL;
	}
#endif	
	for (loop_i = 0; loop_i < I2C_RETRIES; loop_i++) {
		if (i2c_transfer(this_client->adapter, msg, 1) > 0) {
			break;
		}
		pr_err("%s: i2c_transfer fail, retry %d\n", __func__, loop_i);
		msleep_interruptible(I2C_RETRY_DELAY);
	}
	
	if (loop_i >= I2C_RETRIES) {
		pr_err("%s retry over %d\n", __func__, I2C_RETRIES);
		return -EIO;
	}
#if AKM8975_DEBUG_DATA
	printk(KERN_INFO "TxData: len=%02x, addr=%02x\n  data=", length, txData[0]);
	for (i = 0; i < (length-1); i++) {
		printk(KERN_INFO " %02x", txData[i + 1]);
	}
	printk(KERN_INFO "\n");
#endif
	return 0;
}

static int AKECS_SetMode_SngMeasure(void)
{
	char buffer[2];

	/* Set measure mode */
	buffer[0] = AK8975_REG_CNTL;
	buffer[1] = AK8975_MODE_SNG_MEASURE;
	
	/* Set data */
	return AKI2C_TxData(buffer, 2);
}

static int AKECS_SetMode_SelfTest(void)
{
	char buffer[2];
	
	/* Set measure mode */
	buffer[0] = AK8975_REG_CNTL;
	buffer[1] = AK8975_MODE_SELF_TEST;
	/* Set data */
	return AKI2C_TxData(buffer, 2);
}

static int AKECS_SetMode_FUSEAccess(void)
{
	char buffer[2];
	
	/* Set measure mode */
	buffer[0] = AK8975_REG_CNTL;
	buffer[1] = AK8975_MODE_FUSE_ACCESS;
	/* Set data */
	return AKI2C_TxData(buffer, 2);
}

static int AKECS_SetMode_PowerDown(void)
{
	char buffer[2];
	
	/* Set powerdown mode */
	buffer[0] = AK8975_REG_CNTL;
	buffer[1] = AK8975_MODE_POWERDOWN;
	/* Set data */
	return AKI2C_TxData(buffer, 2);
}

static int AKECS_SetMode(char mode)
{
	int ret;
	
	switch (mode) {
		case AK8975_MODE_SNG_MEASURE:
			ret = AKECS_SetMode_SngMeasure();
			break;
		case AK8975_MODE_SELF_TEST:
			ret = AKECS_SetMode_SelfTest();
			break;
		case AK8975_MODE_FUSE_ACCESS:
			ret = AKECS_SetMode_FUSEAccess();
			break;
		case AK8975_MODE_POWERDOWN:
			ret = AKECS_SetMode_PowerDown();
			/* wait at least 100us after changing mode */
			udelay(100);
			break;
		default:
			AKMDBG("%s: Unknown mode(%d)", __func__, mode);
			return -EINVAL;
	}

	return ret;
}

static int AKECS_CheckDevice(void)
{
	char buffer[2];
	int ret;
	
	/* Set measure mode */
	buffer[0] = AK8975_REG_WIA;
	
	/* Read data */
	ret = AKI2C_RxData(buffer, 1);
	if (ret < 0) {
		return ret;
	}
	/* Check read data */
	if (buffer[0] != 0x48) {
		return -ENXIO;
	}
	
	return 0;
}


static void AKECS_SetYPR(short *rbuf)
{
	struct akm8975_data *data = i2c_get_clientdata(this_client);
#if AKM8975_DEBUG_DATA
	printk(KERN_INFO "AKM8975 %s:\n", __func__);
	printk(KERN_INFO "  yaw =%6d, pitch =%6d, roll =%6d\n",
		   rbuf[0], rbuf[1], rbuf[2]);
	printk(KERN_INFO "  tmp =%6d, m_stat =%6d, g_stat =%6d\n",
		   rbuf[3], rbuf[4], rbuf[5]);
	printk(KERN_INFO "  Acceleration[LSB]: %6d,%6d,%6d\n",
	       rbuf[6], rbuf[7], rbuf[8]);
	printk(KERN_INFO "  Geomagnetism[LSB]: %6d,%6d,%6d\n",
	       rbuf[9], rbuf[10], rbuf[11]);
#endif
	/* Report magnetic sensor information */
	if (atomic_read(&m_flag)) {
		input_report_abs(data->input_dev, ABS_RX, rbuf[0]);
		input_report_abs(data->input_dev, ABS_RY, rbuf[1]);
		input_report_abs(data->input_dev, ABS_RZ, rbuf[2]);
		input_report_abs(data->input_dev, ABS_RUDDER, rbuf[4]);
	}
	
	/* Report acceleration sensor information */
	if (atomic_read(&a_flag)) {
		input_report_abs(data->input_dev, ABS_X, rbuf[6]);
		input_report_abs(data->input_dev, ABS_Y, rbuf[7]);
		input_report_abs(data->input_dev, ABS_Z, rbuf[8]);
		input_report_abs(data->input_dev, ABS_WHEEL, rbuf[5]);
	}
	
	/* Report magnetic vector information */
	if (atomic_read(&mv_flag)) {
		input_report_abs(data->input_dev, ABS_HAT0X, rbuf[9]);
		input_report_abs(data->input_dev, ABS_HAT0Y, rbuf[10]);
		input_report_abs(data->input_dev, ABS_BRAKE, rbuf[11]);
	}
	
	input_sync(data->input_dev);
}

static int AKECS_GetOpenStatus(void)
{
	wait_event_interruptible(open_wq, (atomic_read(&open_flag) != 0));
	return atomic_read(&open_flag);
}

static int AKECS_GetCloseStatus(void)
{
	wait_event_interruptible(open_wq, (atomic_read(&open_flag) <= 0));
	return atomic_read(&open_flag);
}

static inline int AKECS_GetSuspendStatus(void)
{
	return atomic_read(&suspend_flag);
}

static void AKECS_CloseDone(void)
{
}

#define AK8975_MEASURE_TIMEOUT 100

static inline int AKECS_GetData(unsigned char data[SENSOR_DATA_SIZE])
{
	int res, i;

	/* Single Measure Mode is set from user-space */
	for (i = AK8975_MEASURE_TIMEOUT; i != 0; i--) {
		/* Get Data Ready Status */
		res = AKI2C_RxData(data, 1);
		if (res) {
			pr_err("%s: AKI2C_RxData 1 error.\n", __func__);
			return res;
		}
		/* If data ready, read them */
		if (data[0] & 0x01) {
			/* Data ready Status, X(H, L), Y(H, L), Z(H, L) data, DERR Status*/
			data[0] = AK8975_REG_ST1;
			res = AKI2C_RxData(data, SENSOR_DATA_SIZE);
			if (res) {
				pr_err("%s: AKI2C_RxData 2 error.\n", __func__);
				return res;
			}
			break;
		}
		msleep_interruptible(I2C_RETRY_DELAY);
	}
	if (i == 0) {
		pr_err("%s: DRDY timeout.\n", __func__);
		return -EIO;
	}

	return 0;
}

/***** akmd functions ********************************************/
static int akmd_open(struct inode *inode, struct file *file)
{
	AKMFUNC("akmd_open");
	return nonseekable_open(inode, file);
}

static int akmd_release(struct inode *inode, struct file *file)
{
	AKMFUNC("akmd_release");
	AKECS_CloseDone();
	return 0;
}

static long akmd_ioctl_int(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	
	/* NOTE: In this function the size of "char" should be 1-byte. */
	char sData[SENSOR_DATA_SIZE];/* for GETDATA */
	char rwbuf[RWBUF_SIZE];		/* for READ/WRITE */
	char mode;					/* for SET_MODE*/
	short value[12];			/* for SET_YPR */
	short delay;				/* for GET_DELAY */
	int status;					/* for OPEN/CLOSE_STATUS */
	int ret = -1;				/* Return value. */
	/*AKMDBG("%s (0x%08X).", __func__, cmd);*/
	
	switch (cmd) {
		case ECS_IOCTL_WRITE:
		case ECS_IOCTL_READ:
			if (argp == NULL) {
				AKMDBG("invalid argument.");
				return -EINVAL;
			}
			if (copy_from_user(&rwbuf, argp, sizeof(rwbuf))) {
				AKMDBG("copy_from_user failed.");
				return -EFAULT;
			}
			break;
		case ECS_IOCTL_SET_MODE:
			if (argp == NULL) {
				AKMDBG("invalid argument.");
				return -EINVAL;
			}
			if (copy_from_user(&mode, argp, sizeof(mode))) {
				AKMDBG("copy_from_user failed.");
				return -EFAULT;
			}
			break;
		case ECS_IOCTL_SET_YPR:
			if (argp == NULL) {
				AKMDBG("invalid argument.");
				return -EINVAL;
			}
			if (copy_from_user(&value, argp, sizeof(value))) {
				AKMDBG("copy_from_user failed.");
				return -EFAULT;
			}
			break;
		default:
			break;
	}
	
	switch (cmd) {
		case ECS_IOCTL_WRITE:
			AKMFUNC("IOCTL_WRITE");
			if ((rwbuf[0] < 2) || (rwbuf[0] > (RWBUF_SIZE-1))) {
				AKMDBG("invalid argument.");
				return -EINVAL;
			}
			ret = AKI2C_TxData(&rwbuf[1], rwbuf[0]);
			if (ret < 0) {
				return ret;
			}
			break;
		case ECS_IOCTL_READ:
			AKMFUNC("IOCTL_READ");
			if ((rwbuf[0] < 1) || (rwbuf[0] > (RWBUF_SIZE-1))) {
				AKMDBG("invalid argument.");
				return -EINVAL;
			}
			ret = AKI2C_RxData(&rwbuf[1], rwbuf[0]);
			if (ret < 0) {
				return ret;
			}
			break;
		case ECS_IOCTL_SET_MODE:
			AKMFUNC("IOCTL_SET_MODE");
			ret = AKECS_SetMode(mode);
			if (ret < 0) {
				return ret;
			}
			break;
		case ECS_IOCTL_GETDATA:
			AKMFUNC("IOCTL_GET_DATA");
			ret = AKECS_GetData(sData);
			if (ret < 0) {
				pr_err("%s: IOCTL_GET_DATA error\n", __func__);
				return ret;
			}
			break;
		case ECS_IOCTL_SET_YPR:
			AKECS_SetYPR(value);
			break;
		case ECS_IOCTL_GET_OPEN_STATUS:
			AKMFUNC("IOCTL_GET_OPEN_STATUS");
			status = AKECS_GetOpenStatus();
			AKMDBG("AKECS_GetOpenStatus returned (%d)", status);
			break;
		case ECS_IOCTL_GET_CLOSE_STATUS:
			AKMFUNC("IOCTL_GET_CLOSE_STATUS");
			status = AKECS_GetCloseStatus();
			AKMDBG("AKECS_GetCloseStatus returned (%d)", status);
			break;
		case ECS_IOCTL_GET_DELAY:
			AKMFUNC("IOCTL_GET_DELAY");
			delay = akmd_delay;
			break;
		case ECS_IOCTL_GET_SUSPEND_STATUS:
			status = AKECS_GetSuspendStatus();
			break;
		default:
			return -ENOTTY;
	}
	
	switch (cmd) {
		case ECS_IOCTL_READ:
			if (copy_to_user(argp, &rwbuf, rwbuf[0]+1)) {
				AKMDBG("copy_to_user failed.");
				return -EFAULT;
			}
			break;
		case ECS_IOCTL_GETDATA:
			if (copy_to_user(argp, &sData, sizeof(sData))) {
				AKMDBG("copy_to_user failed.");
				return -EFAULT;
			}
			break;
		case ECS_IOCTL_GET_OPEN_STATUS:
		case ECS_IOCTL_GET_CLOSE_STATUS:
			if (copy_to_user(argp, &status, sizeof(status))) {
				AKMDBG("copy_to_user failed.");
				return -EFAULT;
			}
			break;
		case ECS_IOCTL_GET_SUSPEND_STATUS:
			if (copy_to_user(argp, &status, sizeof(status))) {
				AKMDBG("copy_to_user failed.");
				return -EFAULT;
			}
			break;
		case ECS_IOCTL_GET_DELAY:
			if (copy_to_user(argp, &delay, sizeof(delay))) {
				AKMDBG("copy_to_user failed.");
				return -EFAULT;
			}
			break;
		default:
			break;
	}
	
	return 0;
}

static long akmd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;
	struct akm8975_data *akm = i2c_get_clientdata(this_client);
	mutex_lock(&akm->akmd_ioctl_lock);
	ret = akmd_ioctl_int(file, cmd, arg);
	mutex_unlock(&akm->akmd_ioctl_lock);
	return ret;
}

static void akm8975_early_suspend(struct early_suspend *handler)
{
	AKMFUNC("akm8975_early_suspend");
	atomic_set(&suspend_flag, 1);
	atomic_set(&reserve_open_flag, atomic_read(&open_flag));
	atomic_set(&open_flag, 0);
	wake_up(&open_wq);
	AKMDBG("suspended with flag=%d", 
	       atomic_read(&reserve_open_flag));
}

static void akm8975_early_resume(struct early_suspend *handler)
{
	AKMFUNC("akm8975_early_resume");
	atomic_set(&open_flag, atomic_read(&reserve_open_flag));
	wake_up(&open_wq);
	AKMDBG("resumed with flag=%d", 
	       atomic_read(&reserve_open_flag));
	atomic_set(&suspend_flag, 0);
}

static int akm8975_suspend(struct device *dev)
{
	/* Powerdown it in suspend */
	AKECS_SetMode_PowerDown();
	return 0;
}

static int akm8975_resume(struct device *dev)
{
	return 0;
}

/*********************************************/
static struct file_operations akmd_fops = {
	.owner = THIS_MODULE,
	.open = akmd_open,
	.release = akmd_release,
	.unlocked_ioctl = akmd_ioctl,
};

static struct miscdevice akmd_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "akm8975_dev",
	.fops = &akmd_fops,
};

/*********************************************/
static int __devinit akm8975_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct akm8975_data *akm;
	int err = 0;
	
	AKMFUNC("akm8975_probe");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "AKM8975 akm8975_probe: check_functionality failed.\n");
		err = -ENODEV;
		goto exit0;
	}
	
	/* Allocate memory for driver data */
	akm = kzalloc(sizeof(struct akm8975_data), GFP_KERNEL);
	if (!akm) {
		printk(KERN_ERR "AKM8975 akm8975_probe: memory allocation failed.\n");
		err = -ENOMEM;
		goto exit0;
	}
	mutex_init(&akm->akm_aot_ioctl_lock);
	mutex_init(&akm->akmd_ioctl_lock);
	
	i2c_set_clientdata(client, akm);
	this_client = client;
	
	/* Check connection */
	err = AKECS_CheckDevice();
	if (err < 0) {
		printk(KERN_ERR "AKM8975 akm8975_probe: set power down mode error\n");
		goto exit1;
	}
	
	/* Declare input device */
	akm->input_dev = input_allocate_device();
	if (!akm->input_dev) {
		err = -ENOMEM;
		printk(KERN_ERR
		       "AKM8975 akm8975_probe: Failed to allocate input device\n");
		goto exit1;
	}
	/* Setup input device */
	set_bit(EV_ABS, akm->input_dev->evbit);
	/* yaw (0, 360) */
	input_set_abs_params(akm->input_dev, ABS_RX, 0, 23040, 0, 0);
	/* pitch (-180, 180) */
	input_set_abs_params(akm->input_dev, ABS_RY, -11520, 11520, 0, 0);
	/* roll (-90, 90) */
	input_set_abs_params(akm->input_dev, ABS_RZ, -5760, 5760, 0, 0);
	/* x-axis acceleration (720 x 8G) */
	input_set_abs_params(akm->input_dev, ABS_X, -5760, 5760, 0, 0);
	/* y-axis acceleration (720 x 8G) */
	input_set_abs_params(akm->input_dev, ABS_Y, -5760, 5760, 0, 0);
	/* z-axis acceleration (720 x 8G) */
	input_set_abs_params(akm->input_dev, ABS_Z, -5760, 5760, 0, 0);
	/* temparature */
	/*
	input_set_abs_params(akm->input_dev, ABS_THROTTLE, -30, 85, 0, 0);
	 */
	/* status of magnetic sensor */
	input_set_abs_params(akm->input_dev, ABS_RUDDER, -32768, 3, 0, 0);
	/* status of acceleration sensor */
	input_set_abs_params(akm->input_dev, ABS_WHEEL, -32768, 3, 0, 0);
	/* x-axis of raw magnetic vector (-4096, 4095) */
	input_set_abs_params(akm->input_dev, ABS_HAT0X, -20480, 20479, 0, 0);
	/* y-axis of raw magnetic vector (-4096, 4095) */
	input_set_abs_params(akm->input_dev, ABS_HAT0Y, -20480, 20479, 0, 0);
	/* z-axis of raw magnetic vector (-4096, 4095) */
	input_set_abs_params(akm->input_dev, ABS_BRAKE, -20480, 20479, 0, 0);
	/* Set name */
	akm->input_dev->name = "compass";
	
	/* Register */
	err = input_register_device(akm->input_dev);
	if (err) {
		printk(KERN_ERR
		       "AKM8975 akm8975_probe: Unable to register input device\n");
		goto exit2;
	}
	
	err = misc_register(&akmd_device);
	if (err) {
		printk(KERN_ERR
			   "AKM8975 akm8975_probe: akmd_device register failed\n");
		goto exit3;
	}
	
	mutex_init(&sense_data_mutex);
	init_waitqueue_head(&open_wq);
	
	/* As default, report all information, modify by jerrymo, all set to 0*/
	atomic_set(&m_flag, 0);
	atomic_set(&a_flag, 0);
	atomic_set(&mv_flag, 0);
	
	akm->akm_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	akm->akm_early_suspend.suspend = akm8975_early_suspend;
	akm->akm_early_suspend.resume = akm8975_early_resume;
	register_early_suspend(&akm->akm_early_suspend);
	
	AKMDBG("successfully probed."); 
	return 0;
	
exit3:
	input_unregister_device(akm->input_dev);
exit2:
	input_free_device(akm->input_dev);
exit1:
	kfree(akm);
exit0:
	return err;
}

static int __devexit akm8975_remove(struct i2c_client *client)
{
	struct akm8975_data *akm = i2c_get_clientdata(client);
	AKMFUNC("akm8975_remove");
	unregister_early_suspend(&akm->akm_early_suspend);
	misc_deregister(&akmd_device);
	input_unregister_device(akm->input_dev);
	kfree(akm);
	AKMDBG("successfully removed.");
	return 0;
}

static void akm8975_shutdown(struct i2c_client *client)
{
	AKECS_SetMode_PowerDown();
}

static const struct i2c_device_id akm8975_id[] = {
	{AKM8975_I2C_NAME, 0 },
	{ }
};

#ifdef CONFIG_PM
static const struct dev_pm_ops akm8975_pm_ops = {
	.suspend = akm8975_suspend,
	.resume = akm8975_resume,
};
#endif

static struct i2c_driver akm8975_driver = {
	.probe		= akm8975_probe,
	.shutdown	= akm8975_shutdown,
	.remove		= __devexit_p(akm8975_remove),
	.id_table	= akm8975_id,
	.driver = {
		.name = AKM8975_I2C_NAME,
#ifdef CONFIG_PM	
		.pm = &akm8975_pm_ops,
#endif	
	},
};

static int __init akm8975_init(void)
{
	printk(KERN_INFO "AKM8975 compass driver: initialize\n");
	return i2c_add_driver(&akm8975_driver);
}

static void __exit akm8975_exit(void)
{
	printk(KERN_INFO "AKM8975 compass driver: release\n");
	i2c_del_driver(&akm8975_driver);
}

module_init(akm8975_init);
module_exit(akm8975_exit);

MODULE_AUTHOR("viral wang <viral_wang@htc.com>");
MODULE_DESCRIPTION("AKM8975 compass driver");
MODULE_LICENSE("GPL");

