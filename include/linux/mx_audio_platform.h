
#ifndef _MX_AUDIO_PLATFORM_DATA_H_
#define _MX_AUDIO_PLATFORM_DATA_H_

struct mx_audio_platform_data
{
	void (*audio_switch)(int High);
};

#endif//_MX_AUDIO_PLATFORM_DATA_H_