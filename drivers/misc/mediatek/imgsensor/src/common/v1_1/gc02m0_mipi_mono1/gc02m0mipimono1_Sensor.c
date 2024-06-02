/*****************************************************************************
* Filename:
 * ---------
 *     GC02m0MIPI_Sensor.c
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
/*#include <asm/atomic.h>*/
/*#include <asm/system.h>*/
/*#include <linux/xlog.h>*/

#include "kd_camera_typedef.h"
//#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "gc02m0mipimono1_Sensor.h"

#undef IMGSENSOR_I2C_1000K
#include "../imgsensor_i2c.h"
#include "../imgsensor_hw.h"

#ifndef VENDOR_EDIT
#define VENDOR_EDIT
#endif


/****************************Modify Following Strings for Debug****************************/
#define PFX "GC02M0_camera_sensor"
#define LOG_1 LOG_INF("GC02M0,MIPI 1LANE\n")
/****************************   Modify end    *******************************************/

#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)

//#define GC02m0OTP_FOR_CUSTOMER
//static kal_uint32 Dgain_ratio = 1;

#ifdef VENDOR_EDIT
/*Shounan.Yang@Camera.Driver  add for 18011  board 20190620*/
#define DEVICE_VERSION_GC02M0    "gc02m0"
#define IMGSENSOR_MODULE_ID_SHINE  0x07
extern void register_imgsensor_deviceinfo(char *name, char *version, u8 module_id);
static uint8_t deviceInfo_register_value;
/* Feiping.Li@Camera.Drv, 20190624, add for speed up sensor init*/
#define USE_BURST_MODE
/* Feiping.Li@Camera.Drv, 20190710, add for pull-up avdd when main sensor is powered */
static int is_using_gc02m0 = 0;
#endif

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static imgsensor_info_struct imgsensor_info = {
       .sensor_id = GC02M0_SENSOR_ID,        //record sensor id defined in Kd_imgsensor.h

       .checksum_value = 0xf7375923,        //checksum value for Camera Auto Test

       .pre = {
             .pclk = 84000000,                //record different mode's pclk
             .linelength = 2192,                //record different mode's linelength
             .framelength = 1268,            //record different mode's framelength
             .startx = 0,                    //record different mode's startx of grabwindow
             .starty = 0,                    //record different mode's starty of grabwindow
             .grabwindow_width = 1600,        //record different mode's width of grabwindow //1296
             .grabwindow_height = 1200,        //record different mode's height of grabwindow //972
             /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
             .mipi_data_lp2hs_settle_dc = 85,//unit , ns
             /*     following for GetDefaultFramerateByScenario()    */
             .max_framerate = 300,
             .mipi_pixel_rate = 67200000,
    },
       .cap = {
             .pclk = 84000000,                //record different mode's pclk
             .linelength = 2192,                //record different mode's linelength
             .framelength = 1268,            //record different mode's framelength
             .startx = 0,                    //record different mode's startx of grabwindow
             .starty = 0,                    //record different mode's starty of grabwindow
             .grabwindow_width = 1600,        //record different mode's width of grabwindow //1296
             .grabwindow_height = 1200,        //record different mode's height of grabwindow //972
             /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
             .mipi_data_lp2hs_settle_dc = 85,//unit , ns
             /*     following for GetDefaultFramerateByScenario()    */
             .max_framerate = 300,
             .mipi_pixel_rate = 67200000,
    },
       .cap1 = {                            //capture for PIP 24fps relative information, capture1 mode must use same framelength, linelength with Capture mode for shutter calculate
             .pclk = 84000000,                //record different mode's pclk
             .linelength = 2192,                //record different mode's linelength
             .framelength = 1268,            //record different mode's framelength
             .startx = 0,                    //record different mode's startx of grabwindow
             .starty = 0,                    //record different mode's starty of grabwindow
             .grabwindow_width = 1600,        //record different mode's width of grabwindow //1296
             .grabwindow_height = 1200,        //record different mode's height of grabwindow //972
             /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
             .mipi_data_lp2hs_settle_dc = 85,//unit , ns
             /*     following for GetDefaultFramerateByScenario()    */
             .max_framerate = 300,
             .mipi_pixel_rate = 67200000,
    },
       .normal_video = {
           	 .pclk = 84000000,                //record different mode's pclk
             .linelength = 2192,                //record different mode's linelength
             .framelength = 1268,            //record different mode's framelength
             .startx = 0,                    //record different mode's startx of grabwindow
             .starty = 0,                    //record different mode's starty of grabwindow
             .grabwindow_width = 1600,        //record different mode's width of grabwindow //1296
             .grabwindow_height = 1200,        //record different mode's height of grabwindow //972
             /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
             .mipi_data_lp2hs_settle_dc = 85,//unit , ns
             /*     following for GetDefaultFramerateByScenario()    */
             .max_framerate = 300,
             .mipi_pixel_rate = 67200000,

    },
       .hs_video = {
             .pclk = 84000000,                //record different mode's pclk
             .linelength = 2192,                //record different mode's linelength
             .framelength = 1268,            //record different mode's framelength
             .startx = 0,                    //record different mode's startx of grabwindow
             .starty = 0,                    //record different mode's starty of grabwindow
             .grabwindow_width = 1600,        //record different mode's width of grabwindow //1296
             .grabwindow_height = 1200,        //record different mode's height of grabwindow //972
             /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
             .mipi_data_lp2hs_settle_dc = 85,//unit , ns
             /*     following for GetDefaultFramerateByScenario()    */
             .max_framerate = 300,
             .mipi_pixel_rate = 67200000,
    },
        .slim_video = {
          	 .pclk = 84000000,                //record different mode's pclk
             .linelength = 2192,                //record different mode's linelength
             .framelength = 1268,            //record different mode's framelength
             .startx = 0,                    //record different mode's startx of grabwindow
             .starty = 0,                    //record different mode's starty of grabwindow
             .grabwindow_width = 1600,        //record different mode's width of grabwindow //1296
             .grabwindow_height = 1200,        //record different mode's height of grabwindow //972
             /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
             .mipi_data_lp2hs_settle_dc = 85,//unit , ns
             /*     following for GetDefaultFramerateByScenario()    */
             .max_framerate = 300,
             .mipi_pixel_rate = 67200000,
    },
         #ifdef VENDOR_EDIT
         /*Feiping.Li@Camera.Driver, 20190524, add for dual cam*/
        .custom1 = {
             .pclk = 84000000,                //record different mode's pclk
             .linelength = 2192,                //record different mode's linelength
             .framelength = 1594,            //record different mode's framelength
             .startx = 0,                    //record different mode's startx of grabwindow
             .starty = 0,                    //record different mode's starty of grabwindow
             .grabwindow_width = 1600,        //record different mode's width of grabwindow //1296
             .grabwindow_height = 1200,        //record different mode's height of grabwindow //972
             /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
             .mipi_data_lp2hs_settle_dc = 85,//unit , ns
             /*     following for GetDefaultFramerateByScenario()    */
             .max_framerate = 240,
             .mipi_pixel_rate = 67200000,
         },
         #endif

        .margin = 16,            //sensor framelength & shutter margin
    	.min_shutter = 4,        //min shutter
        .max_frame_length = 0x1204,//max framelength by sensor register's limitation
        .ae_shut_delay_frame = 0,    //shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2
        .ae_sensor_gain_delay_frame = 0,//sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2
        .ae_ispGain_delay_frame = 2,//isp gain delay frame for AE cycle
        .ihdr_support = 0,      // 1 support; 0 not support
        .ihdr_le_firstline = 0,  // 1 le first ; 0, se first
    	.sensor_mode_num = 6,      //support sensor mode num

        .cap_delay_frame = 2,        //enter capture delay frame num
        .pre_delay_frame = 2,         //enter preview delay frame num
        .video_delay_frame = 2,        //enter video delay frame num
        .hs_video_delay_frame = 2,    //enter high speed video  delay frame num
        .slim_video_delay_frame = 2,//enter slim video delay frame num
        .custom1_delay_frame = 2,//enter custom1 delay frame num

        .isp_driving_current = ISP_DRIVING_2MA, //mclk driving current
        .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,//sensor_interface_type
        .mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
    	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
        .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_MONO,//sensor output first pixel color R
        .mclk = 24,//mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
        .mipi_lane_num = SENSOR_MIPI_1_LANE,//mipi lane num
        .i2c_addr_table = {0x6e,0x6f,0xff},//record sensor support all write id addr, only supprt 4must end with 0xff
        .i2c_speed = 400,
};


