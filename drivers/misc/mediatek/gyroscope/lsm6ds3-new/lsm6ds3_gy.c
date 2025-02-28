/* ST LSM6DS3 Accelerometer and Gyroscope sensor driver
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "cust_gyro.h"
#include "lsm6ds3_gy.h"

#define POWER_NONE_MACRO MT65XX_POWER_NONE
#define LSM6DS3_GYRO_NEW_ARCH		//kk and L compatialbe

/*lenovo-sw caoyi1 add for Gyroscope APK calibration 20150513 begin*/
#define LSM6DS3_MTK_CALIB                             //CALIB USED MTK fatctory MODE
//#define LSM6DS3_CALIB_HAL                             //CALIB FROM LENOVO HAL
/*lenovo-sw caoyi1 add for Gyroscope APK calibration 20150513 end*/

#ifdef LSM6DS3_GYRO_NEW_ARCH
#include "gyroscope.h"
#endif

/*---------------------------------------------------------------------------*/
#define DEBUG 0
/*----------------------------------------------------------------------------*/
#define INV_GYRO_AUTO_CALI  1

#define CONFIG_LSM6DS3_LOWPASS   /*apply low pass filter on output*/
/*----------------------------------------------------------------------------*/
#define LSM6DS3_AXIS_X          0
#define LSM6DS3_AXIS_Y          1
#define LSM6DS3_AXIS_Z          2

#define LSM6DS3_GYRO_AXES_NUM       3
#define LSM6DS3_GYRO_DATA_LEN       6
#define LSM6DS3_GYRO_DEV_NAME        "LSM6DS3_GYRO"
/*----------------------------------------------------------------------------*/
struct gyro_hw lsm6ds3_gyro_hw;
struct gyro_hw* hw = &lsm6ds3_gyro_hw;
struct platform_device* gyroPlatformDevice;
struct gyro_hw* get_cust_gyro_hw(void)
{
	return &lsm6ds3_gyro_hw;
}

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id lsm6ds3_gyro_i2c_id[] = {{LSM6DS3_GYRO_DEV_NAME,0},{}};
/*lenovo-sw caoyi1 modify A+G sensro iic address 20150318 begin*/
//the addr 0x34 is an vitrual address,the real iic address is (0xD6>>1)
//static struct i2c_board_info __initdata i2c_lsm6ds3_gyro={ I2C_BOARD_INFO(LSM6DS3_GYRO_DEV_NAME, 0x34)};
/*lenovo-sw caoyi1 modify A+G sensro iic address 20150318 begin*/

/*lenovo-sw caoyi1 add for Gyroscope APK calibration 20150513 begin*/
#ifdef LSM6DS3_CALIB_HAL
static int calib_gyro_offset[3] = {0,0,0};                  // uguass unit £¬ used for lenovo hal calib
#endif
/*lenovo-sw caoyi1 add for Gyroscope APK calibration 20150513 end*/

/*lenovo-sw caoyi1 add for selftest function 20150708 begin*/
#define LSM6DS3_FTM_ENABLE 0
#ifdef LSM6DS3_FTM_ENABLE
#define ABS_ST(X) ((X) < 0 ? (-1 * (X)) : (X))
#define MIN_ST   150     //dps unit
#define MAX_ST  700
#endif
/*lenovo-sw caoyi1 add for selftest function 20150708 end*/

/*----------------------------------------------------------------------------*/
static int lsm6ds3_gyro_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int lsm6ds3_gyro_i2c_remove(struct i2c_client *client);
static int LSM6DS3_gyro_init_client(struct i2c_client *client, bool enable);
static int lsm6ds3_gyro_resume(struct i2c_client *client);
static int lsm6ds3_gyro_suspend(struct i2c_client *client, pm_message_t msg);
static int lsm6ds3_gyro_local_init(struct platform_device* pdev);
static int lsm6ds3_gyro_local_uninit(void);
static int lsm6ds3_gyro_init_flag = -1;
static struct gyro_init_info  lsm6ds3_gyro_init_info =
{
	.name   = LSM6DS3_GYRO_DEV_NAME,
	.init   = lsm6ds3_gyro_local_init,
	.uninit = lsm6ds3_gyro_local_uninit,
};
/*----------------------------------------------------------------------------*/
typedef enum {
    ADX_TRC_FILTER  = 0x01,
    ADX_TRC_RAWDATA = 0x02,
    ADX_TRC_IOCTL   = 0x04,
    ADX_TRC_CALI	= 0X08,
    ADX_TRC_INFO	= 0X10,
} ADX_TRC;
/*----------------------------------------------------------------------------*/
typedef enum {
    GYRO_TRC_FILTER  = 0x01,
    GYRO_TRC_RAWDATA = 0x02,
    GYRO_TRC_IOCTL   = 0x04,
    GYRO_TRC_CALI	= 0X08,
    GYRO_TRC_INFO	= 0X10,
    GYRO_TRC_DATA	= 0X20,
} GYRO_TRC;
/*----------------------------------------------------------------------------*/
struct scale_factor{
    u8  whole;
    u8  fraction;
};
/*----------------------------------------------------------------------------*/
struct data_resolution {
    struct scale_factor scalefactor;
    int                 sensitivity;
};
/*----------------------------------------------------------------------------*/
#define C_MAX_FIR_LENGTH (32)
/*----------------------------------------------------------------------------*/

struct gyro_data_filter {
    s16 raw[C_MAX_FIR_LENGTH][LSM6DS3_GYRO_AXES_NUM];
    int sum[LSM6DS3_GYRO_AXES_NUM];
    int num;
    int idx;
};
/*----------------------------------------------------------------------------*/
struct lsm6ds3_gyro_i2c_data {
    struct i2c_client *client;
    struct gyro_hw *hw;
    struct hwmsen_convert   cvt;

    /*misc*/
    //struct data_resolution *reso;
    atomic_t                trace;
    atomic_t                suspend;
    atomic_t                selftest;
    atomic_t				filter;
    s16                     cali_sw[LSM6DS3_GYRO_AXES_NUM+1];

    /*data*/

    s8                      offset[LSM6DS3_GYRO_AXES_NUM+1];  /*+1: for 4-byte alignment*/
/*lenovo-sw caoyi1 modify fix gyro data vilation 20150608 begin*/
    s32                     data[LSM6DS3_GYRO_AXES_NUM+1];
/*lenovo-sw caoyi1 modify fix gyro data vilation 20150608 end*/
	int 					sensitivity;

#if defined(CONFIG_LSM6DS3_LOWPASS)
    atomic_t                firlen;
    atomic_t                fir_en;
    struct gyro_data_filter      fir;
#endif
#if INV_GYRO_AUTO_CALI == 1
	s16					 inv_cali_raw[LSM6DS3_GYRO_AXES_NUM+1];
	s16					 temperature;
	struct mutex			temperature_mutex;/* for temperature protection */
	struct mutex			raw_data_mutex;/* for inv_cali_raw[] protection */
#endif
    /*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend    early_drv;
#endif

/*lenovo-sw caoyi1 add for selftest function 20150708 begin*/
#ifdef LSM6DS3_FTM_ENABLE
    u8 resume_LSM6DS3_CTRL1_XL;
    u8 resume_LSM6DS3_CTRL2_G;
    u8 resume_LSM6DS3_CTRL3_C;
    u8 resume_LSM6DS3_CTRL4_C;
    u8 resume_LSM6DS3_CTRL5_C;
    u8 resume_LSM6DS3_CTRL6_C;
    u8 resume_LSM6DS3_CTRL7_G;
    u8 resume_LSM6DS3_CTRL8_XL;
    u8 resume_LSM6DS3_CTRL9_XL;
    u8 resume_LSM6DS3_CTRL10_C;
 #endif
/*lenovo-sw caoyi1 add for selftest function 20150708 end*/
};
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_OF
static const struct of_device_id gyro_of_match[] = {
	{ .compatible = "mediatek,gyro" },
	{},
};
#endif
static struct i2c_driver lsm6ds3_gyro_i2c_driver = {
    .driver = {
        //.owner          = THIS_MODULE,
        .name           = LSM6DS3_GYRO_DEV_NAME,
#ifdef CONFIG_OF
		.of_match_table = gyro_of_match,
#endif
    },
	.probe      		= lsm6ds3_gyro_i2c_probe,
	.remove    			= lsm6ds3_gyro_i2c_remove,

    .suspend            = lsm6ds3_gyro_suspend,
    .resume             = lsm6ds3_gyro_resume,
	.id_table = lsm6ds3_gyro_i2c_id,
};
/*----------------------------------------------------------------------------*/
static struct i2c_client *lsm6ds3_i2c_client = NULL;

static struct lsm6ds3_gyro_i2c_data *obj_i2c_data = NULL;
static bool sensor_power = false;
static bool enable_status = false;

/*----------------------------------------------------------------------------*/
//#define GYRO_TAG                  "[Gyroscope] "
//#define GYRO_FUN(f)               pr_debug(KERN_INFO GYRO_TAG"%s\n", __FUNCTION__)
//#define GYRO_ERR(fmt, args...)    pr_err(KERN_ERR GYRO_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
//#define GYRO_LOG(fmt, args...)    pr_info(KERN_INFO GYRO_TAG fmt, ##args)

/*----------------------------------------------------------------------------*/
#if 0
static void LSM6DS3_dumpReg(struct i2c_client *client)
{
	int i=0;
	u8 addr = 0x10;
	u8 regdata=0;
	for(i=0; i<25 ; i++)
	{
		//dump all
		hwmsen_read_byte(client,addr,&regdata);
		HWM_LOG("Reg addr=%x regdata=%x\n",addr,regdata);
		addr++;
	}
}
#endif

#if INV_GYRO_AUTO_CALI == 1

#define INV_DAEMON_CLASS_NAME  "invensense_daemon_class"
#define INV_DAEMON_DEVICE_NAME  "invensense_daemon_device"

