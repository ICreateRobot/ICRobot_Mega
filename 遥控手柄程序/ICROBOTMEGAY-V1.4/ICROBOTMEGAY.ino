#include <FastLED.h>

#include <Arduino.h>
#include <SPI.h>

#include <BLEDevice.h>
#include "BluetoothSerial.h"
#include <Keypad.h>
//#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
//#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
//#endif
//
//BluetoothSerial SerialBT;

#define SERVICE_UUID "0000ffe0-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID "0000ffe1-0000-1000-8000-00805f9b34fb"

#define ANALOG_PIN_0   36  //右Y摇杆 上小下大
#define ANALOG_PIN_1   39  //右X摇杆 左小右大
#define ANALOG_PIN_2   34  //左Y摇杆 上小下大 
#define ANALOG_PIN_3   35  //左X摇杆 左小右大
#define ANALOG_PIN_4   32  //开关机电压采集
#define R_U            23
#define R_D            22
#define R_M            18
#define L_U            14
#define L_D            12
#define L_M            25
#define powerout       27
//WS2812灯
#define NUM_LEDS 1             // LED灯珠数量
#define DATA_PIN 4              // Arduino输出控制信号引脚
#define LED_TYPE WS2812         // LED灯带型号
#define COLOR_ORDER GRB         // RGB灯珠中红色、绿色、蓝色LED的排列顺序
uint8_t max_bright = 100;       // LED亮度控制变量，可使用数值为 0 ～ 255， 数值越大则光带亮度越高
CRGB leds[NUM_LEDS];            // 建立灯带leds
//
boolean doSacn = true;
boolean doConnect = false;
boolean connected = false;

const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1', '5', '9'},
  {'2', '6', '0'},
  {'3', '7', '#'},
  {'4', '8', '*'}
};
byte rowPins[ROWS] = {26, 13, 15, 2};
byte colPins[COLS] = {5, 19, 21};
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );
BLEAdvertisedDevice* pServer;
BLERemoteCharacteristic* pRemoteCharacteristic;

// 搜索到设备时回调功能
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      if (advertisedDevice.haveServiceUUID() && (String)(advertisedDevice.getServiceUUID().toString().c_str()) == SERVICE_UUID) //寻找到UUID符合要求的主机
      {
        advertisedDevice.getScan()->stop(); // 停止当前扫描
        pServer = new BLEAdvertisedDevice(advertisedDevice); // 暂存设备
        doConnect = true;
        Serial.println("发现想要连接的设备");
      }
    }
};

// 客户端与服务器连接与断开回调功能
class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {}
    void onDisconnect(BLEClient* pclient) {
      connected = false; 
      leds[0] = CRGB ( 255, 0, 0);
      FastLED.show();
      ESP.restart();  //重启复位esp32
      leds[0] = CRGB ( 255, 0, 0);
      FastLED.show();
      Serial.println("失去与设备的连接");
    }
};

// 收到服务推送的数据时的回调函数
void NotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  char buf[length + 1];
  for (size_t i = 0; i < length; i++) {
    buf[i] = pData[i];
  }
  buf[length] = 0;
  Serial.printf("该消息长度为: %d; 内容为: %s\r\n", length, buf);
}

// 用来连接设备获取其中的服务与特征
bool ConnectToServer(void) {
  BLEClient* pClient  = BLEDevice::createClient(); // 创建客户端

  pClient->setClientCallbacks(new MyClientCallback()); // 添加客户端与服务器连接与断开回调功能
  if (!pClient->connect(pServer)) { // 尝试连接设备
    return false;
  }
  Serial.println("连接设备成功");

  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID); // 尝试获取设备中的服务
  if (pRemoteService == nullptr) {
    Serial.println("获取服务失败");
    pClient->disconnect();
    return false;
  }
  Serial.println("获取服务成功");

  pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID); // 尝试获取服务中的特征
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("获取特性失败");
    pClient->disconnect();
    return false;
  }
  Serial.println("获取特征成功");

  if (pRemoteCharacteristic->canRead()) { // 如果特征值可以读取则读取数据
    Serial.printf("该特征值可以读取并且当前值为: %s\r\n", pRemoteCharacteristic->readValue().c_str());
  }
  if (pRemoteCharacteristic->canNotify()) { // 如果特征值启用了推送则添加推送接收处理
    pRemoteCharacteristic->registerForNotify(NotifyCallback);
  }
}
int SELECT_LEVEL = 0;
int BLOCKS_DATA[8] = {0xfe, 0, 0, 0, 0, 0, 0, 0xfc};
int analog_value0 = 0;
int analog_value1 = 0;
int analog_value2 = 0;
int analog_value3 = 0;
int analog_value4 = 0;

char key;
uint8_t stop_flag = 0;
uint8_t power_flag = 0;
uint8_t power_key_flag = 1;