static imgsensor_struct imgsensor = {
	 #ifdef VENDOR_EDIT
	 /*Feiping@Camera.Drv, 20190603, add for set correct mirror/flip */
         .mirror = IMAGE_HV_MIRROR,                //mirrorflip information
         #else
	 .mirror = IMAGE_NORMAL,
	 #endif
         .sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
         .shutter = 0x3ED,                    //current shutter
         .gain = 0x40,                        //current gain
         .dummy_pixel = 0,                    //current dummypixel
         .dummy_line = 0,                    //current dummyline
         .current_fps = 300,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
         .autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
         .test_pattern = KAL_FALSE,        //test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
         .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
         .ihdr_en = 0, //sensor need support LE, SE with HDR feature
         .i2c_write_id = 0x6e,//record current sensor's i2c write id
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[6] =
{
	{ 1600, 1200,	 0,    0, 1600, 1200, 1600, 1200,0000, 0000, 1600, 1200, 	 0,    0, 1600, 1200}, // Preview
	{ 1600, 1200,	 0,    0, 1600, 1200, 1600, 1200,0000, 0000, 1600, 1200, 	 0,    0, 1600, 1200}, // capture
	{ 1600, 1200,	 0,    0, 1600, 1200, 1600, 1200,0000, 0000, 1600, 1200, 	 0,    0, 1600, 1200}, // video
	{ 1600, 1200,	 0,    0, 1600, 1200, 1600, 1200,0000, 0000, 1600, 1200, 	 0,    0, 1600, 1200}, //hight speed video
	{ 1600, 1200,	 0,    0, 1600, 1200, 1600, 1200,0000, 0000, 1600, 1200, 	 0,    0, 1600, 1200},// slim video
	{ 1600, 1200,	 0,    0, 1600, 1200, 1600, 1200,0000, 0000, 1600, 1200, 	 0,    0, 1600, 1200}// custom1
};



static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;

	char pu_send_cmd[1] = {(char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 1, (u8*)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}


static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
#if 1
		char pu_send_cmd[2] = {(char)(addr & 0xFF), (char)(para & 0xFF)};
		iWriteRegI2C(pu_send_cmd, 2, imgsensor.i2c_write_id);
#else
		iWriteReg((u16)addr, (u32)para, 2, imgsensor.i2c_write_id);
#endif
}

static void set_dummy(void)
{
#if 1
	imgsensor.frame_length=imgsensor.frame_length;
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0x41, (imgsensor.frame_length >> 8) & 0xff);
	write_cmos_sensor(0x42, imgsensor.frame_length & 0xFF);
//	LOG_INF("dore, frame length hight= 0x%x, frame length  low= 0x%x\n",read_cmos_sensor(0x41),read_cmos_sensor(0x42));
#endif
}    /*    set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	//return 0x02m0;
	return ((read_cmos_sensor(0xf0) << 8) | read_cmos_sensor(0xf1));
}

static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
    //kal_int16 dummy_line;
    kal_uint32 frame_length = imgsensor.frame_length;
    //unsigned long flags;

    frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
    spin_lock(&imgsensor_drv_lock);
    imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;
    imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;

    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
    {
        imgsensor.frame_length = imgsensor_info.max_frame_length;
        imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
    }
    if (min_framelength_en)
        imgsensor.min_frame_length = imgsensor.frame_length;
    spin_unlock(&imgsensor_drv_lock);
    set_dummy();
}    /*    set_max_framerate  */


/*************************************************************************
* FUNCTION
*    set_shutter
*
* DESCRIPTION
*    This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*    iShutter : exposured lines
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
    unsigned long flags;
    kal_uint16 realtime_fps = 0;
    //kal_uint32 frame_length = 0;
    kal_uint16 cal_shutter=0,G1_low,R_low,B_low,G2_low,G1_high,R_high,B_high,G2_high;
    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	// if shutter bigger than frame_length, should extend frame length first
	 spin_lock(&imgsensor_drv_lock);
	 if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		 imgsensor.frame_length = shutter + imgsensor_info.margin;
	 else
		 imgsensor.frame_length = imgsensor.min_frame_length;
	 if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		 imgsensor.frame_length = imgsensor_info.max_frame_length;
	 spin_unlock(&imgsensor_drv_lock);
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;
	if (shutter > imgsensor_info.max_frame_length - imgsensor_info.margin)
	    shutter = imgsensor_info.max_frame_length - imgsensor_info.margin;
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

	if (imgsensor.autoflicker_en) {

		if(realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296,0);
		else if(realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146,0);
		else {
			set_max_framerate(realtime_fps,0);
		    }
	} else {
		set_max_framerate(realtime_fps,0);
	}

	cal_shutter = shutter;
	write_cmos_sensor(0xfe, 0x00);
        write_cmos_sensor(0x03, (cal_shutter >> 8) & 0x3F);
        write_cmos_sensor(0x04, cal_shutter  & 0xFF);
        //write_cmos_sensor(0x0104, 0);
        write_cmos_sensor(0xfe, 0x04);
	G1_low=read_cmos_sensor(0x08);
	R_low=read_cmos_sensor(0x09);
	B_low=read_cmos_sensor(0x0A);
	G2_low=read_cmos_sensor(0x0B);
	G1_high=read_cmos_sensor(0x0C);
	R_high=read_cmos_sensor(0x0D);
	B_high=read_cmos_sensor(0x0E);
	G2_high=read_cmos_sensor(0x0F);

	LOG_INF("shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
	LOG_INF("G1_dark=0x%x, R_dark =0x%x ,B_dark =0x%x,G2_dark =0x%x\n", (G1_high<<8) | G1_low,(R_high<<8) | R_low,(B_high<<8) | B_low,(G2_high<<8) | G2_low);
        //LOG_INF("celrey-0x03 =0x%x, 0x04 =0x%x\n", read_cmos_sensor(0x03),read_cmos_sensor(0x04));
	write_cmos_sensor(0xfe, 0x00);
}
/*    set_shutter */


/*
static kal_uint16 gain2reg(const kal_uint16 gain)
{
    kal_uint16 reg_gain = 0x0000;

    reg_gain = ((gain / BASEGAIN) << 4) + ((gain % BASEGAIN) * 16 / BASEGAIN);
    reg_gain = reg_gain & 0xFFFF;
    return (kal_uint16)reg_gain;
}*/

