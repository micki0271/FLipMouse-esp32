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
 * 
 * Copyright 2019 Benjamin Aigner <aignerb@technikum-wien.at,
 * beni@asterics-foundation.org>
 */
/**
 * @file 
 * @brief CONTINOUS TASK - MQTT handling
 * 
 * This module is used for connecting the FLipMouse/FABI to
 * a MQTT broker.
 * 
 * @warning Please note, only one WiFi mode is possible: either
 * this device operates in AP mode and serves the webGUI (see task_webgui.h)
 * OR it operates in station mode, connects to a given Wifi and
 * transmits data via MQTT (if enabled)
 * @note Initialise this module only if a MQTT command is used in the
 * configuration.
 * @see task_webgui.h
 * 
 * */

#include "task_smarthome.h"

/** @brief Logging tag for MQTT */
#define LOG_TAG_MQTT "MQTT"
/** @brief Logging tag for REST */
#define LOG_TAG_REST "REST"
/** @brief Logging tag for WiFi */
#define LOG_TAG_WIFI "WIFI"

/** @brief Log level for the MQTT client */
#define LOG_LEVEL_MQTT ESP_LOG_DEBUG

/** @brief Log level for WiFi */
#define LOG_LEVEL_WIFI ESP_LOG_DEBUG

/** @brief Log level for REST calls */
#define LOG_LEVEL_REST ESP_LOG_DEBUG

/** @brief MQTT client handle, used for publishing */
esp_mqtt_client_handle_t mqtt_client;

/** @brief Wifi config
 * @note if this is NOT global -> wifi is not working*/
wifi_config_t wifi_config;

/** @brief Flags for signalling wifi/mqtt/... status
 * @see SH_MQTT_ACTIVE
 * @see SH_WIFI_ACTIVE
 * */
EventGroupHandle_t smarthomestatus;

#define SH_MQTT_ACTIVE (1<<0)
#define SH_MQTT_INITIALIZED (1<<1)
#define SH_WIFI_ACTIVE (1<<2)
#define SH_WIFI_INITIALIZED (1<<3)

/** @brief Import from internal wifi code for decoding error messages */
extern const char* wifi_get_reason(int err);

/** @brief Default event handler for Wifi
 * 
 * This is basically the protocols/mqtt/tcp example's handler code.
 */
static esp_err_t wifi_sh_event_handler(void *ctx, system_event_t *event)
{
  system_event_sta_disconnected_t *disconnected ;
  
  switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
      esp_wifi_connect();
      ESP_LOGI(LOG_TAG,"STA_START, now connecting");
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      ESP_LOGI(LOG_TAG,"GOT_IP, now active");
      xEventGroupSetBits(smarthomestatus, SH_WIFI_ACTIVE);
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      esp_wifi_connect();
      disconnected = &event->event_info.disconnected;
      ESP_LOGD(LOG_TAG, "SYSTEM_EVENT_STA_DISCONNECTED, ssid:%s, ssid_len:%d, bssid:" MACSTR ", reason:%d,%s", \
                   disconnected->ssid, disconnected->ssid_len, MAC2STR(disconnected->bssid), disconnected->reason, wifi_get_reason(disconnected->reason));
      ESP_LOGI(LOG_TAG,"STA_DISCONNECT, now connecting");
      xEventGroupClearBits(smarthomestatus, SH_WIFI_ACTIVE);
      break;
    default:
      break;
  }
  return ESP_OK;
}


