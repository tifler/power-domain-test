/*
 * File: vpd-device.c
 *
 * power test module
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

#include "domain.h"
#include "debug.h"

/*****************************************************************************/

#define VPD_MODULE_NAME         "vpd"
#define ATTR_PTR(_name)         (&dev_attr_##_name)

/*****************************************************************************/

struct vpd_context {
    struct mutex lock;  // refcnt lock
    int refcnt;
};

/*****************************************************************************/

#ifdef  CONFIG_PM_SLEEP
static int vpd_device_suspend(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct vpd_context *vpdc = dev_get_drvdata(dev);

    mutex_lock(&vpdc->lock);
    DBG("=====> [%s-%d] SYSTEM SUSPEND (refcnt = %d) <=====",
            pdev->name, pdev->id, vpdc->refcnt);
    mutex_unlock(&vpdc->lock);

    return 0;
}

static int vpd_device_resume(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct vpd_context *vpdc = dev_get_drvdata(dev);

    mutex_lock(&vpdc->lock);
    DBG("=====> [%s-%d] SYSTEM RESUME(refcnt = %d) <=====",
            pdev->name, pdev->id, vpdc->refcnt);
    mutex_unlock(&vpdc->lock);

    return 0;
}
#endif  /*CONFIG_PM_SLEEP*/

#ifdef  CONFIG_PM_RUNTIME
static int vpd_device_runtime_suspend(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct vpd_context *vpdc = dev_get_drvdata(dev);

    mutex_lock(&vpdc->lock);
    DBG("=====> [%s-%d] RUNTIME SUSPEND(refcnt = %d) <=====",
            pdev->name, pdev->id, vpdc->refcnt);
    mutex_unlock(&vpdc->lock);

    return 0;
}

static int vpd_device_runtime_resume(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct vpd_context *vpdc = dev_get_drvdata(dev);

    mutex_lock(&vpdc->lock);
    DBG("=====> [%s-%d] RUNTIME RESUME(refcnt = %d) <=====",
            pdev->name, pdev->id, vpdc->refcnt);
    mutex_unlock(&vpdc->lock);

    return 0;
}
#endif  /*CONFIG_PM_RUNTIME*/

/*****************************************************************************/

static const struct dev_pm_ops vpd_device_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(
            vpd_device_suspend, vpd_device_resume)
    SET_RUNTIME_PM_OPS(
            vpd_device_runtime_suspend, vpd_device_runtime_resume, NULL)
};

/*****************************************************************************/

static ssize_t vpd_device_sysfs_refcnt_show(
        struct device *dev, struct device_attribute *attr, char *buf)
{
    ssize_t ret;
    struct vpd_context *vpdc = dev_get_drvdata(dev);

    mutex_lock(&vpdc->lock);
    ret = sprintf(buf, "%d\n", vpdc->refcnt);
    mutex_unlock(&vpdc->lock);

    return ret;
}

static ssize_t vpd_device_sysfs_open_store(
        struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
    struct vpd_context *vpdc = dev_get_drvdata(dev);

    pm_runtime_get_sync(dev);

    mutex_lock(&vpdc->lock);
    vpdc->refcnt++;
    mutex_unlock(&vpdc->lock);

    DBG("refcnt = %d", vpdc->refcnt);

    return count;
}

static ssize_t vpd_device_sysfs_close_store(
        struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
    bool unlock = false;
    struct vpd_context *vpdc = dev_get_drvdata(dev);

    mutex_lock(&vpdc->lock);
    if (vpdc->refcnt > 0) {
        unlock = true;
        vpdc->refcnt--;
    }
    mutex_unlock(&vpdc->lock);

    if (unlock)
        pm_runtime_put(dev);

    DBG("refcnt = %d", vpdc->refcnt);

    return count;
}

static DEVICE_ATTR(refcnt, S_IRUGO, vpd_device_sysfs_refcnt_show, NULL);
static DEVICE_ATTR(open, S_IWUSR|S_IWGRP, NULL, vpd_device_sysfs_open_store);
static DEVICE_ATTR(close, S_IWUSR|S_IWGRP, NULL, vpd_device_sysfs_close_store);

static struct device_attribute *attrs[] = {
    ATTR_PTR(refcnt),
    ATTR_PTR(open),
    ATTR_PTR(close),
};

static void vpd_device_init_sysfs(struct platform_device *pdev)
{
    int i;
    int ret;

    TRACE();

    for (i = 0; i < ARRAY_SIZE(attrs); i++) {
        ret = device_create_file(&pdev->dev, attrs[i]);
        if (ret) {
            ERR("device_create_file failed.");
            for (--i; i >= 0; i--)
                device_remove_file(&pdev->dev, attrs[i]);
            break;
        }
    }
}