/*************************************************************************
* FUNCTION
*    set_gain
*
* DESCRIPTION
*    This function is to set global gain to sensor.
*
* PARAMETERS
*    iGain : sensor global gain(base: 0x40)
*
* RETURNS
*    the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/

#define ANALOG_GAIN_1 1024    // 1.00x
#define ANALOG_GAIN_2 1536     // 1.5x
#define ANALOG_GAIN_3 2035     // 1.98x
#define ANALOG_GAIN_4 2519    // 2.460x
#define ANALOG_GAIN_5 3165    // 3.09x
#define ANALOG_GAIN_6 3626    // 3.541
#define ANALOG_GAIN_7 4148    // 4.05x
#define ANALOG_GAIN_8 4593    // 4.485x
#define ANALOG_GAIN_9 5095    // 4.9759
#define ANALOG_GAIN_10 5696   // 5.563x
#define ANALOG_GAIN_11 6270 // 6.123x
#define ANALOG_GAIN_12 6714 // 6.556x
#define ANALOG_GAIN_13 7210 // 7.041x
#define ANALOG_GAIN_14 7686 // 7.506xx
#define ANALOG_GAIN_15 8214  // 8.022x
#define ANALOG_GAIN_16 10337  // 10.095x
//#define ANALOG_GAIN_17 16199  // 15.819x

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 iReg,temp;
	LOG_INF("celery--gain = %d\n",gain);
	iReg = gain<<4;

	if(iReg < 0x400)
		iReg = 0x400;

	if((ANALOG_GAIN_1<= iReg)&&(iReg < ANALOG_GAIN_2))
	{
		write_cmos_sensor(0xfe,  0x00);
		write_cmos_sensor(0xb6,  0x00);
		temp = 1024*iReg/ANALOG_GAIN_1;
		//temp = temp*Dgain_ratio/1024;
		write_cmos_sensor(0xb1, (temp>>8)&0x3f);
		write_cmos_sensor(0xb2, (temp&0xff));
		LOG_INF("GC02m0MIPI analogic gain 1x, GC02m0MIPI add pregain = %d\n",temp);
	}
	else if((ANALOG_GAIN_2<= iReg)&&(iReg < ANALOG_GAIN_3))
	{
		write_cmos_sensor(0xfe,  0x00);
		write_cmos_sensor(0xb6,  0x01);
		temp = 1024*iReg/ANALOG_GAIN_2;
		//temp = temp*Dgain_ratio/1024;
		write_cmos_sensor(0xb1, (temp>>8)&0x3f);
		write_cmos_sensor(0xb2, (temp&0xff));
		LOG_INF("GC02m0MIPI analogic gain 1.185x, GC02m0MIPI add pregain = %d\n",temp);
	}
	else if((ANALOG_GAIN_3<= iReg)&&(iReg < ANALOG_GAIN_4))
	{
		write_cmos_sensor(0xfe,  0x00);
		write_cmos_sensor(0xb6,  0x02);
		temp = 1024*iReg/ANALOG_GAIN_3;
		//temp = temp*Dgain_ratio/1024;
		write_cmos_sensor(0xb1, (temp>>8)&0x3f);
		write_cmos_sensor(0xb2, (temp&0xff));
		LOG_INF("GC02m0MIPI analogic gain 1.4x, GC02m0MIPI add pregain = %d\n",temp);
	}
	else if((ANALOG_GAIN_4<= iReg)&&(iReg < ANALOG_GAIN_5))
	{
		write_cmos_sensor(0xfe,  0x00);
		write_cmos_sensor(0xb6,  0x03);
		temp = 1024*iReg/ANALOG_GAIN_4;
		//temp = temp*Dgain_ratio/1024;
		write_cmos_sensor(0xb1, (temp>>8)&0x3f);
		write_cmos_sensor(0xb2, (temp&0xff));
		LOG_INF("GC02m0MIPI analogic gain 1.659x, GC02m0MIPI add pregain = %d\n",temp);
	}
	else if((ANALOG_GAIN_5<= iReg)&&(iReg < ANALOG_GAIN_6))
	{
		write_cmos_sensor(0xfe,  0x00);
		write_cmos_sensor(0xb6,  0x04);
		temp = 1024*iReg/ANALOG_GAIN_5;
		//temp = temp*Dgain_ratio/1024;
		write_cmos_sensor(0xb1, (temp>>8)&0x3f);
		write_cmos_sensor(0xb2, (temp&0xff));
		LOG_INF("GC02m0MIPI analogic gain 2.0x, GC02m0MIPI add pregain = %d\n",temp);
	}
	else if((ANALOG_GAIN_6<= iReg)&&(iReg < ANALOG_GAIN_7))
	{
		write_cmos_sensor(0xfe,  0x00);
		write_cmos_sensor(0xb6,  0x05);
		temp = 1024*iReg/ANALOG_GAIN_6;
		//temp = temp*Dgain_ratio/1024;
		write_cmos_sensor(0xb1, (temp>>8)&0x3f);
		write_cmos_sensor(0xb2, (temp&0xff));
		LOG_INF("GC02m0MIPI analogic gain 2.37x, GC02m0MIPI add pregain = %d\n",temp);
	}
	else if((ANALOG_GAIN_7<= iReg)&&(iReg < ANALOG_GAIN_8))
	{
		write_cmos_sensor(0xfe,  0x00);
		write_cmos_sensor(0xb6,  0x06);
		temp = 1024*iReg/ANALOG_GAIN_7;
		//temp = temp*Dgain_ratio/1024;
		write_cmos_sensor(0xb1, (temp>>8)&0x3f);
		write_cmos_sensor(0xb2, (temp&0xff));
		LOG_INF("GC02m0MIPI analogic gain 2.8x, GC02m0MIPI add pregain = %d\n",temp);
	}
	else if((ANALOG_GAIN_8<= iReg)&&(iReg < ANALOG_GAIN_9))
	{
		write_cmos_sensor(0xfe,  0x00);
		write_cmos_sensor(0xb6,  0x07);
		temp = 1024*iReg/ANALOG_GAIN_8;
		//temp = temp*Dgain_ratio/1024;
		write_cmos_sensor(0xb1, (temp>>8)&0x3f);
		write_cmos_sensor(0xb2, (temp&0xff));
		LOG_INF("GC02m0MIPI analogic gain 3.318x, GC02m0MIPI add pregain = %d\n",temp);
	}
	else if((ANALOG_GAIN_9<= iReg)&&(iReg < ANALOG_GAIN_10))
	{
		write_cmos_sensor(0xfe,  0x00);
		write_cmos_sensor(0xb6,  0x08);
		temp = 1024*iReg/ANALOG_GAIN_9;
		//temp = temp*Dgain_ratio/1024;
		write_cmos_sensor(0xb1, (temp>>8)&0x3f);
		write_cmos_sensor(0xb2, (temp&0xff));
		LOG_INF("GC02m0MIPI analogic gain 4.0x, GC02m0MIPI add pregain = %d\n",temp);
	}
	else if((ANALOG_GAIN_10<= iReg)&&(iReg < ANALOG_GAIN_11))
	{
		write_cmos_sensor(0xfe,  0x00);
		write_cmos_sensor(0xb6,  0x09);
		temp = 1024*iReg/ANALOG_GAIN_10;
		//temp = temp*Dgain_ratio/1024;
		write_cmos_sensor(0xb1, (temp>>8)&0x3f);
		write_cmos_sensor(0xb2, (temp&0xff));
		LOG_INF("GC02m0MIPI analogic gain 4.74x, GC02m0MIPI add pregain = %d\n",temp);
	}
	else if((ANALOG_GAIN_11<= iReg)&&(iReg < ANALOG_GAIN_12))
	{
		write_cmos_sensor(0xfe,  0x00);
		write_cmos_sensor(0xb6,  0x0a);
		temp = 1024*iReg/ANALOG_GAIN_11;
		//temp = temp*Dgain_ratio/1024;
		write_cmos_sensor(0xb1, (temp>>8)&0x3f);
		write_cmos_sensor(0xb2, (temp&0xff));
		LOG_INF("GC02m0MIPI analogic gain 5.6x, GC02m0MIPI add pregain = %d\n",temp);
	}
	else if((ANALOG_GAIN_12<= iReg)&&(iReg < ANALOG_GAIN_13))
	{
		write_cmos_sensor(0xfe,  0x00);
		write_cmos_sensor(0xb6,  0x0b);
		temp = 1024*iReg/ANALOG_GAIN_12;
		//temp = temp*Dgain_ratio/1024;
		write_cmos_sensor(0xb1, (temp>>8)&0x3f);
		write_cmos_sensor(0xb2, (temp&0xff));
		LOG_INF("GC02m0MIPI analogic gain 6.636x, GC02m0MIPI add pregain = %d\n",temp);
	}
	else if((ANALOG_GAIN_13<= iReg)&&(iReg < ANALOG_GAIN_14))
	{
		write_cmos_sensor(0xfe,  0x00);
		write_cmos_sensor(0xb6,  0x0c);
		temp = 1024*iReg/ANALOG_GAIN_13;
		//temp = temp*Dgain_ratio/1024;
		write_cmos_sensor(0xb1, (temp>>8)&0x3f);
		write_cmos_sensor(0xb2, (temp&0xff));
		LOG_INF("GC02m0MIPI analogic gain 8.0x, GC02m0MIPI add pregain = %d\n",temp);
	}
	else if((ANALOG_GAIN_14<= iReg)&&(iReg < ANALOG_GAIN_15))
	{
		write_cmos_sensor(0xfe,  0x00);
		write_cmos_sensor(0xb6,  0x0d);
		temp = 1024*iReg/ANALOG_GAIN_14;
		//temp = temp*Dgain_ratio/1024;
		write_cmos_sensor(0xb1, (temp>>8)&0x3f);
		write_cmos_sensor(0xb2, (temp&0xff));
		LOG_INF("GC02m0MIPI analogic gain 9.48x, GC02m0MIPI add pregain = %d\n",temp);
	}
	else if((ANALOG_GAIN_15<= iReg)&&(iReg < ANALOG_GAIN_16))
	{
		write_cmos_sensor(0xfe,  0x00);
		write_cmos_sensor(0xb6,  0x0e);
		temp = 1024*iReg/ANALOG_GAIN_15;
		//temp = temp*Dgain_ratio/1024;
		write_cmos_sensor(0xb1, (temp>>8)&0x3f);
		write_cmos_sensor(0xb2, (temp&0xff));
		LOG_INF("GC02m0MIPI analogic gain 11.2x, GC02m0MIPI add pregain = %d\n",temp);
	}
	else //if ((ANALOG_GAIN_16<= iReg)&&(iReg < ANALOG_GAIN_17))
	{
		write_cmos_sensor(0xfe,  0x00);
		write_cmos_sensor(0xb6,  0x0f);
		temp = 1024*iReg/ANALOG_GAIN_16;
		//temp = temp*Dgain_ratio/1024;
		write_cmos_sensor(0xb1, (temp>>8)&0x3f);
		write_cmos_sensor(0xb2, (temp&0xff));
		LOG_INF("GC02m0MIPI analogic gain 13.2725x, GC02m0MIPI add pregain = %d\n",temp);
	}
	/*else
	{
		write_cmos_sensor(0xfe,  0x00);
		write_cmos_sensor(0xb6,  0x10);
		temp = 1024*iReg/ANALOG_GAIN_17;
		//temp = temp*Dgain_ratio/1024;
		write_cmos_sensor(0xb1, (temp>>8)&0x3f);
		write_cmos_sensor(0xb2, (temp&0xff));
		LOG_INF("GC02m0MIPI analogic gain 16x, GC02m0MIPI add pregain = %d\n",temp);
	}*/
	LOG_INF("celery--0xb6 =0x%x, 0xb1 =0x%x,0xb2 =0x%x\n", read_cmos_sensor(0xb6),read_cmos_sensor(0xb1),read_cmos_sensor(0xb2));
	return gain;

}    /*    set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
    LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n",le,se,gain);

}

#ifdef VENDOR_EDIT
/*Feiping@Camera.Drv, 20190603, add for set correct mirror/flip */
static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);
	switch (image_mirror) {

		case IMAGE_NORMAL:
			write_cmos_sensor(0x17, 0x80);   /* Gr*/
			break;

		case IMAGE_H_MIRROR:
			write_cmos_sensor(0x17, 0x81);
			break;

		case IMAGE_V_MIRROR:
			write_cmos_sensor(0x17, 0x82);
			break;

		case IMAGE_HV_MIRROR:
			write_cmos_sensor(0x17, 0x83);  /*Gb*/
			break;
		default:
			LOG_INF("Error image_mirror setting\n");
	}
}
#endif

