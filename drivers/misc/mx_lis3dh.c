#include	<linux/err.h>
#include	<linux/errno.h>
#include	<linux/delay.h>
#include	<linux/fs.h>
#include	<linux/i2c.h>
#include	<linux/input.h>
#include	<linux/uaccess.h>
#include	<linux/workqueue.h>
#include	<linux/irq.h>
#include	<linux/gpio.h>
#include	<linux/interrupt.h>
#include	<linux/slab.h>
#include	<linux/miscdevice.h>

#include	<linux/earlysuspend.h>
#include	<linux/mx_lis3dh.h>

#define	DEBUG	1
#define	G_MAX	16000

static atomic_t suspend_flag = ATOMIC_INIT(0);

struct {
	unsigned int cutoff_ms;
	unsigned int mask;
} lis3dh_acc_odr_table[] = {
		{    1, ODR1250 },
		{    3, ODR400  },
		{    5, ODR200  },
		{   10, ODR100  },
		{   20, ODR50   },
		{   40, ODR25   },
		{  100, ODR10   },
		{ 1000, ODR1    },
};

struct lis3dh_acc_data {
	struct i2c_client *client;
	struct lis3dh_acc_platform_data *pdata;
	struct mutex lock;
	struct delayed_work input_work;
	struct early_suspend early_suspend;
	struct input_dev *input_dev;
	int hw_initialized;
	/* hw_working=-1 means not tested yet */
	int hw_working;
	atomic_t enabled;
	int on_before_suspend;
	u8 sensitivity;
	u8 resume_state[RESUME_ENTRIES];
	struct mutex ioctl_lock;

#ifdef DEBUG
	u8 reg_addr;
#endif

	struct miscdevice misc_device;
	atomic_t opened;     /*misc device open flag*/
};

struct lis3dh_t {
	int x;
	int y;
	int z;
};