static void vpd_device_exit_sysfs(struct platform_device *pdev)
{
    int i;

    TRACE();

    for (i = 0; i < ARRAY_SIZE(attrs); i++)
        device_remove_file(&pdev->dev, attrs[i]);
}

static int vpd_device_probe(struct platform_device *pdev)
{
    int ret;
    struct vpd_context *vpdc;

    TRACE();

    vpdc = devm_kzalloc(&pdev->dev, sizeof(*vpdc), GFP_KERNEL);
    BUG_ON(!vpdc);

    mutex_init(&vpdc->lock);
    vpdc->refcnt = 0;
    platform_set_drvdata(pdev, vpdc);

    pm_runtime_enable(&pdev->dev);
    ret = pm_runtime_get_sync(&pdev->dev);
    if (ret) {
        ERR("pm_runtime_get_sync failed.(%d)", ret);
        return ret;
    }

    TRACE();

    vpd_device_init_sysfs(pdev);

    pm_runtime_put(&pdev->dev);

    DBG(VPD_MODULE_NAME "-%d probed.", pdev->id);

    return 0;
}

static int vpd_device_remove(struct platform_device *pdev)
{
    struct vpd_context *vpdc;

    TRACE();

    pm_runtime_disable(&pdev->dev);
    pm_runtime_set_suspended(&pdev->dev);

    vpdc = platform_get_drvdata(pdev);
    devm_kfree(&pdev->dev, vpdc);
    vpd_device_exit_sysfs(pdev);

    DBG(VPD_MODULE_NAME "-%d removed.", pdev->id);

    return 0;
}

static void vpd_device_device_release(struct device *dev)
{
}

static struct platform_device_id vpd_device_ids[] = {
    {
        .name = VPD_MODULE_NAME,
    },
    { },
};

static struct platform_driver vpd_platform_driver = {
    .probe = vpd_device_probe,
    .remove = vpd_device_remove,
    .id_table = vpd_device_ids,
    .driver = {
        .name = VPD_MODULE_NAME,
        .owner = THIS_MODULE,
        .pm = &vpd_device_pm_ops,
    },
};

static struct platform_device vpd_platform_devices[] = {
    [0] = {
        .name = VPD_MODULE_NAME,
        .id = 0,
        .num_resources = 0,
        .resource = NULL,
        .dev = {
            .release = vpd_device_device_release,
            .platform_data = NULL,
            .pm_domain = &vpd_core_domain.pm_domain,
        },
    },
    [1] = {
        .name = VPD_MODULE_NAME,
        .id = 1,
        .num_resources = 0,
        .resource = NULL,
        .dev = {
            .release = vpd_device_device_release,
            .platform_data = NULL,
            .pm_domain = &vpd_video_domain.pm_domain,
        },
    },
    [2] = {
        .name = VPD_MODULE_NAME,
        .id = 2,
        .num_resources = 0,
        .resource = NULL,
        .dev = {
            .release = vpd_device_device_release,
            .platform_data = NULL,
            .pm_domain = &vpd_video_domain.pm_domain,
        },
    },
};

/*****************************************************************************/

static int __init vpd_init(void)
{
    int i;
    int ret;

    ret = platform_driver_register(&vpd_platform_driver);
    if (ret) {
        ERR("vpd_device_platform_driver register failed.");
        return ret;
    }
    INFO("vpd_device_platform_driver installed.");

    for (i = 0; i < ARRAY_SIZE(vpd_platform_devices); i++) {
        ret = platform_device_register(&vpd_platform_devices[i]);
        if (ret) {
            for (--i; i >=0; i--)
                platform_device_unregister(&vpd_platform_devices[i]);

            platform_driver_unregister(&vpd_platform_driver);
            ERR("vpd_device_device register failed.");
            return ret;
        }
    }
    INFO("vpd_platform_device installed.");

    return 0;
}

static void __exit vpd_exit(void)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(vpd_platform_devices); i++)
        platform_device_unregister(&vpd_platform_devices[i]);

    platform_driver_unregister(&vpd_platform_driver);
}

/*****************************************************************************/

module_init(vpd_init);
module_exit(vpd_exit);

MODULE_AUTHOR("Youngdo, Lee <ydlee@anapass.com>");
MODULE_DESCRIPTION("Power Test Driver");
MODULE_LICENSE("GPL");
