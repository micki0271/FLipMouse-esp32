/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * Copyright 2017 Benjamin Aigner <aignerb@technikum-wien.at,
 * beni@asterics-foundation.org>
 */
/** @file
 * @brief HAL TASK - This file contains the hardware abstraction for all IO related
 * stuff (except ADC).
 * 
 * Following peripherals are utilized here:<br>
 * * Input buttons
 * * RGB LED
 * * IR receiver (TSOP)
 * * IR LED (sender)
 * * Buzzer
 * 
 * @note Compared to the tasks in the folder "function_tasks" all HAL tasks are
 * singletons. Call init to initialize every necessary data structure.
 * 
 * @todo Test LED driver
 * @todo Implement Buzzer
 * @todo Implement IR driver
 * */
 
#include "hal_io.h"

/** @brief Log tag */
#define LOG_TAG "halIO"

/** @brief LED update queue
 * 
 * This queue is used to update the LED color & brightness
 * Please use one uint32_t value with following content:
 * * \<bits 0-7\> RED
 * * \<bits 8-15\> GREEN
 * * \<bits 16-23\> BLUE
 * * \<bits 24-31\> Fading time ([10¹ms] -> value of 200 is 2s)
 * 
 * @note Call halIOInit to initialize this queue.
 * @see halIOInit
 **/
QueueHandle_t halIOLEDQueue = NULL;

/** @brief GPIO ISR handler for buttons (internal/external)
 * 
 * This ISR handler is called on rising&falling edge of each button
 * GPIO.
 * 
 * It sets and clears the VB flags accordingly on each call.
 * These flags are used for button debouncing.
 * @see task_debouncer
 */
static void gpio_isr_handler(void* arg)
{
  uint32_t pin = (uint32_t) arg;
  uint8_t vb = 0;
  BaseType_t xResult, xHigherPriorityTaskWoken = pdFALSE;
  
  switch(pin)
  {
    case HAL_IO_PIN_BUTTON_EXT1: vb = VB_EXTERNAL1; break;
    case HAL_IO_PIN_BUTTON_EXT2: vb = VB_EXTERNAL2; break;
    case HAL_IO_PIN_BUTTON_INT1: vb = VB_INTERNAL1; break;
    case HAL_IO_PIN_BUTTON_INT2: vb = VB_INTERNAL2; break;
    default: return;
  }

  if(gpio_get_level(pin) == 0)
  {
    //set press flag
    xResult = xEventGroupSetBitsFromISR(virtualButtonsIn[vb/4],(1<<(vb%4)),&xHigherPriorityTaskWoken);
    //clear release flag
    xResult = xEventGroupClearBitsFromISR(virtualButtonsIn[vb/4],(1<<(vb%4 + 4)));
  } else {
    //set release flag
    xResult = xEventGroupSetBitsFromISR(virtualButtonsIn[vb/4],(1<<(vb%4 + 4)),&xHigherPriorityTaskWoken);
    //clear press flag
    xResult = xEventGroupClearBitsFromISR(virtualButtonsIn[vb/4],(1<<(vb%4)));
  }
  if(xResult == pdPASS) portYIELD_FROM_ISR();
}

/** @brief Callback to free the memory after finished transmission
 * 
 * This method calls free() on the buffer which is transmitted now.
 * @param channel Channel of RMT enging
 * @param arg rmt_item32_t* pointer to the buffer
 * */
void halIOIRFree(rmt_channel_t channel, void *arg)
{
  if(arg != NULL)
  {
    free((rmt_item32_t*)arg);
  }
}

/** @brief HAL TASK - IR sending task
 * 
 * This task receives via halIOIRSendQueue a pointer and a length
 * of waveform array, which is sent to the RMT unit.
 * 
 * @see halIOIR_t
 * @see halIOIRSendQueue
 * @param param Unused.
 */