/*************************************************************************
* FUNCTION
*    night_mode
*
* DESCRIPTION
*    This function night mode of sensor.
*
* PARAMETERS
*    bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}    /*    night_mode    */

#ifdef USE_BURST_MODE
#define I2C_BUFFER_LEN 360	/* trans# max is 255, each 3 bytes */
extern int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId, u16 transfer_length, u16 timing);

static kal_uint16 table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;
	while (len > IDX) {
		addr = para[IDX];

		{
			puSendCmd[tosend++] = (char)addr;
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)data;
			IDX += 2;
			addr_last = addr;

		}

		/* Write when remain buffer size is less than 4 bytes or reach end of data */
		if ((I2C_BUFFER_LEN - tosend) < 2 || IDX == len || addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_write_id,
								2, imgsensor_info.i2c_speed);
			tosend = 0;
		}
	}
	return 0;
}

static kal_uint16 addr_data_pair_global[] = {
0xfc,0x01,
0xf4,0x41,
0xf5,0xc0,
0xf6,0x44,
0xf8,0x38,
0xf9,0x82,
0xfa,0x00,
0xfd,0x80,
0xfc,0x81,
0xfe,0x03,
0x01,0x0b,
0xf7,0x01,
0xfc,0x80,
0xfc,0x80,
0xfc,0x80,
0xfc,0x8e,
0xfe,0x00,
0x87,0x09,
0xee,0x72,
0xfe,0x01,
0x8c,0x90,
0xfe,0x00,
0x90,0x00,
0x03,0x04,
0x04,0x7d,
0x41,0x04,
0x42,0xF4,
0x05,0x04,
0x06,0x48,
0x07,0x00,
0x08,0x18,
0x9d,0x18,
0x0d,0x04,
0x0e,0xbc,
0x19,0x07,
0x56,0x20,
0x5b,0x00,
0x5e,0x01,
0x21,0x4e,
0x1f,0x11,
0x27,0x30,
0xe6,0x50,
0x39,0x07,
0x43,0x04,
0x46,0x2a,
0x7c,0xa0,
0xd0,0xbe,
0xd1,0x60,
0xd2,0x50,
0xd3,0xf3,
0xde,0x1d,
0xcd,0x05,
0xce,0x77,
0xfc,0x88,
0xfe,0x10,
0xfe,0x00,
0xfc,0x8e,
0xfe,0x00,
0xfe,0x00,
0xfe,0x00,
0xfe,0x00,
0xfc,0x88,
0xfe,0x10,
0xfe,0x00,
0xfc,0x8e,
0xfe,0x04,
0xe0,0x01,
0xfe,0x00,
0xfe,0x00,
0x17,0x83,
0xfe,0x01,
0x53,0x44,
0x89,0x03,
0xfe,0x00,
0xb0,0x70,
0xb1,0x04,
0xb2,0x00,
0xb6,0x00,
0xfe,0x04,
0xd8,0x00,
0xc0,0x40,
0xc0,0x00,
0xc0,0x00,
0xc0,0x00,
0xc0,0x60,
0xc0,0x00,
0xc0,0xc0,
0xc0,0x2a,
0xc0,0x80,
0xc0,0x00,
0xc0,0x00,
0xc0,0x40,
0xc0,0xa0,
0xc0,0x00,
0xc0,0x90,
0xc0,0x19,
0xc0,0xc0,
0xc0,0x00,
0xc0,0xD0,
0xc0,0x2F,
0xc0,0xe0,
0xc0,0x00,
0xc0,0x90,
0xc0,0x39,
0xc0,0x00,
0xc0,0x01,
0xc0,0x20,
0xc0,0x04,
0xc0,0x20,
0xc0,0x01,
0xc0,0xe0,
0xc0,0x0f,
0xc0,0x40,
0xc0,0x01,
0xc0,0xe0,
0xc0,0x1a,
0xc0,0x60,
0xc0,0x01,
0xc0,0x20,
0xc0,0x25,
0xc0,0x80,
0xc0,0x01,
0xc0,0xa0,
0xc0,0x2c,
0xc0,0xa0,
0xc0,0x01,
0xc0,0xe0,
0xc0,0x32,
0xc0,0xc0,
0xc0,0x01,
0xc0,0x20,
0xc0,0x38,
0xc0,0xe0,
0xc0,0x01,
0xc0,0x60,
0xc0,0x3c,
0xc0,0x00,
0xc0,0x02,
0xc0,0xa0,
0xc0,0x40,
0xc0,0x80,
0xc0,0x02,
0xc0,0x18,
0xc0,0x5c,
0xfe,0x00,
0x9f,0x10,
0xfe,0x00,
0x26,0x20,
0xfe,0x01,
0x40,0x22,
0x46,0x7f,
0x49,0x0f,
0x4a,0xf0,
0xfe,0x04,
0x14,0x80,
0x15,0x80,
0x16,0x80,
0x17,0x80,
0xfe,0x01,
0x90,0x01,
0x91,0x00,
0x92,0x06,
0x93,0x00,
0x94,0x00,
0x95,0x04,
0x96,0xb0,
0x97,0x06,
0x98,0x40,
0xfe,0x03,
0x01,0x23,
0x03,0xce,
0x11,0x48,
0x15,0x00,
0xfe,0x01,
0x8c,0x10,
0xfe,0x00,
0x3e,0x00,
};
#endif
static void sensor_init(void)
{
	LOG_INF("E");

	/*system*/
	/*bayer fullsize:
	**mipi : 672Mbps
	**Wqpclk: 42MHz  WPclk:168MHz
	**Rqpclk: 33.6MHz Rpclk:134.4MHz
	*/
	#ifdef USE_BURST_MODE
	table_write_cmos_sensor(addr_data_pair_global,
		sizeof(addr_data_pair_global) / sizeof(kal_uint16));
	#else
	/*system*/
	write_cmos_sensor(0xfc,0x01);
	write_cmos_sensor(0xf4,0x41);
	write_cmos_sensor(0xf5,0xc0);
	write_cmos_sensor(0xf6,0x44);
	write_cmos_sensor(0xf8,0x38);
	write_cmos_sensor(0xf9,0x82);
	write_cmos_sensor(0xfa,0x00);
	write_cmos_sensor(0xfd,0x80);
	write_cmos_sensor(0xfc,0x81);
	write_cmos_sensor(0xfe,0x03);
	write_cmos_sensor(0x01,0x0b);
	write_cmos_sensor(0xf7,0x01);
	write_cmos_sensor(0xfc,0x80);
	write_cmos_sensor(0xfc,0x80);
	write_cmos_sensor(0xfc,0x80);
	write_cmos_sensor(0xfc,0x8e);

	/*CISCTL*/
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0x87,0x09);
	write_cmos_sensor(0xee,0x72);
	write_cmos_sensor(0xfe,0x01);
	write_cmos_sensor(0x8c,0x90);
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0x90,0x00);
	write_cmos_sensor(0x03,0x04);
	write_cmos_sensor(0x04,0x7d);
	write_cmos_sensor(0x41,0x04);
	write_cmos_sensor(0x42,0xF4);
	write_cmos_sensor(0x05,0x04);
	write_cmos_sensor(0x06,0x48);
	write_cmos_sensor(0x07,0x00);
	write_cmos_sensor(0x08,0x18);
	write_cmos_sensor(0x9d,0x18);
	write_cmos_sensor(0x0d,0x04);
	write_cmos_sensor(0x0e,0xbc);
	write_cmos_sensor(0x19,0x07);
	write_cmos_sensor(0x56,0x20);
	write_cmos_sensor(0x5b,0x00);
	write_cmos_sensor(0x5e,0x01);
	write_cmos_sensor(0x21,0x4e);
	write_cmos_sensor(0x1f,0x11);
	write_cmos_sensor(0x27,0x30);
	write_cmos_sensor(0xe6,0x50);
	write_cmos_sensor(0x39,0x07);
	write_cmos_sensor(0x43,0x04);
	write_cmos_sensor(0x46,0x2a);
	write_cmos_sensor(0x7c,0xa0);
	write_cmos_sensor(0xd0,0xbe);
	write_cmos_sensor(0xd1,0x60);
	write_cmos_sensor(0xd2,0x50);
	write_cmos_sensor(0xd3,0xf3);
	write_cmos_sensor(0xde,0x1d);
	/*analog current*/
	write_cmos_sensor(0xcd,0x05);
	write_cmos_sensor(0xce,0x77);

	/*CISCTL RESET*/
	write_cmos_sensor(0xfc,0x88);
	write_cmos_sensor(0xfe,0x10);
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0xfc,0x8e);
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0xfc,0x88);
	write_cmos_sensor(0xfe,0x10);
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0xfc,0x8e);
	write_cmos_sensor(0xfe,0x04);
	write_cmos_sensor(0xe0,0x01);
	write_cmos_sensor(0xfe,0x00);

	/*ISP*/
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0x17,0x83);
	write_cmos_sensor(0xfe,0x01);
	write_cmos_sensor(0x53,0x44);
	write_cmos_sensor(0x89,0x03);
	/*Gain*/

	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0xb0,0x70);
	write_cmos_sensor(0xb1,0x04);
	write_cmos_sensor(0xb2,0x00);
	write_cmos_sensor(0xb6,0x00);
	write_cmos_sensor(0xfe,0x04);
	write_cmos_sensor(0xd8,0x00);
	write_cmos_sensor(0xc0,0x40);  //1x
	write_cmos_sensor(0xc0,0x00);
	write_cmos_sensor(0xc0,0x00);
	write_cmos_sensor(0xc0,0x00);
	write_cmos_sensor(0xc0,0x60);  //1.5
	write_cmos_sensor(0xc0,0x00);
	write_cmos_sensor(0xc0,0xc0);
	write_cmos_sensor(0xc0,0x2a);
	write_cmos_sensor(0xc0,0x80);  //2x
	write_cmos_sensor(0xc0,0x00);
	write_cmos_sensor(0xc0,0x00);
	write_cmos_sensor(0xc0,0x40);
	write_cmos_sensor(0xc0,0xa0);  //2.5x
	write_cmos_sensor(0xc0,0x00);
	write_cmos_sensor(0xc0,0x90);
	write_cmos_sensor(0xc0,0x19);
	write_cmos_sensor(0xc0,0xc0);  //3x
	write_cmos_sensor(0xc0,0x00);
	write_cmos_sensor(0xc0,0xD0);
	write_cmos_sensor(0xc0,0x2F);
	write_cmos_sensor(0xc0,0xe0);  //3.5x
	write_cmos_sensor(0xc0,0x00);
	write_cmos_sensor(0xc0,0x90);
	write_cmos_sensor(0xc0,0x39);
	write_cmos_sensor(0xc0,0x00);  //4x
	write_cmos_sensor(0xc0,0x01);
	write_cmos_sensor(0xc0,0x20);
	write_cmos_sensor(0xc0,0x04);
	write_cmos_sensor(0xc0,0x20);  //4.5x
	write_cmos_sensor(0xc0,0x01);
	write_cmos_sensor(0xc0,0xe0);
	write_cmos_sensor(0xc0,0x0f);
	write_cmos_sensor(0xc0,0x40);  //5x
	write_cmos_sensor(0xc0,0x01);
	write_cmos_sensor(0xc0,0xe0);
	write_cmos_sensor(0xc0,0x1a);
	write_cmos_sensor(0xc0,0x60);  //5.5x
	write_cmos_sensor(0xc0,0x01);
	write_cmos_sensor(0xc0,0x20);
	write_cmos_sensor(0xc0,0x25);
	write_cmos_sensor(0xc0,0x80);  //6x
	write_cmos_sensor(0xc0,0x01);
	write_cmos_sensor(0xc0,0xa0);
	write_cmos_sensor(0xc0,0x2c);
	write_cmos_sensor(0xc0,0xa0);  //6.5x
	write_cmos_sensor(0xc0,0x01);
	write_cmos_sensor(0xc0,0xe0);
	write_cmos_sensor(0xc0,0x32);
	write_cmos_sensor(0xc0,0xc0);  //7x
	write_cmos_sensor(0xc0,0x01);
	write_cmos_sensor(0xc0,0x20);
	write_cmos_sensor(0xc0,0x38);
	write_cmos_sensor(0xc0,0xe0);  //7.5x
	write_cmos_sensor(0xc0,0x01);
	write_cmos_sensor(0xc0,0x60);
	write_cmos_sensor(0xc0,0x3c);
	write_cmos_sensor(0xc0,0x00);  //8x
	write_cmos_sensor(0xc0,0x02);
	write_cmos_sensor(0xc0,0xa0);
	write_cmos_sensor(0xc0,0x40);
	write_cmos_sensor(0xc0,0x80);  //10x
	write_cmos_sensor(0xc0,0x02);
	write_cmos_sensor(0xc0,0x18);
	write_cmos_sensor(0xc0,0x5c);
	/*write_cmos_sensor(0xc0,0x00);  //16x
	write_cmos_sensor(0xc0,0x04);
	write_cmos_sensor(0xc0,0x70);
	write_cmos_sensor(0xc0,0x40);*/
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0x9f,0x10);

	/*BLK*/
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0x26,0x20);
	write_cmos_sensor(0xfe,0x01);
	write_cmos_sensor(0x40,0x22);
	write_cmos_sensor(0x46,0x7f);
	write_cmos_sensor(0x49,0x0f);
	write_cmos_sensor(0x4a,0xf0);
	write_cmos_sensor(0xfe,0x04);
	write_cmos_sensor(0x14,0x80);
	write_cmos_sensor(0x15,0x80);
	write_cmos_sensor(0x16,0x80);
	write_cmos_sensor(0x17,0x80);

	/*Window 1600X1200*/
	write_cmos_sensor(0xfe,0x01);
	write_cmos_sensor(0x90,0x01);
	write_cmos_sensor(0x91,0x00);
	write_cmos_sensor(0x92,0x06);
	write_cmos_sensor(0x93,0x00);
	write_cmos_sensor(0x94,0x00);
	write_cmos_sensor(0x95,0x04);
	write_cmos_sensor(0x96,0xb0);
	write_cmos_sensor(0x97,0x06);
	write_cmos_sensor(0x98,0x40);

	/*mipi*/
	write_cmos_sensor(0xfe,0x03);
	write_cmos_sensor(0x01,0x23);
	write_cmos_sensor(0x03,0xce);
	write_cmos_sensor(0x11,0x48);
	write_cmos_sensor(0x15,0x00);
	write_cmos_sensor(0xfe,0x01);
	write_cmos_sensor(0x8c,0x10);
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0x3e,0x00);
	#endif
}    /* sensor_init  */


