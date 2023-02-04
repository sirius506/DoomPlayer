#ifndef __DOOMPLAYER_H__
#define __DOOMPLAYER_H__
#include <string.h>
#include "main.h"
#include "SEGGER_RTT.h"
#include "cmsis_os.h"

#include "targetdev_conf.h"
#include "doomfs.h"

typedef StaticTask_t osStaticThreadDef_t;
typedef StaticQueue_t osStaticMessageQDef_t;
typedef StaticSemaphore_t osStaticMutexDef_t;
typedef StaticSemaphore_t osStaticSemaphoreDef_t;
typedef StaticTimer_t osStaticTimerDef_t;
typedef StaticEventGroup_t osStaticEventGroupDef_t;

#define	STR(S)	#S
#define	TASK_DEF(tname, stacksize, tpriority) \
 SECTION_DTCMRAM static uint32_t taskbuff_##tname[stacksize]; \
 SECTION_DTCMRAM osStaticThreadDef_t ContBlock_##tname; \
 const osThreadAttr_t attributes_##tname = { \
  .name = STR(tname), \
  .cb_mem = &ContBlock_##tname, \
  .cb_size = sizeof(ContBlock_##tname), \
  .stack_mem = &taskbuff_##tname[0], \
  .stack_size = sizeof(taskbuff_##tname), \
  .priority = (osPriority_t) tpriority, \
};

#define	MESSAGEQ_DEF(qname, buffer_ptr, buffer_size) \
 SECTION_DTCMRAM static osStaticMessageQDef_t ContBlock_##qname; \
 static const osMessageQueueAttr_t attributes_##qname = { \
  .name = STR(qname), \
  .cb_mem = &ContBlock_##qname, \
  .cb_size = sizeof(ContBlock_##qname), \
  .mq_mem = buffer_ptr, \
  .mq_size = buffer_size, \
};

#define	SEMAPHORE_DEF(sname) \
 SECTION_DTCMRAM static osStaticSemaphoreDef_t ContBlock_##sname; \
 static const osSemaphoreAttr_t attributes_##sname = { \
  .name = STR(sname), \
  .cb_mem = &ContBlock_##sname, \
  .cb_size = sizeof(ContBlock_##sname), \
};

#define	MUTEX_DEF(mname) \
  SECTION_DTCMRAM static osStaticMutexDef_t ContBlock_##mname; \
  static const osMutexAttr_t attributes_##mname = { \
    .name = STR(mname), \
    .cb_mem = &ContBlock_##mname, \
    .cb_size = sizeof(ContBlock_##mname), \
};

#define	EVFLAG_DEF(ename) \
  SECTION_DTCMRAM static osStaticEventGroupDef_t ContBlock_##ename; \
  static const osEventFlagsAttr_t attributes_##ename = { \
    .name = STR(ename), \
    .cb_mem = &ContBlock_##ename, \
    .cb_size = sizeof(ContBlock_##ename), \
};

#endif