static int lis3dh_acc_i2c_read(struct lis3dh_acc_data *acc,
				u8 * buf, int len)
{
	int err;
	int tries = 0;
	struct i2c_client *client = acc->client;

	struct i2c_msg	msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags & I2C_M_TEN,
			.len = 1,
			.buf = buf,
		},
		{
			.addr = client->addr,
			.flags = (client->flags & I2C_M_TEN) | I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	do {
		err = i2c_transfer(client->adapter, msgs, 2);
		if (err != 2) {
			pr_err("%s: i2c_transfer fail, retry %d\n", 
					__func__, tries + 1);

			msleep_interruptible(I2C_RETRY_DELAY);
		}
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		dev_err(&client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lis3dh_acc_i2c_write(struct lis3dh_acc_data *acc, u8 * buf, int len)
{
	int err;
	int tries = 0;
	struct i2c_client *client = acc->client;

	struct i2c_msg msgs[] = {
		{
		 .addr = client->addr,
		 .flags = client->flags & I2C_M_TEN,
		 .len = len + 1,
		 .buf = buf,
		},
	};

	do {
		err = i2c_transfer(client->adapter, msgs, 1);
		if (err != 1) {
			pr_err("%s: i2c_transfer fail, retry %d\n",
				       	__func__, tries + 1);

			msleep_interruptible(I2C_RETRY_DELAY);
		}
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lis3dh_acc_hw_init(struct lis3dh_acc_data *acc)
{
	int err = -1;
	u8 buf[7];

	buf[0] = CTRL_REG1;
	buf[1] = acc->resume_state[RES_CTRL_REG1];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = TEMP_CFG_REG;
	buf[1] = acc->resume_state[RES_TEMP_CFG_REG];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = FIFO_CTRL_REG;
	buf[1] = acc->resume_state[RES_FIFO_CTRL_REG];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | TT_THS);
	buf[1] = acc->resume_state[RES_TT_THS];
	buf[2] = acc->resume_state[RES_TT_LIM];
	buf[3] = acc->resume_state[RES_TT_TLAT];
	buf[4] = acc->resume_state[RES_TT_TW];
	err = lis3dh_acc_i2c_write(acc, buf, 4);
	if (err < 0)
		goto err_resume_state;
	buf[0] = TT_CFG;
	buf[1] = acc->resume_state[RES_TT_CFG];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | INT_THS1);
	buf[1] = acc->resume_state[RES_INT_THS1];
	buf[2] = acc->resume_state[RES_INT_DUR1];
	err = lis3dh_acc_i2c_write(acc, buf, 2);
	if (err < 0)
		goto err_resume_state;
	buf[0] = INT_CFG1;
	buf[1] = acc->resume_state[RES_INT_CFG1];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;


	buf[0] = (I2C_AUTO_INCREMENT | CTRL_REG2);
	buf[1] = acc->resume_state[RES_CTRL_REG2];
	buf[2] = acc->resume_state[RES_CTRL_REG3];
	buf[3] = acc->resume_state[RES_CTRL_REG4];
	buf[4] = acc->resume_state[RES_CTRL_REG5];
	buf[5] = acc->resume_state[RES_CTRL_REG6];
	err = lis3dh_acc_i2c_write(acc, buf, 5);
	if (err < 0)
		goto err_resume_state;

	acc->hw_initialized = 1;
	pr_info(KERN_INFO "%s: hw init done\n", LIS3DH_ACC_DEV_NAME);
	return 0;

err_resume_state:
	acc->hw_initialized = 0;
	dev_err(&acc->client->dev, "hw init error 0x%x,0x%x: %d\n", buf[0],
			buf[1], err);
	return err;
}

static void lis3dh_acc_device_power_off(struct lis3dh_acc_data *acc)
{
	int err;
	u8 buf[2] = { CTRL_REG1, LIS3DH_ACC_PM_OFF };

	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		dev_err(&acc->client->dev, "soft power off failed: %d\n", err);
	
	if (acc->hw_initialized) {
		acc->hw_initialized = 0;
	}

}

static int lis3dh_acc_device_power_on(struct lis3dh_acc_data *acc)
{
	int err = -1;

	if (!acc->hw_initialized) {
		err = lis3dh_acc_hw_init(acc);
		if (acc->hw_working == 1 && err < 0) {
			lis3dh_acc_device_power_off(acc);
			return err;
		}
	}

	return 0;
}

int lis3dh_acc_update_g_range(struct lis3dh_acc_data *acc, u8 new_g_range)
{
	int err=-1;

	u8 sensitivity;
	u8 buf[2];
	u8 updated_val;
	u8 init_val;
	u8 new_val;
	u8 mask = LIS3DH_ACC_FS_MASK | HIGH_RESOLUTION;

	switch (new_g_range) {
	case LIS3DH_ACC_G_2G:
		sensitivity = SENSITIVITY_2G;
		break;
	case LIS3DH_ACC_G_4G:
		sensitivity = SENSITIVITY_4G;
		break;
	case LIS3DH_ACC_G_8G:
		sensitivity = SENSITIVITY_8G;
		break;
	case LIS3DH_ACC_G_16G:
		sensitivity = SENSITIVITY_16G;
		break;
	default:
		dev_err(&acc->client->dev, "invalid g range requested: %u\n",
				new_g_range);
		return -EINVAL;
	}

	if (atomic_read(&acc->enabled)) {
		/* Updates configuration register 4,
		* which contains g range setting */
		buf[0] = CTRL_REG4;
		err = lis3dh_acc_i2c_read(acc, buf, 1);
		if (err < 0)
			goto error;
		init_val = buf[0];
		acc->resume_state[RES_CTRL_REG4] = init_val;
		new_val = new_g_range | HIGH_RESOLUTION;
		updated_val = ((mask & new_val) | ((~mask) & init_val));
		buf[1] = updated_val;
		buf[0] = CTRL_REG4;
		err = lis3dh_acc_i2c_write(acc, buf, 1);
		if (err < 0)
			goto error;
		acc->resume_state[RES_CTRL_REG4] = updated_val;
		acc->sensitivity = sensitivity;
	}

	return err;
error:
	dev_err(&acc->client->dev, "update g range failed 0x%x,0x%x: %d\n",
			buf[0], buf[1], err);

	return err;
}

int lis3dh_acc_update_odr(struct lis3dh_acc_data *acc, int poll_interval_ms)
{
	int err = -1;
	int i;
	u8 config[2];

	/* Following, looks for the longest possible odr interval scrolling the
	 * odr_table vector from the end (shortest interval) backward (longest
	 * interval), to support the poll_interval requested by the system.
	 * It must be the longest interval lower then the poll interval.*/
	for (i = ARRAY_SIZE(lis3dh_acc_odr_table) - 1; i >= 0; i--) {
		if (lis3dh_acc_odr_table[i].cutoff_ms < poll_interval_ms)
			break;
	}

	pr_info("%s: cutoff_ms = %d, odr = 0x%02x\n",
		__func__, lis3dh_acc_odr_table[i].cutoff_ms, 
		lis3dh_acc_odr_table[i].mask);

	config[1] = lis3dh_acc_odr_table[i].mask;

	config[1] |= LIS3DH_ACC_ENABLE_ALL_AXES;

	/* If device is currently enabled, we need to write new
	 *  configuration out to it */
	if (atomic_read(&acc->enabled)) {
		config[0] = CTRL_REG1;
		err = lis3dh_acc_i2c_write(acc, config, 1);
		if (err < 0)
			goto error;
		acc->resume_state[RES_CTRL_REG1] = config[1];
	}

	return err;

error:
	dev_err(&acc->client->dev, "update odr failed 0x%x,0x%x: %d\n",
			config[0], config[1], err);

	return err;
}


static int lis3dh_acc_register_write(struct lis3dh_acc_data *acc, u8 *buf,
		u8 reg_address, u8 new_value)
{
	int err = -1;

	/* Sets configuration register at reg_address
	 *  NOTE: this is a straight overwrite  */
		buf[0] = reg_address;
		buf[1] = new_value;
		err = lis3dh_acc_i2c_write(acc, buf, 1);
		if (err < 0)
			return err;
	return err;
}


static int lis3dh_acc_get_acceleration_data(struct lis3dh_acc_data *acc,
		int *xyz)
{
	int err = -1;
	/* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	u8 acc_data[6];
	s16 *hw_d = (s16 *)acc_data;
	u8 sensitivity = acc->sensitivity;
	struct lis3dh_acc_platform_data *pdata = acc->pdata;

	acc_data[0] = (I2C_AUTO_INCREMENT | AXISDATA_REG);
	err = lis3dh_acc_i2c_read(acc, acc_data, 6);
	if (err < 0)
		return err;

	/*adjust x, y, z coordinates*/
	xyz[0] = (((pdata->negate_x) ? (-hw_d[pdata->axis_map_x]) : 
		   (hw_d[pdata->axis_map_x])) >> 4) * sensitivity;
	xyz[1] = (((pdata->negate_y) ? (-hw_d[pdata->axis_map_y]) :
		   (hw_d[pdata->axis_map_y])) >> 4) * sensitivity;
	xyz[2] = (((pdata->negate_z) ? (-hw_d[pdata->axis_map_z]) : 
		   (hw_d[pdata->axis_map_z])) >> 4) * sensitivity;

	#ifdef DEBUG
	/*
		printk(KERN_INFO "%s hw_d[0]=%d, hw_d[1]=%d, dw_d[2]=%d, \
		read x=%d, y=%d, z=%d\n", LIS3DH_ACC_DEV_NAME, hw_d[0],
		hw_d[1], hw_d[2], xyz[0], xyz[1], xyz[2]);
	*/
	#endif
	
	return err;
}


static int lis3dh_acc_enable(struct lis3dh_acc_data *acc)
{
	int err;

	pr_info("%s calls %s\n", current->comm, __func__);

	if (!atomic_read(&acc->enabled)) {
		err = lis3dh_acc_device_power_on(acc);
		if (err < 0) {
			pr_err("%s: lis3dh_acc_device_power_on fail\n",
				       	__func__);
			return err;
		}
		atomic_set(&acc->enabled, 1);
	}

	return 0;
}

static int lis3dh_acc_disable(struct lis3dh_acc_data *acc)
{
	pr_debug("%s calls %s\n", current->comm, __func__);

	if (atomic_read(&acc->enabled)) {
		lis3dh_acc_device_power_off(acc);
		atomic_set(&acc->enabled, 0);
	}

	return 0;
}

static ssize_t read_single_reg(struct device *dev, char *buf, u8 reg)
{
	ssize_t ret;
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	int err;

	u8 data = reg;
	err = lis3dh_acc_i2c_read(acc, &data, 1);
	if (err < 0)
		return err;
	ret = sprintf(buf, "0x%02x\n", data);
	return ret;

}

static int write_reg(struct device *dev, const char *buf, u8 reg,
		u8 mask, int resumeIndex)
{
	int err = -1;
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	u8 x[2];
	u8 new_val;
	unsigned long val;

	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;

	new_val=((u8) val & mask);
	x[0] = reg;
	x[1] = new_val;
	err = lis3dh_acc_register_write(acc, x,reg,new_val);
	if (err < 0)
		return err;
	acc->resume_state[resumeIndex] = new_val;
	return err;
}

/* selftest start */
#define SELFTEST_MEASURE_TIMEOUT 100
#define SELFTEST_ZYXDA (0x1 << 3)
#define SELFTEST_SAMPLES 100

static int selftest_init_lis3dh(struct lis3dh_acc_data *acc)
{
	unsigned char buf[5];
	pr_info("%s\n", __func__);

	/* BDU=1, ODR=200Hz, FS=+/-2G */
	buf[0] = I2C_AUTO_INCREMENT | CTRL_REG1;
	buf[1] = ODR200 | 0xFF;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = 0x80;	/* BDU=1 */

	return lis3dh_acc_i2c_write(acc, buf, 4);
}

static int selftest_enable(struct lis3dh_acc_data *acc)
{
	unsigned char buf[2];

	buf[0] = CTRL_REG4;
	buf[1] = 0x82;	/* BDU=1, ST1 = 1, ST0 = 0 */

	pr_debug("%s\n", __func__);

	return lis3dh_acc_i2c_write(acc, buf, 1);
}

static void selftest_disable(struct lis3dh_acc_data *acc)
{
	unsigned char buf[2];

	buf[1] = 0x00;
	pr_debug("%s\n", __func__);

	/* Disable sensor */
	buf[0] = CTRL_REG1;
	lis3dh_acc_i2c_write(acc, buf, 1);
	/* Disable selftest */
	buf[0] = CTRL_REG4;
	lis3dh_acc_i2c_write(acc, buf, 1);
}

static int selftest_wait_ZYXDA(struct lis3dh_acc_data *acc)
{
	int i, ret;
	unsigned char data_ready;

	pr_debug("%s\n", __func__);

	for (i = SELFTEST_MEASURE_TIMEOUT; i != 0; i--) {
		data_ready = STATUS_REG;
		ret = lis3dh_acc_i2c_read(acc, &data_ready, 1);
		if (ret < 0) {
			pr_err("%s: lis3dh_i2c_read fail, retry %d\n", 
					__func__, i);
			msleep(I2C_RETRY_DELAY);
			continue;
		} else if (data_ready & SELFTEST_ZYXDA) {
			pr_debug("%s: data ready\n", __func__);
			break;
		}
	}
	if (i == 0) {
		pr_err("%s: failed\n", __func__);
		return ret;
	}

	return 0;
}

static int selftest_read(struct lis3dh_acc_data *acc, struct lis3dh_t *data)
{
	int total[3];
	int i, ret;

	pr_debug("%s\n", __func__);

	total[0] = 0;
	total[1] = 0;
	total[2] = 0;
	for (i = 0; i < SELFTEST_SAMPLES; i++) {
		ret = selftest_wait_ZYXDA(acc);
		if (ret) {
			pr_err("%s: selftest_check_ZYXDA fail\n", __func__);
			return ret;
		}
		mutex_lock(&acc->lock);
		ret = lis3dh_acc_get_acceleration_data(acc, (int *)data);
		mutex_unlock(&acc->lock);
		if (ret < 0) {
			pr_err("%s: lis3dh_read_gyro_values fail\n", __func__);
			return ret;
		}
		pr_debug("%s: data: x = %d, y = %d, z = %d\n", __func__, 
				data->x, data->y, data->z);
		total[0] += data->x;
		total[1] += data->y;
		total[2] += data->z;
		pr_debug("%s: total: x = %d, y = %d, z = %d\n", __func__, 
				total[0], total[1], total[2]);
	}
	data->x = total[0] / SELFTEST_SAMPLES;
	data->y = total[1] / SELFTEST_SAMPLES;
	data->z = total[2] / SELFTEST_SAMPLES;
	pr_debug("%s: average: x = %d, y = %d, z = %d\n", __func__,
		       	data->x, data->y, data->z);

	return 0;
}

/*
 * Part Number Min_X Max_X Min_Y Max_Y Min_Z Max_Z Unit
 * LIS3DH 60 1700 60 1700 60 1400 LSB (@ FS = +/-2g,2.8v)
 */

#define CONVERT_TO_MG   1      /* for range = +/-2g */ 

static int check_selftest_result(struct lis3dh_t *data_nost,
		struct lis3dh_t *data_st)
{
	data_st->x = abs(data_st->x - data_nost->x) * CONVERT_TO_MG;
	data_st->y = abs(data_st->y - data_nost->y) * CONVERT_TO_MG;
	data_st->z = abs(data_st->z - data_nost->z) * CONVERT_TO_MG;

	if(data_st->x >= 60 && data_st->x <= 1700){
		pr_debug("expect 60 =< x <= 1700, x = %d selftest pass\n", data_st->x);
	}

	if(data_st->y >= 60 && data_st->y <= 1700){
		pr_debug("expect 60 =< y <= 1700, y = %d selftest pass\n", data_st->y);
	}

	if(data_st->z >= 60 && data_st->z <= 1400){
		pr_debug("expect 60 =< z <= 1400, z = %d selftest pass\n",data_st->z);
	}

	if(data_st->x >= 60 && data_st->x <= 1700 && data_st->y >= 60
			&& data_st->y <= 1700 && data_st->z >= 60 &&
			data_st->z <= 1400){
		return 1;
	}
	return -1;
}

static int lis3dh_selftest(struct lis3dh_acc_data *acc, int *test_result,
	       	struct lis3dh_t *self_data)
{
	int ret;
	struct lis3dh_t data_nost;

	/* Initialize Sensor, turn on sensor, enable P/R/Y */
	ret = selftest_init_lis3dh(acc);
	if (ret < 0) {
		pr_err("%s: selftest_init_lis3dh fail\n", __func__);
		return ret;
	}

	/* Wait for stable output */
	pr_debug("%s: wait for stable output\n", __func__);
	msleep(800);

	/* Read out normal output */
	ret = selftest_read(acc, &data_nost);
	if (ret < 0) {
		pr_err("%s: selftest_read fail\n", __func__);
		return ret;
	}
	pr_debug("%s: normal output: x = %d, y = %d, z = %d\n",
		__func__, data_nost.x, data_nost.y, data_nost.z);
	
	/* Enable self test */
	ret = selftest_enable(acc);
	if (ret < 0) {
		pr_err("%s: selftest_enable failed\n", __func__);
		return ret;
	}
	
	/* ODR=200HZ, wait for 3 * ODR */
	mdelay(3 * (1000 / 200));
	
	/* Read out selftest output */
	ret = selftest_read(acc, self_data);
	if (ret < 0) {
		pr_err("%s: selftest_read fail\n", __func__);
		return ret;
	}
	
	/* Check output */
	ret = check_selftest_result(&data_nost, self_data);
	if (ret < 0) {
		pr_err("%s: ***fail***\n", __func__);
		*test_result = 0;
	} else {
		pr_info("%s: ***success***\n", __func__);
		*test_result = 1;
	}
	
	/* selftest disable */
	selftest_disable(acc);

	return ret;
}
/* selftest end */

static ssize_t attr_get_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int val;
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	mutex_lock(&acc->lock);
	val = acc->pdata->poll_interval;
	mutex_unlock(&acc->lock);
	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long interval_ms;

	if (strict_strtoul(buf, 10, &interval_ms))
		return -EINVAL;
	if (!interval_ms)
		return -EINVAL;
	mutex_lock(&acc->lock);
	acc->pdata->poll_interval = interval_ms;
	lis3dh_acc_update_odr(acc, interval_ms);
	mutex_unlock(&acc->lock);
	return size;
}

static ssize_t attr_get_range(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	char val;
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	char range = 2;
	mutex_lock(&acc->lock);
	val = acc->pdata->g_range ;
	switch (val) {
	case LIS3DH_ACC_G_2G:
		range = 2;
		break;
	case LIS3DH_ACC_G_4G:
		range = 4;
		break;
	case LIS3DH_ACC_G_8G:
		range = 8;
		break;
	case LIS3DH_ACC_G_16G:
		range = 16;
		break;
	}
	mutex_unlock(&acc->lock);
	return sprintf(buf, "%d\n", range);
}

static ssize_t attr_set_range(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	mutex_lock(&acc->lock);
	acc->pdata->g_range = val;
	lis3dh_acc_update_g_range(acc, val);
	mutex_unlock(&acc->lock);
	return size;
}

static ssize_t attr_get_enable(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	int val = atomic_read(&acc->enabled);
	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_enable(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val)
		lis3dh_acc_enable(acc);
	else
		lis3dh_acc_disable(acc);

	return size;
}

static ssize_t attr_set_intconfig1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev,buf,INT_CFG1,NO_MASK,RES_INT_CFG1);
}

