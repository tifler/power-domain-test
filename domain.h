#ifndef __VPD_DOMAIN_H__
#define __VPD_DOMAIN_H__

#define to_gdm_pm_domain(pd)    \
    container_of(pd, struct gdm_pm_domain, pm_domain)

#define GDM_PM_DOMAIN_MAX_CHILDREN      8

struct gdm_pm_domain {
    const char *name;
    struct gdm_pm_domain *parent;
    struct gdm_pm_domain *children[GDM_PM_DOMAIN_MAX_CHILDREN];
    struct dev_pm_domain pm_domain;
    struct mutex lock;
    unsigned refcnt;
    bool is_on;
    unsigned long children_on_bits;
    int (*power_on)(struct gdm_pm_domain *gpd);
    int (*power_off)(struct gdm_pm_domain *gpd);
};

extern struct gdm_pm_domain vpd_core_domain;
extern struct gdm_pm_domain vpd_video_domain;

enum {
    GDM_PD_VID_TOP,
    GDM_PD_VID_GPU,
    GDM_PD_VID_IN,
    GDM_PD_VID_OUT,
    GDM_PD_VID_COD,
    GDM_PD_VID_SYS,
};

#endif  /*__VPD_DOMAIN_H__*/
