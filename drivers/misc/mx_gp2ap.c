/*
 *  gp2ap012a00f.c - sharp proximity and  ambient  light  sensor driver
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/sysfs.h>
#include <linux/ctype.h>
#include <linux/hwmon-sysfs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/earlysuspend.h>
#include <asm/uaccess.h>
#include <linux/mx_gp2ap.h>

#include <asm/gpio.h>
#include <plat/gpio-cfg.h>

/* For the new proximity sensor, please define NEW_PS, otherwise, please undefine it */
#define NEW_PS

#ifdef NEW_PS
#define PS_CALIB_VALUE 3	/* the default count value without barrier */
#define PS_NEAR_THRESHOLD 14	/* the count when the barrier is near about 3cm ~ 4cm */
#define PS_FAR_THRESHOLD 8	/* the count when the barrier is far about 4cm ~ 5cm */
#else
#define PS_CALIB_VALUE 6 	/* the default count value without barrier */
#define PS_NEAR_THRESHOLD 40	/* the count when the barrier is near about 3cm ~ 4cm */
#define PS_FAR_THRESHOLD 30	/* the count when the barrier is far about 4cm ~ 5cm */
#endif

#define PS_CALIB_TO_NEAR(calib)	((calib) + (PS_NEAR_THRESHOLD - PS_CALIB_VALUE))
#define PS_NEAR_TO_FAR(near) ((near) - (PS_NEAR_THRESHOLD - PS_FAR_THRESHOLD))

#define PS_FAR_MEASURE_CYCLE	MEASURE_CYCLE_8	/* when the barrier is far, 16 is tested without no ambient disturbance problem */
#define PS_FAR_INTVAL_TIME	INTVAL_TIME_8 /* 16: 3mA, 8: 5mA, 4: 9mA, 0: 28mA */

#define PS_NEAR_MEASURE_CYCLE	MEASURE_CYCLE_0		/* 28ms */
#define PS_NEAR_INTVAL_TIME	INTVAL_TIME_16		/* 3mA */

/*110mA, PS, 327.hHz, not reset*/
#define reg4_data(intval) ((u8)((intval) | LED_CURRENT_110 | INT_SETTING_PS | LED_FREQUENCY_327K))

static u16 read_ps_calibvalue(struct gp2ap_data *gp2ap);
static u8 ps_intval[2] = {PS_NEAR_INTVAL_TIME, PS_FAR_INTVAL_TIME};
static u8 ps_measure_cycles[2] = {PS_NEAR_MEASURE_CYCLE, PS_FAR_MEASURE_CYCLE};
static atomic_t gp2ap_als_start = ATOMIC_INIT(0);

#define __ALS_RANGE_X2   0 
#define __ALS_RANGE_X8   1 
#define __ALS_RANGE_X128 2 

#define __INTVAL_TIME_0 0
#define __INTVAL_TIME_8 1

#define CURRENT_INTVAL_TIME(intval_time) \
	(((intval_time) == __INTVAL_TIME_0) ? INTVAL_TIME_0 : INTVAL_TIME_8)

/*
 * i2c read byte function, if success, return zero, else return negative value
 */
static int gp2ap_i2c_read_byte(struct i2c_client *client, u8 reg, u8 *val)
{
	int ret = 0, retry = I2C_RETRIES;
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags & I2C_M_TEN,	/* write */
			.len = 1,
			.buf = &reg,
		}, {
			.addr = client->addr,
			.flags = (client->flags & I2C_M_RD) | I2C_M_RD,
			.len = 1,
			.buf = val,
		},
	};

	while (retry--) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)
			return 0;

		pr_err("%s: i2c_transfer fail, retry %d\n", __func__, I2C_RETRIES - retry);
		msleep_interruptible(I2C_RETRY_DELAY);
	}

	dev_err(&client->dev, "gp2ap sensor i2c read byte error(ret = %d).\n", ret);

	return ret;
}

/*
 * i2c write byte function, if success, return zero, else return negative value
 */
static int gp2ap_i2c_write_byte(struct i2c_client *client, u8 reg, u8 val)
{
	int ret = 0, retry = I2C_RETRIES;
	struct i2c_msg msg;
	u8 buf[2];

	buf[0] = reg;
	buf[1] = val;
	msg.addr = client->addr;
	msg.flags = 0;	/* write */
	msg.len = 2;
	msg.buf = buf;

	while (retry--) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)
			return 0;

		pr_err("%s: i2c_transfer fail, retry %d\n", __func__, I2C_RETRIES - retry);
		msleep_interruptible(I2C_RETRY_DELAY);
	}

	dev_err(&client->dev, "gp2ap sensor i2c write byte error(ret = %d).\n", ret);

	return ret;
}

static int gp2ap_i2c_read_multibytes(struct i2c_client *client, u8 reg, u8* buf, int count)
{
	int ret = 0, retry = I2C_RETRIES;
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags & I2C_M_TEN,	/* write */
			.len = 1,
			.buf = &reg,
		}, {
			.addr = client->addr,
			.flags = (client->flags & I2C_M_TEN) | I2C_M_RD,
			.len = count,
			.buf = buf,
		},
	};

	while (retry--) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)
			return 0;

		pr_err("%s: i2c_transfer fail, retry %d\n", __func__, I2C_RETRIES - retry);
		msleep_interruptible(I2C_RETRY_DELAY);
	}

	dev_err(&client->dev, "gp2ap sensor i2c read multi bytes error(ret = %d).\n", ret);

	return ret;
}

static int gp2ap_i2c_write_multibytes(struct i2c_client *client, u8 reg, u8 *buf, int count)
{
	int ret = 0, i, retry = I2C_RETRIES;
	struct i2c_msg msg;
	u8 wbuf[count + 1];

	wbuf[0] = reg;
	for (i = 0; i < count; i++)
		wbuf[i + 1] = buf[i];

	msg.addr = client->addr;
	msg.flags = client->flags & I2C_M_TEN;	/*write */
	msg.len = count + 1;
	msg.buf = wbuf;

	while (retry--) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)
			return 0;
		pr_err("%s: i2c_transfer fail, retry %d\n", __func__, I2C_RETRIES - retry);
		msleep_interruptible(I2C_RETRY_DELAY);
	};

	dev_err(&client->dev, "gp2ap sensor i2c write multi bytes error(ret = %d).\n", ret);

	return ret;
}

/*
 * Detect the device,
 * just return read command1 register to see success or not
 */
static int gp2ap_detect(struct i2c_client *client)
{
	u8 dummy;

	return gp2ap_i2c_read_byte(client, REG_COMMAND1, &dummy);
}

static int gp2ap_power_down(struct gp2ap_data *gp2ap)
{
	return gp2ap_i2c_write_byte(gp2ap->client, REG_COMMAND1, 0);
}