static ssize_t attr_get_intconfig1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev,buf,INT_CFG1);
}

static ssize_t attr_set_duration1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev,buf,INT_DUR1,INT1_DURATION_MASK,RES_INT_DUR1);
}

static ssize_t attr_get_duration1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev,buf,INT_DUR1);
}

static ssize_t attr_set_thresh1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev,buf,INT_THS1,INT1_THRESHOLD_MASK,RES_INT_THS1);
}

static ssize_t attr_get_thresh1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev,buf,INT_THS1);
}

static ssize_t attr_get_source1(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return read_single_reg(dev,buf,INT_SRC1);
}

static ssize_t attr_set_click_cfg(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev,buf,TT_CFG,TAP_CFG_MASK,RES_TT_CFG);
}

static ssize_t attr_get_click_cfg(struct device *dev,
		struct device_attribute *attr,	char *buf)
{

	return read_single_reg(dev,buf,TT_CFG);
}

static ssize_t attr_get_click_source(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev,buf,TT_SRC);
}

static ssize_t attr_set_click_ths(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev,buf,TT_THS,TAP_THS_MASK,RES_TT_THS);
}

static ssize_t attr_get_click_ths(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev,buf,TT_THS);
}

static ssize_t attr_set_click_tlim(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev,buf,TT_LIM,TAP_TLIM_MASK,RES_TT_LIM);
}