static struct class *inv_daemon_class;
static struct device *inv_daemon_device;
static int inv_mpl_motion_state;
static int inv_gyro_power_state;
static ssize_t inv_mpl_motion_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int result;
	unsigned long data;
	char *envp[2];

	result = kstrtoul(buf, 10, &data);
	if (result)
		return result;

	if (data)
		envp[0] = "STATUS=MOTION";
	else
		envp[0] = "STATUS=NOMOTION";
	envp[1] = NULL;
	result = kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);

	inv_mpl_motion_state = data;

	return count;
}
static ssize_t inv_mpl_motion_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", inv_mpl_motion_state);
}

static ssize_t inv_gyro_data_ready_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	sysfs_notify(&dev->kobj, NULL, "inv_gyro_data_ready");
	return count;
}
static ssize_t inv_gyro_data_ready_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "1\n");
}

static ssize_t inv_gyro_power_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int result;
	unsigned long data;

	result = kstrtoul(buf, 10, &data);
	if (result)
		return result;

	inv_gyro_power_state = data;

	sysfs_notify(&dev->kobj, NULL, "inv_gyro_power_state");
	return count;
}
static ssize_t inv_gyro_power_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", inv_gyro_power_state);
}

static DEVICE_ATTR(inv_mpl_motion, S_IRUGO | S_IWUSR, inv_mpl_motion_show, inv_mpl_motion_store);
static DEVICE_ATTR(inv_gyro_data_ready, S_IRUGO | S_IWUSR, inv_gyro_data_ready_show, inv_gyro_data_ready_store);
static DEVICE_ATTR(inv_gyro_power_state, S_IRUGO | S_IWUSR, inv_gyro_power_state_show, inv_gyro_power_state_store);

static struct device_attribute *inv_daemon_dev_attributes[] = {
	&dev_attr_inv_mpl_motion,
	&dev_attr_inv_gyro_data_ready,
	&dev_attr_inv_gyro_power_state,
};
#endif

/*--------------------gyroscopy power control function----------------------------------*/
static void LSM6DS3_power(struct gyro_hw *hw, unsigned int on)
{
#if 0
	static unsigned int power_on = 0;

	if(hw->power_id != POWER_NONE_MACRO)		// have externel LDO
	{
		GYRO_LOG("power %s\n", on ? "on" : "off");
		if(power_on == on)	// power status not change
		{
			GYRO_LOG("ignore power control: %d\n", on);
		}
		else if(on)	// power on
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "LSM6DS3"))
			{
				GYRO_ERR("power on fails!!\n");
			}
		}
		else	// power off
		{
			if (!hwPowerDown(hw->power_id, "LSM6DS3"))
			{
				GYRO_ERR("power off fail!!\n");
			}
		}
	}
	power_on = on;
#endif
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int LSM6DS3_gyro_write_rel_calibration(struct lsm6ds3_gyro_i2c_data *obj, int dat[LSM6DS3_GYRO_AXES_NUM])
{
    obj->cali_sw[LSM6DS3_AXIS_X] = obj->cvt.sign[LSM6DS3_AXIS_X]*dat[obj->cvt.map[LSM6DS3_AXIS_X]];
    obj->cali_sw[LSM6DS3_AXIS_Y] = obj->cvt.sign[LSM6DS3_AXIS_Y]*dat[obj->cvt.map[LSM6DS3_AXIS_Y]];
    obj->cali_sw[LSM6DS3_AXIS_Z] = obj->cvt.sign[LSM6DS3_AXIS_Z]*dat[obj->cvt.map[LSM6DS3_AXIS_Z]];
#if DEBUG
		if(atomic_read(&obj->trace) & GYRO_TRC_CALI)
		{
			GYRO_LOG("test  (%5d, %5d, %5d) ->(%5d, %5d, %5d)->(%5d, %5d, %5d))\n",
				obj->cvt.sign[LSM6DS3_AXIS_X],obj->cvt.sign[LSM6DS3_AXIS_Y],obj->cvt.sign[LSM6DS3_AXIS_Z],
				dat[LSM6DS3_AXIS_X], dat[LSM6DS3_AXIS_Y], dat[LSM6DS3_AXIS_Z],
				obj->cvt.map[LSM6DS3_AXIS_X],obj->cvt.map[LSM6DS3_AXIS_Y],obj->cvt.map[LSM6DS3_AXIS_Z]);
			GYRO_LOG("write gyro calibration data  (%5d, %5d, %5d)\n",
				obj->cali_sw[LSM6DS3_AXIS_X],obj->cali_sw[LSM6DS3_AXIS_Y],obj->cali_sw[LSM6DS3_AXIS_Z]);
		}
#endif
    return 0;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS3_gyro_ResetCalibration(struct i2c_client *client)
{
	struct lsm6ds3_gyro_i2c_data *obj = i2c_get_clientdata(client);

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	return 0;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS3_gyro_ReadCalibration(struct i2c_client *client, int dat[LSM6DS3_GYRO_AXES_NUM])
{
    struct lsm6ds3_gyro_i2c_data *obj = i2c_get_clientdata(client);

    dat[obj->cvt.map[LSM6DS3_AXIS_X]] = obj->cvt.sign[LSM6DS3_AXIS_X]*obj->cali_sw[LSM6DS3_AXIS_X];
    dat[obj->cvt.map[LSM6DS3_AXIS_Y]] = obj->cvt.sign[LSM6DS3_AXIS_Y]*obj->cali_sw[LSM6DS3_AXIS_Y];
    dat[obj->cvt.map[LSM6DS3_AXIS_Z]] = obj->cvt.sign[LSM6DS3_AXIS_Z]*obj->cali_sw[LSM6DS3_AXIS_Z];

#if DEBUG
	if(atomic_read(&obj->trace) & GYRO_TRC_CALI)
	{
		GYRO_LOG("Read gyro calibration data  (%5d, %5d, %5d)\n",
			dat[LSM6DS3_AXIS_X],dat[LSM6DS3_AXIS_Y],dat[LSM6DS3_AXIS_Z]);
	}
#endif

    return 0;
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static int LSM6DS3_gyro_WriteCalibration(struct i2c_client *client, int dat[LSM6DS3_GYRO_AXES_NUM])
{
	struct lsm6ds3_gyro_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int cali[LSM6DS3_GYRO_AXES_NUM];

	GYRO_FUN();
	if(!obj || ! dat)
	{
		GYRO_ERR("null ptr!!\n");
		return -EINVAL;
	}
	else
	{
		cali[obj->cvt.map[LSM6DS3_AXIS_X]] = obj->cvt.sign[LSM6DS3_AXIS_X]*obj->cali_sw[LSM6DS3_AXIS_X];
		cali[obj->cvt.map[LSM6DS3_AXIS_Y]] = obj->cvt.sign[LSM6DS3_AXIS_Y]*obj->cali_sw[LSM6DS3_AXIS_Y];
		cali[obj->cvt.map[LSM6DS3_AXIS_Z]] = obj->cvt.sign[LSM6DS3_AXIS_Z]*obj->cali_sw[LSM6DS3_AXIS_Z];
		cali[LSM6DS3_AXIS_X] += dat[LSM6DS3_AXIS_X];
		cali[LSM6DS3_AXIS_Y] += dat[LSM6DS3_AXIS_Y];
		cali[LSM6DS3_AXIS_Z] += dat[LSM6DS3_AXIS_Z];
#if DEBUG
		if(atomic_read(&obj->trace) & GYRO_TRC_CALI)
		{
			GYRO_LOG("write gyro calibration data  (%5d, %5d, %5d)-->(%5d, %5d, %5d)\n",
				dat[LSM6DS3_AXIS_X], dat[LSM6DS3_AXIS_Y], dat[LSM6DS3_AXIS_Z],
				cali[LSM6DS3_AXIS_X],cali[LSM6DS3_AXIS_Y],cali[LSM6DS3_AXIS_Z]);
		}
#endif
		return LSM6DS3_gyro_write_rel_calibration(obj, cali);
	}

	return err;
}
/*----------------------------------------------------------------------------*/
static int LSM6DS3_CheckDeviceID(struct i2c_client *client)
{
	u8 databuf[10];
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);
	databuf[0] = LSM6DS3_FIXED_DEVID;

	res = hwmsen_read_byte(client,LSM6DS3_WHO_AM_I,databuf);
    GYRO_LOG(" LSM6DS3  id %x!\n",databuf[0]);
	if(databuf[0]!=LSM6DS3_FIXED_DEVID)
	{
		return LSM6DS3_ERR_IDENTIFICATION;
	}

	if (res < 0)
	{
		return LSM6DS3_ERR_I2C;
	}

	return LSM6DS3_SUCCESS;
}

//----------------------------------------------------------------------------//
static int LSM6DS3_gyro_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2] = {0};
	int res = 0;

	if(enable == sensor_power)
	{
		GYRO_LOG("Sensor power status is newest!\n");
		return LSM6DS3_SUCCESS;
	}

	if(hwmsen_read_byte(client, LSM6DS3_CTRL2_G, databuf))
	{
		GYRO_ERR("read lsm6ds3 power ctl register err!\n");
		return LSM6DS3_ERR_I2C;
	}

	if(true == enable)
	{
		databuf[0] &= ~LSM6DS3_GYRO_ODR_MASK;//clear lsm6ds3 gyro ODR bits
		databuf[0] |= LSM6DS3_GYRO_ODR_104HZ; //default set 100HZ for LSM6DS3 gyro
	}
	else
	{
		// do nothing
		databuf[0] &= ~LSM6DS3_GYRO_ODR_MASK;//clear lsm6ds3 gyro ODR bits
		databuf[0] |= LSM6DS3_GYRO_ODR_POWER_DOWN; //POWER DOWN
	}
	databuf[1] = databuf[0];
	databuf[0] = LSM6DS3_CTRL2_G;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_LOG("LSM6DS3 set power mode: ODR 100hz failed!\n");
		return LSM6DS3_ERR_I2C;
	}
	else
	{
		GYRO_LOG("set LSM6DS3 gyro power mode:ODR 100HZ ok %d!\n", enable);
	}

	sensor_power = enable;

	return LSM6DS3_SUCCESS;
}

