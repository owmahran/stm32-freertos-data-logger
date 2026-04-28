/**
 * @file    fault_task.c
 * @brief   FaultMonitorTask — watches sensor bounds, triggers alerts.
 *
 * Highest priority task (PRIORITY_FAULT = 4). Blocks on xAlertSem
 * using xSemaphoreTake(portMAX_DELAY) so it consumes zero CPU when
 * everything is in range.
 *
 * Two paths to fault detection:
 *
 *  1. Polled path (this task): After every sensor reading is
 *     processed, DataProcessTask could call xSemaphoreGive(xAlertSem)
 *     if limits are breached. This task then immediately preempts
 *     everything else and handles the alert.
 *
 *  2. ISR path (commented example): An ADC watchdog interrupt can
 *     call xSemaphoreGiveFromISR() to wake this task with hardware
 *     latency, without involving the scheduler tick.
 *
 * Alert handling:
 *  - Blinks the onboard LED (PA5) at 10 Hz
 *  - Logs the fault over UART
 *  - Holds the alert state for FAULT_HOLD_MS then re-arms
 *
 * NOTE: In a safety-critical system this task would also write a
 * fault code to EEPROM/flash and trigger a watchdog reset if the
 * fault persists beyond a safe timeout.
 */

#include "tasks.h"
#include "shared_resources.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

#define FAULT_LED_PORT          GPIOA
#define FAULT_LED_PIN           GPIO_PIN_5
#define FAULT_BLINK_PERIOD_MS   100     /* 10 Hz blink while fault is active */
#define FAULT_HOLD_MS           3000    /* How long to hold alert before re-arm */

/**
 * @brief   FaultMonitorTask entry function.
 */
void vFaultMonitorTask(void *pvParameters)
{
    (void)pvParameters;

    UART_Log("[FAULT_MON] Task started. Thresholds: T=[%.1f, %.1f]C P=[%.1f, %.1f]hPa\r\n",
             TEMP_LOWER_LIMIT_C, TEMP_UPPER_LIMIT_C,
             PRES_LOWER_LIMIT_HPA, PRES_UPPER_LIMIT_HPA);

    for (;;)
    {
        /*
         * Block here until another task (or ISR) calls xSemaphoreGive().
         * This task is effectively SUSPENDED — it takes no CPU time
         * until a fault event occurs.
         */
        xSemaphoreTake(xAlertSem, portMAX_DELAY);

        /* ---- FAULT ACTIVE ---- */
        UART_Log("[FAULT_MON] !!! ALERT TRIGGERED at t=%lu ms !!!\r\n",
                 xTaskGetTickCount() * portTICK_PERIOD_MS);
        UART_Log("[FAULT_MON] Entering fault-hold for %d ms\r\n", FAULT_HOLD_MS);

        TickType_t faultStart = xTaskGetTickCount();
        TickType_t holdTicks  = pdMS_TO_TICKS(FAULT_HOLD_MS);

        /* Blink LED rapidly for the duration of the hold period */
        while ((xTaskGetTickCount() - faultStart) < holdTicks)
        {
            HAL_GPIO_TogglePin(FAULT_LED_PORT, FAULT_LED_PIN);
            vTaskDelay(pdMS_TO_TICKS(FAULT_BLINK_PERIOD_MS));
        }

        /* Clear alert: turn LED off, log recovery */
        HAL_GPIO_WritePin(FAULT_LED_PORT, FAULT_LED_PIN, GPIO_PIN_RESET);
        UART_Log("[FAULT_MON] Fault hold expired. Re-arming.\r\n");

        /*
         * Drain any extra semaphore gives that arrived during the hold
         * period so we start clean.
         */
        while (xSemaphoreTake(xAlertSem, 0) == pdTRUE) {}
    }
}

/* --------------------------------------------------------------------------
 * Example: call this from DataProcessTask when a limit is exceeded.
 * Safe to call from task context; use xSemaphoreGiveFromISR() from ISRs.
 * -------------------------------------------------------------------------- */
void FaultMonitor_TriggerAlert(void)
{
    xSemaphoreGive(xAlertSem);
}

/* --------------------------------------------------------------------------
 * Example ISR: ADC Analog Watchdog fires when input exceeds hardware limits.
 * void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef *hadc)
 * {
 *     BaseType_t xHigherPriorityTaskWoken = pdFALSE;
 *     xSemaphoreGiveFromISR(xAlertSem, &xHigherPriorityTaskWoken);
 *     portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
 * }
 * -------------------------------------------------------------------------- */