static ssize_t attr_get_click_tlim(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev,buf,TT_LIM);
}

static ssize_t attr_set_click_tlat(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev,buf,TT_TLAT,TAP_TLAT_MASK,RES_TT_TLAT);
}

static ssize_t attr_get_click_tlat(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev,buf,TT_TLAT);
}

static ssize_t attr_set_click_tw(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev,buf,TT_TLAT,TAP_TW_MASK,RES_TT_TLAT);
}

static ssize_t attr_get_click_tw(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev,buf,TT_TLAT);
}

static ssize_t attr_get_raw_data(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	int xyz[3] = { 0 };
	int err;

	mutex_lock(&acc->lock);

	err = lis3dh_acc_get_acceleration_data(acc, xyz);
	if (err < 0) {
		dev_err(&acc->client->dev, "get_acceleration_data failed\n");
		mutex_unlock(&acc->lock);
		return err;
	}
	mutex_unlock(&acc->lock);
	
	return sprintf(buf, "%d %d %d\n", xyz[0], xyz[1], xyz[2]);
}

static ssize_t attr_get_selftest_data(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	struct lis3dh_t self_data;
	int self_result, ret;

	ret = lis3dh_selftest(acc, &self_result, &self_data);
	
	ret = sprintf(buf, "SELFTEST RESULT : %d, %d, %d\n", 
			self_data.x, self_data.y, self_data.z);

	return ret;

}