static int LSM6DS3_Set_RegInc(struct i2c_client *client, bool inc)
{
	u8 databuf[2] = {0};
	int res = 0;
	//GYRO_FUN();

	if(hwmsen_read_byte(client, LSM6DS3_CTRL3_C, databuf))
	{
		GYRO_ERR("read LSM6DS3_CTRL1_XL err!\n");
		return LSM6DS3_ERR_I2C;
	}
	else
	{
		GYRO_LOG("read  LSM6DS3_CTRL1_XL register: 0x%x\n", databuf[0]);
	}

	if(inc)
	{
		databuf[0] |= LSM6DS3_CTRL3_C_IFINC;

		databuf[1] = databuf[0];
		databuf[0] = LSM6DS3_CTRL3_C;

		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			GYRO_ERR("write full scale register err!\n");
			return LSM6DS3_ERR_I2C;
		}
	}
	return LSM6DS3_SUCCESS;
}

static int LSM6DS3_gyro_SetFullScale(struct i2c_client *client, u8 gyro_fs)
{
	u8 databuf[2] = {0};
	int res = 0;
	GYRO_FUN();

	if(hwmsen_read_byte(client, LSM6DS3_CTRL2_G, databuf))
	{
		GYRO_ERR("read LSM6DS3_CTRL2_G err!\n");
		return LSM6DS3_ERR_I2C;
	}
	else
	{
		GYRO_LOG("read  LSM6DS3_CTRL2_G register: 0x%x\n", databuf[0]);
	}

	databuf[0] &= ~LSM6DS3_GYRO_RANGE_MASK;//clear
	databuf[0] |= gyro_fs;

	databuf[1] = databuf[0];
	databuf[0] = LSM6DS3_CTRL2_G;

	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write full scale register err!\n");
		return LSM6DS3_ERR_I2C;
	}
	return LSM6DS3_SUCCESS;
}

/*----------------------------------------------------------------------------*/
// set the gyro sample rate
static int LSM6DS3_gyro_SetSampleRate(struct i2c_client *client, u8 sample_rate)
{
	u8 databuf[2] = {0};
	int res = 0;
	GYRO_FUN();

	res = LSM6DS3_gyro_SetPowerMode(client, true);	//set Sample Rate will enable power and should changed power status
	if(res != LSM6DS3_SUCCESS)
	{
		return res;
	}

	if(hwmsen_read_byte(client, LSM6DS3_CTRL2_G, databuf))
	{
		GYRO_ERR("read gyro data format register err!\n");
		return LSM6DS3_ERR_I2C;
	}
	else
	{
		GYRO_LOG("read  gyro data format register: 0x%x\n", databuf[0]);
	}

	databuf[0] &= ~LSM6DS3_GYRO_ODR_MASK;//clear
	databuf[0] |= sample_rate;

	databuf[1] = databuf[0];
	databuf[0] = LSM6DS3_CTRL2_G;

	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write sample rate register err!\n");
		return LSM6DS3_ERR_I2C;
	}

	return LSM6DS3_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS3_ReadGyroData(struct i2c_client *client, char *buf, int bufsize)
{
	char databuf[6];
	int data[3];
#if INV_GYRO_AUTO_CALI == 1
	int ret = 0;
#endif
	struct lsm6ds3_gyro_i2c_data *obj = i2c_get_clientdata(client);

	if(sensor_power == false)
	{
		LSM6DS3_gyro_SetPowerMode(client, true);
	}

#if INV_GYRO_AUTO_CALI == 1
	ret = hwmsen_read_block(client, LSM6DS3_OUT_TEMP_L, databuf, 2);

	if(hwmsen_read_block(client, LSM6DS3_OUT_TEMP_L, databuf, 2))
	{
		GYRO_ERR("LSM6DS3 read temperature data  error\n");
		return -2;
	}
	mutex_lock(&obj->temperature_mutex);
	obj->temperature = ((s16)((databuf[1]) | (databuf[0] << 8)));
	mutex_unlock(&obj->temperature_mutex);
#endif

	if(hwmsen_read_block(client, LSM6DS3_OUTX_L_G, databuf, 6))
	{
		GYRO_ERR("LSM6DS3 read gyroscope data  error\n");
		return -2;
	}
	else
	{
		obj->data[LSM6DS3_AXIS_X] = (s16)((databuf[LSM6DS3_AXIS_X*2+1] << 8) | (databuf[LSM6DS3_AXIS_X*2]));
		obj->data[LSM6DS3_AXIS_Y] = (s16)((databuf[LSM6DS3_AXIS_Y*2+1] << 8) | (databuf[LSM6DS3_AXIS_Y*2]));
		obj->data[LSM6DS3_AXIS_Z] = (s16)((databuf[LSM6DS3_AXIS_Z*2+1] << 8) | (databuf[LSM6DS3_AXIS_Z*2]));
		
	#if DEBUG
		if(atomic_read(&obj->trace) & GYRO_TRC_RAWDATA)
		{
			GYRO_LOG("read gyro register: %x, %x, %x, %x, %x, %x",
				databuf[0], databuf[1], databuf[2], databuf[3], databuf[4], databuf[5]);
			GYRO_LOG("get gyro raw data (0x%08X, 0x%08X, 0x%08X) -> (%5d, %5d, %5d)\n",
				obj->data[LSM6DS3_AXIS_X],obj->data[LSM6DS3_AXIS_Y],obj->data[LSM6DS3_AXIS_Z],
				obj->data[LSM6DS3_AXIS_X],obj->data[LSM6DS3_AXIS_Y],obj->data[LSM6DS3_AXIS_Z]);
			GYRO_LOG("get gyro cali data (%5d, %5d, %5d)\n",
				obj->cali_sw[LSM6DS3_AXIS_X],obj->cali_sw[LSM6DS3_AXIS_Y],obj->cali_sw[LSM6DS3_AXIS_Z]);
		}
	#endif
	#if INV_GYRO_AUTO_CALI == 1
		mutex_lock(&obj->raw_data_mutex);
		/*remap coordinate*/
		obj->inv_cali_raw[obj->cvt.map[LSM6DS3_AXIS_X]] = obj->cvt.sign[LSM6DS3_AXIS_X]*obj->data[LSM6DS3_AXIS_X];
		obj->inv_cali_raw[obj->cvt.map[LSM6DS3_AXIS_Y]] = obj->cvt.sign[LSM6DS3_AXIS_Y]*obj->data[LSM6DS3_AXIS_Y];
		obj->inv_cali_raw[obj->cvt.map[LSM6DS3_AXIS_Z]] = obj->cvt.sign[LSM6DS3_AXIS_Z]*obj->data[LSM6DS3_AXIS_Z];
		mutex_unlock(&obj->raw_data_mutex);
	#endif
	#ifdef LSM6DS3_MTK_CALIB
		obj->data[LSM6DS3_AXIS_X] += obj->cali_sw[LSM6DS3_AXIS_X];
		obj->data[LSM6DS3_AXIS_Y] += obj->cali_sw[LSM6DS3_AXIS_Y];
		obj->data[LSM6DS3_AXIS_Z] += obj->cali_sw[LSM6DS3_AXIS_Z];
	#endif

		/*remap coordinate*/
		data[obj->cvt.map[LSM6DS3_AXIS_X]] = obj->cvt.sign[LSM6DS3_AXIS_X]*obj->data[LSM6DS3_AXIS_X];
		data[obj->cvt.map[LSM6DS3_AXIS_Y]] = obj->cvt.sign[LSM6DS3_AXIS_Y]*obj->data[LSM6DS3_AXIS_Y];
		data[obj->cvt.map[LSM6DS3_AXIS_Z]] = obj->cvt.sign[LSM6DS3_AXIS_Z]*obj->data[LSM6DS3_AXIS_Z];
	}

	/* Out put the degree/second(o/s) */
	data[LSM6DS3_AXIS_X] = data[LSM6DS3_AXIS_X]*LSM6DS3_GYRO_SENSITIVITY_2000DPS*131/1000;
	data[LSM6DS3_AXIS_Y] = data[LSM6DS3_AXIS_Y]*LSM6DS3_GYRO_SENSITIVITY_2000DPS*131/1000;
	data[LSM6DS3_AXIS_Z] = data[LSM6DS3_AXIS_Z]*LSM6DS3_GYRO_SENSITIVITY_2000DPS*131/1000;

#ifdef LSM6DS3_CALIB_HAL
	data[LSM6DS3_AXIS_X] = data[LSM6DS3_AXIS_X] - calib_gyro_offset[LSM6DS3_AXIS_X];
	data[LSM6DS3_AXIS_Y] = data[LSM6DS3_AXIS_Y] - calib_gyro_offset[LSM6DS3_AXIS_Y];
	data[LSM6DS3_AXIS_Z] = data[LSM6DS3_AXIS_Z] - calib_gyro_offset[LSM6DS3_AXIS_Z];
#endif

	sprintf(buf, "%x %x %x", data[LSM6DS3_AXIS_X],data[LSM6DS3_AXIS_Y],data[LSM6DS3_AXIS_Z]);

#if DEBUG
	if(atomic_read(&obj->trace) & GYRO_TRC_DATA)
	{
		GYRO_LOG("get gyro data packet:[%d %d %d]\n", data[0], data[1], data[2]);
	}
#endif

	return 0;
}