void Shutdown(void)
{
  analog_value4 = analogRead(ANALOG_PIN_4);
  if (analog_value4 > 4000)
  {
    power_key_flag = 1;
  }
  if ((power_flag == 0) && (power_key_flag == 1)) //开机
  {
    if (analog_value4 <= 3800)
    {
      delay(10);
      if (analog_value4 <= 3800)
      {
        delay(10);
        if (analog_value4 <= 3800)
        {
          digitalWrite(powerout, HIGH);
          delay(10);
          digitalWrite(powerout, HIGH);
          power_flag = 1;
          power_key_flag = 0;
          leds[0] = CRGB::Red;          // 设置灯带中第一个灯珠颜色为红色，leds[0]为第一个灯珠，leds[1]为第二个灯珠
          FastLED.show();               // 更新LED色彩
        }
      }
    }
  }
  else if ((power_flag == 1) && (power_key_flag == 1)) //关机
  { 
    if (analog_value4 <= 2500)
    {
      delay(200);
      if (analog_value4 <= 2500)
      {
        delay(300);
        if (analog_value4 <= 2500)
        { 
          delay(100);
          leds[0] = CRGB ( 0, 0, 0);//leds[0] = CRGB::Red;          // 设置灯带中第一个灯珠颜色为红色，leds[0]为第一个灯珠，leds[1]为第二个灯珠
          FastLED.show();               // 更新LED色彩
          digitalWrite(powerout, LOW);
          delay(20);
          digitalWrite(powerout, LOW);
          power_flag = 0;
          power_key_flag = 0;
          while (1);
        }
      }
    }
  }
}
uint8_t KEY_DOWN = 0;
void KEY_Control(void)//按键控制
{
  KEY_DOWN = KEY_Scan(0);
  Serial.println(KEY_DOWN);
}
uint8_t KEY_Scan(uint8_t mode)
{
  static uint8_t key_up = 1;   //按键松开标志
  uint8_t KEY0, KEY1, KEY2, KEY3;
  if (mode == 1)key_up = 1; //支持连按
  KEY0 = digitalRead(R_U);
  KEY1 = digitalRead(R_D);
  KEY2 = digitalRead(L_U);
  KEY3 = digitalRead(L_D);

  if (key_up && (KEY0 == 1 || KEY1 == 1 || KEY2 == 0 || KEY3 == 0))
  {
    key_up = 0;
    if (KEY0 == 1)       return 1;
    else if (KEY1 == 1)  return 2;
    else if (KEY2 == 0)  return 3;
    else if (KEY3 == 0) return 4;
  }
  else if (KEY0 == 0 && KEY1 == 0 && KEY2 == 1 && KEY3 == 1)key_up = 1;
  return 0;   //无按键按下
}
uint8_t Light_flag = 0;