/*
 * the write command order is:
 * command2
 * command3
 * command4
 * als_lt_lsb
 * .......
 * command1 last
 */
static int gp2ap_set_als_irq_mode(struct gp2ap_data *gp2ap)
{
	int ret;
	u8 buf[7];

	/* The interrupt happens every 0.8s (100ms * 8) for current setting */
	/* 25ms; default range:x2*/
	buf[0] = ALS_RESOLUTION_14 | gp2ap->CURRENT_ALS_RANGE[gp2ap->current_range];   
	buf[1] = INT_TYPE_PULSE;   /* auto light cancel:off; int type:pulse*/
	buf[2] = CURRENT_INTVAL_TIME(gp2ap->current_intval_time) | INT_SETTING_ALS;   /* ALS int */

	ret = gp2ap_i2c_write_multibytes(gp2ap->client, REG_COMMAND2, buf, 3);
	if (ret < 0) {
		pr_err("%s()->%d: gp2ap_i2c_write_multibytes fail!\n", __func__, __LINE__);
		return ret;
	}

	/* Report all light info */
	buf[0] = 0x3c;	 /* low threshold 60 */
	buf[1] = 0x00;
	buf[2] = 0x3c;	 /* high threshold 60 */
	buf[3] = 0x00;

	ret = gp2ap_i2c_write_multibytes(gp2ap->client, REG_ATS_LLSB, buf, 4);
	if (ret < 0) {
		pr_err("%s()->%d: gp2ap_i2c_write_multibytes fail!\n", __func__, __LINE__);
		return ret;
	}

	buf[0] = SOFTWARE_OPERATION | CONTINUE_OPERATION | OPERATING_MODE_ALS
	       	| ALS_EXTERNAL_CALCULATION;
	
	ret = gp2ap_i2c_write_byte(gp2ap->client, REG_COMMAND1, buf[0]);
	if (ret < 0) {
		pr_err("%s()->%d: gp2ap_i2c_write_byte fail!\n", __func__, __LINE__);
		return ret;
	}

	return 0;
}

/*
 * Should update thresholds for the first setting and while the calib value
 * re-calculated
 */
static void get_ps_thresholds(struct gp2ap_data *gp2ap)
{
	u16 ps_far_threshold, ps_near_threshold;

	mutex_lock(&gp2ap->lock);
	if (gp2ap->init_threshold_flag) {
		gp2ap->ps_near_threshold = PS_CALIB_TO_NEAR(read_ps_calibvalue(gp2ap));
		gp2ap->ps_far_threshold = PS_NEAR_TO_FAR(gp2ap->ps_near_threshold);
		gp2ap->init_threshold_flag = 0;
	}
	ps_near_threshold = gp2ap->ps_near_threshold;
	ps_far_threshold = gp2ap->ps_far_threshold;
	mutex_unlock(&gp2ap->lock);

	pr_info("%s: ps_near_threshold = %d, ps_far_threshold = %d\n",
		__func__, ps_near_threshold, ps_far_threshold);

	gp2ap->ps_thresholds[0] = LSB(ps_far_threshold);	/* low threshold */
	gp2ap->ps_thresholds[1] = MSB(ps_far_threshold);
	gp2ap->ps_thresholds[2] = LSB(ps_near_threshold);	/* high threshold */
	gp2ap->ps_thresholds[3] = MSB(ps_near_threshold);
}

static int gp2ap_set_ps_mode(struct gp2ap_data *gp2ap, int mode)
{
	int ret;
	int ps_status = gp2ap->ps_data;
	u8 buf[4];

	pr_debug("%s: PS: %s, Measure cycles: %x, Measure Intval: %x\n",
		__func__, ps_status ? "FAR" : "NEAR",
		ps_measure_cycles[ps_status], ps_intval[ps_status]);

	buf[0] = ps_measure_cycles[ps_status];
	buf[1] = INT_TYPE_PULSE | PS_RESOLUTION_10 | PS_RANGE_X4;
	buf[2] = reg4_data(ps_intval[ps_status]);

	ret = gp2ap_i2c_write_multibytes(gp2ap->client, REG_COMMAND2, buf, 3);
	if (ret < 0) {
		pr_err("%s()->%d: gp2ap_i2c_write_multibytes fail!\n", __func__, __LINE__);
		return ret;
	}

	if (PS_IRQ_MODE == mode) {
		if (unlikely(gp2ap->reset_threshold_flag)) {
			get_ps_thresholds(gp2ap);
			gp2ap->reset_threshold_flag = 0;
		}
		ret = gp2ap_i2c_write_multibytes(gp2ap->client, REG_PTS_LLSB, gp2ap->ps_thresholds, 4);
		if (ret < 0) {
			pr_err("%s()->%d: gp2ap_i2c_write_multibytes fail!\n", __func__, __LINE__);
			return ret;
		}
	}

	buf[0] = SOFTWARE_OPERATION | OPERATING_MODE_PS;
	switch (mode) {
	case PS_IRQ_MODE:
		buf[0] |= CONTINUE_OPERATION;
		break;
	case PS_SHUTDOWN_MODE:
	default:
		buf[0] |= AUTO_SHUTDOWN;
		break;
	}
	ret = gp2ap_i2c_write_byte(gp2ap->client, REG_COMMAND1, buf[0]);
	if (ret < 0) {
		pr_err("%s()->%d: gp2ap_i2c_write_byte fail!\n", __func__, __LINE__);
		return ret;
	}

#if 0
	if (PS_SHUTDOWN_MODE == mode) {
		/* 10bit, 4 cycle: 1.56msec × 2 x 8 = 24.96
		 * wait for mearsuing about > 24.96
		 */
		msleep_interruptible(50);
	}
#endif

	return 0;
}

static int gp2ap_set_ps_irq_mode(struct gp2ap_data *gp2ap)
{
	gp2ap_set_ps_mode(gp2ap, PS_IRQ_MODE);

	return 0;
}

static int gp2ap_set_ps_powerdown_mode(struct gp2ap_data *gp2ap, int current_status)
{
	/* Note: Auto shutdown mode only measure one time, measure cycles and
	 * intval doesn't take effect
	 */
	gp2ap_set_ps_mode(gp2ap, PS_SHUTDOWN_MODE);

	return 0;
}