#ifdef DEBUG
/* PAY ATTENTION: These DEBUG funtions don't manage resume_state */
static ssize_t attr_reg_set(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	int rc;
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	u8 x[2];
	unsigned long val;

	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;
	mutex_lock(&acc->lock);
	x[0] = acc->reg_addr;
	mutex_unlock(&acc->lock);
	x[1] = val;
	rc = lis3dh_acc_i2c_write(acc, x, 1);
	/*TODO: error need to be managed */
	return size;
}

static ssize_t attr_reg_get(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	ssize_t ret;
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	int rc;
	u8 data;

	mutex_lock(&acc->lock);
	data = acc->reg_addr;
	mutex_unlock(&acc->lock);
	rc = lis3dh_acc_i2c_read(acc, &data, 1);
	/*TODO: error need to be managed */
	ret = sprintf(buf, "0x%02x\n", data);
	return ret;
}

static ssize_t attr_addr_set(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;
	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;
	mutex_lock(&acc->lock);
	acc->reg_addr = val;
	mutex_unlock(&acc->lock);
	return size;
}
#endif

static struct device_attribute attributes[] = {

	__ATTR(pollrate_ms, 0664, attr_get_polling_rate, attr_set_polling_rate),
	__ATTR(range, 0664, attr_get_range, attr_set_range),
	__ATTR(enable, 0664, attr_get_enable, attr_set_enable),
	__ATTR(int1_config, 0664, attr_get_intconfig1, attr_set_intconfig1),
	__ATTR(int1_duration, 0664, attr_get_duration1, attr_set_duration1),
	__ATTR(int1_threshold, 0664, attr_get_thresh1, attr_set_thresh1),
	__ATTR(int1_source, 0444, attr_get_source1, NULL),
	__ATTR(click_config, 0664, attr_get_click_cfg, attr_set_click_cfg),
	__ATTR(click_source, 0444, attr_get_click_source, NULL),
	__ATTR(click_threshold, 0664, attr_get_click_ths, attr_set_click_ths),
	__ATTR(click_timelimit, 0664, attr_get_click_tlim, attr_set_click_tlim),
	__ATTR(click_timelatency, 0664, attr_get_click_tlat,
							attr_set_click_tlat),
	__ATTR(click_timewindow, 0664, attr_get_click_tw, attr_set_click_tw),
	__ATTR(raw_data, 0444, attr_get_raw_data, NULL),
	__ATTR(selftest_data, 0444, attr_get_selftest_data, NULL),

#ifdef DEBUG
	__ATTR(reg_value, 0600, attr_reg_get, attr_reg_set),
	__ATTR(reg_addr, 0200, NULL, attr_addr_set),
#endif
};

static int create_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(dev, attributes + i))
			goto error;
	return 0;

error:
	for ( ; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "%s:Unable to create interface\n", __func__);
	return -1;
}

static int remove_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
	return 0;
}