/*lenovo-sw caoyi1 add for selftest function 20150708 begin*/
#ifdef LSM6DS3_FTM_ENABLE
static int lsm303d_gyro_get_ST_data(struct i2c_client *client, int *xyz)
{
	int i;
	s16 gyro_data[6];
	int hw_d[3] = {0};
	u8 buf[2];
	char databuf[6] = {0};
	int xyz_sum[3] = {0};

	if(hwmsen_read_block(client, LSM6DS3_OUTX_L_G, databuf, 6))
	{
		GYRO_ERR("LSM6DS3 read gyro data  error\n");
		return -2;
	}

	for(i=0; i<5;)
	{
		if(hwmsen_read_byte(client, LSM6DS3_STATUS_REG, buf))
		{
			GYRO_ERR("read gyro data format register err!\n");
			return LSM6DS3_ERR_I2C;
		}
		else
		{
			GYRO_LOG("read  gyro status  register: 0x%x\n", buf[0]);
		}

		if(buf[0] & 0x02)                                                       // checking status reg  0x01 bit)
		{
			i++;
			if(hwmsen_read_block(client, LSM6DS3_OUTX_L_G, databuf, 6))
	        {
				GYRO_ERR("LSM6DS3 read gyro data  error\n");
				return -2;
	        }

			gyro_data[0] = (s16)((databuf[LSM6DS3_AXIS_X*2+1] << 8) | (databuf[LSM6DS3_AXIS_X*2]));
			gyro_data[1] = (s16)((databuf[LSM6DS3_AXIS_Y*2+1] << 8) | (databuf[LSM6DS3_AXIS_Y*2]));
			gyro_data[2] = (s16)((databuf[LSM6DS3_AXIS_Z*2+1] << 8) | (databuf[LSM6DS3_AXIS_Z*2]));

			GYRO_ERR("raw--gyro  x %d, y%d,z %d ,i %d \n",gyro_data[0],gyro_data[1],gyro_data[2],i);

			hw_d[0] = gyro_data[0] * LSM6DS3_GYRO_SENSITIVITY_2000DPS;                          // gyro selftest is using dps unit
			hw_d[1] = gyro_data[1] * LSM6DS3_GYRO_SENSITIVITY_2000DPS;
			hw_d[2] = gyro_data[2] * LSM6DS3_GYRO_SENSITIVITY_2000DPS;

		    GYRO_ERR("x %d, y%d,z %d ,i %d \n",hw_d[0],hw_d[1],hw_d[2],i);

			xyz_sum[0] =  hw_d[0] + xyz_sum[0] ;
			xyz_sum[1] =  hw_d[1] + xyz_sum[1] ;
			xyz_sum[2] =  hw_d[2] + xyz_sum[2] ;
		}
		else
		{
			msleep(25);
		}
	}

	xyz[0] = xyz_sum[0];
	xyz[1] = xyz_sum[1];
	xyz[2] = xyz_sum[2];

	GYRO_ERR("sum gyro x %d, y%d,z %d ,i %d \n",xyz[0], xyz[1], xyz[2] ,i);

	return 0;
}
#endif
/*lenovo-sw caoyi1 add for selftest function 20150708 end*/

/*----------------------------------------------------------------------------*/
static int LSM6DS3_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
{
	u8 databuf[10];

	memset(databuf, 0, sizeof(u8)*10);

	if((NULL == buf)||(bufsize<=30))
	{
		return -1;
	}

	if(NULL == client)
	{
		*buf = 0;
		return -2;
	}

	sprintf(buf, "LSM6DS3 Chip");
	return 0;
}

#if INV_GYRO_AUTO_CALI == 1
/*----------------------------------------------------------------------------*/
static int LSM6DS3_ReadGyroDataRaw(struct i2c_client *client, char *buf, int bufsize)
{
	struct lsm6ds3_gyro_i2c_data *obj = i2c_get_clientdata(client);

	mutex_lock(&obj->raw_data_mutex);
	/* return gyro raw LSB in device orientation */
	sprintf(buf, "%x %x %x", obj->inv_cali_raw[LSM6DS3_AXIS_X], obj->inv_cali_raw[LSM6DS3_AXIS_Y],
		obj->inv_cali_raw[LSM6DS3_AXIS_Z]);

#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_DATA)
		GYRO_LOG("get gyro raw data packet:[%d %d %d]\n", obj->inv_cali_raw[0], obj->inv_cali_raw[1],
			obj->inv_cali_raw[2]);
#endif
	mutex_unlock(&obj->raw_data_mutex);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS3_ReadTemperature(struct i2c_client *client, char *buf, int bufsize)
{
	struct lsm6ds3_gyro_i2c_data *obj = i2c_get_clientdata(client);

	mutex_lock(&obj->temperature_mutex);
	sprintf(buf, "%x", obj->temperature);

#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_DATA)
		GYRO_LOG("get gyro temperature:[%d]\n", obj->temperature);
#endif
	mutex_unlock(&obj->temperature_mutex);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS3_ReadPowerStatus(struct i2c_client *client, char *buf, int bufsize)
{
#if DEBUG
	GYRO_LOG("get gyro PowerStatus:[%d]\n", sensor_power);
#endif

	sprintf(buf, "%x", sensor_power);

	return 0;
}
#endif

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm6ds3_i2c_client;
	char strbuf[LSM6DS3_BUFSIZE];
	if(NULL == client)
	{
		GYRO_ERR("i2c client is null!!\n");
		return 0;
	}

	LSM6DS3_ReadChipInfo(client, strbuf, LSM6DS3_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
// FIXME: read real chipid
static ssize_t show_chipid_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm6ds3_i2c_client;
	char strbuf[LSM6DS3_BUFSIZE] = "unknown";
	if(NULL == client)
	{
		GYRO_ERR("i2c client is null!!\n");
		return 0;
	}

	//LSM6DS3_ReadChipInfo(client, strbuf, LSM6DS3_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm6ds3_i2c_client;
	char strbuf[LSM6DS3_BUFSIZE];

	if(NULL == client)
	{
		GYRO_ERR("i2c client is null!!\n");
		return 0;
	}

	LSM6DS3_ReadGyroData(client, strbuf, LSM6DS3_BUFSIZE);

	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct lsm6ds3_gyro_i2c_data *obj = obj_i2c_data;
	if (obj == NULL)
	{
		GYRO_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct lsm6ds3_gyro_i2c_data *obj = obj_i2c_data;
	int trace;
	if (obj == NULL)
	{
		GYRO_ERR("i2c_data obj is null!!\n");
		return count;
	}

	if(1 == sscanf(buf, "0x%x", &trace))
	{
		atomic_set(&obj->trace, trace);
	}
	else
	{
		GYRO_ERR("invalid content: '%s', length = %zu\n", buf, count);
	}

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct lsm6ds3_gyro_i2c_data *obj = obj_i2c_data;
	if (obj == NULL)
	{
		GYRO_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if(obj->hw)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d %d (%d %d)\n",
	            obj->hw->i2c_num, obj->hw->direction, obj->hw->power_id, obj->hw->power_vol);
	}
	else
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}
	return len;
}

static ssize_t store_status_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct lsm6ds3_gyro_i2c_data *obj = obj_i2c_data;
	int direction;
	if (obj == NULL)
	{
		GYRO_ERR("i2c_data obj is null!!\n");
		return count;
	}

	if(1 == sscanf(buf, "%d", &direction))
	{
		obj->hw->direction = direction;
		hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	}
	else
	{
		GYRO_ERR("invalid content: '%s', length = %zu\n", buf, count);
	}

	return count;
}

/*lenovo-sw caoyi1 add for Gyroscope APK calibration 20150513 begin*/
#ifdef LSM6DS3_CALIB_HAL
static ssize_t show_cali_gyrox_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm6ds3_i2c_client;
        int data;

	if(NULL == client)
	{
		GYRO_ERR("i2c client is null!!\n");
		return 0;
	}
	data = calib_gyro_offset[LSM6DS3_AXIS_X];

	return snprintf(buf, PAGE_SIZE, "%d \n", data);
}

/*----------------------------------------------------------------------------*/
static ssize_t store_cali_gyrox_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct lsm6ds3_gyro_i2c_data *obj = obj_i2c_data;
	int cali_x;
	if (obj == NULL)
	{
		GYRO_ERR("i2c_data obj is null!!\n");
		return count;
	}

	if(1 == sscanf(buf, "%d", &cali_x))
	{
		calib_gyro_offset[LSM6DS3_AXIS_X] = cali_x;
	}
	else
	{
		GYRO_ERR("invalid content: '%s', length = %zu\n", buf, count);
	}

	return count;
}

static ssize_t show_cali_gyroy_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm6ds3_i2c_client;
	int data;

	if(NULL == client)
	{
		GYRO_ERR("i2c client is null!!\n");
		return 0;
	}
	data = calib_gyro_offset[LSM6DS3_AXIS_Y];

	return snprintf(buf, PAGE_SIZE, "%d \n", data);
}

/*----------------------------------------------------------------------------*/
static ssize_t store_cali_gyroy_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct lsm6ds3_gyro_i2c_data *obj = obj_i2c_data;
	int cali_y;
	if (obj == NULL)
	{
		GYRO_ERR("i2c_data obj is null!!\n");
		return count;
	}

	if(1 == sscanf(buf, "%d", &cali_y))
	{
		calib_gyro_offset[LSM6DS3_AXIS_Y] = cali_y;
	}
	else
	{
		GYRO_ERR("invalid content: '%s', length = %zu\n", buf, count);
	}

	return count;
}

static ssize_t show_cali_gyroz_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm6ds3_i2c_client;
        int data;

	if(NULL == client)
	{
		GYRO_ERR("i2c client is null!!\n");
		return 0;
	}
	data = calib_gyro_offset[LSM6DS3_AXIS_Z];

	return snprintf(buf, PAGE_SIZE, "%d \n", data);
}