static int gp2ap_set_debug_mode(struct gp2ap_data *gp2ap, int current_status)
{
	int ret;
	u8 buf[4];
	int ps_status = gp2ap->ps_data;

	buf[0] = ps_measure_cycles[ps_status];
	buf[1] = PS_RESOLUTION_10 | PS_RANGE_X4;
	buf[2] = reg4_data(ps_intval[ps_status]);

	ret = gp2ap_i2c_write_multibytes(gp2ap->client, REG_COMMAND2, buf, 3);
	if (ret < 0) {
		pr_err("%s()->%d: gp2ap_i2c_write_multibytes fail!\n", __func__, __LINE__);
		return ret;
	}

	buf[0] = SOFTWARE_OPERATION | AUTO_SHUTDOWN | OPERATING_MODE_DEBUG;
	ret = gp2ap_i2c_write_byte(gp2ap->client, REG_COMMAND1, buf[0]);
	if (ret < 0) {
		pr_err("%s()->%d: gp2ap_i2c_write_byte fail!\n", __func__, __LINE__);
		return ret;
	}

	/* 10bit, 4 cycle: 1.56msec × 2 x 4 x intval_times= 12.5 x 16
	 * wait for mearsuing about > 12.5
	 */
	msleep_interruptible(50);

	return 0;
}

/* The initial status is far */
static void gp2ap_ps_report_far(struct input_dev *idp)
{
	input_report_abs(idp, ABS_PS, PS_FAR);
	input_sync(idp);
}

static u16 gp2ap_get_ps_data(struct gp2ap_data *gp2ap)
{
	int ret;
	u16 sub_value;
	u8 rbuf[2];

	ret = gp2ap_i2c_read_multibytes(gp2ap->client, REG_D2_LSB, rbuf, 2);
	if (ret < 0) {
		pr_err("%s()->%d:gp2ap_i2c_read_multibytes!\n",
			__func__, __LINE__);
		return ret;
	}
	sub_value = rbuf[1] * 256 + rbuf[0];

	return sub_value;
}

static int gp2ap_start_work(struct gp2ap_data *gp2ap, int enabled_sensors)
{
	int ret = 0;

	if (enabled_sensors & ID_PS) {
		enable_irq(gp2ap->irq);   /*enable irq first*/
		ret = gp2ap_set_ps_irq_mode(gp2ap);
		if (ret < 0) {
			disable_irq_nosync(gp2ap->irq);
			pr_err("%s()->%d:set gp2ap ps irq mode fail !\n",
				__func__, __LINE__);
			return ret;
		}
		/* Init it as FAR when start, this is required for the phone call. */
		gp2ap_ps_report_far(gp2ap->input_dev);
		gp2ap->ps_data = PS_FAR;
	} else if (enabled_sensors & ID_ALS) {
		gp2ap->current_range = __ALS_RANGE_X8;
		gp2ap->current_intval_time = __INTVAL_TIME_0;
		enable_irq(gp2ap->irq);   /*enable irq first*/
		ret = gp2ap_set_als_irq_mode(gp2ap);
		if (ret < 0) {
			disable_irq_nosync(gp2ap->irq);
			pr_err("%s()->%d:set gp2ap als mode fail !\n",
				__func__, __LINE__);
			return ret;
		}
		/* Ensure the 1st value of a new start is always reported. */
		atomic_set(&gp2ap_als_start, 1);
	} else {
		pr_err("%s()->%d:unknown enabled sensor(%d)!\n",
			__func__, __LINE__, gp2ap->enabled_sensors);
		return -EINVAL;
	}

	return 0;
}

static void gp2ap_stop_work(struct gp2ap_data *gp2ap)
{
	disable_irq(gp2ap->irq);    /*don't use disable_irq_nosync() here*/

	gp2ap_power_down(gp2ap);
}

static int gp2ap_set_enable(struct gp2ap_data *gp2ap,
						int sensor_id,
						int enable)
{
	int old_enabled_sensors, to_be_enabled_sensors, als_to_ps, ps_to_als;
	int ret = 0;

	/* check the id flag */
	if ((sensor_id != ID_ALS) && (sensor_id != ID_PS)) {
		pr_err("%s()->%d: uninvalid sensor id value %d!\n",
			__func__, __LINE__, sensor_id);
		ret = -EINVAL;
		goto out;
	}

	/*now we get new enabled sensors mask*/
	to_be_enabled_sensors = old_enabled_sensors = gp2ap->enabled_sensors;
	if (enable)
		to_be_enabled_sensors |= sensor_id;
	else
		to_be_enabled_sensors &= ~sensor_id;

	pr_info("%s(): old enabled_sensors is %d, new is %d.\n", __func__,
		old_enabled_sensors, to_be_enabled_sensors);

	if (old_enabled_sensors == to_be_enabled_sensors) {
		pr_info("%s sensor has been %s.\n",
			(sensor_id == ID_ALS) ? "als" : "ps",
			enable ? "enabled" : "disabled");
		goto out;
	}
	gp2ap->enabled_sensors = to_be_enabled_sensors;

	ps_to_als = (sensor_id == ID_ALS) && (old_enabled_sensors & ID_PS);
	if (ps_to_als) {
		/* Ignore ALS operations when PS is on */
		goto out;
	}

	als_to_ps = (sensor_id == ID_PS) && (old_enabled_sensors & ID_ALS);

	if (!to_be_enabled_sensors || als_to_ps) {
		gp2ap_stop_work(gp2ap);
	}

	gp2ap->sensor_id = sensor_id;
	if (to_be_enabled_sensors) {
		ret = gp2ap_start_work(gp2ap, to_be_enabled_sensors);
		if (ret < 0)
			pr_err("%s: gp2ap_start_work fail!\n", __func__);
		else
			gp2ap->enabled_sensors = to_be_enabled_sensors;
	}
out:
	return ret;
}

static ssize_t gp2ap_als_enable_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2ap_data *gp2ap = i2c_get_clientdata(client);
	int enabled;

	enabled = !!(gp2ap->enabled_sensors & ID_ALS);

	return sprintf(buf, "%d\n", enabled);
}

static ssize_t gp2ap_als_enable_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2ap_data *gp2ap = i2c_get_clientdata(client);
	int enable = simple_strtol(buf, NULL, 10);
	int ret;

	ret = gp2ap_set_enable(gp2ap, ID_ALS, enable);
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t gp2ap_ps_enable_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2ap_data *gp2ap = i2c_get_clientdata(client);
	int enabled;

	enabled = !!(gp2ap->enabled_sensors & ID_PS);

	return sprintf(buf, "%d\n", enabled);
}

static ssize_t gp2ap_ps_enable_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2ap_data *gp2ap = i2c_get_clientdata(client);
	int enable = simple_strtol(buf, NULL, 10);
	int ret;

	ret = gp2ap_set_enable(gp2ap, ID_PS, enable);
	if (ret < 0)
		return ret;
	pr_info("%s: %d\n", __func__, count);

	return count;
}