static int lis3dh_acc_validate_pdata(struct lis3dh_acc_data *acc)
{
	acc->pdata->poll_interval = max(acc->pdata->poll_interval,
			acc->pdata->min_interval);

	if (acc->pdata->axis_map_x > 2 ||
		acc->pdata->axis_map_y > 2 ||
		 acc->pdata->axis_map_z > 2) {
		dev_err(&acc->client->dev, "invalid axis_map value "
			"x:%u y:%u z%u\n", acc->pdata->axis_map_x,
				acc->pdata->axis_map_y, acc->pdata->axis_map_z);
		return -EINVAL;
	}

	/* Only allow 0 and 1 for negation boolean flag */
	if (acc->pdata->negate_x > 1 || acc->pdata->negate_y > 1
			|| acc->pdata->negate_z > 1) {
		dev_err(&acc->client->dev, "invalid negate value "
			"x:%u y:%u z:%u\n", acc->pdata->negate_x,
				acc->pdata->negate_y, acc->pdata->negate_z);
		return -EINVAL;
	}

	/* Enforce minimum polling interval */
	if (acc->pdata->poll_interval < acc->pdata->min_interval) {
		dev_err(&acc->client->dev, "minimum poll interval violated\n");
		return -EINVAL;
	}

	return 0;
}

