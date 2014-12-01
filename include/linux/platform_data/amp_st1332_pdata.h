#ifndef _LINUX_AMP_ST1332_PDATA_H
#define _LINUX_AMP_ST1332_PDATA_H

/*
 * Optional platform data
 *
 * Use this if you want the driver to configure the touch interrupt input pin
 */
struct amp_st1332_pdata {
	int touch_gpio;
};

#endif

