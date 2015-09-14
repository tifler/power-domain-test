# power-domain-test

## Objectives

I'd like to use pm_domain's power management operations instead of drivers' 
dev_pm_ops. So I created this pseudo kernel module to test pm domain ops.

## Preparing test

1. Load module

`$ sudo insmod ./power-domain-test.ko`

2. Runtime PM Test

    1. Increase reference count.
        `$ sudo echo 1 > /sys/devices/platform/vpd.0/open`
    2.  Decrease reference count.
        `$ sudo echo 1 > /sys/devices/platform/vpd.0/close`

3. System Suspend Test

`$ sudo echo devices > /sys/power/pm_test`
`$ sudo echo disk > /sys/power/state`