static int lis3dh_acc_input_init(struct lis3dh_acc_data *acc)
{
	int err;

	acc->input_dev = input_allocate_device();
	if (!acc->input_dev) {
		err = -ENOMEM;
		dev_err(&acc->client->dev, "input device allocation failed\n");
		goto err0;
	}

	acc->input_dev->name = LIS3DH_ACC_DEV_NAME;
	acc->input_dev->id.bustype = BUS_I2C;
	acc->input_dev->dev.parent = &acc->client->dev;

	input_set_drvdata(acc->input_dev, acc);

	set_bit(EV_ABS, acc->input_dev->evbit);
	/*	next is used for interruptA sources data if the case */
	set_bit(ABS_MISC, acc->input_dev->absbit);
	/*	next is used for interruptB sources data if the case */
	set_bit(ABS_WHEEL, acc->input_dev->absbit);

	input_set_abs_params(acc->input_dev, ABS_X, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(acc->input_dev, ABS_Y, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(acc->input_dev, ABS_Z, -G_MAX, G_MAX, FUZZ, FLAT);
	/*	next is used for interruptA sources data if the case */
	input_set_abs_params(acc->input_dev, ABS_MISC, INT_MIN, INT_MAX, 0, 0);
	/*	next is used for interruptB sources data if the case */
	input_set_abs_params(acc->input_dev, ABS_WHEEL, INT_MIN, INT_MAX, 0, 0);

	err = input_register_device(acc->input_dev);
	if (err) {
		dev_err(&acc->client->dev,
				"unable to register input device %s\n",
				acc->input_dev->name);
		goto err1;
	}

	return 0;

err1:
	input_free_device(acc->input_dev);
err0:
	return err;
}

static void lis3dh_acc_input_cleanup(struct lis3dh_acc_data *acc)
{
	input_unregister_device(acc->input_dev);
	input_free_device(acc->input_dev);
}

/*
  *lis3dh misc device file operation functions inplement
*/
static int lis3dh_misc_open(struct inode *inode, struct file *file)
{
	struct lis3dh_acc_data *lis3dh = container_of(file->private_data, 
				struct lis3dh_acc_data, misc_device);
	file->private_data = lis3dh;
	
	return 0;
}

static int lis3dh_misc_close(struct inode *inode, struct file *file)
{
	return 0;
}

static long lis3dh_misc_ioctl_init(struct file *file, 
		unsigned int cmd, unsigned long arg)
{
	int ret, enable;
	int acc_data[3];
	int delay;
	int test_result;
	struct lis3dh_acc_data *lis3dh = file->private_data;
	struct lis3dh_t self_data;
	int status;

	switch (cmd) {
	case LIS3DH_IOCTL_SET_ENABLE:
		ret = copy_from_user(&enable, (void __user *)arg, sizeof(int));
		if (ret) {
			pr_err("%s()->%d:copy enable operation error!\n",
				__FUNCTION__, __LINE__);
			return -EINVAL;
		}

		if (enable) {
			ret = lis3dh_acc_enable(lis3dh);
			if (ret < 0) {
				pr_err("%s()->%d:start work fail!\n",
					__FUNCTION__, __LINE__);
				return ret;
			}
		}
		else {
			ret = lis3dh_acc_disable(lis3dh);
			if (ret < 0) {
				pr_err("%s()->%d:start work fail!\n",
					__FUNCTION__, __LINE__);
				return ret;
			}
		}
		break;
	case LIS3DH_IOCTL_GET_ENABLE:
		enable = atomic_read(&lis3dh->enabled);
		ret = copy_to_user((void __user *)arg, &enable, sizeof(int));
		if (ret) {
			pr_err("%s()->%d:copy enable operation error!\n",
				__FUNCTION__, __LINE__);
			return -EINVAL;
		}
		break;
	case LIS3DH_IOCTL_SET_DELAY:
		ret = copy_from_user(&delay, (void __user *)arg, sizeof(int));
		if (ret) {
			pr_err("%s()->%d:copy enable operation error!\n",
				__FUNCTION__, __LINE__);
			return -EINVAL;
		}

		lis3dh->pdata->poll_interval = delay;
		lis3dh_acc_update_odr(lis3dh, lis3dh->pdata->poll_interval);
		break;
	case LIS3DH_IOCTL_GET_DELAY:
		delay = lis3dh->pdata->poll_interval;
		ret = copy_to_user((void __user *)arg, &delay, sizeof(int));
		if (ret) {
			pr_err("%s()->%d:copy delay operation error!\n",
				__FUNCTION__, __LINE__);
			return -EINVAL;
		}
		break;
	case LIS3DH_IOCTL_READ_ACCEL_XYZ:
		ret = lis3dh_acc_get_acceleration_data(lis3dh, acc_data);
		if (ret) {
			pr_err("%s()->%d:read accelerater data error!\n",
				__FUNCTION__, __LINE__);
			return -EIO;
		}

		ret = copy_to_user((void __user *)arg, acc_data, sizeof(int) * 3);
		if (ret) {
			pr_err("%s()->%d:copy accelerater data error!\n",
				__FUNCTION__, __LINE__);
			return -EINVAL;
		}
		
		break;
	case LIS3DH_IOCTL_SELFTEST:
		ret = lis3dh_selftest(lis3dh, &test_result, &self_data);

		if (copy_to_user((void __user *)arg, &test_result,
				       	sizeof(int)) != 0) {
			pr_err("%s: copy_to_user error\n", __func__);
			return -EFAULT;
		}
		break;
	case LIS3DH_IOCTL_GET_SUSPEND_STATUS:
		status = atomic_read(&suspend_flag);
		if (copy_to_user((void __user *)arg, &status, sizeof(int)) != 0) {
			pr_err("%s: copy_to_user error\n", __func__);
			return -EFAULT;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static long lis3dh_misc_ioctl(struct file *file, 
		unsigned int cmd, unsigned long arg)
{
	long ret;
	struct lis3dh_acc_data *lis3dh = file->private_data;
	mutex_lock(&lis3dh->ioctl_lock);
	ret = lis3dh_misc_ioctl_init(file, cmd, arg);
	mutex_unlock(&lis3dh->ioctl_lock);
	return ret;
}

static void lis3dh_check_device(struct lis3dh_acc_data *acc)
{
	int err = 0;
	u8 buf[1];
	buf[0] = WHO_AM_I;
	err = lis3dh_acc_i2c_read(acc, buf, 1);
	if (err < 0) {
		dev_warn(&acc->client->dev, "Error reading WHO_AM_I: is device "
			"available/working?\n");
	} 
	
	if (buf[0] != WHOAMI_LIS3DH_ACC) {
		dev_err(&acc->client->dev,
			"device unknown. Expected: 0x%x,"
			" Replies: 0x%x\n", WHOAMI_LIS3DH_ACC, buf[0]);
		err = -1; /* choose the right coded error */
		goto err_unknown_device;
	}

err_unknown_device:
	acc->hw_initialized = 0;
}

static void lis3dh_init_register(struct lis3dh_acc_data *acc)
{
	/*initialize register values*/
	memset(acc->resume_state, 0, ARRAY_SIZE(acc->resume_state));
	acc->resume_state[RES_CTRL_REG1]     = LIS3DH_ACC_ENABLE_ALL_AXES; 
	acc->resume_state[RES_CTRL_REG2]     = 0x00;                       
	acc->resume_state[RES_CTRL_REG3]     = 0x00;                       
	acc->resume_state[RES_CTRL_REG4]     = 0x00;                       
	acc->resume_state[RES_CTRL_REG5]     = 0x00;                       
	acc->resume_state[RES_CTRL_REG6]     = 0x00;                       
	acc->resume_state[RES_TEMP_CFG_REG]  = 0x00;                       
	acc->resume_state[RES_FIFO_CTRL_REG] = 0x00;                       
	acc->resume_state[RES_INT_CFG1]      = 0x00;                       
	acc->resume_state[RES_INT_THS1]      = 0x00;                       
	acc->resume_state[RES_INT_DUR1]      = 0x00;                       
	acc->resume_state[RES_TT_CFG]        = 0x00;                       
	acc->resume_state[RES_TT_THS]        = 0x00;                       
	acc->resume_state[RES_TT_LIM]        = 0x00;                       
	acc->resume_state[RES_TT_TLAT]       = 0x00;                       
	acc->resume_state[RES_TT_TW]         = 0x00;                       
	
	pr_info("%s():is done\n",__func__);
}

/*
  *lis3dh misc device file operation
  *here just need to define open, release and ioctl functions.
*/
struct file_operations lis3dh_fops = 
{
	.owner = THIS_MODULE,
	.open = lis3dh_misc_open,
	.release = lis3dh_misc_close,
	.unlocked_ioctl = lis3dh_misc_ioctl,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void lis3dh_early_suspend(struct early_suspend *h)
{
	pr_debug("%s\n", __func__);

	atomic_set(&suspend_flag, 1);
}

static void lis3dh_late_resume(struct early_suspend *h)
{
	pr_debug("%s\n", __func__);

	atomic_set(&suspend_flag, 0);
}
#endif

#ifdef CONFIG_PM
static int lis3dh_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lis3dh_acc_data *acc = i2c_get_clientdata(client);

	pr_debug("%s\n", __func__);
	acc->on_before_suspend = atomic_read(&acc->enabled);
	if (acc->on_before_suspend)
		lis3dh_acc_disable(acc);

	return 0;
}
static int lis3dh_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lis3dh_acc_data *acc = i2c_get_clientdata(client);

	pr_debug("%s\n", __func__);
	if (acc->on_before_suspend)
		lis3dh_acc_enable(acc);

	return 0;
}
#endif

static int __devinit lis3dh_acc_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct lis3dh_acc_data *acc;
	int err = -1;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "LIS3DH lis3dh_acc_probe:check_functionality failed.\n");
		err = -ENODEV;
		goto exit;
	}

	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		err = -ENODEV;
		goto err_platformdata;
	}

	acc = kzalloc(sizeof(struct lis3dh_acc_data), GFP_KERNEL);
	if (acc == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for module data: "
					"%d\n", err);
		goto err_platformdata;
	}

	mutex_init(&acc->ioctl_lock);
	mutex_init(&acc->lock);
	acc->client = client;
	i2c_set_clientdata(client, acc);

	acc->pdata = kmalloc(sizeof(*acc->pdata), GFP_KERNEL);
	if (acc->pdata == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for pdata: %d\n",
				err);
		goto err_freedata;
	}

	memcpy(acc->pdata, client->dev.platform_data, sizeof(*acc->pdata));

	err = lis3dh_acc_validate_pdata(acc);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto err_kfree_pdata;
	}
	
	/*read the lis3dh chip "who am i" */
	lis3dh_check_device(acc);

	/*lis3dh init the device register value*/
	lis3dh_init_register(acc);

	err = lis3dh_acc_device_power_on(acc);
	if (err < 0) {
		dev_err(&client->dev, "power on failed: %d\n", err);
		goto err_pdata_init;
	}

	atomic_set(&acc->enabled, 1);

	err = lis3dh_acc_update_g_range(acc, acc->pdata->g_range);
	if (err < 0) {
		dev_err(&client->dev, "update_g_range failed\n");
		goto  err_power_off;
	}

	err = lis3dh_acc_update_odr(acc, acc->pdata->poll_interval);
	if (err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto  err_power_off;
	}

	err = lis3dh_acc_input_init(acc);
	if (err < 0) {
		dev_err(&client->dev, "input init failed\n");
		goto err_power_off;
	}

	err = create_sysfs_interfaces(&client->dev);
	if (err < 0) {
		dev_err(&client->dev,
		   "device LIS3DH_ACC_DEV_NAME sysfs register failed\n");
		goto err_input_cleanup;
	}

	lis3dh_acc_device_power_off(acc);

	/* As default, do not report information */
	atomic_set(&acc->enabled, 0);

	acc->misc_device.minor = MISC_DYNAMIC_MINOR; 
	acc->misc_device.name  = "lis3dh";           
	acc->misc_device.fops  = &lis3dh_fops;       
	err = misc_register(&acc->misc_device);
	if (err < 0) {
		pr_err("%s()->%d:can not create misc device!\n",
			__FUNCTION__, __LINE__);
		goto err_remove_sysinterface;
	}

	dev_info(&client->dev, "%s: probed\n", LIS3DH_ACC_DEV_NAME);