static void preview_setting(void)
{
	LOG_INF("E!\n");
	write_cmos_sensor(0x41,0x04);
	write_cmos_sensor(0x42,0xf4);
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0x3e,0x90);
}    /*    preview_setting  */

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n",currefps);
	write_cmos_sensor(0xfe,0x04);
	write_cmos_sensor(0x3e,0xf4);
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0x3e,0x90);
}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n",currefps);
	write_cmos_sensor(0xfe,0x04);
	write_cmos_sensor(0x3e,0xf4);
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0x3e,0x90);
}

static void hs_video_setting(void)
{
   	LOG_INF("E\n");
	write_cmos_sensor(0xfe,0x04);
	write_cmos_sensor(0x3e,0xf4);
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0x3e,0x90);

	/*SYS*/
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
	write_cmos_sensor(0xfe,0x04);
	write_cmos_sensor(0x3e,0xf4);
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0x3e,0x90);
	/*SYS*/
}

//Feiping.Li@Camera.Driver, 20190524, add for 19301 dual cam
static void custom1_setting(void)
{
	LOG_INF("E!\n");
	write_cmos_sensor(0x41,0x06);
	write_cmos_sensor(0x42,0x3a);
	write_cmos_sensor(0xfe,0x00);
	write_cmos_sensor(0x3e,0x90);
}    /*    preview_setting  */


