
#ifndef GENL_TEST_H
#define GENL_TEST_H

#include <linux/netlink.h>

#ifndef __KERNEL__
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#endif

#define GENL_TEST_FAMILY_NAME		"genl_test"
#define GENL_TEST_MCGRP0_NAME		"genl_mcgrp0"

#define GENL_TEST_ATTR_MSG_MAX		256
#define GENL_TEST_HELLO_INTERVAL	5000

enum {
	GENL_TEST_C_UNSPEC,		/* Must NOT use element 0 */
        GENL_TEST_C_MSG,
	HCSR_SET_PIN,
	HCSR_SET_PATTERN_1,
	HCSR_SET_PATTERN_2,
	HCSR_GET_SPEED,
        HCSR_CLEANUP,
};

enum genl_test_multicast_groups {
	GENL_TEST_MCGRP0,
	GENL_TEST_MCGRP1,
	GENL_TEST_MCGRP2,
};
#define GENL_TEST_MCGRP_MAX		3

#ifndef __KERNEL__
static char* genl_test_mcgrp_names[GENL_TEST_MCGRP_MAX] = {
	GENL_TEST_MCGRP0_NAME,
};
#endif
enum genl_test_attrs {
	GENL_TEST_ATTR_UNSPEC,		/* Must NOT use element 0 */
        GENL_TEST_ATTR_MSG,
	GENL_TEST_ATTR_DEVID,
	GENL_TEST_ATTR_GPIO_TRIGGER,
	GENL_TEST_ATTR_GPIO_ECHO,
	GENL_TEST_ATTR_GPIO_LED,
	GENL_TEST_ATTR_FREQ,
	GENL_TEST_ATTR_SAMPLE,
	GENL_TEST_ATTR_ANI1,
	GENL_TEST_ATTR_ANI2,
	GENL_TEST_ATTR_ANI3,
	GENL_TEST_ATTR_ANI4,
	GENL_TEST_ATTR_ANI5,
	GENL_TEST_ATTR_ANI6,
	GENL_TEST_ATTR_ANI7,
	GENL_TEST_ATTR_ANI8,
	GENL_TEST_ATTR_ANI9,
	GENL_TEST_ATTR_ANI10,
	GENL_TEST_ATTR_ANI11,
	GENL_TEST_ATTR_ANI12,
	GENL_TEST_ATTR_ANI13,
	GENL_TEST_ATTR_ANI14,
	__GENL_TEST_ATTR__MAX,
};
#define GENL_TEST_ATTR_MAX (__GENL_TEST_ATTR__MAX - 1)

static struct nla_policy genl_test_policy[GENL_TEST_ATTR_MAX+1] = {
	//[GENL_TEST_ATTR_DEVID] = {
	[GENL_TEST_ATTR_MSG] = {
		//.type = NLA_U32,
		.type = NLA_STRING,
#ifdef __KERNEL__
		.len = GENL_TEST_ATTR_MSG_MAX
#else
		.maxlen = GENL_TEST_ATTR_MSG_MAX
#endif
	},
       [GENL_TEST_ATTR_DEVID] = {
                .type = NLA_U32,
       },
       [GENL_TEST_ATTR_GPIO_ECHO] = {
                .type = NLA_U32,
       },
       [GENL_TEST_ATTR_GPIO_LED] = {
                .type = NLA_U32,
       },
       [GENL_TEST_ATTR_FREQ] = {
                .type = NLA_U32,
       },
       [GENL_TEST_ATTR_SAMPLE] = {
                .type = NLA_U32,
       },
       [GENL_TEST_ATTR_ANI1] = {
                .type = NLA_U32,
       },
       [GENL_TEST_ATTR_ANI2] = {
                .type = NLA_U32,
       },
       [GENL_TEST_ATTR_ANI3] = {
                .type = NLA_U32,
       },
       [GENL_TEST_ATTR_ANI4] = {
                .type = NLA_U32,
       },
       [GENL_TEST_ATTR_ANI5] = {
                .type = NLA_U32,
       },
       [GENL_TEST_ATTR_ANI6] = {
                .type = NLA_U32,
       },
       [GENL_TEST_ATTR_ANI7] = {
                .type = NLA_U32,
       },
       [GENL_TEST_ATTR_ANI8] = {
                .type = NLA_U32,
       },
       [GENL_TEST_ATTR_ANI9] = {
                .type = NLA_U32,
       },
       [GENL_TEST_ATTR_ANI10] = {
                .type = NLA_U32,
       },
       [GENL_TEST_ATTR_ANI11] = {
                .type = NLA_U32,
       },
       [GENL_TEST_ATTR_ANI12] = {
                .type = NLA_U32,
       },
       [GENL_TEST_ATTR_ANI13] = {
                .type = NLA_U32,
       },
       [GENL_TEST_ATTR_ANI14] = {
                .type = NLA_U32,
       },
};

#endif