#ifdef CONFIG_HAS_EARLYSUSPEND
	acc->early_suspend.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 2; 
	acc->early_suspend.suspend = lis3dh_early_suspend;            
	acc->early_suspend.resume  = lis3dh_late_resume;              
	register_early_suspend(&acc->early_suspend);
#endif

	return 0;

err_remove_sysinterface:
	remove_sysfs_interfaces(&client->dev);
err_input_cleanup:
	lis3dh_acc_input_cleanup(acc);
err_power_off:
	lis3dh_acc_device_power_off(acc);
err_pdata_init:
	if (acc->pdata->exit)
		acc->pdata->exit();
err_kfree_pdata:
	kfree(acc->pdata);
err_freedata:
	kfree(acc);
err_platformdata:
	pr_err("%s: Driver Init failed\n", LIS3DH_ACC_DEV_NAME);
exit:	
	return err;
}

static int __devexit lis3dh_acc_remove(struct i2c_client *client)
{
	struct lis3dh_acc_data *acc = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&acc->early_suspend);
#endif

	lis3dh_acc_input_cleanup(acc);
	lis3dh_acc_device_power_off(acc);
	remove_sysfs_interfaces(&client->dev);

	if (acc->pdata->exit)
		acc->pdata->exit();
	kfree(acc->pdata);
	kfree(acc);

	return 0;
}

static const struct i2c_device_id lis3dh_acc_id[] = { 
	{ LIS3DH_ACC_DEV_NAME, 0 }, 
	{ }, 
};
MODULE_DEVICE_TABLE(i2c, lis3dh_acc_id);

#ifdef CONFIG_PM
static const struct dev_pm_ops lis3dh_pm_ops = {
	.suspend = lis3dh_suspend, 
	.resume  = lis3dh_resume,  
};
#endif

static struct i2c_driver lis3dh_acc_driver = {
	.probe    = lis3dh_acc_probe,               
	.remove   = __devexit_p(lis3dh_acc_remove), 
	.id_table = lis3dh_acc_id,                  

	.driver = {
		.owner = THIS_MODULE,
		.name = LIS3DH_ACC_DEV_NAME,
#ifdef CONFIG_PM
		.pm = &lis3dh_pm_ops,
#endif
	},
};

static int __init lis3dh_acc_init(void)
{
	return i2c_add_driver(&lis3dh_acc_driver);
}

static void __exit lis3dh_acc_exit(void)
{
	i2c_del_driver(&lis3dh_acc_driver);
}

module_init(lis3dh_acc_init);
module_exit(lis3dh_acc_exit);

MODULE_DESCRIPTION("lis3dh digital accelerometer sysfs driver");
MODULE_AUTHOR("Meizu");
MODULE_LICENSE("GPL");