static ssize_t gp2ap_report_time_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	int report_time;

	/*the intermittent time = RESOLUTION * INTVAL_TIME 
	 ALS_RESOLUTION_14 refer to 25ms.*/
	
	report_time = ALS_DELAYTIME + (25 * 8); 
	
	return sprintf(buf, "%d\n", report_time);
}

static ssize_t gp2ap_als_data_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2ap_data *gp2ap = i2c_get_clientdata(client);

	if (!(gp2ap->enabled_sensors & ID_ALS)) {
		pr_err("%s()->%d:the als mode is not set!\n",
			__func__, __LINE__);

		return -EINVAL;
	}

	pr_info("als data0 is %d  : als data1 is %d \n", gp2ap->als_data[0],
			gp2ap->als_data[1]);

	return sprintf(buf, "%d  %d\n", gp2ap->als_data[0], gp2ap->als_data[1]);
}

static ssize_t gp2ap_ps_data_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2ap_data *gp2ap = i2c_get_clientdata(client);
	int ret, ps_data;
	u8 rbuf[2];

	if (!(gp2ap->enabled_sensors & ID_PS)) {
		pr_err("%s()->%d:the ps mode is not set!\n",
			__func__, __LINE__);

		return -EINVAL;
	}

	ret = gp2ap_i2c_read_multibytes(gp2ap->client, REG_D2_LSB, rbuf, 2);
	if (ret < 0) {
		pr_err("%s()->%d:read REG_PS_D2_LSB reg fail!\n",
			__func__, __LINE__);
		return ret;
	}
	ps_data = (rbuf[1] << 8) | rbuf[0];

	pr_info("ps data is %d.\n", ps_data);

	return sprintf(buf, "%d\n", ps_data);
}

static ssize_t gp2ap_debug_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	int debug = simple_strtol(buf, NULL, 10);
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2ap_data *gp2ap = i2c_get_clientdata(client);

	pr_info("%s():set debug %d.\n", __func__, debug);

	if (debug != gp2ap->debug)
		gp2ap->debug = debug;

	return count;
}

static ssize_t gp2ap_ps_debug_data_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2ap_data *gp2ap = i2c_get_clientdata(client);
	int ret;
	int off_value, on_value, sub_value;
	u8 rbuf[2];
	static int ps_pre_status = PS_FAR;

	if (gp2ap->enabled_sensors != 0)
		gp2ap_stop_work(gp2ap);

	ret = gp2ap_set_debug_mode(gp2ap, ps_pre_status);
	if (ret < 0) {
		pr_err("%s()->%d:gp2ap_set_debug_mode fail!\n",
			__func__, __LINE__);
		return ret;
	}

	ret = gp2ap_i2c_read_multibytes(gp2ap->client, REG_D1_LSB, rbuf, 2);
	if (ret < 0) {
		pr_err("%s()->%d:gp2ap_i2c_read_multibytes!\n",
			__func__, __LINE__);
		return ret;
	}
	off_value = rbuf[1] * 256 + rbuf[0];

	ret = gp2ap_i2c_read_multibytes(gp2ap->client, REG_D2_LSB, rbuf, 2);
	if (ret < 0) {
		pr_err("%s()->%d:gp2ap_i2c_read_multibytes!\n",
			__func__, __LINE__);
		return ret;
	}
	on_value = rbuf[1] * 256 + rbuf[0];

	ret = gp2ap_set_ps_powerdown_mode(gp2ap, ps_pre_status);
	if (ret < 0) {
		pr_err("%s()->%d:gp2ap_set_powerdown_mode fail!\n",
			__func__, __LINE__);
		return ret;
	}

	ret = gp2ap_i2c_read_multibytes(gp2ap->client, REG_D2_LSB, rbuf, 2);
	if (ret < 0) {
		pr_err("%s()->%d:gp2ap_i2c_read_multibytes!\n",
			__func__, __LINE__);
		return ret;
	}
	sub_value = rbuf[1] * 256 + rbuf[0];
	if (sub_value >= gp2ap->ps_near_threshold) {
		/* near: sub_value becomes bigger */
		ps_pre_status = PS_NEAR;
	} else if (sub_value <= gp2ap->ps_far_threshold) {
		/* far: sub_value becomes smaller */
		ps_pre_status = PS_FAR;
	}

	pr_info("off value %d, on value %d, sub value %d.\n",
		off_value, on_value, sub_value);

	gp2ap_power_down(gp2ap);
	if (gp2ap->enabled_sensors != 0)
		gp2ap_start_work(gp2ap, gp2ap->enabled_sensors);

	return sprintf(buf, "%d %d %d\n", off_value, on_value, sub_value);
}

static ssize_t gp2ap_ReflectData_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2ap_data *gp2ap = i2c_get_clientdata(client);
	u16 reg_data = 0;

	gp2ap_set_ps_powerdown_mode(gp2ap, PS_FAR);

	reg_data = gp2ap_get_ps_data(gp2ap);
	if (reg_data < 0) {
		pr_err("%s: get ps data error!(ret = %d)\n", __func__, reg_data);
		reg_data = -1;
	}

	return sprintf(buf, "%u\n", reg_data);
}

static inline u16 __read_ps_calibvalue(void)
{
	struct file *fp;
	char buf[256];
	loff_t  pos = 0;
	ssize_t rb;
	u16 calib_value = PS_CALIB_VALUE;

	fp = filp_open(PROXIMITY_CALIB_FILE, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("open %s error, err = %ld!\n", PROXIMITY_CALIB_FILE, PTR_ERR(fp));
	} else {
		rb = kernel_read(fp, pos, buf, sizeof(buf));
		if (rb > 0) {
			buf[rb] = '\0';
			calib_value = simple_strtol(buf, NULL, 10);
			pr_debug("in read_prox_calibvalue: rb is %d, read buf is %s, calib_value is %d\n", rb, buf, calib_value);
		} else {
			pr_err("read %s file error!, rb is %d\n", PROXIMITY_CALIB_FILE, rb);
		}
		filp_close(fp, NULL);
	}

	return calib_value;
}

static u16 read_ps_calibvalue(struct gp2ap_data *gp2ap)
{
	u16 ps_calib_value;

	if (likely(gp2ap->calib_value_readed))
		ps_calib_value = gp2ap->ps_calib_value;
	else {
		ps_calib_value = __read_ps_calibvalue();
		gp2ap->calib_value_readed = 1;
	}
	return ps_calib_value;
}

static ssize_t gp2ap_threshold_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2ap_data *gp2ap = i2c_get_clientdata(client);

	mutex_lock(&gp2ap->lock);
	gp2ap->ps_near_threshold = PS_CALIB_TO_NEAR(read_ps_calibvalue(gp2ap));
	mutex_unlock(&gp2ap->lock);

	return sprintf(buf, "%d\n", gp2ap->ps_near_threshold);
}

