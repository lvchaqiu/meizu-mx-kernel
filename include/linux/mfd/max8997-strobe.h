/*
 * max8997-strobe.h - flash led control driver for the Maxim 8997
 *
 */

#ifndef __LINUX_MFD_MAX8997_STROBE_H
#define __LINUX_MFD_MAX8997_STROBE_H


/*MAX8997 flash led  control registers*/

#define STROBE_FLASH1_CUR                  (0x63)
#define STROBE_FLASH2_CUR                  (0x64)
#define STROBE_MOVIE_CUR                   (0x65)
#define STROBE_GSMB_CUR                    (0x66)
#define STROBE_BOOST_CNTL                  (0x67)
#define STROBE_LED_CNTL                    (0x68)
#define STROBE_FLASH_CNTL                  (0x69)
#define STROBE_WDT_CNTL                    (0x6a)
#define STROBE_MAXFLASH1                   (0x6b)
#define STROBE_MAXFLASH2                   (0x6c)
#define STROBE_INT_STATUS                  (0x6d)
#define STROBE_INT_STATUS_MASK             (0x6e)


#define BOOST_CNTL_VOL_SHIFT				(0)
#define BOOST_CNTL_VOL_MASK				    (0xf<<BOOST_CNTL_VOL_SHIFT)
#define BOOST_CNTL_MODE_SHIFT				(4)
#define BOOST_CNTL_MODE_MASK				(0x3<<BOOST_CNTL_MODE_SHIFT)
#define BOOST_CNTL_EN_SHIFT					(6)
#define BOOST_CNTL_EN_MASK					(0x1<<BOOST_CNTL_EN_SHIFT)

#define LED_CNTL_FLASH_EN_SHIFT				(0)
#define LED_CNTL_FLASH_EN_MASK			    (0x7<<LED_CNTL_FLASH_EN_SHIFT)
#define LED_CNTL_MOVIE_EN_SHIFT			    (3)
#define LED_CNTL_MOVIE_EN_MASK			    (0x7<<LED_CNTL_MOVIE_EN_SHIFT)

#define FLASH_CNTL_TMR_DUR_SHIFT			(0)
#define FLASH_CNTL_TMR_DUR_MASK			    (0x1f<<FLASH_CNTL_TMR_DUR_SHIFT)
#define FLASH_CNTL_TMR_MODE_SHIFT			(6)
#define FLASH_CNTL_TMR_MODE_MASK			(0x1<<FLASH_CNTL_TMR_MODE_SHIFT)
#define FLASH_CNTL_TMR_CNTL_SHIFT			(7)
#define FLASH_CNTL_TMR_CNTL_MASK			(0x1<<FLASH_CNTL_TMR_CNTL_SHIFT)

#define MAX_TIMER_DURATION                  800  //ms
#define MIN_TIMER_DURATION                  25   //ms

#define MIN_FLASH_CUR                       23440 //mA; amper 1000 time for no float operation in kernel
#define MAX_FLASH_CUR                       750000  

#define FLASH_CUR_CNTL_SHIFT                 (3)
#define FLASH_CUR_CNTL_MASK                  (0x1f << FLASH_CUR_CNTL_SHIFT)

#define MIN_MOVIE_CUR                       15625  
#define MAX_MOVIE_CUR                       250000  

#define MOVIE_CUR_CNTL_SHIFT                 (4)
#define MOVIE_CUR_CNTL_MASK                  (0xf << MOVIE_CUR_CNTL_SHIFT)


enum boost_mode{
	BOOST_ADAPTIVE = 0,                 //BOOST_CNT[0:3] is not valide under this kind of mode.
	BOOST_PROGRAMMABLE,                 //output voltage depends on the boost cntl voltage BOOST_CNT[0:3]
	BOOST_DROP_OUT,	
};
//here we are unlikely to use FLED2, so three types of mode are enough to use.
typedef enum {
    STROBE_NONE = 0,
    STROBE_FLASH_ONLY,
    STROBE_MOVIE_ONLY,
    STROBE_BOTH,
	STROBE_FLASH_BY_FLED_EN,//Ext.signal high-level
	STROBE_MOVIE_BY_FLED_EN,
	STROBE_BOTH_BY_FLED_EN
}strobe_mode;

//Don't use! these are just manifesting the datasheet of max8997.
enum flash_mode{
	BOTH_FLASH_I2C_DISABLE = 0,
	FLED1_FLASH_I2C_ENABLE,		//while FLED2 flash mode is disabled.
	FLED2_FLASH_I2C_ENABLE,
	BOTH_FLASH_I2C_ENABLE,
	FLASH_INVALIDE,
	FLED1_FLASH_EXT_ENABLE,		//while FLED2 flash mode is disabled
	FLED2_FLASH_EXT_ENABLE,
	BOTH_FLASH_EXT_ENABLE,	
};

enum movie_mode{
	BOTH_MOVIE_I2C_DISABLE = 0,
	FLED1_MOVIE_I2C_ENABLE,		//while FLED2 movie mode is disabled.
	FLED2_MOVIE_I2C_ENABLE,
	BOTH_MOVIE_I2C_ENABLE,
	MOVIE_INVALIDE,
	FLED1_MOVIE_EXT_ENABLE,		//while FLED2 movie mode is disabled and the mode is controled by FLED_EN
	FLED2_MOVIE_EXT_ENABLE,
	BOTH_MOVIE_EXT_ENABLE,		
};
//extern functions : can be used by camera components.

extern int osron_flash_led_set_current(int cur);
extern int osron_flash_led_mode_set(strobe_mode mode);
extern int osron_flash_led_enable(int enable);
extern int osron_flash_led_set_duration(int timer_duration);

#endif /*  __LINUX_MFD_MAX8997_STROBE_H */