/** @brief Default event_handler for mqtt
 * 
 * This is basically the protocols/mqtt/tcp example's handler code.
 * Used to signal changing MQTT status/information.
 */
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    int msg_id = 0;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(LOG_TAG, "MQTT_EVENT_CONNECTED");
            xEventGroupSetBits(smarthomestatus, SH_MQTT_ACTIVE);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(LOG_TAG, "MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits(smarthomestatus, SH_MQTT_ACTIVE);
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(LOG_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            ESP_LOGI(LOG_TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(LOG_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(LOG_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(LOG_TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(LOG_TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(LOG_TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}


/** @brief Init the MQTT task and the wifi
 * 
 * This init function initializes the wifi in station mode and the MQTT task.
 * 
 * @see NVS_STATIONNAME
 * @see NVS_STATIONPW
 * @see NVS_MQTT_BROKER
 * @note Please activate only if necessary by any configuration.
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t taskMQTTInit(void)
{
  //nothing to start...
  if(mqttactive != 0) return ESP_OK;
  
  esp_log_level_set(LOG_TAG, LOG_LEVEL_MQTT);
  
  char wifipw[64];
  char wifiname[32];
  /** @brief MQTT broker host name/ip */
  char mqttbroker[101];
  esp_err_t ret;

  //get wifi connection infos
  ret = halStorageNVSLoadString(NVS_STATIONNAME,wifiname);
  if(ret != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error reading wifi name, cannot connect: %d",ret);
    return ESP_FAIL;
  }
  
  ret = halStorageNVSLoadString(NVS_STATIONPW,wifipw);
  if(ret != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error reading wifi password, cannot connect: %d",ret);
    return ESP_FAIL;
  }
  
  //get mqtt broker URL
  ret = halStorageNVSLoadString(NVS_MQTT_BROKER,mqttbroker);
  if(ret != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error reading MQTT broker, cannot connect: %d",ret);
    return ESP_FAIL;
  }

  //init Wifi in station mode
  tcpip_adapter_init();
  //if event loop init fails, we might have an already initialized event loop
	//in this case, just add the handler of this file.
  if(esp_event_loop_init(wifi_sh_event_handler, NULL) != ESP_OK)
  {
		if(esp_event_loop_set_cb(wifi_sh_event_handler,NULL) != ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"Error initialising event loop");
      return ESP_FAIL;
    }
	}
  
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  if(esp_wifi_init(&cfg) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error init wifi, cannot connect");
    return ESP_FAIL;
  }
    
  if(esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error setting wifi storage, cannot connect");
    return ESP_FAIL;
  }
  //wifi_config_t wifi_config;
  strncpy((char*)wifi_config.sta.ssid,wifiname,31);
  strncpy((char*)wifi_config.sta.password,wifipw,63);
      /*wifi_config_t wifi_config = {
        .sta = {
            .ssid = "aat-technikum2.4GHz",
            .password = "2AATractive$$"
        },
    };*/
  
  if(esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error setting wifi mode, cannot connect");
    return ESP_FAIL;
  }
  if(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error setting wifi config, cannot connect");
    return ESP_FAIL;
  }
  ESP_LOGI(LOG_TAG, "start the WIFI SSID:[%s]/[%s]", wifi_config.sta.ssid,wifi_config.sta.password);
  if(esp_wifi_start() != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error starting wifi, cannot connect");
    return ESP_FAIL;
  }
  
  //init mqtt
  esp_mqtt_client_config_t mqtt_cfg = {
    .uri = mqttbroker,
    .event_handle = mqtt_event_handler,
  };
  mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  
  //now we are initialized, do not do this again...
  mqttactive = 1;
  return ESP_OK;
}

/** @brief Init Wifi
 * 
 * This init function initializes the wifi in station mode.
 * 
 * @see NVS_STATIONNAME
 * @see NVS_STATIONPW
 * @note Please activate only if necessary by any configuration.
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t taskWifiInit(void)
{
  //check if already initialized, if yes we are finished here, if no:
  //set bit immediately and continue
  if(xEventGroupGetBits(smarthomestatus) & SH_WIFI_INITIALIZED) return ESP_OK;
  xEventGroupSetBits(smarthomestatus, SH_WIFI_INITIALIZED);
  
  esp_log_level_set(LOG_TAG, LOG_LEVEL_WIFI);
  
  char wifipw[64];
  char wifiname[32];
  esp_err_t ret;

  //get wifi connection infos
  ret = halStorageNVSLoadString(NVS_STATIONNAME,wifiname);
  if(ret != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error reading wifi name, cannot connect: %d",ret);
    return ESP_FAIL;
  }
  
  ret = halStorageNVSLoadString(NVS_STATIONPW,wifipw);
  if(ret != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error reading wifi password, cannot connect: %d",ret);
    return ESP_FAIL;
  }

  //init Wifi in station mode
  tcpip_adapter_init();
  //if event loop init fails, we might have an already initialized event loop
	//in this case, just add the handler of this file.
  if(esp_event_loop_init(wifi_sh_event_handler, NULL) != ESP_OK)
  {
		if(esp_event_loop_set_cb(wifi_sh_event_handler,NULL) != ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"Error initialising event loop");
      return ESP_FAIL;
    }
	}
  
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  if(esp_wifi_init(&cfg) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error init wifi, cannot connect");
    return ESP_FAIL;
  }
    
  if(esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error setting wifi storage, cannot connect");
    return ESP_FAIL;
  }
  //wifi_config_t wifi_config;
  strncpy((char*)wifi_config.sta.ssid,wifiname,31);
  strncpy((char*)wifi_config.sta.password,wifipw,63);

  if(esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error setting wifi mode, cannot connect");
    return ESP_FAIL;
  }
  if(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error setting wifi config, cannot connect");
    return ESP_FAIL;
  }
  ESP_LOGI(LOG_TAG, "start the WIFI SSID:[%s]/[%s]", wifi_config.sta.ssid,wifi_config.sta.password);
  if(esp_wifi_start() != ESP_OK)
  {
    ESP_LOGE(LOG_TAG,"Error starting wifi, cannot connect");
    return ESP_FAIL;
  }
  
  //init mqtt
  esp_mqtt_client_config_t mqtt_cfg = {
    .uri = mqttbroker,
    .event_handle = mqtt_event_handler,
  };
  mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  
  //now we are initialized, do not do this again...
  mqttactive = 1;
  return ESP_OK;
}

/** @brief Deinit the MQTT task and the wifi
 * 
 * This function deactives MQTT/WiFi in station mode. It is necessary
 * if the webGUI should be switched on.
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t taskMQTTDeInit(void)
{
  //nothing to stop...
  if(mqttactive == 0) return ESP_OK;
  
  esp_mqtt_client_stop(mqtt_client);
  esp_mqtt_client_destroy(mqtt_client);
  
  esp_wifi_disconnect();
  esp_wifi_stop();
  esp_wifi_deinit();
  
  mqttactive = 0;
  
  //TODO: de-init wifi and stop MQTT task
  return ESP_OK;
}

/** @brief Publish data via MQTT
 * 
 * This function publishes the given data.
 * The parameter topic_payload will be split into the topic name
 * and the corresponding payload to publish.
 * The splitting is done by the currently set MQTT delimiter character,
 * which can be modified with "AT MS". MQTT_DELIMITER will be used
 * if no other character is set.
 * 
 * @see MQTT_DELIMITER
 * @return ESP_OK on success, ESP_FAIL otherwise
 * */
esp_err_t taskMQTTPublish(char* topic_payload)
{
  
      //if we got our IP, connect mqtt...
      esp_mqtt_client_start(mqtt_client);
  
  ///TODO: auf MQTT flag prüfen, starten/init und dann auf flag warten
  if(mqttactive == 0)
  {
    if(taskMQTTInit() != ESP_OK)
    {
      ESP_LOGE(LOG_TAG,"Error init MQTT");
      return ESP_FAIL;
    } else {
      ESP_LOGI(LOG_TAG,"Publish without initialised MQTT, init now");
    }
  }
  
  char delim[3] = ":";
  esp_err_t ret = halStorageNVSLoadString(NVS_MQTT_DELIM,delim);
  if(ret != ESP_OK)
  {
    ESP_LOGI(LOG_TAG,"Using default delimiter");
    delim[0] = ':';
    delim[1] = '\0';
  }
  
  
  //split into topic & payload
  char* sep = strpbrk(topic_payload,&delim[0]);
  if(sep == NULL)
  {
    ESP_LOGE(LOG_TAG,"Wrong delimiter, cannot send MQTT message");
    return ESP_FAIL;
  }
  //save topic data
  char* topic = malloc(sep-topic_payload+1);
  strncpy(topic,topic_payload,sep-topic_payload+1);
  topic[sep-topic_payload] = '\0';
  
  char* payload = malloc(strnlen(sep+1,200)+1);
  strncpy(payload,sep+1,strnlen(sep+1,200)+1);
  payload[strnlen(sep+1,200)] = '\0';
  
  ESP_LOGI(LOG_TAG,"Publish: %s @ %s",payload,topic);
  
  //publish message
  esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 0, 0);
  
  //free allocated strings
  if(topic != NULL) free(topic);
  if(payload != NULL) free(payload);
  
  return ESP_OK;
}