static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
    LOG_INF("enable: %d\n", enable);

    if (enable) {
        write_cmos_sensor(0xfe, 0x01);
        write_cmos_sensor(0x8c, 0x11);
    } else {
        write_cmos_sensor(0xfe, 0x10);
        write_cmos_sensor(0x8c, 0x10);
    }
    spin_lock(&imgsensor_drv_lock);
    imgsensor.test_pattern = enable;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*    get_imgsensor_id
*
* DESCRIPTION
*    This function get the sensor ID
*
* PARAMETERS
*    *sensorID : return the sensor ID
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	#ifdef VENDOR_EDIT
	/* Feiping.Li@Camera.Drv, 20190603, add for 19301 two mono sensor install in any place*/
	int I2C_BUS = -1;
	I2C_BUS = i2c_adapter_id(pgi2c_cfg_legacy->pinst->pi2c_client->adapter);
	LOG_INF("gc02mo_mipi_mono_Sensor I2C_BUS = %d\n", I2C_BUS);
	if(I2C_BUS != 4){
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	#endif

    	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        	spin_lock(&imgsensor_drv_lock);
        	imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        	spin_unlock(&imgsensor_drv_lock);
        	do {
            	*sensor_id = return_sensor_id();
            	if (*sensor_id == imgsensor_info.sensor_id) {
			#ifdef VENDOR_EDIT
			/*Shounan.Yang@Camera.Drv, 2019.6.18 add for register device info*/
			imgsensor_info.module_id = IMGSENSOR_MODULE_ID_SHINE;
			if (deviceInfo_register_value == 0x00) {
				register_imgsensor_deviceinfo("Cam_r2", DEVICE_VERSION_GC02M0,
								imgsensor_info.module_id);
				deviceInfo_register_value=0x01;
			}
			#endif
			LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
			#ifdef VENDOR_EDIT
			/* Feiping.Li@Camera.Drv, 20190710, add for pull-up avdd when main sensor is powered*/
			if (is_using_gc02m0 == 0)  //successfully find gc02m0
				set_gc02m0_flag(IMGSENSOR_SENSOR_IDX_SUB2);
			is_using_gc02m0 = 1;
			#endif
                	return ERROR_NONE;
            	}
            	LOG_INF("Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
            	retry--;
        	} while(retry > 0);
        	i++;
        	retry = 2;
    	}
    	if (*sensor_id != imgsensor_info.sensor_id) {
        	// if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF
        	*sensor_id = 0xFFFFFFFF;
        	return ERROR_SENSOR_CONNECT_FAIL;
    	}

    	return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*    open
*
* DESCRIPTION
*    This function initialize the registers of CMOS sensor
*
* PARAMETERS
*    None
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    kal_uint32 sensor_id = 0;
    LOG_1;

    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            sensor_id = return_sensor_id();
            if (sensor_id == imgsensor_info.sensor_id) {
                LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
                break;
            }
            LOG_INF("Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
            retry--;
        } while(retry > 0);
        i++;
        if (sensor_id == imgsensor_info.sensor_id)
            break;
        retry = 2;
    }
    if (imgsensor_info.sensor_id != sensor_id)
        return ERROR_SENSOR_CONNECT_FAIL;

    /*Don't Remove!!*/
    //gc02m0_gcore_identify_otp();

    /* initail sequence write in  */
    sensor_init();

    /*write registers from sram*/
    //gc02m0_gcore_update_otp();

    spin_lock(&imgsensor_drv_lock);

    imgsensor.autoflicker_en= KAL_FALSE;
    imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.dummy_pixel = 0;
    imgsensor.dummy_line = 0;
    imgsensor.ihdr_en = 0;
    imgsensor.test_pattern = KAL_FALSE;
    imgsensor.current_fps = imgsensor_info.pre.max_framerate;
    spin_unlock(&imgsensor_drv_lock);

    return ERROR_NONE;
}    /*    open  */


