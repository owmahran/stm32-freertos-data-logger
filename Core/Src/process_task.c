/**
 * @file    process_task.c
 * @brief   DataProcessTask — applies a moving average filter to sensor data.
 *
 * Blocks on xQueueReceive waiting for SensorReading_t items produced by
 * SensorReadTask. For each item it updates two circular-buffer moving
 * averages (temperature and pressure) and writes the result to a
 * shared ProcessedReading_t protected by a mutex.
 *
 * Design notes:
 *  - xQueueReceive with portMAX_DELAY: task stays BLOCKED (off the
 *    ready list) until an item arrives — zero CPU cost while waiting.
 *  - The processed result is written under a mutex so DisplayTask
 *    can safely read it at any time without tearing.
 *  - Moving average uses a fixed-size circular buffer; no malloc.
 */

#include "tasks.h"
#include "shared_resources.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>

/* --------------------------------------------------------------------------
 * Shared processed reading + protecting mutex
 * -------------------------------------------------------------------------- */
static ProcessedReading_t  s_latestReading;
static SemaphoreHandle_t   s_readingMutex = NULL;

/* --------------------------------------------------------------------------
 * Moving average state (circular buffers)
 * -------------------------------------------------------------------------- */
typedef struct
{
    float    buf[MOVING_AVG_WINDOW];
    uint8_t  head;
    uint8_t  count;
    float    sum;
} MovingAvg_t;

static MovingAvg_t s_tempAvg = {0};
static MovingAvg_t s_presAvg = {0};

/**
 * @brief   Push a new sample into a moving average filter.
 * @return  Current average.
 */
static float MovingAvg_Update(MovingAvg_t *avg, float newSample)
{
    /* Subtract the oldest value before overwriting */
    avg->sum -= avg->buf[avg->head];
    avg->buf[avg->head] = newSample;
    avg->sum += newSample;

    avg->head = (avg->head + 1) % MOVING_AVG_WINDOW;
    if (avg->count < MOVING_AVG_WINDOW) { avg->count++; }

    return avg->sum / (float)avg->count;
}

/**
 * @brief   Thread-safe read of the latest ProcessedReading_t.
 * @param   out  Destination buffer; filled under mutex.
 * @return  pdTRUE if a valid reading was available, pdFALSE otherwise.
 */
BaseType_t DataProcess_GetLatest(ProcessedReading_t *out)
{
    if (s_readingMutex == NULL) { return pdFALSE; }

    if (xSemaphoreTake(s_readingMutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        memcpy(out, &s_latestReading, sizeof(ProcessedReading_t));
        xSemaphoreGive(s_readingMutex);
        return pdTRUE;
    }
    return pdFALSE;
}

/**
 * @brief   DataProcessTask entry function.
 */
void vDataProcessTask(void *pvParameters)
{
    (void)pvParameters;

    SensorReading_t  raw;
    ProcessedReading_t processed;

    /* Create mutex protecting s_latestReading */
    s_readingMutex = xSemaphoreCreateMutex();
    configASSERT(s_readingMutex != NULL);

    UART_Log("[DATA_PROC] Task started. Window=%d samples\r\n", MOVING_AVG_WINDOW);

    for (;;)
    {
        /* Block indefinitely until SensorReadTask sends a reading */
        if (xQueueReceive(xSensorQueue, &raw, portMAX_DELAY) == pdTRUE)
        {
            /* Apply moving average filters */
            processed.temperature_avg = MovingAvg_Update(&s_tempAvg, raw.temperature_raw);
            processed.pressure_avg    = MovingAvg_Update(&s_presAvg, raw.pressure_raw);
            processed.temperature_raw = raw.temperature_raw;
            processed.pressure_raw    = raw.pressure_raw;
            processed.timestamp_ms    = raw.timestamp_ms;

            /* Write under mutex so DisplayTask sees a consistent snapshot */
            if (xSemaphoreTake(s_readingMutex, pdMS_TO_TICKS(5)) == pdTRUE)
            {
                memcpy(&s_latestReading, &processed, sizeof(ProcessedReading_t));
                xSemaphoreGive(s_readingMutex);
            }

            UART_Log("[DATA_PROC] t=%lu ms  T_raw=%.2f  T_avg=%.2f  P_raw=%.1f  P_avg=%.1f\r\n",
                     processed.timestamp_ms,
                     processed.temperature_raw,
                     processed.temperature_avg,
                     processed.pressure_raw,
                     processed.pressure_avg);
        }
    }
}