/*----------------------------------------------------------------------------*/
static ssize_t store_cali_gyroz_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct lsm6ds3_gyro_i2c_data *obj = obj_i2c_data;
	int cali_z;
	if (obj == NULL)
	{
		GYRO_ERR("i2c_data obj is null!!\n");
		return count;
	}

	if(1 == sscanf(buf, "%d", &cali_z))
	{
		calib_gyro_offset[LSM6DS3_AXIS_Z] = cali_z;
	}
	else
	{
		GYRO_ERR("invalid content: '%s', length = %zu\n", buf, count);
	}

	return count;
}
#endif
/*lenovo-sw caoyi1 add for Gyroscope APK calibration 20150513 end*/

/*lenovo-sw caoyi1 add for selftest function 20150708 begin*/
#ifdef LSM6DS3_FTM_ENABLE
static int back_ST_gyro_reg_config(struct i2c_client  *client )
{
	struct lsm6ds3_gyro_i2c_data *priv = obj_i2c_data;
	u8 databuf[2];
	int res;

	res = hwmsen_read_byte(client, LSM6DS3_CTRL1_XL, databuf);
	if(res)
	{
		GYRO_ERR("read LSM6DS3_CTRL1_XL err!\n");
		return LSM6DS3_ERR_I2C;
	}
	priv ->resume_LSM6DS3_CTRL1_XL = databuf[0];

	res = hwmsen_read_byte(client, LSM6DS3_CTRL2_G, databuf);
	if(res)
	{
		GYRO_ERR("read LSM6DS3_CTRL2_G err!\n");
		return LSM6DS3_ERR_I2C;
	}
	priv ->resume_LSM6DS3_CTRL2_G = databuf[0];

	res = hwmsen_read_byte(client, LSM6DS3_CTRL3_C, databuf);
	if(res)
	{
		GYRO_ERR("read LSM6DS3_CTRL3_C err!\n");
		return LSM6DS3_ERR_I2C;
	}
	priv ->resume_LSM6DS3_CTRL3_C = databuf[0];

	res = hwmsen_read_byte(client, LSM6DS3_CTRL4_C, databuf);
	if(res)
	{
		GYRO_ERR("read LSM6DS3_CTRL4_C err!\n");
		return LSM6DS3_ERR_I2C;
	}
	priv ->resume_LSM6DS3_CTRL4_C = databuf[0];

	res = hwmsen_read_byte(client, LSM6DS3_CTRL5_C, databuf);
	if(res)
	{
		GYRO_ERR("read LSM6DS3_CTRL5_C err!\n");
		return LSM6DS3_ERR_I2C;
	}
	priv ->resume_LSM6DS3_CTRL5_C = databuf[0];

	res = hwmsen_read_byte(client, LSM6DS3_CTRL6_C, databuf);
	if(res)
	{
		GYRO_ERR("read LSM6DS3_CTRL6_C err!\n");
		return LSM6DS3_ERR_I2C;
	}
	priv ->resume_LSM6DS3_CTRL6_C = databuf[0];

	res = hwmsen_read_byte(client, LSM6DS3_CTRL7_G, databuf);
	if(res)
	{
		GYRO_ERR("read LSM6DS3_CTRL7_G err!\n");
		return LSM6DS3_ERR_I2C;
	}
	priv ->resume_LSM6DS3_CTRL7_G = databuf[0];

	res = hwmsen_read_byte(client, LSM6DS3_CTRL8_XL, databuf);
	if(res)
	{
		GYRO_ERR("read LSM6DS3_CTRL8_XL err!\n");
		return LSM6DS3_ERR_I2C;
	}
	priv ->resume_LSM6DS3_CTRL8_XL = databuf[0];

	res = hwmsen_read_byte(client, LSM6DS3_CTRL9_XL, databuf);
	if(res)
	{
		GYRO_ERR("read LSM6DS3_CTRL9_XL err!\n");
		return LSM6DS3_ERR_I2C;
	}
	priv ->resume_LSM6DS3_CTRL9_XL = databuf[0];

	res = hwmsen_read_byte(client, LSM6DS3_CTRL10_C, databuf);
	if(res)
	{
		GYRO_ERR("read LSM6DS3_CTRL10_C err!\n");
		return LSM6DS3_ERR_I2C;
	}
	priv ->resume_LSM6DS3_CTRL10_C= databuf[0];

	return 0;
}

static int recover_ST_gyro_reg_config(struct i2c_client  *client )
{
	struct lsm6ds3_gyro_i2c_data *priv = obj_i2c_data;
	u8 databuf[2];
	int res;

	databuf[1] = priv ->resume_LSM6DS3_CTRL1_XL  ;
	databuf[0] = LSM6DS3_CTRL1_XL;

	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL1_XL err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] =  priv ->resume_LSM6DS3_CTRL2_G;
	databuf[0] = LSM6DS3_CTRL2_G;

	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL2_G err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] =  priv ->resume_LSM6DS3_CTRL3_C;
	databuf[0] = LSM6DS3_CTRL3_C;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL3_C err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] =  priv ->resume_LSM6DS3_CTRL4_C;
	databuf[0] = LSM6DS3_CTRL4_C;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL4_C err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] =  priv ->resume_LSM6DS3_CTRL5_C;
	databuf[0] = LSM6DS3_CTRL5_C;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL5_C err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] =  priv ->resume_LSM6DS3_CTRL6_C;
	databuf[0] = LSM6DS3_CTRL6_C;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL6_C err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] =  priv ->resume_LSM6DS3_CTRL7_G;
	databuf[0] = LSM6DS3_CTRL7_G;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL7_G err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] =  priv ->resume_LSM6DS3_CTRL8_XL;
	databuf[0] = LSM6DS3_CTRL8_XL;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL8_XL err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] =  priv ->resume_LSM6DS3_CTRL9_XL;
	databuf[0] = LSM6DS3_CTRL9_XL;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL9_XL err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] =  priv ->resume_LSM6DS3_CTRL10_C;
	databuf[0] = LSM6DS3_CTRL10_C;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL10_C err!\n");
		return LSM6DS3_ERR_I2C;
	}

	return 0;
}

