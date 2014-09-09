#ifndef _LINUX_NH_FT5X06_PDATA_H
#define _LINUX_NH_FT5X06_PDATA_H

/*
 * Optional platform data
 *
 * Use this if you want the driver to configure the touch interrupt input pin
 */
struct nh_ft5x06_pdata {
	int touch_gpio;
};

#endif
