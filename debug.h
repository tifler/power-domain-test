#ifndef __XDEBUG_H__
#define __XDEBUG_H__

#define DBG(fmt, args...)   \
    printk("[%s:%s:%d] " fmt "\n", __FILE__, __func__, __LINE__, ## args)

#define ERR                 DBG
#define INFO                DBG

#define TRACE()             DBG("")

#endif  /*__XDEBUG_H__*/
