/* sensor_task.h */
#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H
#include "FreeRTOS.h"
void vSensorReadTask(void *pvParameters);
#endif

/* ------------------------------------------------------------------ */

/* process_task.h */
#ifndef PROCESS_TASK_H
#define PROCESS_TASK_H
#include "FreeRTOS.h"
#include "shared_resources.h"
void vDataProcessTask(void *pvParameters);
BaseType_t DataProcess_GetLatest(ProcessedReading_t *out);
#endif

/* ------------------------------------------------------------------ */

/* display_task.h */
#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H
void vDisplayTask(void *pvParameters);
#endif

/* ------------------------------------------------------------------ */

/* fault_task.h */
#ifndef FAULT_TASK_H
#define FAULT_TASK_H
void vFaultMonitorTask(void *pvParameters);
void FaultMonitor_TriggerAlert(void);
#endif