static ssize_t gp2ap_CalibValue_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2ap_data *gp2ap = i2c_get_clientdata(client);

	mutex_lock(&gp2ap->lock);
	gp2ap->ps_calib_value = read_ps_calibvalue(gp2ap);
	mutex_unlock(&gp2ap->lock);

	return sprintf(buf, "%d\n", gp2ap->ps_calib_value);
}

static ssize_t gp2ap_CalibValue_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2ap_data *gp2ap = i2c_get_clientdata(client);
	u16 value = simple_strtoul(buf, NULL, 10);

	mutex_lock(&gp2ap->lock);
	gp2ap->ps_calib_value = value;
	gp2ap->ps_near_threshold = PS_CALIB_TO_NEAR(value);
	gp2ap->ps_far_threshold = PS_NEAR_TO_FAR(gp2ap->ps_near_threshold);
	/* should reset the thresholds while setting ps irq mode */
	gp2ap->reset_threshold_flag = 1;
	mutex_unlock(&gp2ap->lock);

	pr_info("gp2ap proximity threshold is set to %d.\n", gp2ap->ps_near_threshold);

	return count;
}

/* sysfs attributes operation function*/
static DEVICE_ATTR(als_enable, 0664,
	gp2ap_als_enable_show, gp2ap_als_enable_store);
static DEVICE_ATTR(ps_enable, 0664,
	gp2ap_ps_enable_show, gp2ap_ps_enable_store);
static DEVICE_ATTR(report_time, 0664,
	gp2ap_report_time_show, NULL);
static DEVICE_ATTR(als_data, S_IRUGO,
	gp2ap_als_data_show, NULL);
static DEVICE_ATTR(ps_data, S_IRUGO,
	gp2ap_ps_data_show, NULL);
static DEVICE_ATTR(debug, S_IWUSR,
	NULL, gp2ap_debug_store);
static DEVICE_ATTR(ps_debug_data, S_IRUGO,
	gp2ap_ps_debug_data_show, NULL);
static DEVICE_ATTR(CalibValue, 0664, gp2ap_CalibValue_show, gp2ap_CalibValue_store);
static DEVICE_ATTR(ReflectData, S_IRUGO, gp2ap_ReflectData_show, NULL);
static DEVICE_ATTR(threshold, S_IRUGO, gp2ap_threshold_show, NULL);

static struct attribute *gp2ap_attributes[] = {
	&dev_attr_als_enable.attr,
	&dev_attr_report_time.attr,
	&dev_attr_als_data.attr,
	&dev_attr_ps_enable.attr,
	&dev_attr_ps_data.attr,
	&dev_attr_debug.attr,
	&dev_attr_ps_debug_data.attr,
	&dev_attr_CalibValue.attr,
	&dev_attr_ReflectData.attr,
	&dev_attr_threshold.attr,
	NULL,
};

static struct attribute_group gp2ap_attribute_group = {
	.attrs = gp2ap_attributes,
};

static unsigned long calculate_light_range8_lux(unsigned long d0,
		unsigned long d1)
{
	int a = 0, b = 0;
	 /* c = range@x8/  (2 ^ (14bits - 11) = 8 / (2^(14-11) = 8 /8 =1 */
	int c = 1;
	unsigned long lux;
	static unsigned long prev_lux;
	if (d0 == 0) {
		lux = 0;
	} else {
		if (d1 * 100 <= d0 * 98) {
			if (d1 * 100 <= d0 * 70) {
				a = 9091;
				b = 0;
				lux = c * ((a * d0) - (b * d1));
				lux = lux / 1000;
			} else if (d1 * 100 <= d0 * 75) {
				a = 1329;
				b = 1769;
				lux = c * ((a * d0) - (b * d1));
				lux = lux / 10;
			} else {
				a = 1030;
				b = 1051;
				lux = c * ((a * d0) - (b * d1));
				lux = lux / 1000;
			}
			prev_lux = lux;
		} else {
			/* Use the previous normal one while r > 0.98 for this condition
			 * means the detection value is wrong
			 */
			lux = prev_lux;
		}
	}

	return lux;
}

#define Factor_C 25
static unsigned long calculate_light_range2_lux(unsigned long  d0,
		unsigned long d1)
{
	int a = 0, b = 0;
	/* c = range@x8/  (2 ^ (14bits - 11) = 8 / (2^(14-11) = 8 /8 =1 */
	int c = 25 / Factor_C; 
	unsigned long lux;
	static unsigned long prev_lux;
	
	if (d0 == 0) {
		lux = 0;
	} else {
		if (d1 * 100 <= d0 * 98) {
			if (d1 * 100 <= d0 * 70) {
				a = 9091;
				b = 0;
				lux = c * ((a * d0) - (b * d1));
				lux = lux / (100000 / Factor_C);
			} else if (d1 * 100 <= d0 * 75) {
				a = 1329;
				b = 1769;
				lux = c * ((a * d0) - (b * d1));
				lux = lux / (1000 / Factor_C);
			} else {
				a = 1030;
				b = 1051;
				lux = c * ((a * d0) - (b * d1));
				lux = lux / (100000 / Factor_C);
			}
			prev_lux = lux;
		} else {
			/* Use the previous normal one while r > 0.98 for 
			   this condition means the detection value is wrong
			 */
			lux = prev_lux;
		}
	}
	return lux;
}

static unsigned long calculate_light_high_lux(unsigned long d0,
		unsigned long d1)
{
	int a = 0, b = 0;
	 /* c = range@x128 /  (2 ^ (14bits - 11) = 128 / (2^(14-11) = 128 /8 =16 */
	int c = 16;
	unsigned long lux;
	static unsigned long prev_lux;

	if (d0 == 0) {
		lux = 0;
	} else {
		if (d1 * 100 <= d0 * 98) {
			if (d1 * 100 <= d0 * 70) {
				a = 9091;
				b = 0;
				lux = c * ((a * d0) - (b * d1));
				lux = lux / 1000;
			} else if (d1 * 100 <= d0 * 75) {
				a = 1329;
				b = 1769;
				lux = c * ((a * d0) - (b * d1));
				lux = lux / 10;
			} else{
				a = 165;
				b = 33;
				lux = c * ((a * d0) - (b * d1));
				lux = lux / 1000;
			}
		prev_lux = lux;
		} else {
			/* Use the previous normal one while r > 0.98 for
			   this condition  means the detection value is wrong
			 */
			lux = prev_lux;
		}
	}
	
	return lux;
}

static void gp2ap_als_reset(struct gp2ap_data *gp2ap)
{
	int ret = 0;

	gp2ap_stop_work(gp2ap);
	enable_irq(gp2ap->irq);
	
	ret = gp2ap_set_als_irq_mode(gp2ap);
	if (ret < 0) {
		disable_irq_nosync(gp2ap->irq);
		pr_err("%s()->%d:set gp2ap als mode fail !\n",
			__func__, __LINE__);
		return;
	}
	/* Ensure the 1st value of a new start is always reported. */
	atomic_set(&gp2ap_als_start, 1);
}