/*************************************************************************
* FUNCTION
*    close
*
* DESCRIPTION
*
*
* PARAMETERS
*    None
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
    LOG_INF("E\n");

    /*No Need to implement this function*/

    return ERROR_NONE;
}    /*    close  */


/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*    This function start the sensor preview.
*
* PARAMETERS
*    *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    #ifdef VENDOR_EDIT
    /*Feiping@Camera.Drv, 20190603, add for set correct mirror/flip */
    set_mirror_flip(imgsensor.mirror);
    #endif
    return ERROR_NONE;
}    /*    preview   */


/*************************************************************************
* FUNCTION
*    capture
*
* DESCRIPTION
*    This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                          MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
    if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {//PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
        imgsensor.pclk = imgsensor_info.cap1.pclk;
        imgsensor.line_length = imgsensor_info.cap1.linelength;
        imgsensor.frame_length = imgsensor_info.cap1.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    } else {
        if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
            LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",imgsensor.current_fps,imgsensor_info.cap.max_framerate/10);
        imgsensor.pclk = imgsensor_info.cap.pclk;
        imgsensor.line_length = imgsensor_info.cap.linelength;
        imgsensor.frame_length = imgsensor_info.cap.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    }
    spin_unlock(&imgsensor_drv_lock);
    capture_setting(imgsensor.current_fps);
    #ifdef VENDOR_EDIT
    /*Feiping@Camera.Drv, 20190603, add for set correct mirror/flip */
    set_mirror_flip(imgsensor.mirror);
    #endif
    return ERROR_NONE;
}    /* capture() */


static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
    imgsensor.pclk = imgsensor_info.normal_video.pclk;
    imgsensor.line_length = imgsensor_info.normal_video.linelength;
    imgsensor.frame_length = imgsensor_info.normal_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
    //imgsensor.current_fps = 300;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    normal_video_setting(imgsensor.current_fps);
    #ifdef VENDOR_EDIT
    /*Feiping@Camera.Drv, 20190603, add for set correct mirror/flip */
    set_mirror_flip(imgsensor.mirror);
    #endif
    return ERROR_NONE;
}    /*    normal_video   */


static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    imgsensor.pclk = imgsensor_info.hs_video.pclk;
    //imgsensor.video_mode = KAL_TRUE;
    imgsensor.line_length = imgsensor_info.hs_video.linelength;
    imgsensor.frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    hs_video_setting();
    #ifdef VENDOR_EDIT
    /*Feiping@Camera.Drv, 20190603, add for set correct mirror/flip */
    set_mirror_flip(imgsensor.mirror);
    #endif
    return ERROR_NONE;
}    /*    hs_video   */


static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
    imgsensor.pclk = imgsensor_info.slim_video.pclk;
    imgsensor.line_length = imgsensor_info.slim_video.linelength;
    imgsensor.frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    slim_video_setting();
    #ifdef VENDOR_EDIT
    /*Feiping@Camera.Drv, 20190603, add for set correct mirror/flip */
    set_mirror_flip(imgsensor.mirror);
    #endif
    return ERROR_NONE;
}    /*    slim_video     */

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	#ifdef VENDOR_EDIT
	/*Feiping@Camera.Drv, 20190603, add for set correct mirror/flip */
	set_mirror_flip(imgsensor.mirror);
	#endif
	return ERROR_NONE;
}	/* custom1 */

static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0, cal_shutter = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/*if shutter bigger than frame_length, should extend frame length first*/
	spin_lock(&imgsensor_drv_lock);
	if(frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

	if (imgsensor.autoflicker_en) {
		if (realtime_fps >= 297 && realtime_fps <= 305) {
			set_max_framerate(296, 0);
		} else if (realtime_fps >= 147 && realtime_fps <= 150) {
			set_max_framerate(146, 0);
		} else {
			set_max_framerate(realtime_fps, 0);
		}
	} else {
		set_max_framerate(realtime_fps, 0);
	}

        if (shutter == (imgsensor.frame_length-1))
            shutter += 1;
        if(shutter > 16383)
            shutter = 16383;
        if(shutter < 1)
            shutter = 1;
	cal_shutter = shutter;
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x03, (cal_shutter >> 8) & 0x3F);
	write_cmos_sensor(0x04, cal_shutter & 0xFF);

	LOG_INF("Exit! shutter = %d, framelength = %d\n", shutter, imgsensor.frame_length);
	LOG_INF("Exit! cal_shutter = %d, ", cal_shutter);
}

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
    LOG_INF("E\n");
    sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
    sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

    sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
    sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

    sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
    sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


    sensor_resolution->SensorHighSpeedVideoWidth     = imgsensor_info.hs_video.grabwindow_width;
    sensor_resolution->SensorHighSpeedVideoHeight     = imgsensor_info.hs_video.grabwindow_height;

    sensor_resolution->SensorSlimVideoWidth     = imgsensor_info.slim_video.grabwindow_width;
    sensor_resolution->SensorSlimVideoHeight     = imgsensor_info.slim_video.grabwindow_height;

    sensor_resolution->SensorCustom1Width = imgsensor_info.custom1.grabwindow_width;
    sensor_resolution->SensorCustom1Height = imgsensor_info.custom1.grabwindow_height;

    return ERROR_NONE;
}    /*    get_resolution    */


static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                      MSDK_SENSOR_INFO_STRUCT *sensor_info,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("scenario_id = %d\n", scenario_id);


    //sensor_info->SensorVideoFrameRate = imgsensor_info.normal_video.max_framerate/10; /* not use */
    //sensor_info->SensorStillCaptureFrameRate= imgsensor_info.cap.max_framerate/10; /* not use */
    //imgsensor_info->SensorWebCamCaptureFrameRate= imgsensor_info.v.max_framerate; /* not use */

    sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
    sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
    sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorInterruptDelayLines = 4; /* not use */
    sensor_info->SensorResetActiveHigh = FALSE; /* not use */
    sensor_info->SensorResetDelayCount = 5; /* not use */

    sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
    sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
    sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
    sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

    sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
    sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
    sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
    sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
    sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
    sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;

    sensor_info->SensorMasterClockSwitch = 0; /* not use */
    sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

    sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;          /* The frame of setting shutter default 0 for TG int */
    sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;    /* The frame of setting sensor gain */
    sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
    sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
    sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
    sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

    sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
    sensor_info->SensorClockFreq = imgsensor_info.mclk;
    sensor_info->SensorClockDividCount = 3; /* not use */
    sensor_info->SensorClockRisingCount = 0;
    sensor_info->SensorClockFallingCount = 2; /* not use */
    sensor_info->SensorPixelClockCount = 3; /* not use */
    sensor_info->SensorDataLatchCount = 2; /* not use */

    sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
    sensor_info->SensorHightSampling = 0;    // 0 is default 1x
    sensor_info->SensorPacketECCOrder = 1;

    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

            sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

            break;

        case MSDK_SCENARIO_ID_CUSTOM1:
            sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
            break;

        default:
            sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
            break;
    }

    return ERROR_NONE;
}    /*    get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("scenario_id = %d\n", scenario_id);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.current_scenario_id = scenario_id;
    spin_unlock(&imgsensor_drv_lock);
    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            preview(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            capture(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            normal_video(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            hs_video(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            slim_video(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            custom1(image_window, sensor_config_data);
            break;
        default:
            LOG_INF("Error ScenarioId setting");
            preview(image_window, sensor_config_data);
            return ERROR_INVALID_SCENARIO_ID;
    }
    return ERROR_NONE;
}    /* control() */


