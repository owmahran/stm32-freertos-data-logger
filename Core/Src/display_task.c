/**
 * @file    display_task.c
 * @brief   DisplayTask — formats processed data and sends over UART.
 *
 * Runs at the lowest priority. Wakes every DISPLAY_TASK_PERIOD_MS,
 * fetches the latest ProcessedReading_t from DataProcessTask, and
 * formats a human-readable string to the terminal.
 *
 * Also owns the UART mutex and implements UART_Log() which all tasks
 * call for diagnostic output.
 *
 * Design notes:
 *  - All UART output is serialised through xUartMutex to prevent
 *    garbled lines when multiple tasks log simultaneously.
 *  - HAL_UART_Transmit is used in blocking mode here; for production
 *    replace with DMA + semaphore to free the CPU during transmission.
 */

#include "tasks.h"
//#include "process_task.h"
#include "shared_resources.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * UART serialisation mutex — taken by UART_Log(), given back after Tx
 * -------------------------------------------------------------------------- */
static SemaphoreHandle_t xUartMutex = NULL;

/* Shared Tx scratch buffer; only touched under xUartMutex */
static char s_txBuf[256];

/**
 * @brief   Thread-safe printf over UART.
 *          All tasks call this instead of HAL_UART_Transmit directly.
 */
void UART_Log(const char *fmt, ...)
{
    if (xUartMutex == NULL) { return; }

    /* Wait up to 50 ms for the UART to be free */
    if (xSemaphoreTake(xUartMutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        va_list args;
        va_start(args, fmt);
        int len = vsnprintf(s_txBuf, sizeof(s_txBuf), fmt, args);
        va_end(args);

        if (len > 0)
        {
            HAL_UART_Transmit(&huart2, (uint8_t *)s_txBuf, (uint16_t)len, 100);
        }

        xSemaphoreGive(xUartMutex);
    }
}

/**
 * @brief   DisplayTask entry function.
 */
void vDisplayTask(void *pvParameters)
{
    (void)pvParameters;

    TickType_t         xLastWakeTime;
    ProcessedReading_t reading;

    /* Create the UART mutex before any task calls UART_Log */
    xUartMutex = xSemaphoreCreateMutex();
    configASSERT(xUartMutex != NULL);

    /* Brief startup banner */
    UART_Log("\r\n");
    UART_Log("============================================\r\n");
    UART_Log("  STM32 FreeRTOS Multi-Sensor Data Logger  \r\n");
    UART_Log("  Build: " __DATE__ " " __TIME__ "         \r\n");
    UART_Log("============================================\r\n");
    UART_Log("[DISPLAY] Task started. Output period=%d ms\r\n",
             DISPLAY_TASK_PERIOD_MS);

    xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(DISPLAY_TASK_PERIOD_MS));

        if (DataProcess_GetLatest(&reading) == pdTRUE)
        {
            UART_Log(
                "[DISPLAY] %8lu ms | "
                "TEMP: %6.2f C (avg %.2f) | "
                "PRES: %7.1f hPa (avg %.1f)\r\n",
                reading.timestamp_ms,
                reading.temperature_raw,
                reading.temperature_avg,
                reading.pressure_raw,
                reading.pressure_avg
            );
        }
        else
        {
            UART_Log("[DISPLAY] WARN: no data from DataProcessTask yet\r\n");
        }
    }
}