/*
 * gp2ap ALS delayed work function
 */
static void gp2ap_als_dwork_func(struct work_struct *work)
{
	struct gp2ap_data *gp2ap = container_of((struct delayed_work *)work, 
			struct gp2ap_data, als_dwork);
	struct i2c_client *client = gp2ap->client;
	struct input_dev *input_dev = gp2ap->input_dev;
	u8 buf[2], buf1[2];
	unsigned long data0, data1;
        int ret;
	unsigned long light_lux;
	bool gp2ap_reset_als = 0;

	ret = gp2ap_i2c_read_multibytes(client, REG_D0_LSB, buf, 2);
	if (ret < 0) {
		pr_err("%s()->%d:read REG_ALS_D0_LSB reg fail!\n",
			__func__, __LINE__);
		return;
	}

	ret = gp2ap_i2c_read_multibytes(client, REG_D1_LSB, buf1, 2);
	if (ret < 0) {
		pr_err("%s()->%d:read REG_ALS_D1_LSB reg fail!\n",
			__func__, __LINE__);
		return;
	}

	data0 = (buf[1] << 8) | buf[0];  /*CLEAR*/
	data1 = (buf1[1] << 8) | buf1[0]; /*IR*/

#ifdef DEBUG
	if (gp2ap->debug)
		pr_info("als data0 is %d, data1 is %d.\n", data0,data1);
#endif

	gp2ap->prev_range = gp2ap->current_range;

	/* When the current range is X8 and data0 is less than 16000,or 
	   when the current range is X128 and data0 is less than 1000, 
	   belonging to the low illumination.Accordingly,it is a high illumination
	 */
	if((gp2ap->current_range == __ALS_RANGE_X2)/*LOW MODE*/ 
			&& (data0 >= 200
			       	&& (data0 <= 16383 && data1 <=16383))){
		gp2ap->current_range = __ALS_RANGE_X8;
		gp2ap_reset_als =1;
	}else if((gp2ap->current_range == __ALS_RANGE_X8) 
			&& data0 <= 50){
		gp2ap->current_range = __ALS_RANGE_X2;
		gp2ap_reset_als = 1;	
	}else if(gp2ap->current_range == __ALS_RANGE_X128 
			&& (data0 < 1000)){
			gp2ap->current_range = __ALS_RANGE_X8;
			gp2ap_reset_als = 1;
	}else if((gp2ap->current_range == __ALS_RANGE_X8)/*HIGH MODE*/
			&& (data0 >= 16000) 
			&& (data0 <= 16383 && data1 <= 16383)){
		gp2ap->current_range = __ALS_RANGE_X128;	
		gp2ap_reset_als = 1;
	}else{
		pr_debug("do nothing\n");		
	}

	/*according to the current range, calculate the report light value*/	
	if(unlikely(data0 > 16383 || data1 > 16383)){
		light_lux = 16383;
		pr_debug("the gp2ap sensor detect the light value is overflow\n");
	}else{
		if(gp2ap->current_range == __ALS_RANGE_X2){
			light_lux = calculate_light_range2_lux(data0, data1);
			if(gp2ap->prev_range == __ALS_RANGE_X8){
				light_lux = light_lux * 4;
			}
		}else if(gp2ap->current_range == __ALS_RANGE_X8){
				light_lux = calculate_light_range8_lux(data0, data1);
				if(gp2ap->prev_range == __ALS_RANGE_X2){
					light_lux = light_lux / 4;
				}else if(gp2ap->prev_range == __ALS_RANGE_X128){
					light_lux = light_lux * 16;
				}
		} else {
			light_lux = calculate_light_high_lux(data0, data1);
			if(gp2ap->prev_range == __ALS_RANGE_X8){
				light_lux = light_lux / 16;
			}
		}
		gp2ap->prev_lux = light_lux;
	}

	/* If the consecutive two are the same, the value will not be reported,
	 * so, force to generate a difference.
	 */
	if (atomic_read(&gp2ap_als_start)) {
		light_lux += 1;
		atomic_set(&gp2ap_als_start, 0);
	}

	input_report_abs(input_dev, ABS_ALS, light_lux);
	input_sync(input_dev);

	gp2ap->als_data[0] = data0;
	gp2ap->als_data[1] = data1;
	
	pr_debug("light_lux is %ld, data0 is %ld, data1 is %ld,range is %d\n",
			light_lux, data0,data1, gp2ap->current_range);

	/*when the gp2ap als enable the first time, set the intval_time is 0
	  but after set the intval_time as 8 times,so the intermittent 
	  operation is 25ms * 8 = 200ms; 
	 */
	if(gp2ap->current_intval_time == __INTVAL_TIME_0) {
		gp2ap->current_intval_time = __INTVAL_TIME_8;
		gp2ap_reset_als = 1;
	}

	if (gp2ap_reset_als) {
		gp2ap_als_reset(gp2ap);
	}
}

/*
 * gp2ap PS interrupt handler work function
 */
static void gp2ap_ps_handler(struct gp2ap_data *gp2ap)
{
	struct i2c_client *client = gp2ap->client;
	struct input_dev *idp = gp2ap->input_dev;
	u8 command1 = 0;   /*command1 for write back*/
	int ret, ps_data;

	mutex_lock(&gp2ap->lock);

	ret = gp2ap_i2c_read_byte(client, REG_COMMAND1, &command1);
	if (ret < 0) {
		pr_err("%s()->%d:read command1 reg fail!\n",
			__func__, __LINE__);
		mutex_unlock(&gp2ap->lock);
		return;
	}

	ps_data = !(command1 & PS_DETECTION_MASK);

	/* Reset the measure cycle and intval when switch between NEAR and FAR
	 *
	 * Note: Must power off before resetting, otherwise, the chip will not
	 * function as expected.
	 */
	if (gp2ap->ps_data != ps_data) {
		gp2ap_power_down(gp2ap);
		/* Only report for the changes */
		input_report_abs(idp, ABS_PS, ps_data);
		input_sync(idp);
		gp2ap->ps_data = ps_data;
		gp2ap_set_ps_irq_mode(gp2ap);

		if (is_far(ps_data))
			pr_info("*************far*************\n");
		else
			pr_info("*************near*************\n");
	}

	/*if user debug, to read ps data*/
	if (gp2ap->debug) {
		u8 buf[2];
		int ps_debug_data;

		ret = gp2ap_i2c_read_multibytes(client, REG_D2_LSB, buf, 2);
		if (ret < 0) {
			pr_err("%s()->%d:read REG_ALS_D2_LSB reg fail!\n",
				__func__, __LINE__);
		} else {
			ps_debug_data = buf[1] * 256 + buf[0];
			pr_info("%s():ps adc data is %d.\n", __func__, ps_debug_data);
		}
	}
	mutex_unlock(&gp2ap->lock);
}