static kal_uint32 set_video_mode(UINT16 framerate)
{//This Function not used after ROME
    LOG_INF("framerate = %d\n ", framerate);
    // SetVideoMode Function should fix framerate
    if (framerate == 0)
        // Dynamic frame rate
        return ERROR_NONE;
    spin_lock(&imgsensor_drv_lock);
    if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
        imgsensor.current_fps = 296;
    else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
        imgsensor.current_fps = 146;
    else
        imgsensor.current_fps = framerate;
    spin_unlock(&imgsensor_drv_lock);
    set_max_framerate(imgsensor.current_fps,1);

    return ERROR_NONE;
}


static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
    LOG_INF("enable = %d, framerate = %d \n", enable, framerate);
    spin_lock(&imgsensor_drv_lock);
    if (enable) //enable auto flicker
        imgsensor.autoflicker_en = KAL_TRUE;
    else //Cancel Auto flick
        imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
    kal_uint32 frame_length;

    LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            set_dummy();
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            if(framerate == 0)
                return ERROR_NONE;
            frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            set_dummy();
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
                frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
                spin_lock(&imgsensor_drv_lock);
                imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ? (frame_length - imgsensor_info.cap1.framelength) : 0;
                imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
                imgsensor.min_frame_length = imgsensor.frame_length;
                spin_unlock(&imgsensor_drv_lock);
            } else {
                if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
                LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",framerate,imgsensor_info.cap.max_framerate/10);
                frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
                spin_lock(&imgsensor_drv_lock);
                imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
                imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
                imgsensor.min_frame_length = imgsensor.frame_length;
                spin_unlock(&imgsensor_drv_lock);
            }
            set_dummy();
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            set_dummy();
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;
            imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            set_dummy();
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line =(frame_length > imgsensor_info.custom1.framelength) ? (frame_length - imgsensor_info.custom1.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            if (imgsensor.frame_length > imgsensor.shutter)
                set_dummy();
            break;

        default:  //coding with  preview scenario by default
            frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            set_dummy();
            LOG_INF("error scenario_id = %d, we use preview scenario \n", scenario_id);
            break;
    }
    return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
    LOG_INF("scenario_id = %d\n", scenario_id);

    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            *framerate = imgsensor_info.pre.max_framerate;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *framerate = imgsensor_info.normal_video.max_framerate;
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *framerate = imgsensor_info.cap.max_framerate;
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            *framerate = imgsensor_info.hs_video.max_framerate;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            *framerate = imgsensor_info.slim_video.max_framerate;
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            *framerate = imgsensor_info.custom1.max_framerate;
            break;
        default:
            break;
    }

    return ERROR_NONE;
}


static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
                             UINT8 *feature_para,UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16=(UINT16 *) feature_para;
    UINT16 *feature_data_16=(UINT16 *) feature_para;
    UINT32 *feature_return_para_32=(UINT32 *) feature_para;
    UINT32 *feature_data_32=(UINT32 *) feature_para;
    unsigned long long *feature_data=(unsigned long long *) feature_para;
    //unsigned long long *feature_return_para=(unsigned long long *) feature_para;

    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

    LOG_INF("feature_id = %d\n", feature_id);
    switch (feature_id) {
        case SENSOR_FEATURE_GET_PERIOD:
            *feature_return_para_16++ = imgsensor.line_length;
            *feature_return_para_16 = imgsensor.frame_length;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
            *feature_return_para_32 = imgsensor.pclk;
            *feature_para_len=4;
            break;
            /*Longyuan.Yang@camera.driver add for video crash */
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
            switch (*feature_data) {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                        *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                                = imgsensor_info.cap.pclk;
                        break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                        *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                                = imgsensor_info.normal_video.pclk;
                        break;
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                        *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                                = imgsensor_info.hs_video.pclk;
                        break;
                case MSDK_SCENARIO_ID_CUSTOM1:
                        *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                                = imgsensor_info.custom1.pclk;
                        break;
                case MSDK_SCENARIO_ID_SLIM_VIDEO:
                        *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                                = imgsensor_info.slim_video.pclk;
                        break;
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                default:
                        *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                                = imgsensor_info.pre.pclk;
                        break;
                }
                break;
        case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
            switch (*feature_data) {
            case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                    *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                            = (imgsensor_info.cap.framelength << 16)
                                     + imgsensor_info.cap.linelength;
                    break;
            case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                    *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                            = (imgsensor_info.normal_video.framelength << 16)
                                    + imgsensor_info.normal_video.linelength;
                    break;
            case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                    *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                            = (imgsensor_info.hs_video.framelength << 16)
                                     + imgsensor_info.hs_video.linelength;
                    break;
            case MSDK_SCENARIO_ID_SLIM_VIDEO:
                    *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                            = (imgsensor_info.slim_video.framelength << 16)
                                     + imgsensor_info.slim_video.linelength;
                    break;
            case MSDK_SCENARIO_ID_CUSTOM1:
                    *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                            = (imgsensor_info.custom1.framelength << 16)
                                     + imgsensor_info.custom1.linelength;
                    break;
            case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            default:
                    *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                            = (imgsensor_info.pre.framelength << 16)
                                     + imgsensor_info.pre.linelength;
                    break;
            }
            break;
        case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
            {
                    kal_uint32 rate;

                    switch (*feature_data) {
                    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                        rate = imgsensor_info.cap.mipi_pixel_rate;
                        break;
                    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                        rate = imgsensor_info.normal_video.mipi_pixel_rate;
                        break;
                    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                        rate = imgsensor_info.hs_video.mipi_pixel_rate;
                        break;
                    case MSDK_SCENARIO_ID_SLIM_VIDEO:
                        rate = imgsensor_info.slim_video.mipi_pixel_rate;
                        break;
                    case MSDK_SCENARIO_ID_CUSTOM1:
                        rate = imgsensor_info.custom1.mipi_pixel_rate;
                        break;
                    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                    default:
                        rate = imgsensor_info.pre.mipi_pixel_rate;
                        break;
                    }
                    *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
                }
                break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            set_shutter(*feature_data);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            night_mode((BOOL) *feature_data);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            set_gain((UINT16) *feature_data);
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
            break;
        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
            // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
            // if EEPROM does not exist in camera module.
            *feature_return_para_32=LENS_DRIVER_ID_DO_NOT_CARE;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_SET_VIDEO_MODE:
            set_video_mode(*feature_data);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            get_imgsensor_id(feature_return_para_32);
            break;
        case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
            set_shutter_frame_length((UINT16) (*feature_data), (UINT16) (*(feature_data + 1)));
            break;
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            set_auto_flicker_mode((BOOL)*feature_data_16,*(feature_data_16+1));
            break;
        case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
            set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
            break;
        case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
            get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
            break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            set_test_pattern_mode((BOOL)*feature_data);
            break;
        case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing
            *feature_return_para_32 = imgsensor_info.checksum_value;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_SET_FRAMERATE:
            LOG_INF("current fps :%d\n", (UINT32)*feature_data);
            spin_lock(&imgsensor_drv_lock);
            imgsensor.current_fps = *feature_data;
            spin_unlock(&imgsensor_drv_lock);
            break;
        case SENSOR_FEATURE_SET_HDR:
            LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data);
            spin_lock(&imgsensor_drv_lock);
            imgsensor.ihdr_en = (BOOL)*feature_data;
            spin_unlock(&imgsensor_drv_lock);
            break;
        case SENSOR_FEATURE_GET_CROP_INFO:
            LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);

            wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

            switch (*feature_data_32) {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[1],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[2],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[3],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_SLIM_VIDEO:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[4],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_CUSTOM1:
                    memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5],
                        sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                default:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
            }
            break;
        case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
            LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",(UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
            ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
            break;
        default:
            break;
    }

    return ERROR_NONE;
}    /*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
    open,
    get_info,
    get_resolution,
    feature_control,
    control,
    close
};

UINT32 GC02M0_MIPI_MONO1_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&sensor_func;
    return ERROR_NONE;
}    /*    GC02m0_MIPI_RAW_SensorInit    */