static ssize_t show_gyro_self_data(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm6ds3_i2c_client;

	int temp[3] ;
	int OUTX_NOST,OUTY_NOST,OUTZ_NOST;
	int OUTX_ST,OUTY_ST,OUTZ_ST;
	int ABS_OUTX,ABS_OUTY,ABS_OUTZ;
	//int err;
	int res;
	u8 databuf[2];
	u8 result = -1;

	res = back_ST_gyro_reg_config(client);
	if(res < 0)
	{
		GYRO_ERR("back ST_reg err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] = 0x00 ;
	databuf[0] = LSM6DS3_CTRL1_XL;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL1_XL err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] = 0x5c ;
	databuf[0] = LSM6DS3_CTRL2_G;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL2_G err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] = 0x44 ;
	databuf[0] = LSM6DS3_CTRL3_C;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL3_C err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] = 0x00 ;
	databuf[0] = LSM6DS3_CTRL4_C;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL4_C err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] = 0x00 ;
	databuf[0] = LSM6DS3_CTRL5_C;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL5_C err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] = 0x00 ;
	databuf[0] = LSM6DS3_CTRL6_C;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL6_C err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] = 0x00 ;
	databuf[0] = LSM6DS3_CTRL7_G;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL7_G err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] = 0x00 ;
	databuf[0] = LSM6DS3_CTRL8_XL;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL8_XL err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] = 0x00 ;
	databuf[0] = LSM6DS3_CTRL9_XL;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL9_XL err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] = 0x38 ;
	databuf[0] = LSM6DS3_CTRL10_C;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL10_C err!\n");
		return LSM6DS3_ERR_I2C;
	}

	//LSM6DS3_dumpReg(client);
	msleep(800);

	GYRO_ERR("setting ST!\n");

	lsm303d_gyro_get_ST_data(client, temp);

	OUTX_NOST =  temp[0] /5000;                                 // mdps transfer to dps
	OUTY_NOST =  temp[1] /5000;
	OUTZ_NOST =  temp[2] /5000;
	GYRO_ERR("OUTX_NOST %d, OUTY_NOST %d,OUTZ_NOST %d \n",OUTX_NOST,OUTY_NOST,OUTZ_NOST);

	databuf[1] = 0x04 ;
	databuf[0] = LSM6DS3_CTRL5_C;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write LSM6DS3_CTRL5_C err!\n");
		return LSM6DS3_ERR_I2C;
	}

	//LSM6DS3_dumpReg(client);
	msleep(60);

	lsm303d_gyro_get_ST_data(client, temp);

	OUTX_ST =  temp[0] /5000;
	OUTY_ST =  temp[1] /5000;
	OUTZ_ST =  temp[2] /5000;
	GYRO_ERR("OUTX_ST %d, OUTY_ST %d,OUTZ_ST %d \n",OUTX_ST,OUTY_ST,OUTZ_ST);

	ABS_OUTX = ABS_ST(OUTX_ST - OUTX_NOST);
	ABS_OUTY = ABS_ST(OUTY_ST - OUTY_NOST);
	ABS_OUTZ = ABS_ST(OUTZ_ST - OUTZ_NOST);

	if(((ABS_OUTX >= MIN_ST)&&(ABS_OUTX <= MAX_ST))
	 	&& ((ABS_OUTY >= MIN_ST)&&(ABS_OUTY <= MAX_ST))
	        && ((ABS_OUTZ >= MIN_ST)&&(ABS_OUTZ <= MAX_ST)))
	{
		GYRO_ERR(" SELF TEST SUCCESS ABS_OUTX,ABS_OUTY,ABS_OUTZ:%d ,%d,%d\n", ABS_OUTX,ABS_OUTY,ABS_OUTZ);
		result = 1;
	}
	else
	{
		GYRO_ERR(" SELF TEST FAIL ABS_OUTX,ABS_OUTY,ABS_OUTZ:%d ,%d,%d\n", ABS_OUTX,ABS_OUTY,ABS_OUTZ);
		result = 0;
	}

	/*  databuf[1] = 0x00 ;
	databuf[0] = LSM6DS3_CTRL1_XL;

	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GSE_ERR("write LSM6DS3_CTRL1_XL err!\n");
		return LSM6DS3_ERR_I2C;
	}

	databuf[1] = 0x00 ;
	databuf[0] = LSM6DS3_CTRL5_C;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GSE_ERR("write LSM6DS3_CTRL5_C err!\n");
		return LSM6DS3_ERR_I2C;
	}

	LSM6DS3_init_client(client, false); */

	res = recover_ST_gyro_reg_config(client);

	if(res < 0)
	{
		GYRO_ERR("recover ST_reg err!\n");
		return LSM6DS3_ERR_I2C;
	}

	return sprintf(buf, "%d\n", result);
}
#endif
/*lenovo-sw caoyi1 add for selftest function 20150708 end*/

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo, S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(chipid, S_IRUGO, show_chipid_value, NULL);
static DRIVER_ATTR(sensordata, S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, show_trace_value, store_trace_value);
static DRIVER_ATTR(status, S_IWUSR | S_IRUGO, show_status_value, store_status_value);
/*lenovo-sw caoyi1 add for Gyroscope APK calibration 20150513 begin*/
// add for lenovo calib_hal
#ifdef LSM6DS3_CALIB_HAL
static DRIVER_ATTR(gyroscope_cali_val_x, S_IWUSR | S_IRUGO, show_cali_gyrox_value, store_cali_gyrox_value);
static DRIVER_ATTR(gyroscope_cali_val_y, S_IWUSR | S_IRUGO, show_cali_gyroy_value, store_cali_gyroy_value);
static DRIVER_ATTR(gyroscope_cali_val_z, S_IWUSR | S_IRUGO, show_cali_gyroz_value, store_cali_gyroz_value);
#endif
/*lenovo-sw caoyi1 add for Gyroscope APK calibration 20150513 end*/
/*lenovo-sw caoyi1 add for selftest function 20150708 begin*/
#ifdef LSM6DS3_FTM_ENABLE
static DRIVER_ATTR(selftest,  S_IRUGO , show_gyro_self_data, NULL);
#endif
/*lenovo-sw caoyi1 add for selftest function 20150708 end*/
/*----------------------------------------------------------------------------*/
static struct driver_attribute *LSM6DS3_attr_list[] = {
	&driver_attr_chipinfo,     /*chip information*/
	&driver_attr_chipid,     /*chip id*/
	&driver_attr_sensordata,   /*dump sensor data*/
	&driver_attr_trace,        /*trace log*/
	&driver_attr_status,
/*lenovo-sw caoyi1 add for Gyroscope APK calibration 20150513 begin*/
#ifdef LSM6DS3_CALIB_HAL
	&driver_attr_gyroscope_cali_val_x,   /* add for lenovo hal calib syspoint by jonny */
	&driver_attr_gyroscope_cali_val_y ,   /* add for lenovo hal calib syspoint by jonny */
	&driver_attr_gyroscope_cali_val_z ,   /* add for lenovo hal calib syspoint by jonny */
#endif
/*lenovo-sw caoyi1 add for Gyroscope APK calibration 20150513 end*/
/*lenovo-sw caoyi1 add for selftest function 20150708 begin*/
#ifdef LSM6DS3_FTM_ENABLE
	&driver_attr_selftest,
#endif
/*lenovo-sw caoyi1 add for selftest function 20150708 end*/
};
/*----------------------------------------------------------------------------*/
static int lsm6ds3_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(LSM6DS3_attr_list)/sizeof(LSM6DS3_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if(0 != (err = driver_create_file(driver,  LSM6DS3_attr_list[idx])))
		{
			GYRO_ERR("driver_create_file (%s) = %d\n",  LSM6DS3_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int lsm6ds3_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof( LSM6DS3_attr_list)/sizeof( LSM6DS3_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver,  LSM6DS3_attr_list[idx]);
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS3_gyro_init_client(struct i2c_client *client, bool enable)
{
	struct lsm6ds3_gyro_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	GYRO_LOG("%s lsm6ds3 addr %x!\n", __FUNCTION__, client->addr);

	res = LSM6DS3_CheckDeviceID(client);
	if(res != LSM6DS3_SUCCESS)
	{
		return res;
	}

	res = LSM6DS3_Set_RegInc(client, true);
	if(res != LSM6DS3_SUCCESS)
	{
		return res;
	}

	res = LSM6DS3_gyro_SetFullScale(client,LSM6DS3_GYRO_RANGE_2000DPS);//we have only this choice
	if(res != LSM6DS3_SUCCESS)
	{
		return res;
	}

	//
	res = LSM6DS3_gyro_SetSampleRate(client, LSM6DS3_GYRO_ODR_104HZ);
	if(res != LSM6DS3_SUCCESS )
	{
		return res;
	}
	res = LSM6DS3_gyro_SetPowerMode(client, enable);
	if(res != LSM6DS3_SUCCESS)
	{
		return res;
	}

	GYRO_LOG("LSM6DS3_gyro_init_client OK!\n");
	//acc setting

#ifdef CONFIG_LSM6DS3_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif

	return LSM6DS3_SUCCESS;
}
/*----------------------------------------------------------------------------*/
#ifdef LSM6DS3_GYRO_NEW_ARCH
static int lsm6ds3_gyro_open_report_data(int open)
{
    //should queuq work to report event if  is_report_input_direct=true
    return 0;
}

// if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL
static int lsm6ds3_gyro_enable_nodata(int en)
{
	int value = en;
	int err = 0;
	char databuf[6];
	int ret = 0;
	struct lsm6ds3_gyro_i2c_data *priv = obj_i2c_data;
	
	if(priv == NULL)
	{
		GYRO_ERR("obj_i2c_data is NULL!\n");
		return -1;
	}

	if(value == 1)
	{
		enable_status = true;
	}
	else
	{
		enable_status = false;
	}
	GYRO_LOG("enable value=%d, sensor_power =%d\n",value,sensor_power);
	if(((value == 0) && (sensor_power == false)) ||((value == 1) && (sensor_power == true)))
	{
		GYRO_LOG("Gsensor device have updated!\n");
	}
	else
	{
		err = LSM6DS3_gyro_SetPowerMode( priv->client, enable_status);
	}
	
	/****zhaolong add for test************/
	ret = hwmsen_read_block(priv->client, LSM6DS3_OUT_TEMP_L, databuf, 2);
	printk("zhaolong debug:LSM6DS3_OUT_TEMP_L==111===databuf[0]=0x%x, databuf[1]=0x%x, databuf[2]=0x%x\n", databuf[0], databuf[1], databuf[2]);
	ret = hwmsen_read_block(priv->client, LSM6DS3_OUT_TEMP_H, databuf, 2);
	printk("zhaolong debug:LSM6DS3_OUT_TEMP_H==222===databuf[0]=0x%x, databuf[1]=0x%x, databuf[2]=0x%x\n", databuf[0], databuf[1], databuf[2]);
	/*****************end*****************/

	GYRO_LOG("mc3xxx_enable_nodata OK!\n");
	return err;
}

static int lsm6ds3_gyro_set_delay(u64 ns)
{
#if 0
    int value =0;

    value = (int)ns/1000/1000;

	struct lsm6ds3_gyro_i2c_data *priv = obj_i2c_data;

	if(priv == NULL)
	{
		GYRO_ERR("obj_i2c_data is NULL!\n");
		return -1;
	}

    GYRO_LOG("mc3xxx_set_delay (%d), chip only use 1024HZ \n",value);
#endif
    return 0;
}

static int lsm6ds3_gyro_get_data(int* x ,int* y,int* z, int* status)
{
    char buff[LSM6DS3_BUFSIZE];
	struct lsm6ds3_gyro_i2c_data *priv = obj_i2c_data;

	if(priv == NULL)
	{
		GYRO_ERR("obj_i2c_data is NULL!\n");
		return -1;
	}
	if(atomic_read(&priv->trace) & GYRO_TRC_DATA)
	{
		GYRO_LOG("%s (%d),	\n",__FUNCTION__,__LINE__);
	}
	memset(buff, 0, sizeof(buff));
	LSM6DS3_ReadGyroData(priv->client, buff, LSM6DS3_BUFSIZE);

	sscanf(buff, "%x %x %x", x, y, z);
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;

    return 0;
}
#endif

#ifndef LSM6DS3_GYRO_NEW_ARCH
/*----------------------------------------------------------------------------*/
int LSM6DS3_gyro_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	struct lsm6ds3_gyro_i2c_data *priv = (struct lsm6ds3_gyro_i2c_data*)self;
	struct hwm_sensor_data* gyro_data;
	char buff[LSM6DS3_BUFSIZE];

	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				GYRO_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			else
			{

			}
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				GYRO_ERR("Enable gyroscope parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value == 1)
				{
					enable_status = true;
				}
				else
				{
					enable_status = false;
				}
				GYRO_LOG("enable value=%d, sensor_power =%d\n",value,sensor_power);
				if(((value == 0) && (sensor_power == false)) ||((value == 1) && (sensor_power == true)))
				{
					GYRO_LOG("Gsensor device have updated!\n");
				}
				else
				{
					err = LSM6DS3_gyro_SetPowerMode( priv->client, enable_status);
				}
			#if INV_GYRO_AUTO_CALI == 1
				inv_gyro_power_state = sensor_power;
				/* put this in where gyro power is changed, waking up mpu daemon */
				sysfs_notify(&inv_daemon_device->kobj, NULL, "inv_gyro_power_state");
			#endif
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				GYRO_ERR("get gyroscope data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				gyro_data = (hwm_sensor_data *)buff_out;
				LSM6DS3_ReadGyroData(priv->client, buff, LSM6DS3_BUFSIZE);
				sscanf(buff, "%x %x %x", &gyro_data->values[0],
									&gyro_data->values[1], &gyro_data->values[2]);
				gyro_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
				gyro_data->value_divide = 1000;
			#if INV_GYRO_AUTO_CALI == 1
				/* put this in where gyro data is ready to report to hal, waking up mpu daemon */
				sysfs_notify(&inv_daemon_device->kobj, NULL, "inv_gyro_data_ready");
			#endif
				if(atomic_read(&priv->trace) & GYRO_TRC_DATA)
				{
					GYRO_LOG("===>LSM6DS3_gyro_operate x=%d,y=%d,z=%d \n", gyro_data->values[0],gyro_data->values[1],gyro_data->values[2]);
				}
			}
			break;
		default:
			GYRO_ERR("gyroscope operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}

	return err;
}
#endif

/******************************************************************************
 * Function Configuration
******************************************************************************/
static int lsm6ds3_open(struct inode *inode, struct file *file)
{
	file->private_data = lsm6ds3_i2c_client;

	if(file->private_data == NULL)
	{
		GYRO_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

/*----------------------------------------------------------------------------*/
static int lsm6ds3_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/*----------------------------------------------------------------------------*/
static long lsm6ds3_gyro_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;

	char strbuf[LSM6DS3_BUFSIZE] = {0};
	void __user *data;
	long err = 0;
	int copy_cnt = 0;
	struct SENSOR_DATA sensor_data;
	int cali[3] = {0};
	int smtRes=0;
	//GYRO_FUN();

	if(_IOC_DIR(cmd) & _IOC_READ)
	{
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	}
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
	{
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}

	if(err)
	{
		GYRO_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch(cmd)
	{
		case GYROSCOPE_IOCTL_INIT:
			LSM6DS3_gyro_init_client(client, false);
			break;

		case GYROSCOPE_IOCTL_SMT_DATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;
			}

			GYRO_LOG("IOCTL smtRes: %d!\n", smtRes);
			copy_cnt = copy_to_user(data, &smtRes,  sizeof(smtRes));

			if(copy_cnt)
			{
				err = -EFAULT;
				GYRO_ERR("copy gyro data to user failed!\n");
			}
			GYRO_LOG("copy gyro data to user OK: %d!\n", copy_cnt);
			break;


		case GYROSCOPE_IOCTL_READ_SENSORDATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;
			}

			LSM6DS3_ReadGyroData(client, strbuf, LSM6DS3_BUFSIZE);
			if(copy_to_user(data, strbuf, sizeof(strbuf)))
			{
				err = -EFAULT;
				break;
			}
			break;

		case GYROSCOPE_IOCTL_SET_CALI:
			data = (void __user*)arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;
			}
			if(copy_from_user(&sensor_data, data, sizeof(sensor_data)))
			{
				err = -EFAULT;
				break;
			}
			else
			{
				cali[LSM6DS3_AXIS_X] = sensor_data.x*1000/131/LSM6DS3_GYRO_SENSITIVITY_2000DPS;
				cali[LSM6DS3_AXIS_Y] = sensor_data.y*1000/131/LSM6DS3_GYRO_SENSITIVITY_2000DPS;
				cali[LSM6DS3_AXIS_Z] = sensor_data.z*1000/131/LSM6DS3_GYRO_SENSITIVITY_2000DPS;
				err = LSM6DS3_gyro_WriteCalibration(client, cali);
			}
			break;

		case GYROSCOPE_IOCTL_CLR_CALI:
			err = LSM6DS3_gyro_ResetCalibration(client);
			break;

		case GYROSCOPE_IOCTL_GET_CALI:
			data = (void __user*)arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;
			}
			err = LSM6DS3_gyro_ReadCalibration(client, cali);
			if(err)
			{
				break;
			}

			sensor_data.x = cali[LSM6DS3_AXIS_X]*LSM6DS3_GYRO_SENSITIVITY_2000DPS*131/1000;
			sensor_data.y = cali[LSM6DS3_AXIS_Y]*LSM6DS3_GYRO_SENSITIVITY_2000DPS*131/1000;
			sensor_data.z = cali[LSM6DS3_AXIS_Z]*LSM6DS3_GYRO_SENSITIVITY_2000DPS*131/1000;

			if(copy_to_user(data, &sensor_data, sizeof(sensor_data)))
			{
				err = -EFAULT;
				break;
			}
			break;

	#if INV_GYRO_AUTO_CALI == 1
		case GYROSCOPE_IOCTL_READ_SENSORDATA_RAW:
			data = (void __user *) arg;
			if (data == NULL) {
				err = -EINVAL;
				break;
			}

			LSM6DS3_ReadGyroDataRaw(client, strbuf, LSM6DS3_BUFSIZE);
			if (copy_to_user(data, strbuf, sizeof(strbuf))) {
				err = -EFAULT;
				break;
			}
			break;

		case GYROSCOPE_IOCTL_READ_TEMPERATURE:
			data = (void __user *) arg;
			if (data == NULL) {
				err = -EINVAL;
				break;
			}

			LSM6DS3_ReadTemperature(client, strbuf, LSM6DS3_BUFSIZE);
			if (copy_to_user(data, strbuf, sizeof(strbuf))) {
				err = -EFAULT;
				break;
			}
			break;

		case GYROSCOPE_IOCTL_GET_POWER_STATUS:
			data = (void __user *) arg;
			if (data == NULL) {
				err = -EINVAL;
				break;
			}

			LSM6DS3_ReadPowerStatus(client, strbuf, LSM6DS3_BUFSIZE);
			if (copy_to_user(data, strbuf, sizeof(strbuf))) {
				err = -EFAULT;
				break;
			}
			break;
	#endif

		default:
			GYRO_ERR("unknown IOCTL: 0x%08x\n", cmd);
			err = -ENOIOCTLCMD;
			break;
	}
	return err;
}

#ifdef CONFIG_COMPAT
static long lsm6ds3_gyro_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;

	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
		 case COMPAT_GYROSCOPE_IOCTL_INIT:
			 if(arg32 == NULL)
			 {
				 GYRO_ERR("invalid argument.");
				 return -EINVAL;
			 }

			 ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_INIT,
							(unsigned long)arg32);
			 if (ret){
			 	GYRO_ERR("GYROSCOPE_IOCTL_INIT unlocked_ioctl failed.");
				return ret;
			 }

			 break;

		 case COMPAT_GYROSCOPE_IOCTL_SET_CALI:
			 if(arg32 == NULL)
			 {
				 GYRO_ERR("invalid argument.");
				 return -EINVAL;
			 }

			 ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_SET_CALI,
							(unsigned long)arg32);
			 if (ret){
			 	GYRO_ERR("GYROSCOPE_IOCTL_SET_CALI unlocked_ioctl failed.");
				return ret;
			 }

			 break;

		 case COMPAT_GYROSCOPE_IOCTL_CLR_CALI:
			 if(arg32 == NULL)
			 {
				 GYRO_ERR("invalid argument.");
				 return -EINVAL;
			 }

			 ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_CLR_CALI,
							(unsigned long)arg32);
			 if (ret){
			 	GYRO_ERR("GYROSCOPE_IOCTL_CLR_CALI unlocked_ioctl failed.");
				return ret;
			 }

			 break;

		 case COMPAT_GYROSCOPE_IOCTL_GET_CALI:
			 if(arg32 == NULL)
			 {
				 GYRO_ERR("invalid argument.");
				 return -EINVAL;
			 }

			 ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_GET_CALI,
							(unsigned long)arg32);
			 if (ret){
			 	GYRO_ERR("GYROSCOPE_IOCTL_GET_CALI unlocked_ioctl failed.");
				return ret;
			 }

			 break;

		 case COMPAT_GYROSCOPE_IOCTL_READ_SENSORDATA:
			 if(arg32 == NULL)
			 {
				 GYRO_ERR("invalid argument.");
				 return -EINVAL;
			 }

			 ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_READ_SENSORDATA,
							(unsigned long)arg32);
			 if (ret){
			 	GYRO_ERR("GYROSCOPE_IOCTL_READ_SENSORDATA unlocked_ioctl failed.");
				return ret;
			 }

			 break;

		 default:
			 printk(KERN_ERR "%s not supported = 0x%04x", __FUNCTION__, cmd);
			 return -ENOIOCTLCMD;
			 break;
	}
	return ret;
}
#endif

