#include "WiFi.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
//#include "StringArray.h"

#include <PubSubClient.h>
#include <base64.h>
#include <libb64/cencode.h>


//esp32-cam针脚定义 拍照相关
constexpr int kCameraPin_PWDN   =  32;
constexpr int kCameraPin_RESET  =  -1;  // NC
constexpr int kCameraPin_XCLK   =   0;
constexpr int kCameraPin_SIOD   =  26;
constexpr int kCameraPin_SIOC   =  27;
constexpr int kCameraPin_Y9     =  35;
constexpr int kCameraPin_Y8     =  34;
constexpr int kCameraPin_Y7     =  39;
constexpr int kCameraPin_Y6     =  36;
constexpr int kCameraPin_Y5     =  21;
constexpr int kCameraPin_Y4     =  19;
constexpr int kCameraPin_Y3     =  18;
constexpr int kCameraPin_Y2     =   5;
constexpr int kCameraPin_VSYNC  =  25;
constexpr int kCameraPin_HREF   =  23;
constexpr int kCameraPin_PCLK   =  22;

// WiFi
const char *ssid = "HONOR V20"; // Enter your WiFi name
const char *password = "yubaolin";  // Enter WiFi password

// MQTT Broker
const char *mqtt_broker = "broker.emqx.io";
const char *mqtt_TopicName = "esp32/test1";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

//设置相机帧的分辨率大小
framesize_t resolution_ = FRAMESIZE_QVGA;


//定义延时时间1000=1s
#define SLEEP_DELAY 60000 //延迟60s
#define FILE_PHOTO "/photo.jpg"

// OV2640 相机模组的针脚定义
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22


// 选择板子型号
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER


//#define ESP32_CLIENT_ID = WiFi.macAddress()
//const char* esp_client_id = WiFi.macAddress()
WiFiClient mqttClient;
PubSubClient client(mqttClient);

// const int LED_BUILTIN = 4;
// boolean shotflag = false;

void setup_camera() {

     // OV2640 camera module
     camera_config_t config;
    config.pin_pwdn     = kCameraPin_PWDN;
    config.pin_reset    = kCameraPin_RESET;
    config.pin_xclk     = kCameraPin_XCLK;
    config.pin_sscb_sda = kCameraPin_SIOD;
    config.pin_sscb_scl = kCameraPin_SIOC;
    config.pin_d7       = kCameraPin_Y9;
    config.pin_d6       = kCameraPin_Y8;
    config.pin_d5       = kCameraPin_Y7;
    config.pin_d4       = kCameraPin_Y6;
    config.pin_d3       = kCameraPin_Y5;
    config.pin_d2       = kCameraPin_Y4;
    config.pin_d1       = kCameraPin_Y3;
    config.pin_d0       = kCameraPin_Y2;
    config.pin_vsync    = kCameraPin_VSYNC;
    config.pin_href     = kCameraPin_HREF;
    config.pin_pclk     = kCameraPin_PCLK;
    config.xclk_freq_hz = 20000000;
    config.ledc_timer   = LEDC_TIMER_0;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size   = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count     = 1;

  esp_err_t err = esp_camera_init(&config);
  Serial.printf("esp_camera_init: 0x%x\n", err);

  // sensor_t *s = esp_camera_sensor_get();
  // s->set_framesize(s, FRAMESIZE_QVGA);   
  }

String msg;
int timeCount = 0;
void getimg(){//拍照分段发送到mqtt

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb){
        Serial.printf("width: %d, height: %d, buf: 0x%x, len: %d\n", fb->width, fb->height, fb->buf, fb->len);
        char data[4104];
        //client.publish(mqtt_TopicName, "0");
        for (int i = 0; i < fb->len; i++){

            sprintf(data, "%02X", *((fb->buf + i)));
            msg += data;
            if (msg.length() == 4096){
                timeCount += 1;
                client.beginPublish(mqtt_TopicName, msg.length(), 0);
                client.print(msg);
                client.endPublish();
                msg = "";
            }
        }
        if (msg.length() > 0){
            client.beginPublish(mqtt_TopicName, msg.length(), 0);
            client.print(msg);
            client.endPublish();
            msg = "";
        }
        client.publish(mqtt_TopicName, "1");
        timeCount = 0;
        esp_camera_fb_return(fb);
    }
}

void setup_wifi() {
  delay(10);
  // 连接WIFI
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address : ");
  Serial.println(WiFi.localIP());
}

//MQTT重连
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32CAMClient", mqtt_username, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// 回调函数
// void callback(char *topic, byte *payload, unsigned int length)
// {
//     // Serial.print("Message arrived [");
//     // Serial.print(topic); // 打印主题信息
//     // Serial.print("] ");
//     // for (int i = 0; i < length; i++)
//     // {
//     //     Serial.print((char)payload[i]); // 打印主题内容
//     // }
//     // Serial.println();
//     if ((char)payload[0] == '1')
//     {
//         shotflag = true;
//     }
//     if ((char)payload[0] == '0')
//     {
//         shotflag = false;
//     }
// }


void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  setup_camera(); //设置相机
  setup_wifi();		//连接WIFI
  client.setServer(mqtt_broker, mqtt_port); //连接MQTT Broker
  if (client.connect("ESP32CAMClient", mqtt_username, mqtt_password)) {
      Serial.println("mqtt connected");
    } 
  // publish and subscribe
  // client.publish(mqtt_TopicName, "Hi EMQX I'm ESP32 ^^");
  // client.subscribe(mqtt_TopicName);
}

void loop() {
  getimg();

  if (!client.connected()) {
    reconnect();
  }

  delay(1000);
}
