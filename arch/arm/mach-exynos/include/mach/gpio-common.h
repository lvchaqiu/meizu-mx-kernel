#include <sound/soc.h>

#define MX_FACTORY_TEST_BT				0
#define MX_FACTORY_TEST_CAMERA			1
#define MX_FACTORY_TEST_ALL				2

extern void (*exynos4_sleep_gpio_set)(void);

#ifdef	CONFIG_WM8958_ALWAYS_ON_SUSPEND	
extern int wm8994_isalwayson(struct snd_soc_codec *codec);	
#endif
#ifdef CONFIG_SND_SOC_WM8994
extern int wm8994_get_playback_path(struct snd_soc_codec *codec);
#endif
extern int mx_is_factory_test_mode(int type);
extern int mx_set_factory_test_led(int on);