/*----------------------------------------------------------------------------*/
static struct file_operations lsm6ds3_gyro_fops = {
	.owner = THIS_MODULE,
	.open = lsm6ds3_open,
	.release = lsm6ds3_release,
	.unlocked_ioctl = lsm6ds3_gyro_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = lsm6ds3_gyro_compat_ioctl,
#endif
};

/*----------------------------------------------------------------------------*/
static struct miscdevice lsm6ds3_gyro_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gyroscope",
	.fops = &lsm6ds3_gyro_fops,
};

/*----------------------------------------------------------------------------*/
#ifndef CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int lsm6ds3_gyro_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct lsm6ds3_gyro_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	GYRO_FUN();

	if(msg.event == PM_EVENT_SUSPEND)
	{
		if(obj == NULL)
		{
			GYRO_ERR("null pointer!!\n");
			return -1;;
		}
		atomic_set(&obj->suspend, 1);
		err = LSM6DS3_gyro_SetPowerMode(obj->client, false);
		if(err)
		{
			GYRO_ERR("write power control fail!!\n");
			return err;
		}

		sensor_power = false;

		LSM6DS3_power(obj->hw, 0);
	#if INV_GYRO_AUTO_CALI == 1
		inv_gyro_power_state = sensor_power;
		/* inv_gyro_power_state = 0; */
		/* put this in where gyro power is changed, waking up mpu daemon */
		sysfs_notify(&inv_daemon_device->kobj, NULL, "inv_gyro_power_state");
	#endif
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int lsm6ds3_gyro_resume(struct i2c_client *client)
{
	struct lsm6ds3_gyro_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	GYRO_FUN();

	if(obj == NULL)
	{
		GYRO_ERR("null pointer!!\n");
		return -EINVAL;
	}

	LSM6DS3_power(obj->hw, 1);

	err = LSM6DS3_gyro_SetPowerMode(obj->client, enable_status);
	if(err)
	{
		GYRO_ERR("initialize client fail! err code %d!\n", err);
		return err;
	}
	atomic_set(&obj->suspend, 0);

	return 0;
}
/*----------------------------------------------------------------------------*/
#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void lsm6ds3_gyro_early_suspend(struct early_suspend *h)
{
	struct lsm6ds3_gyro_i2c_data *obj = container_of(h, struct lsm6ds3_gyro_i2c_data, early_drv);
	int err;
	GYRO_FUN();

	if(obj == NULL)
	{
		GYRO_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1);
	err = LSM6DS3_gyro_SetPowerMode(obj->client, false);
	if(err)
	{
		GYRO_ERR("write power control fail!!\n");
		return;
	}

	sensor_power = false;

	LSM6DS3_power(obj->hw, 0);
}

/*----------------------------------------------------------------------------*/
static void lsm6ds3_gyro_late_resume(struct early_suspend *h)
{
	struct lsm6ds3_gyro_i2c_data *obj = container_of(h, struct lsm6ds3_gyro_i2c_data, early_drv);
	int err;
	GYRO_FUN();

	if(obj == NULL)
	{
		GYRO_ERR("null pointer!!\n");
		return;
	}

	LSM6DS3_power(obj->hw, 1);
	err = LSM6DS3_gyro_SetPowerMode(obj->client, enable_status);
	if(err)
	{
		GYRO_ERR("initialize client fail! err code %d!\n", err);
		return;
	}
	atomic_set(&obj->suspend, 0);
}
/*----------------------------------------------------------------------------*/
#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int lsm6ds3_gyro_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct lsm6ds3_gyro_i2c_data *obj;

#ifdef LSM6DS3_GYRO_NEW_ARCH
	struct gyro_control_path ctl={0};
	struct gyro_data_path data={0};
#else
	struct hwmsen_object gyro_sobj;
#endif
	int err = 0;
#if INV_GYRO_AUTO_CALI == 1
	int i;
	int result;
#endif
	GYRO_FUN();

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(struct lsm6ds3_gyro_i2c_data));

	obj->hw = hw;
	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if(err)
	{
		GYRO_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit;
	}

	client->addr = 0x6B;

	obj_i2c_data = obj;
	obj->client = client;
	new_client = obj->client;
	i2c_set_clientdata(new_client,obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);

	lsm6ds3_i2c_client = new_client;
	err = LSM6DS3_gyro_init_client(new_client, false);
	if(err)
	{
		goto exit_init_failed;
	}

	err = misc_register(&lsm6ds3_gyro_device);
	if(err)
	{
		GYRO_ERR("lsm6ds3_gyro_device misc register failed!\n");
		goto exit_misc_device_register_failed;
	}