void  Matrices_KEY_Control(void)  //矩阵按键控制
{
  
}
uint8_t running_flag = 0;
uint8_t Angle = 100;
void ADC_Control(void)
{
  analog_value0 = analogRead(ANALOG_PIN_0);
  delay(10);
  analog_value1 = analogRead(ANALOG_PIN_1);
  delay(10);
  analog_value2 = analogRead(ANALOG_PIN_2);
  delay(10);
  analog_value3 = analogRead(ANALOG_PIN_3);
   delay(10);
  //摇杆运动标志
Serial.println(analog_value3);
  //水弹舵机
  if (analog_value1 < 1200)
  {
    BLOCKS_DATA[4] = 0x22;//
  }
  else if (analog_value1 < 700)
  {
    BLOCKS_DATA[4] = 0x24;//
  }
  else if (analog_value1 > 2600)
  {
    BLOCKS_DATA[4] = 0x23;//
  }
  else if (analog_value1 > 3000)
  {
    BLOCKS_DATA[4] = 0x25;//
  }
  //旋转
  if (analog_value0 < 1000)//右摇杆 左转
  {
    BLOCKS_DATA[1] = 0x14;
    BLOCKS_DATA[2] = (1700 - analog_value0) / 7;
    running_flag = 1;
//    Serial.println("开始搜索设备");
//    delay(1000);
  }
  else if (analog_value0 > 2200)//右摇杆 右转
  {
    BLOCKS_DATA[1] = 0x15;
    BLOCKS_DATA[2] = ( analog_value0 - 1700) / 7;
    running_flag = 1;
//    Serial.println("开始搜索设备");
//    delay(1000);
  }
  ////左摇杆 上下左右
  if (analog_value2 < 1100)//摇上 最小240
  {
    BLOCKS_DATA[1] = 0x10;//前进
    BLOCKS_DATA[2] = (1700 - analog_value2) / 7;
    running_flag = 1;
//    Serial.println("开始搜索设备");
//    delay(1000);
  }
  else if (analog_value2 > 2200)//摇下 最大3300
  {
    BLOCKS_DATA[1] = 0x11;//后退
    BLOCKS_DATA[2] = (analog_value2 - 1700) / 7;
    running_flag = 1;
//    Serial.println("开始搜索设备");
//    delay(1000);
  }
  else if (analog_value3 < 1500)//摇左 最小560
  {
    BLOCKS_DATA[1] = 0x13;//左移
    BLOCKS_DATA[2] = (2000 - analog_value3) / 7;
    running_flag = 1;
  }
  else if (analog_value3 > 2100)//摇右 最大3350
  {
    BLOCKS_DATA[1] = 0x12;//右移
    BLOCKS_DATA[2] = (analog_value3 - 2000) / 7;
    running_flag = 1;
  }
  if (((analog_value0 > 1100) && (analog_value0 < 2200) &&(analog_value2 > 1100) && (analog_value2 < 2200) && (analog_value3 > 1500) && (analog_value3 < 2100)) && running_flag == 1)
  {
    BLOCKS_DATA[1] = 0x20;//
    running_flag = 0;
  }
}
void connect_ble(void)
{
  // 如果需要扫描则进行扫描
  if (connected == 0)
  {
    //搜索连接蓝牙
    if (key == '#') {
      Serial.println("开始搜索设备");
      BLEDevice::getScan()->clearResults();
      BLEDevice::getScan()->start(0); // 持续搜索设备././
    }
  }
  // 如果找到设备就尝试连接设备
  if (doConnect) {
    if (ConnectToServer()) {
      connected = true;
      leds[0] = CRGB::Blue;          // 设置灯带中第一个灯珠颜色为红色，leds[0]为第一个灯珠，leds[1]为第二个灯珠
      FastLED.show();               // 更新LED色彩
    }
    else {
      doSacn = true;
    }
    doConnect = false;
  }
}
void setup() {
  Serial.begin(9600);
  //  SerialBT.begin("ESP32test"); //Bluetooth device name
  Serial.println();
  pinMode(ANALOG_PIN_0, INPUT);
  pinMode(ANALOG_PIN_1, INPUT);
  pinMode(ANALOG_PIN_2, INPUT);
  pinMode(ANALOG_PIN_3, INPUT);
  pinMode(ANALOG_PIN_4, INPUT);
  pinMode(R_U, INPUT);
  pinMode(R_D, INPUT);
  pinMode(R_M, INPUT_PULLUP);
  pinMode(L_U, INPUT);
  pinMode(L_D, INPUT);
  pinMode(L_M, INPUT_PULLUP);
  pinMode(powerout, OUTPUT);
  //灯带
  LEDS.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);  // 初始化灯带
  FastLED.setBrightness(max_bright);
  leds[0] = CRGB ( 255, 0, 0);
  FastLED.show();               // 更新LED色彩
  delay(100);
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(80);
}
void loop() {
  Shutdown();
  key = keypad.getKey();
  if (key != NO_KEY) {
    //Serial.println(key);
  }
  else
  {
    key = 0;
  }
  //搜索连接蓝牙
  connect_ble();
  // ADC_Control();
  // KEY_Control();
  // Matrices_KEY_Control();
  // ADC_Control();
  if (connected)// 如果已经连接就可以向设备发送数据
  {
    // KEY_Control();
    if (digitalRead(R_D) == 1)
    {
      BLOCKS_DATA[5] = 0x30;//开  舵机
    }
    else if (digitalRead(L_D) == 1)
    {
      BLOCKS_DATA[5] = 0x31;//关 舵机
    }
    if ((digitalRead(L_U) == 1) || (digitalRead(R_U) == 1))
    {
      BLOCKS_DATA[3] = 0x10;//打水弹
    }
    else
    {
      BLOCKS_DATA[3] = 0;//停止打水弹
    }
    Matrices_KEY_Control();
    ADC_Control();
    if ((BLOCKS_DATA[0] == 0xfe) && (BLOCKS_DATA[7] == 0xfc)) // 如果可以向特征值写数据
    {
      if ((BLOCKS_DATA[1] != 0) || (BLOCKS_DATA[2] != 0) || (BLOCKS_DATA[3] != 0) || (BLOCKS_DATA[4] != 0) || (BLOCKS_DATA[5] != 0) || (BLOCKS_DATA[6] != 0))
      {
        Serial.println(BLOCKS_DATA[1]);
        stop_flag = 1;
        for (uint8_t i = 0; i < 8; i++)
        {
          pRemoteCharacteristic->writeValue(BLOCKS_DATA[i]);
        }
        delay(30);
        BLOCKS_DATA[0] = 0xfe;
        BLOCKS_DATA[7] == 0xfc;
        for (uint8_t i = 1; i < 7; i++)
        {
          BLOCKS_DATA[i] = 0;
        }
      }
    }
  }
}
