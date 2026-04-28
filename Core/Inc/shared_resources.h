/**
 * @file    shared_resources.h
 * @brief   Shared RTOS handles, data types, and task priorities.
 *
 * Every task includes this header. Defines the inter-task
 * communication objects and the canonical SensorReading_t struct
 * that flows through the queue.
 */

#ifndef SHARED_RESOURCES_H
#define SHARED_RESOURCES_H


#include "stm32f4xx_hal.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include <stdint.h>

/* --------------------------------------------------------------------------
 * Build-time configuration
 * -------------------------------------------------------------------------- */
#define SENSOR_QUEUE_LENGTH     8       /* Max items in xSensorQueue           */
#define MOVING_AVG_WINDOW       5       /* Samples in moving average filter    */

#define TEMP_UPPER_LIMIT_C      70.0f   /* Fault threshold — temperature       */
#define TEMP_LOWER_LIMIT_C      (-10.0f)
#define PRES_UPPER_LIMIT_HPA    1100.0f /* Fault threshold — pressure          */
#define PRES_LOWER_LIMIT_HPA    900.0f

#define SENSOR_TASK_PERIOD_MS   100     /* SensorReadTask period               */
#define DISPLAY_TASK_PERIOD_MS  500     /* DisplayTask UART output period      */

/* --------------------------------------------------------------------------
 * Task priorities  (0 = lowest, configMAX_PRIORITIES-1 = highest)
 * -------------------------------------------------------------------------- */
#define PRIORITY_DISPLAY    1
#define PRIORITY_PROCESS    2
#define PRIORITY_SENSOR     3
#define PRIORITY_FAULT      4   /* Highest — preempts everything on alert */

/* --------------------------------------------------------------------------
 * Data structures passed through the queue
 * -------------------------------------------------------------------------- */

/** Raw ADC reading from SensorReadTask, sent into xSensorQueue. */
typedef struct
{
    float    temperature_raw;   /* Degrees Celsius, direct from ADC conversion */
    float    pressure_raw;      /* hPa, direct from ADC conversion             */
    uint32_t timestamp_ms;      /* xTaskGetTickCount() * portTICK_PERIOD_MS    */
    uint8_t  sensor_id;         /* Reserved for multi-sensor expansion         */
} SensorReading_t;

/** Filtered reading produced by DataProcessTask, stored for DisplayTask. */
typedef struct
{
    float    temperature_avg;   /* Moving average over MOVING_AVG_WINDOW samples */
    float    pressure_avg;
    float    temperature_raw;   /* Last raw sample for comparison                 */
    float    pressure_raw;
    uint32_t timestamp_ms;
} ProcessedReading_t;

/* --------------------------------------------------------------------------
 * Shared RTOS object handles
 * Defined in main.c, extern'd here so all tasks can access them.
 * -------------------------------------------------------------------------- */
extern QueueHandle_t     xSensorQueue;   /* SensorRead -> DataProcess            */
extern SemaphoreHandle_t xAlertSem;      /* FaultMonitor gives, FaultMonitor takes*/

/* --------------------------------------------------------------------------
 * UART handle — tasks call UART_Log() rather than HAL directly
 * -------------------------------------------------------------------------- */
extern UART_HandleTypeDef huart2;

/* --------------------------------------------------------------------------
 * Utility: UART printf wrapper (implemented in display_task.c)
 * Uses a FreeRTOS mutex internally to serialise access.
 * -------------------------------------------------------------------------- */
void UART_Log(const char *fmt, ...);

#endif /* SHARED_RESOURCES_H */
