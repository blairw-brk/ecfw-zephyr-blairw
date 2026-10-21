#ifdef CONFIG_LOM_MGMT_FAULT_GPIO_MON

#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include "mec172x_adl_n_cadiz.h"
#include "task_handler.h"

#include "lom_mgmt_proc_inc.h"
#include "fault_gpio_mon.h"

#include "host_event.h"

LOG_MODULE_DECLARE(lom_mgmt, CONFIG_LOM_MGMT_PROC_LOG_LEVEL);

K_SEM_DEFINE(fgmon_thread_started_sem, 0, 1);

static void fault_gpio_mon_thread(void *p1, void *p2, void *p3);

K_THREAD_DEFINE(fgmon_thread_tid, 1024, fault_gpio_mon_thread,
	NULL, NULL, NULL, EC_TASK_PRIORITY, K_INHERIT_PERMS, -1);

#define MON_PERIOD_MS 100

struct fault_gpio_info {
	uint32_t pin;
	int active_level;
	int record_level;
	uint8_t event;
	char * name;
};

#define ACTIVE_LOW  0
#define ACTIVE_HIGH 1

static struct fault_gpio_info fault_gpios[] =
{
	{CATERR_EC_N,    ACTIVE_LOW, ACTIVE_HIGH, HOST_EVENT_CATERR,   "CATERR"  },
	{THRMTRIP_3P3_N, ACTIVE_LOW, ACTIVE_HIGH, HOST_EVENT_THRMTRIP, "THRMTRIP"},
};

static void fault_gpio_mon_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	k_sem_give(&fgmon_thread_started_sem);

	while (true) {
		for (unsigned i = 0; i < ARRAY_SIZE(fault_gpios); i++) {
			struct fault_gpio_info *fgi = &fault_gpios[i];

			int level = gpio_read_pin(fgi->pin);
			if (level < 0) {
				LOG_ERR("read GPIO-%s fail: %d", fgi->name, level);
				continue;
			}

			//LOG_DBG_EVENT("GPIO-%s level is %d", fgi->name, level);

			if (level == fgi->active_level && fgi->record_level != level) {
				LOG_WRN("FAULT GPIO-%s ACTIVED!", fgi->name);
				host_event_submit(fgi->event);
			}

			fgi->record_level = level;
		}

		k_msleep(MON_PERIOD_MS);
	}
}

int fault_gpio_mon_thread_start(void)
{
	k_thread_name_set(fgmon_thread_tid, "LOM_MGMT_FGMON");
	k_thread_start(fgmon_thread_tid);

	k_sem_take(&fgmon_thread_started_sem, K_FOREVER);

	LOG_INF("Fault GPIO Monitor thread started. polling interval: %d(ms)", MON_PERIOD_MS);

	for (unsigned i = 0; i < ARRAY_SIZE(fault_gpios); i++) {
		LOG_INF("  monitoring GPIO-%s(%d), active level %d",
			fault_gpios[i].name, fault_gpios[i].pin, fault_gpios[i].active_level);
	}

	return 0;
}

#endif