void halIOIRSendTask(void * param)
{
  halIOIR_t recv;
  
  if(halIOIRSendQueue == NULL)
  {
    ESP_LOGW(LOG_TAG, "halIOIRSendQueue not initialised");
    while(halIOIRSendQueue == NULL) vTaskDelay(1000/portTICK_PERIOD_MS);
  }
  
  while(1)
  {
    //wait for updates
    if(xQueueReceive(halIOIRSendQueue,&recv,10000))
    {
      if(recv.buffer == NULL) continue;
      //check if there is an ongoing transmission. If yes, block.
      rmt_wait_tx_done(0, portMAX_DELAY);
      
      //register the callback again with the pointer to the buffer
      //after finished transmission, this callback frees this buffer
      rmt_register_tx_end_callback(halIOIRFree,recv.buffer);
      //To send data according to the waveform items.
      rmt_write_items(0, recv.buffer, recv.count, false);
    }
  }
}

/** @brief HAL TASK - IR receiving (recording) task
 * 
 * This task is used to store data from the RMT unit if the receiving
 * of an IR code is started.
 * By sending an element to the queue halIOIRRecvQueue, this task starts
 * to receive any incoming IR signals.
 * The status of the struct is updated (either finished, timeout or error)
 * on any status change.
 * Poll this value to see if the receiver is finished.
 * 
 * @see halIOIR_t
 * @see halIOIRRecvQueue
 * @todo How to start the receiver?
 * @param param Unused.
 */
void halIOIRRecvTask(void * param)
{
  halIOIR_t* recv;
  RingbufHandle_t rb = NULL;
  //get RMT RX ringbuffer
  rmt_get_ringbuf_handle(4, &rb);
  
  if(halIOIRRecvQueue == NULL)
  {
    ESP_LOGW(LOG_TAG, "halIOIRRecvQueue not initialised");
    while(halIOIRRecvQueue == NULL) vTaskDelay(1000/portTICK_PERIOD_MS);
  }
  
  while(1)
  {
    //wait for updates (triggered receiving)
    if(xQueueReceive(halIOIRRecvQueue,&recv,10000))
    {
      //acquire buffer for ALL receiving elements
      rmt_item32_t *buf = malloc(sizeof(rmt_item32_t)*TASK_HAL_IR_RECV_MAXIMUM_EDGES);
      uint16_t offset = 0;
      size_t rx_size = 0;
      
      //check buffer
      if(buf == NULL)
      {
        ESP_LOGE(LOG_TAG,"Not enough memory for receiving IR commands!");
        continue;
      }
      
      //start receiving on channel 4, flush all buffer elements
      rmt_rx_start(4, 1);
      //set target struct
      recv->status = IR_RECEIVING;
      //wait for one item until timeout or data is valid
      rmt_item32_t* item = (rmt_item32_t*) xRingbufferReceive(rb, &rx_size, TASK_HAL_IR_RECV_TIMEOUT/portTICK_PERIOD_MS);
      //got one item
      if(item) {
        //put data into buffer
        memcpy(&buf[offset], item, sizeof(rmt_item32_t)*rx_size);
        offset+= rx_size;
        //give item back to ringbuffer
        vRingbufferReturnItem(rb, (void*) item);
        //too much
        if(offset == TASK_HAL_IR_RECV_MAXIMUM_EDGES)
        {
          ESP_LOGE(LOG_TAG,"Too much IR edges, finished");
          recv->status = IR_OVERFLOW;
          continue;
        }
      } else {
        //timeout, cancel
        rmt_rx_stop(4);
        //update status accordingly
        if(offset > TASK_HAL_IR_RECV_MINIMUM_EDGES)
        {
          //save everything necessary to pointer from queue
          recv->buffer = buf;
          recv->count = offset;
          recv->status = IR_FINISHED;
        } else {
          recv->status = IR_TOOSHORT;
          ESP_LOGE(LOG_TAG,"Too short, no cmd");
          continue;
        }
      }
    }
  }
}