static irqreturn_t gp2ap_irq_handler(int irq, void *dev_id)
{
	struct gp2ap_data *gp2ap = (struct gp2ap_data  *)dev_id;
	struct i2c_client *client = gp2ap->client;
	int enabled_sensors = gp2ap->enabled_sensors;
	int ret = 0;
	u8 data;

	/* Clear interrupts */
	ret = gp2ap_i2c_read_byte(client, REG_COMMAND1, &data);
	if (ret < 0) {
		pr_err("%s: gp2ap_i2c_read_byte Failed\n", __func__);
		return IRQ_NONE;
	}
	data &= (~ALS_INTERRUPT_MASK & ~PS_INTERRUPT_MASK);
	ret = gp2ap_i2c_write_byte(client, REG_COMMAND1, data);
	if (ret < 0) {
		pr_err("%s: gp2ap_i2c_write_byte Failed!\n", __func__);
		return IRQ_NONE;
	}

	if (enabled_sensors & ID_PS) {
		pr_debug("%s: ********** ps ***********\n", __func__);
		gp2ap_ps_handler(gp2ap);
	} else {
		pr_debug("%s: ********** als ***********\n", __func__);
		queue_delayed_work(gp2ap->als_wq, &gp2ap->als_dwork, msecs_to_jiffies(ALS_DELAYTIME));
	}

	return IRQ_HANDLED;
}

