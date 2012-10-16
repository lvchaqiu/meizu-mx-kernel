
#ifndef _MX_SPDIF_PLATFORM_DATA_H_
#define _MX_SPDIF_PLATFORM_DATA_H_
struct mx_spdif_platform_data
{
	void (*spdif_output_enable)(int en);
};
#endif//_MX_SPDIF_PLATFORM_DATA_H_