/** @brief HAL TASK - Buzzer update task
 * 
 * This task takes an instance of the buzzer update struct and creates
 * a tone on the buzzer accordingly.
 * Done via the LEDC driver & vTaskDelays
 * 
 * @see halIOBuzzer_t
 * @see halIOBuzzerQueue
 * @param param Unused.
 */
void halIOBuzzerTask(void * param)
{
  halIOBuzzer_t recv;
  
  if(halIOBuzzerQueue == NULL)
  {
    ESP_LOGW(LOG_TAG, "halIOLEDQueue not initialised");
    while(halIOBuzzerQueue == NULL) vTaskDelay(1000/portTICK_PERIOD_MS);
  }
  
  while(1)
  {
    //wait for updates
    if(xQueueReceive(halIOBuzzerQueue,(void*)&recv,10000))
    {
      //set duty, set frequency
      //do a tone only if frequency is != 0, otherwise it is just a pause
      if(recv.frequency != 0)
      {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, recv.frequency);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
      }
      
      //delay for duration
      vTaskDelay(recv.duration / portTICK_PERIOD_MS);
      
      //set duty to 0
      ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
      ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
  }
}

/** @brief HAL TASK - LED update task
 * 
 * This task simply takes an uint32_t value from the LED queue
 * and calls the ledc_fading_... methods of the esp-idf.
 * 
 * @see halIOLEDQueue
 * @param param Unused.
 */
void halIOLEDTask(void * param)
{
  uint32_t recv = 0;
  uint32_t duty = 0;
  uint32_t fade = 0;
  if(halIOLEDQueue == NULL)
  {
    ESP_LOGW(LOG_TAG, "halIOLEDQueue not initialised");
    while(halIOLEDQueue == NULL) vTaskDelay(1000/portTICK_PERIOD_MS);
  }
  while(1)
  {
    //wait for updates
    if(xQueueReceive(halIOLEDQueue,&recv,10000))
    {
      //updates received, sending to ledc driver
      
      //get fading time, unit is 10ms
      fade = ((recv & 0xFF000000) >> 24) * 10;
      
      //set fade with time (target duty and fading time)
      
      //1.) RED: map to 10bit and set to fading unit
      duty = (recv & 0x000000FF) * 2 * 2; 
      ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_0, \
        duty, fade);
      
      //2.) GREEN: map to 10bit and set to fading unit
      duty = ((recv & 0x0000FF00) >> 8) * 2 * 2; 
      ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_1, \
        duty, fade);
      
      //3.) BLUE: map to 10bit and set to fading unit
      duty = ((recv & 0x00FF0000) >> 16) * 2 * 2; 
      ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_2, \
        duty, fade);
      
      //start fading for RGB
      ledc_fade_start(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
      ledc_fade_start(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, LEDC_FADE_NO_WAIT);
      ledc_fade_start(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2, LEDC_FADE_NO_WAIT);
    }
  }
}

#define RMT_TICK_10_US    (80000000/RMT_CLK_DIV/100000)   /*!< RMT counter value for 10 us.(Source clock is APB clock) */
#define RMT_CLK_DIV      100    /*!< RMT counter clock divider */

/** @brief Initializing IO HAL
 * 
 * This method initializes the IO HAL stuff:<br>
 * * GPIO interrupt on 2 external and one internal button
 * * RMT engine (recording and replaying of infrared commands)
 * * LEDc driver for the RGB LED output
 * * PWM for buzzer output
 * */