static int gp2ap_create_input(struct gp2ap_data *gp2ap)
{
	int ret;
	struct input_dev *dev;

	dev = input_allocate_device();
	if (!dev) {
		pr_err("%s()->%d:can not alloc memory to gp2ap input device!\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	set_bit(EV_ABS, dev->evbit);
	input_set_capability(dev, EV_ABS, ABS_ALS);
	input_set_abs_params(dev, ABS_ALS, 0, 65535, 0, 0);  /*the max value 16bit */
	
	set_bit(EV_ABS, dev->evbit);
	input_set_capability(dev, EV_ABS, ABS_PS);
	input_set_abs_params(dev, ABS_PS, 0, 1, 0, 0);  /*the max value 1bit*/
	dev->name = "gp2ap";
	dev->dev.parent = &gp2ap->client->dev;

	ret = input_register_device(dev);
	if (ret < 0) {
		pr_err("%s()->%d:can not register gp2ap input device!\n",
			__func__, __LINE__);
		input_free_device(dev);
		return ret;
	}

	gp2ap->input_dev = dev;
	input_set_drvdata(gp2ap->input_dev, gp2ap);

	return 0;
}

static void gp2ap_free_input(struct gp2ap_data *gp2ap)
{
	input_unregister_device(gp2ap->input_dev);
	input_free_device(gp2ap->input_dev);
}

/*
 * gp2ap misc device file operation functions inplement
 */
static int gp2ap_misc_open(struct inode *inode, struct file *file)
{
	struct gp2ap_data *gp2ap = container_of((struct miscdevice *)file->private_data,
							struct gp2ap_data,
							misc_device);

	if (atomic_xchg(&gp2ap->opened, 1)) {
		pr_err("%s()->%d:request gp2ap private data error!\n",
			__func__, __LINE__);
		return -EBUSY;
	}

	file->private_data = gp2ap;
	atomic_set(&gp2ap->opened, 1);

	return 0;
}

static int gp2ap_misc_close(struct inode *inode, struct file *file)
{
	struct gp2ap_data *gp2ap = file->private_data;

	atomic_set(&gp2ap->opened, 0);

	return 0;
}

static long gp2ap_misc_ioctl_int(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret, enable, sensor_id;
	struct gp2ap_data *gp2ap = (struct gp2ap_data *)file->private_data;

	switch (cmd) {
	case GP2AP_IOCTL_SET_ALS_ENABLE:
	case GP2AP_IOCTL_SET_PS_ENABLE:
		ret = copy_from_user(&enable, (void __user *)arg, sizeof(int));
		if (ret) {
			pr_err("%s()->%d:copy enable operation error!\n",
				__func__, __LINE__);
			return -EINVAL;
		}

		sensor_id = (cmd == GP2AP_IOCTL_SET_ALS_ENABLE) ? ID_ALS : ID_PS;
		gp2ap_set_enable(gp2ap, sensor_id, enable);

		break;
	case GP2AP_IOCTL_GET_ALS_ENABLE:
	case GP2AP_IOCTL_GET_PS_ENABLE:
		sensor_id = (cmd == GP2AP_IOCTL_GET_ALS_ENABLE) ? ID_ALS : ID_PS;
		enable = !!(gp2ap->enabled_sensors & sensor_id);

		ret = copy_to_user((void __user *)arg, &enable, sizeof(int));
		if (ret) {
			pr_err("%s()->%d:copy enable operation error!\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static long gp2ap_misc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;
	struct gp2ap_data *gp2ap = (struct gp2ap_data *)file->private_data;
	mutex_lock(&gp2ap->ioctl_lock);
	ret = gp2ap_misc_ioctl_int(file, cmd, arg);
	mutex_unlock(&gp2ap->ioctl_lock);
	return ret;
}
/*
 * gp2ap misc device file operation
 * here just need to define open, release and ioctl functions.
 */
struct file_operations const gp2ap_fops = {
	.owner = THIS_MODULE,
	.open = gp2ap_misc_open,
	.release = gp2ap_misc_close,
	.unlocked_ioctl = gp2ap_misc_ioctl,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void gp2ap_early_suspend(struct early_suspend *h)
{
	struct gp2ap_data *gp2ap = container_of(h, struct gp2ap_data, early_suspend);

	pr_info("%s: enabled_sensors = %x\n", __func__, gp2ap->enabled_sensors);

	if (gp2ap->enabled_sensors & ID_PS) {
		enable_irq_wake(gp2ap->irq);
		gp2ap->irq_wake_enabled = 1;
	} else {
		cancel_delayed_work_sync(&gp2ap->als_dwork);
	}
}

static void gp2ap_late_resume(struct early_suspend *h)
{
	struct gp2ap_data *gp2ap = container_of(h, struct gp2ap_data, early_suspend);

	if (gp2ap->irq_wake_enabled) {
		disable_irq_wake(gp2ap->irq);
		gp2ap->irq_wake_enabled = 0;
	}
}
#endif

static int __devinit gp2ap_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	struct gp2ap_data *gp2ap;

	pr_info("%s(): address 0x%02x, id %s.\n", __func__, client->addr, id->name);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s()->%d:i2c adapter don't support i2c operation!\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	/*detect device chip*/
	ret = gp2ap_detect(client);
	if (ret < 0) {
		pr_err("%s()->%d:gp2ap012a00f sensor doesn't exist!\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	/*request private data*/
	gp2ap = kzalloc(sizeof(struct gp2ap_data), GFP_KERNEL);
	if (!gp2ap) {
		pr_err("%s()->%d:can not alloc memory to private data !\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	mutex_init(&gp2ap->ioctl_lock);

	/*set client private data*/
	gp2ap->client = client;
	i2c_set_clientdata(client, gp2ap);

	/*create input device for reporting data*/
	ret = gp2ap_create_input(gp2ap);
	if (ret < 0) {
		pr_err("%s()->%d:can not create input device!\n",
			__func__, __LINE__);
		goto err_free_gp2ap_data;
	}

	if (client->irq <= 0) {
		pr_err("%s()->%d:can not found irq pin!\n",
			__func__, __LINE__);
		goto err_free_gp2ap_data;
	}

	/*before operate the interrupt pin, we should request it*/
	ret = gpio_request(client->irq, "gp2ap");
	if (ret < 0) {
		pr_err("%s()->%d:can not request gpio %d!\n",
			__func__, __LINE__, client->irq);
		goto err_free_input;
	}

	gp2ap->irq = gpio_to_irq(client->irq);
	if (gp2ap->irq) {
		/*pull up the gpio*/
		s3c_gpio_setpull(client->irq, S3C_GPIO_PULL_UP);
		ret = request_threaded_irq(gp2ap->irq, NULL, gp2ap_irq_handler,
			IRQF_TRIGGER_FALLING, client->name, gp2ap);
		if (ret < 0) {
			pr_err("%s()->%d:can not request threaded irq for %d!\n",
				__func__, __LINE__, gp2ap->irq);
			goto err_free_gpio;
		}
		disable_irq_nosync(gp2ap->irq);  /*enable irq when we use it*/
	} else {
		pr_err("%s()->%d:can not get irq number from gpio %d!\n",
			__func__, __LINE__, client->irq);
		ret = -ENODEV;
		goto err_free_gpio;
	}

	gp2ap->misc_device.minor = MISC_DYNAMIC_MINOR;
	gp2ap->misc_device.name = "gp2ap";
	gp2ap->misc_device.fops = &gp2ap_fops;
	ret = misc_register(&gp2ap->misc_device);
	if (ret < 0) {
		pr_err("%s()->%d:can not create misc device!\n",
			__func__, __LINE__);
		goto err_free_irq;
	}

	/* create sysfs attributes */
	ret = sysfs_create_group(&client->dev.kobj, &gp2ap_attribute_group);
	if (ret < 0) {
		pr_err("%s()->%d:can not create sysfs group attributes!\n",
			__func__, __LINE__);
		goto err_unregister_misc;
	}

	mutex_init(&gp2ap->lock);
	gp2ap->als_wq = create_singlethread_workqueue("gp2ap_als_wq");
	INIT_DELAYED_WORK(&gp2ap->als_dwork, gp2ap_als_dwork_func);
	gp2ap->debug = 0;
	gp2ap->enabled_sensors = 0;
	gp2ap->irq_wake_enabled = 0;
	gp2ap->reset_threshold_flag = 1;
	gp2ap->init_threshold_flag = 1;

	gp2ap->CURRENT_ALS_RANGE[0] = ALS_RANGE_X2;
	gp2ap->CURRENT_ALS_RANGE[1] = ALS_RANGE_X8;
	gp2ap->CURRENT_ALS_RANGE[2] = ALS_RANGE_X128;

	/* We can not access the calib file in early boot, use our preset one
	 * and reset it when enable the ps
	 */
	gp2ap->calib_value_readed = 0;
	gp2ap->ps_calib_value = PS_CALIB_VALUE;
	gp2ap->ps_near_threshold = PS_CALIB_TO_NEAR(gp2ap->ps_calib_value);
	gp2ap->ps_far_threshold = PS_NEAR_TO_FAR(gp2ap->ps_near_threshold);
	atomic_set(&gp2ap->opened, 0);
	/* Report an initial status as FAR */
	gp2ap_ps_report_far(gp2ap->input_dev);
	gp2ap->ps_data = PS_FAR;

#ifdef CONFIG_HAS_EARLYSUSPEND
	gp2ap->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
	gp2ap->early_suspend.suspend = gp2ap_early_suspend;
	gp2ap->early_suspend.resume = gp2ap_late_resume;
	register_early_suspend(&gp2ap->early_suspend);
#endif

	pr_info("gp2ap sensor probed !\n");

	return 0;

err_unregister_misc:
	misc_deregister(&gp2ap->misc_device);
err_free_irq:
	free_irq(gp2ap->irq, gp2ap);
err_free_gpio:
	gpio_free(client->irq);
err_free_input:
	gp2ap_free_input(gp2ap);
err_free_gp2ap_data:
	kfree(gp2ap);

	return ret;
}

static int __devexit gp2ap_remove(struct i2c_client *client)
{
	struct gp2ap_data *gp2ap = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&gp2ap->early_suspend);
#endif
	sysfs_remove_group(&client->dev.kobj,&gp2ap_attribute_group);
	misc_deregister(&gp2ap->misc_device);
	free_irq(gp2ap->irq, gp2ap);
	gpio_free(client->irq);
	gp2ap_free_input(gp2ap);
	kfree(gp2ap);

	return 0;
}

static void gp2ap_shutdown(struct i2c_client *client)
{
	struct gp2ap_data *gp2ap = i2c_get_clientdata(client);

	gp2ap_power_down(gp2ap);
}

static const struct i2c_device_id gp2ap_id[] = {
	{ "gp2ap020a00f", 0 },
	{},
};

static struct i2c_driver gp2ap_driver = {
	.driver = {
		.name	= GP2AP_DEV_NAME,
		.owner	= THIS_MODULE,
	},
	.probe	= gp2ap_probe,
	.shutdown = gp2ap_shutdown,
	.remove = __devexit_p(gp2ap_remove),
	.id_table = gp2ap_id,
};

static int __init gp2ap_init(void)
{
	return i2c_add_driver(&gp2ap_driver);
}

static void __exit gp2ap_exit(void)
{
	i2c_del_driver(&gp2ap_driver);
}

module_init(gp2ap_init);
module_exit(gp2ap_exit);

MODULE_DESCRIPTION("sharp ambient light and proximity sensor driver");
MODULE_AUTHOR("Meizu");
MODULE_LICENSE("GPL");
