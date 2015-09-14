/*
 * File: domain.c
 *
 * virtual power domain 
 *
 * Copyright (C) 2015 Anapass Inc.
 * Author: Youngdo, Lee <ydlee@anapass.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>

/*****************************************************************************/

#include "domain.h"
#include "debug.h"

/*****************************************************************************/

#define gdm_pm_domain_on(gpd)       __gdm_pm_domain_onoff(gpd, true)
#define gdm_pm_domain_off(gpd)      __gdm_pm_domain_onoff(gpd, false)

/*****************************************************************************/

static inline int __gdm_pm_domain_is_on(struct gdm_pm_domain *gpd)
{
    return gpd->is_on;
}

static int __gdm_pm_domain_onoff(struct gdm_pm_domain *gpd, bool on)
{
    if (on) {
        if (!__gdm_pm_domain_is_on(gpd))
            DBG(">>> Turn %s power domain %s.", on ? "on" : "off", gpd->name);
    }
    else {
        if (__gdm_pm_domain_is_on(gpd))
            DBG(">>> Turn %s power domain %s.", on ? "on" : "off", gpd->name);
    }
    gpd->is_on = on;
    return 0;
}

static int gdm_pm_domain_get(struct gdm_pm_domain *gpd)
{
    mutex_lock(&gpd->lock);
    if (gpd->refcnt > 0)
        gdm_pm_domain_on(gpd);
    mutex_unlock(&gpd->lock);
    return 0;
}

static int gdm_pm_domain_put(struct gdm_pm_domain *gpd)
{
    mutex_lock(&gpd->lock);
    gdm_pm_domain_off(gpd);
    mutex_unlock(&gpd->lock);
    return 0;
}

static int gdm_pm_domain_runtime_get(struct gdm_pm_domain *gpd)
{
    mutex_lock(&gpd->lock);
    if (!gpd->refcnt)
        gdm_pm_domain_on(gpd);
    gpd->refcnt++;
    mutex_unlock(&gpd->lock);
    return 0;
}

static int gdm_pm_domain_runtime_put(struct gdm_pm_domain *gpd)
{
    mutex_lock(&gpd->lock);
    if (gpd->refcnt > 0) {
        gpd->refcnt--;
        if (!gpd->refcnt)
            gdm_pm_domain_off(gpd);
    }
    mutex_unlock(&gpd->lock);
    return 0;
}

static int gdm_pm_domain_can_power_down(struct gdm_pm_domain *gpd)
{
    return 0;
}

#ifdef  CONFIG_PM_SLEEP
static int vpd_domain_suspend(struct device *dev)
{
    int ret;
    struct gdm_pm_domain *gpd = to_gdm_pm_domain(dev->pm_domain);

    ret = pm_generic_suspend(dev);

    // When all devices in this domain are powered off,
    // domain's power can be turned off.
    // To do this, we check PMU's domain switch bits.
    if (gdm_pm_domain_can_power_down(gpd))
        gdm_pm_domain_put(gpd);

    //DBG("domain %s system suspend.", gpd->name);

    return ret;
}

static int vpd_domain_resume(struct device *dev)
{
    int ret;
    struct gdm_pm_domain *gpd = to_gdm_pm_domain(dev->pm_domain);

    gdm_pm_domain_get(gpd);

    //DBG("domain %s system resume.", gpd->name);

    ret = pm_generic_resume(dev);

    return ret;
}
#else
#define vpd_domain_suspend    NULL
#define vpd_domain_resume     NULL
#endif  /*CONFIG_PM_SLEEP*/


#ifdef  CONFIG_PM_RUNTIME
static int vpd_domain_runtime_suspend(struct device *dev)
{
    int ret;
    struct gdm_pm_domain *gpd = to_gdm_pm_domain(dev->pm_domain);

    ret = pm_generic_runtime_suspend(dev);

    gdm_pm_domain_runtime_put(gpd);
    DBG("domain %s runtime suspend.", gpd->name);

    return ret;
}

static int vpd_domain_runtime_resume(struct device *dev)
{
    int ret;
    struct gdm_pm_domain *gpd = to_gdm_pm_domain(dev->pm_domain);

    DBG("domain %s runtime resume.", gpd->name);
    gdm_pm_domain_runtime_get(gpd);

    ret = pm_generic_runtime_resume(dev);

    return ret;
}
#else
#define vpd_domain_runtime_suspend    NULL
#define vpd_domain_runtime_resume     NULL
#endif  /*CONFIG_PM_RUNTIME*/

struct gdm_pm_domain vpd_core_domain = {
    .name = "vpd_core_domain",
    .pm_domain = {
        .ops = {
            SET_SYSTEM_SLEEP_PM_OPS(
                    vpd_domain_suspend, vpd_domain_resume)
            SET_RUNTIME_PM_OPS(
                    vpd_domain_runtime_suspend, vpd_domain_runtime_resume, NULL)
            //USE_PLATFORM_PM_SLEEP_OPS
        },
    },
    .lock = __MUTEX_INITIALIZER(vpd_core_domain.lock),
};

struct gdm_pm_domain vpd_video_domain = {
    .name = "vpd_video_domain",
    .pm_domain = {
        .ops = {
            SET_SYSTEM_SLEEP_PM_OPS(
                    vpd_domain_suspend, vpd_domain_resume)
            SET_RUNTIME_PM_OPS(
                    vpd_domain_runtime_suspend, vpd_domain_runtime_resume, NULL)
            //USE_PLATFORM_PM_SLEEP_OPS
        },
    },
    .lock = __MUTEX_INITIALIZER(vpd_video_domain.lock),
};