esp_err_t halIOInit(void)
{
  generalConfig_t *cfg = configGetCurrent();
  
  if(cfg == NULL)
  {
    ESP_LOGE(LOG_TAG,"general Config is NULL!!!");
    while(cfg == NULL) 
    {
      vTaskDelay(1000/portTICK_PERIOD_MS);
      cfg = configGetCurrent();
    }
  }
  
  /*++++ init GPIO interrupts for 2 external & 2 internal buttons ++++*/
  gpio_config_t io_conf;
  //disable pull-down mode
  io_conf.pull_down_en = 0;
  //interrupt of rising edge
  io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
  //bit mask of the pins
  io_conf.pin_bit_mask = (1<<HAL_IO_PIN_BUTTON_EXT1) | (1<<HAL_IO_PIN_BUTTON_EXT2) | \
    (1<<HAL_IO_PIN_BUTTON_INT1) | (1<<HAL_IO_PIN_BUTTON_INT2);
  //set as input mode    
  io_conf.mode = GPIO_MODE_INPUT;
  //enable pull-up mode
  io_conf.pull_up_en = 1;
  gpio_config(&io_conf);
  
  //TODO: ret vals prüfen
  
  //install gpio isr service
  gpio_install_isr_service(0);
  
  //hook isr handler for specific gpio pin
  gpio_isr_handler_add(HAL_IO_PIN_BUTTON_EXT1, gpio_isr_handler, (void*) HAL_IO_PIN_BUTTON_EXT1);
  gpio_isr_handler_add(HAL_IO_PIN_BUTTON_EXT2, gpio_isr_handler, (void*) HAL_IO_PIN_BUTTON_EXT2);
  gpio_isr_handler_add(HAL_IO_PIN_BUTTON_INT1, gpio_isr_handler, (void*) HAL_IO_PIN_BUTTON_INT1);
  gpio_isr_handler_add(HAL_IO_PIN_BUTTON_INT2, gpio_isr_handler, (void*) HAL_IO_PIN_BUTTON_INT2);
  
  /*++++ init infrared drivers (via RMT engine) ++++*/
  halIOIRRecvQueue = xQueueCreate(8,sizeof(halIOIR_t*));
  halIOIRSendQueue = xQueueCreate(8,sizeof(halIOIR_t));
  
  //transmitter
  rmt_config_t rmtcfg;
  rmtcfg.channel = 0;
  rmtcfg.gpio_num = HAL_IO_PIN_IR_SEND;
  rmtcfg.mem_block_num = HAL_IO_IR_MEM_BLOCKS;
  rmtcfg.clk_div = RMT_CLK_DIV;
  rmtcfg.tx_config.loop_en = false;
  rmtcfg.tx_config.carrier_duty_percent = 50;
  rmtcfg.tx_config.carrier_freq_hz = 38000;
  rmtcfg.tx_config.carrier_level = 1;
  rmtcfg.tx_config.carrier_en = 1;
  rmtcfg.tx_config.idle_level = 0;
  rmtcfg.tx_config.idle_output_en = true;
  rmtcfg.rmt_mode = RMT_MODE_TX;
  rmt_config(&rmtcfg);
  rmt_driver_install(rmtcfg.channel, 0, 0);
  if(xTaskCreate(halIOIRSendTask,"irsend",TASK_HAL_IR_SEND_STACKSIZE, 
    (void*)NULL,TASK_HAL_IR_SEND_PRIORITY, NULL) == pdPASS)
  {
    ESP_LOGD(LOG_TAG,"created IR send task");
  } else {
    ESP_LOGE(LOG_TAG,"error creating IR send task");
    return ESP_FAIL;
  }
  
  //receiver
  rmt_config_t rmt_rx;
  rmt_rx.channel = 4;
  rmt_rx.gpio_num = HAL_IO_PIN_IR_RECV;
  rmt_rx.clk_div = RMT_CLK_DIV;
  rmt_rx.mem_block_num = HAL_IO_IR_MEM_BLOCKS;
  rmt_rx.rmt_mode = RMT_MODE_RX;
  rmt_rx.rx_config.filter_en = true;
  rmt_rx.rx_config.filter_ticks_thresh = 100;
  rmt_rx.rx_config.idle_threshold = cfg->irtimeout * 100 * (RMT_TICK_10_US);
  rmt_config(&rmt_rx);
  rmt_driver_install(rmt_rx.channel, 1000, 0);
  if(xTaskCreate(halIOIRRecvTask,"irrecv",TASK_HAL_IR_RECV_STACKSIZE, 
    (void*)NULL,TASK_HAL_IR_RECV_PRIORITY, NULL) == pdPASS)
  {
    ESP_LOGD(LOG_TAG,"created IR receive task");
  } else {
    ESP_LOGE(LOG_TAG,"error creating IR receive task");
    return ESP_FAIL;
  }
  
  
  
  /*++++ init RGB LEDc driver ++++*/
  
  //init RGB queue & ledc driver
  halIOLEDQueue = xQueueCreate(8,sizeof(uint32_t));
  ledc_timer_config_t led_timer = {
    .duty_resolution = LEDC_TIMER_10_BIT, // resolution of PWM duty
    .freq_hz = 5000,                      // frequency of PWM signal
    .speed_mode = LEDC_HIGH_SPEED_MODE,           // timer mode
    .timer_num = LEDC_TIMER_0            // timer index
  };
  // Set configuration of timer0 for high speed channels
  ledc_timer_config(&led_timer);
  ledc_channel_config_t led_channel[3] = {
    {
      .channel    = LEDC_CHANNEL_0,
      .duty       = 0,
      .gpio_num   = HAL_IO_PIN_LED_RED,
      .speed_mode = LEDC_HIGH_SPEED_MODE,
      .timer_sel  = LEDC_TIMER_0
    } , {
      .channel    = LEDC_CHANNEL_1,
      .duty       = 0,
      .gpio_num   = HAL_IO_PIN_LED_GREEN,
      .speed_mode = LEDC_HIGH_SPEED_MODE,
      .timer_sel  = LEDC_TIMER_0
    } , {
      .channel    = LEDC_CHANNEL_2,
      .duty       = 0,
      .gpio_num   = HAL_IO_PIN_LED_BLUE,
      .speed_mode = LEDC_HIGH_SPEED_MODE,
      .timer_sel  = LEDC_TIMER_0
    }
  };
  //apply config to LED driver channels
  for (uint8_t ch = 0; ch < 3; ch++) {
    ledc_channel_config(&led_channel[ch]);
  }
  //activate fading
  ledc_fade_func_install(0);
  //start LED update task
  if(xTaskCreate(halIOLEDTask,"ledtask",TASK_HAL_LED_STACKSIZE, 
    (void*)NULL,TASK_HAL_LED_PRIORITY, NULL) == pdPASS)
  {
    ESP_LOGD(LOG_TAG,"created LED task");
  } else {
    ESP_LOGE(LOG_TAG,"error creating task");
    return ESP_FAIL;
  }
  
  
  /*++++ INIT buzzer ++++*/
  //we will use the LEDC unit for the buzzer
  //because RMT has no lower frequency than 611Hz (according to example)
  halIOBuzzerQueue = xQueueCreate(32,sizeof(halIOBuzzer_t));
  ledc_timer_config_t buzzer_timer = {
    .duty_resolution = LEDC_TIMER_10_BIT, // resolution of PWM duty
    .freq_hz = 100,                      // frequency of PWM signal
    .speed_mode = LEDC_LOW_SPEED_MODE,           // timer mode
    .timer_num = LEDC_TIMER_0            // timer index
  };
  ledc_timer_config(&buzzer_timer);
  ledc_channel_config_t buzzer_channel = {
    .channel    = LEDC_CHANNEL_0,
    .duty       = 0,
    .gpio_num   = HAL_IO_PIN_BUZZER,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .timer_sel  = LEDC_TIMER_0
  };
  ledc_channel_config(&buzzer_channel);
  
  //start buzzer update task
  if(xTaskCreate(halIOBuzzerTask,"buzztask",TASK_HAL_BUZZER_STACKSIZE, 
    (void*)NULL,TASK_HAL_BUZZER_PRIORITY, NULL) == pdPASS)
  {
    ESP_LOGD(LOG_TAG,"created buzzer task");
  } else {
    ESP_LOGE(LOG_TAG,"error creating buzzer task");
    return ESP_FAIL;
  }
  
  
  return ESP_OK;
}