#include "tasks.h"
#include "shared_resources.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <math.h>

#define SENSOR_TASK_PERIOD_MS 100

static uint32_t s_droppedSamples = 0;
static float s_phase = 0.0f;

void vSensorReadTask(void *pvParameters)
{
    (void)pvParameters;

    TickType_t      xLastWakeTime;
    SensorReading_t reading;

    xLastWakeTime = xTaskGetTickCount();

    UART_Log("[SENSOR] Task started. Period=%d ms\r\n", SENSOR_TASK_PERIOD_MS);

    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS));

        /* Simulated sensor values — replace with ADC reads on real hardware */
        reading.temperature_raw = 25.0f + 8.0f * sinf(s_phase);
        reading.pressure_raw    = 1013.0f + 20.0f * cosf(s_phase * 0.7f);
        s_phase += 0.15f;

        reading.timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        reading.sensor_id    = 0;

        if (xQueueSend(xSensorQueue, &reading, 0) != pdTRUE)
        {
            s_droppedSamples++;
            UART_Log("[SENSOR] WARN: queue full, sample dropped (total=%lu)\r\n",
                     s_droppedSamples);
        }
    }
}