#ifdef LSM6DS3_GYRO_NEW_ARCH
	err = lsm6ds3_create_attr(&(lsm6ds3_gyro_init_info.platform_diver_addr->driver));
#else
	err = lsm6ds3_create_attr(&lsm6ds3_driver.driver);
#endif
	if(err)
	{
		GYRO_ERR("lsm6ds3 create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

#ifndef LSM6DS3_GYRO_NEW_ARCH
	gyro_sobj.self = obj;
	gyro_sobj.polling = 1;
	gyro_sobj.sensor_operate = LSM6DS3_gyro_operate;
	err = hwmsen_attach(ID_GYROSCOPE, &gyro_sobj);
	if(err)
	{
		GYRO_ERR("hwmsen_attach Gyroscope fail = %d\n", err);
		goto exit_kfree;
	}
#else
	ctl.open_report_data= lsm6ds3_gyro_open_report_data;
	ctl.enable_nodata = lsm6ds3_gyro_enable_nodata;
	ctl.set_delay  = lsm6ds3_gyro_set_delay;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = obj->hw->is_batch_supported;

	err = gyro_register_control_path(&ctl);
	if(err)
	{
		GYRO_ERR("register acc control path err\n");
		goto exit_kfree;
	}

	data.get_data = lsm6ds3_gyro_get_data;
	data.vender_div = DEGREE_TO_RAD;
	err = gyro_register_data_path(&data);
	if(err)
	{
		GYRO_ERR("register acc data path err= %d\n", err);
		goto exit_kfree;
	}
#endif

#if INV_GYRO_AUTO_CALI == 1
	mutex_init(&obj->temperature_mutex);
	mutex_init(&obj->raw_data_mutex);

	/* create a class to avoid event drop by uevent_ops->filter function (dev_uevent_filter()) */
	inv_daemon_class = class_create(THIS_MODULE, INV_DAEMON_CLASS_NAME);
	if (IS_ERR(inv_daemon_class)) {
		GYRO_ERR("cannot create inv daemon class, %s\n", INV_DAEMON_CLASS_NAME);
		goto exit_class_create_failed;
	}

	inv_daemon_device = kzalloc(sizeof(struct device), GFP_KERNEL);
	if (!inv_daemon_device) {
		GYRO_ERR("cannot allocate inv daemon device, %s\n", INV_DAEMON_DEVICE_NAME);
		goto exit_device_register_failed;
	}
	inv_daemon_device->init_name = INV_DAEMON_DEVICE_NAME;
	inv_daemon_device->class = inv_daemon_class;
	inv_daemon_device->release = (void (*)(struct device *))kfree;
	result = device_register(inv_daemon_device);
	if (result) {
		GYRO_ERR("cannot register inv daemon device, %s\n", INV_DAEMON_DEVICE_NAME);
		goto exit_device_register_failed;
	}

	result = 0;
	for (i = 0; i < ARRAY_SIZE(inv_daemon_dev_attributes); i++) {
		result = device_create_file(inv_daemon_device, inv_daemon_dev_attributes[i]);
		if (result)
			break;
	}
	if (result) {
		while (--i >= 0)
			device_remove_file(inv_daemon_device, inv_daemon_dev_attributes[i]);
		GYRO_ERR("cannot create inv daemon dev attr.\n");
		goto exit_create_file_failed;
	}
#endif

#ifdef LSM6DS3_GYRO_NEW_ARCH
	lsm6ds3_gyro_init_flag = 0;
#endif
	GYRO_LOG("%s: OK\n", __func__);
	return 0;

#if INV_GYRO_AUTO_CALI == 1
exit_create_file_failed:
	device_unregister(inv_daemon_device);
exit_device_register_failed:
	class_destroy(inv_daemon_class);
exit_class_create_failed:
	hwmsen_detach(ID_GYROSCOPE);
#endif
exit_create_attr_failed:
	misc_deregister(&lsm6ds3_gyro_device);
exit_misc_device_register_failed:
exit_init_failed:
exit_kfree:
	kfree(obj);
	obj = NULL;
exit:
#ifdef LSM6DS3_GYRO_NEW_ARCH
	lsm6ds3_gyro_init_flag = -1;
#endif
	GYRO_ERR("%s: err = %d\n", __func__, err);
	return err;
}

/*----------------------------------------------------------------------------*/
static int lsm6ds3_gyro_i2c_remove(struct i2c_client *client)
{
	int err = 0;

#if INV_GYRO_AUTO_CALI == 1
	int i;

	for (i = 0; i < ARRAY_SIZE(inv_daemon_dev_attributes); i++)
		device_remove_file(inv_daemon_device, inv_daemon_dev_attributes[i]);

	device_unregister(inv_daemon_device);
	class_destroy(inv_daemon_class);
#endif
#ifndef LSM6DS3_GYRO_NEW_ARCH
	err = lsm6ds3_delete_attr(&lsm6ds3_driver.driver);
#else
	err = lsm6ds3_delete_attr(&(lsm6ds3_gyro_init_info.platform_diver_addr->driver));
#endif
	if(err)
	{
		GYRO_ERR("lsm6ds3_gyro_i2c_remove fail: %d\n", err);
	}

	err = misc_deregister(&lsm6ds3_gyro_device);
	if(err)
	{
		GYRO_ERR("misc_deregister lsm6ds3_gyro_device fail: %d\n", err);
	}

	lsm6ds3_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}
/*----------------------------------------------------------------------------*/
static int lsm6ds3_gyro_local_init(struct platform_device* pdev)
{
	//struct gyro_hw *gy_hw = get_cust_gyro_hw();
	//GYRO_FUN();
	gyroPlatformDevice = pdev;

	LSM6DS3_power(hw, 1);

	if(i2c_add_driver(&lsm6ds3_gyro_i2c_driver))
	{
		GYRO_ERR("add driver error\n");
		return -1;
	}
	if(lsm6ds3_gyro_init_flag == -1)
	{
		GYRO_ERR("%s init failed!\n", __FUNCTION__);
		return -1;
	}
	return 0;
}

static int lsm6ds3_gyro_local_uninit(void)
{
	struct gyro_hw *gy_hw = get_cust_gyro_hw();

	GYRO_FUN();
	LSM6DS3_power(gy_hw, 0);
	i2c_del_driver(&lsm6ds3_gyro_i2c_driver);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init lsm6ds3_gyro_init(void)
{
	const char* name = "mediatek,lsm6ds3_gyro";

	hw = get_gyro_dts_func(name, hw);
	if (!hw)
		GYRO_ERR("get dts info fail\n");

	gyro_driver_add(&lsm6ds3_gyro_init_info);
	GYRO_ERR("lsm6ds3_gyro_init\n");
	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit lsm6ds3_gyro_exit(void)
{
	GYRO_FUN();
}
/*----------------------------------------------------------------------------*/
module_init(lsm6ds3_gyro_init);
module_exit(lsm6ds3_gyro_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LSM6DS3 Accelerometer and gyroscope driver");
MODULE_AUTHOR("xj.wang@mediatek.com, darren.han@st.com");
/*----------------------------------------------------------------- LSM6DS3 ------------------------------------------------------------------*/
