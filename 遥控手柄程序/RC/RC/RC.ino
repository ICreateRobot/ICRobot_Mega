#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "BluetoothSerial.h"
#include "stdlib.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <stdio.h>
#include <string>
#include <Ticker.h>
#include <EEPROM.h>

#define WS2812_PIN  4  //WS2812

//电源
#define POWER_K_OUT 27//电源输出
#define POWER_K_IN  32//电源输入





//摇杆们
#define R_Y   36  //右Y摇杆 上小下大
#define R_X   39  //右X摇杆 左小右大
#define R_M   18  //右摇杆按键

#define L_Y   34  //左Y摇杆 上小下大 
#define L_X   35  //左X摇杆 左小右大
#define L_M   25  //左摇杆按键


#define R_U          23
#define R_D          22
#define L_U          14
#define L_D          12

#define MED_Threshold 300//死区

Adafruit_NeoPixel pixels(1, WS2812_PIN, NEO_GRB + NEO_KHZ800);//第一个参数led个数，第二个引脚号，第三个rgb模式，第四个800khz-ws2812

struct _KEY
{
  bool T;
  bool O;

  bool Power_L;
  bool Power_R;
  bool UP;
  bool DO;
  bool TL;
  bool TR;
  bool X;
  bool Y;

  bool A;
  bool B;

  bool L_K;
  bool R_K;
  bool L_U_K;
  bool L_D_K;
  bool R_U_K;
  bool R_D_K;
} Key;

struct stu {

  uint16_t MAX;//极大值
  uint16_t MIN;//极小值
  uint16_t MED;//中值
  uint16_t Data;//当前值
  uint8_t  Equivalence;//校准后的值
} RR_X, RR_Y, RL_X, RL_Y;

uint8_t Get_Matrix_X()
{
  uint8_t Matrix_X_Data;
  pinMode(26, OUTPUT);
  pinMode(13, OUTPUT);
  pinMode(15, OUTPUT);
  pinMode(2, OUTPUT);

  pinMode(5, INPUT_PULLUP);
  pinMode(19, INPUT_PULLUP);
  pinMode(21, INPUT_PULLUP);

  digitalWrite(26, 0);
  digitalWrite(13, 0);
  digitalWrite(15, 0);
  digitalWrite(2, 0);

  Matrix_X_Data = + (!digitalRead(5) << 6)
                  + (!digitalRead(19) << 5)
                  + (!digitalRead(21) << 4);
  return Matrix_X_Data;
}
uint8_t Get_Matrix_Y()
{
  uint8_t Matrix_Y_Data;
  pinMode(26, INPUT_PULLUP);
  pinMode(13, INPUT_PULLUP);
  pinMode(15, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);

  pinMode(5, OUTPUT);
  pinMode(19, OUTPUT);
  pinMode(21, OUTPUT);

  digitalWrite(5, 0);
  digitalWrite(19, 0);
  digitalWrite(21, 0);

  Matrix_Y_Data =   (!digitalRead(26) << 3)
                    + (!digitalRead(13) << 2)
                    + (!digitalRead(15) << 1)
                    + (!digitalRead(2) << 0);
  return Matrix_Y_Data;
}
void Get_Key()
{
  uint8_t Get_Key_Data;
  Get_Key_Data = Get_Matrix_X() | Get_Matrix_Y();
  if ((Get_Key_Data & 66) == 66)
    Key.TL = 1;
  else
    Key.TL = 0;

  if ((Get_Key_Data & 72) == 72)
    Key.TR = 1;
  else
    Key.TR = 0;

  if ((Get_Key_Data & 68) == 68)
    Key.UP = 1;
  else
    Key.UP = 0;

  if ((Get_Key_Data & 65) == 65)
    Key.DO = 1;
  else
    Key.DO = 0;

  if ((Get_Key_Data & 18) == 18)
    Key.T = 1;
  else
    Key.T = 0;

  if ((Get_Key_Data & 17) == 17)
    Key.O = 1;
  else
    Key.O = 0;

  if ((Get_Key_Data & 24) == 24)
    Key. Power_L = 1;
  else
    Key.Power_L = 0;

  if ((Get_Key_Data & 20) == 20)
    Key.Power_R = 1;
  else
    Key.Power_R = 0;

  if ((Get_Key_Data & 33) == 33)
    Key.Y = 1;
  else
    Key.Y = 0;

  if ((Get_Key_Data & 40) == 40)
    Key.B = 1;
  else
    Key.B = 0;

  if ((Get_Key_Data & 34) == 34)
    Key.A = 1;
  else
    Key.A = 0;

  if ((Get_Key_Data & 36) == 36)
    Key.X = 1;
  else
    Key.X = 0;

  Key.L_K   = !digitalRead(L_M);
  Key.R_K   = !digitalRead(R_M);
  Key.L_U_K = digitalRead(L_U);
  Key.L_D_K = digitalRead(L_D);
  Key.R_U_K = digitalRead(R_U);
  Key.R_D_K = digitalRead(R_D);
}



boolean doSacn = true;
boolean doConnect = false;
boolean connected = false;

//蓝牙开始
BLEAdvertisedDevice* pServer;
BLERemoteCharacteristic* pRemoteCharacteristic;
#define SERVICE_UUID "0000fe3c-0000-1000-8000-00805f9b34f0"
#define CHARACTERISTIC_UUID "0000fe3c-0000-1000-8000-00805f9b34f1"

// #define SERVICE_UUID "0000ffe0-0000-1000-8000-00805f9b34fb"
// #define CHARACTERISTIC_UUID "0000ffe1-0000-1000-8000-00805f9b34fb"
uint8_t BLE_OUT[6];
uint8_t BLE_OUT_Last[6];
BLEClient* pClient  = BLEDevice::createClient(); // 创建客户端
void NotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);

uint8_t BLE_Pattern = 4;//蓝牙的当前模式 0状态未知  1 打开搜索 搜索指定MAC 2 发现指定MAC的设备 等待连接  3已连接   4需要重新绑定设备 5根本不需要连接
String BLE_Mac;
unsigned long BLE_Search_Begin_Time;
uint8_t Last_BLE_Pattern;//上次蓝牙的模式
uint8_t Push_EN = false;
uint8_t Push_Buff_Count;

// 搜索到设备时回调功能
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      if (BLE_Pattern == 1) //有缓存的MAC地址
      {
        if ((String)(advertisedDevice.getAddress().toString().c_str()) == BLE_Mac) //寻找到UUID符合要求的主机
        {
          advertisedDevice.getScan()->stop(); // 停止当前扫描
          pServer = new BLEAdvertisedDevice(advertisedDevice); // 暂存设备
          doConnect = true;
          Serial.println("发现指定MAC的设备");
          BLE_Pattern = 2;//2 发现指定MAC的设备 等待连接
        }
      }
      if (BLE_Pattern == 4) //2 需要重新绑定
      {
        if (advertisedDevice.haveServiceUUID() && (String)(advertisedDevice.getServiceUUID().toString().c_str()) == SERVICE_UUID) //寻找到UUID符合要求的主机
          // if (advertisedDevice.haveServiceUUID()) //寻找到UUID符合要求的主机
        {
          advertisedDevice.getScan()->stop(); // 停止当前扫描
          pServer = new BLEAdvertisedDevice(advertisedDevice); // 暂存设备
          doConnect = true;
          Serial.print("发现想要连接的设备 其UUID为");
          Serial.println((String)(advertisedDevice.getServiceUUID().toString().c_str()));
        }
      }
    }
};

// 客户端与服务器连接与断开回调功能
class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
      pixels.setPixelColor(0, pixels.Color(0, 0, 200));
      pixels.show();//刷新
      Serial.println("连接设备成功");
      if ( BLE_Pattern != 2) //说明不是通过指定MAC搜索来的 需要存MAC
      {
        Serial.println("保存设备MAC");
        BLE_Mac = (String)(pServer->getAddress().toString().c_str());
        Serial.println(BLE_Mac);
        EEPROM.writeString(2, BLE_Mac);
        EEPROM.commit();
        Serial.println(EEPROM.readString(2));
      }
      BLE_Pattern = 3;//已连接
      connected = true;
    }
    void onDisconnect(BLEClient* pclient)
    {
      Push_EN = false;
      connected = false;
      pixels.setPixelColor(0, pixels.Color(200, 0, 0)); //注意是从0开始，第一个led对应0
      pixels.show();//刷新
      Serial.println("失去与设备的连接");
      ESP.restart();
      doSacn = true;
      //Serial.println(BLE_Pattern);
      if (BLE_Pattern == 3)
      if (BLE_Pattern != 5)
      {
        BLE_Pattern = 0;//退回到开始状态
      }
    }
};

// 收到服务推送的数据时的回调函数
void NotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  // char buf[length + 1];
  // for (size_t i = 0; i < length; i++)
  // {
  //   buf[i] = pData[i];
  // }
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
  Push_EN = true;
  if (pRemoteCharacteristic->canNotify()) { // 如果特征值启用了推送则添加推送接收处理
    pRemoteCharacteristic->registerForNotify(NotifyCallback);
  }
}
//试图连接蓝牙
void Connect_ble()
{
  if (doSacn == true)
  {
    if ((BLE_Pattern != 5)) //确定不是不需要连接
      if (BLE_Pattern != 3) //搜索之后太久没连上需要重新搜索
      {
        if ((millis() - BLE_Search_Begin_Time) > 3000)
        {
          BLE_Pattern = 0;
        }
        Last_BLE_Pattern = BLE_Pattern;
      }

    if (BLE_Pattern == 0) //蓝牙状态未知
    {
      BLE_Mac = EEPROM.readString(2);
      Serial.println("开始搜索设备");
      BLE_Pattern = 1; //1 打开搜索 搜索指定MAC
      BLEDevice::getScan()->clearResults();
      BLEDevice::getScan()->start(3, false);
      BLE_Search_Begin_Time = millis();
      Serial.println(BLE_Mac);
    }
  }
  // 如果按键按下
  if (Key.T)
  {
    if (connected != 0)//如果连着设备就先断掉
    {
      pClient->disconnect();
      delay(500);
    }
    Serial.println("开始搜索随机设备");
    BLE_Pattern = 4;//选择模式重新绑定设备
    BLEDevice::getScan()->clearResults();
    BLEDevice::getScan()->start(3, false);
    BLE_Search_Begin_Time = millis();
  }

  // 如果找到设备就尝试连接设备
  if (doConnect) {
    ConnectToServer();
    doConnect = false;
  }
}

//蓝牙结束
uint8_t power_flag = 0;
uint8_t power_key_flag = 1;


//开关机管理

void Shutdown(void)
{
  if (analogRead(POWER_K_IN) > 4000)
  {
    power_key_flag = 1;
  }
  if ((power_flag == 0) && (power_key_flag == 1)) //开机
  {
    if (analogRead(POWER_K_IN) <= 3800)
    {
      delay(10);
      if (analogRead(POWER_K_IN) <= 3800)
      {
        delay(10);
        if (analogRead(POWER_K_IN) <= 3800)
        {
          Serial.println("开机");

          digitalWrite(POWER_K_OUT, HIGH);
          delay(10);
          digitalWrite(POWER_K_OUT, HIGH);
          power_flag = 1;
          power_key_flag = 0;
          pixels.setPixelColor(0, pixels.Color(200, 0, 0));
          pixels.show();//刷新
        }
      }
    }
  }
  else if ((power_flag == 1) && (power_key_flag == 1)) //关机
  { 
    if (analogRead(POWER_K_IN) <= 2500)
    {
      delay(10);
      if (analogRead(POWER_K_IN) <= 2500)
      { 
        delay(10);
        if (analogRead(POWER_K_IN) <= 2500)
        {
          pixels.setPixelColor(0, pixels.Color(0, 0, 0));
          pixels.show();//刷新
          digitalWrite(POWER_K_OUT, LOW);
          delay(20);
          digitalWrite(POWER_K_OUT, LOW);
          power_flag = 0;
          power_key_flag = 0;
          Serial.println("关机");
          while (1);
        }
      }
    }
  }
}
//
void Calculate(struct stu *rocker) //处理校准参数
{
  if (rocker->Data > rocker->MAX)
    rocker->Data = rocker->MAX;
  if (rocker->Data < rocker->MIN)
    rocker->Data = rocker->MIN;
  uint8_t RE_Calculate = 100;
  if (rocker->Data > (rocker->MED + MED_Threshold)) //大于中值上阈值
  {
    RE_Calculate = (uint8_t)map(rocker->Data, rocker->MED + MED_Threshold, rocker->MAX, 101, 200);
  }
  if (rocker->Data < (rocker->MED - MED_Threshold)) //小于中下阈值
  {
    RE_Calculate = (uint8_t)map(rocker->Data, rocker->MIN, rocker->MED - MED_Threshold, 0, 99);
  }
  rocker->Equivalence = RE_Calculate;
}
void Read_Rocker()//读取摇杆
{
  RL_X.Data = (analogRead(L_X) + analogRead(L_X) + analogRead(L_X) + analogRead(L_X) + analogRead(L_X) + analogRead(L_X)) / 6;
  RL_Y.Data = (analogRead(L_Y) + analogRead(L_Y) + analogRead(L_Y) + analogRead(L_Y) + analogRead(L_Y) + analogRead(L_Y)) / 6;
  RR_X.Data = (analogRead(R_X) + analogRead(R_X) + analogRead(R_X) + analogRead(R_X) + analogRead(R_X) + analogRead(R_X)) / 6;
  RR_Y.Data = (analogRead(R_Y) + analogRead(R_Y) + analogRead(R_Y) + analogRead(R_Y) + analogRead(R_Y) + analogRead(R_Y)) / 6;

  Calculate(&RL_X);
  Calculate(&RL_Y);
  Calculate(&RR_X);
  Calculate(&RR_Y);
  if ((Key.Power_L + Key.Power_R) == 2)
  {
    uint16_t Trigger_time = 1000;
    BLE_Pattern = 5;
    if (connected != 0)
    {
      pClient->disconnect();
    }
    while ((Key.Power_L + Key.Power_R) == 2)
    {

      Trigger_time --;
      Get_Key();
      delay(1);
      Serial.println(Trigger_time);
      if (Trigger_time == 0)
      {
        //切换到校准模式初始化参数 取得中值
        RR_X.MAX = 0;
        RR_X.MIN = 0xffff;
        RR_X.MED = analogRead(R_X) - 0;
        RR_Y.MAX = 0;
        RR_Y.MIN = 0xffff;
        RR_Y.MED = analogRead(R_Y) - 0;
        RL_X.MAX = 0;
        RL_X.MIN = 0xffff;
        RL_X.MED = analogRead(L_X) - 0;
        RL_Y.MAX = 0;
        RL_Y.MIN = 0xffff;
        RL_Y.MED = analogRead(L_Y) - 0;
        pixels.setPixelColor(0, pixels.Color(200, 0, 200)); //注意是从0开始，第一个led对应0
        pixels.show();//刷新
        break;
      }
    }
    while (!Trigger_time)
    {
      RL_X.Data = (analogRead(L_X) + analogRead(L_X) + analogRead(L_X) + analogRead(L_X) + analogRead(L_X) + analogRead(L_X)) / 6;
      RL_Y.Data = (analogRead(L_Y) + analogRead(L_Y) + analogRead(L_Y) + analogRead(L_Y) + analogRead(L_Y) + analogRead(L_Y)) / 6;
      RR_X.Data = (analogRead(R_X) + analogRead(R_X) + analogRead(R_X) + analogRead(R_X) + analogRead(R_X) + analogRead(R_X)) / 6;
      RR_Y.Data = (analogRead(R_Y) + analogRead(R_Y) + analogRead(R_Y) + analogRead(R_Y) + analogRead(R_Y) + analogRead(R_Y)) / 6;
      //极大值采集
      if (RL_X.Data > RL_X.MAX)
      {
        RL_X.MAX = RL_X.Data;
      }
      if (RL_Y.Data > RL_Y.MAX)
      {
        RL_Y.MAX = RL_Y.Data;
      }
      if (RR_X.Data > RR_X.MAX)
      {
        RR_X.MAX = RR_X.Data;
      }
      if (RR_Y.Data > RR_Y.MAX)
      {
        RR_Y.MAX = RR_Y.Data;
      }

      //极小值采集
      if (RL_X.Data < RL_X.MIN)
      {
        RL_X.MIN = RL_X.Data;
      }
      if (RL_Y.Data < RL_Y.MIN)
      {
        RL_Y.MIN = RL_Y.Data;
      }
      if (RR_X.Data < RR_X.MIN)
      {
        RR_X.MIN = RR_X.Data;
      }
      if (RR_Y.Data < RR_Y.MIN)
      {
        RR_Y.MIN = RR_Y.Data;
      }

      if (analogRead(POWER_K_IN) < 4000)
      {
        EEPROM.writeShort(50, (int16_t)RL_X.MAX - MED_Threshold);
        EEPROM.writeShort(55, (int16_t)RL_X.MED);
        EEPROM.writeShort(60, (int16_t)RL_X.MIN + MED_Threshold);

        EEPROM.writeShort(65, (int16_t)RL_Y.MAX - MED_Threshold);
        EEPROM.writeShort(70, (int16_t)RL_Y.MED);
        EEPROM.writeShort(75, (int16_t)RL_Y.MIN + MED_Threshold);

        EEPROM.writeShort(80, (int16_t)RR_X.MAX - MED_Threshold);
        EEPROM.writeShort(85, (int16_t)RR_X.MED);
        EEPROM.writeShort(90, (int16_t)RR_X.MIN + MED_Threshold);

        EEPROM.writeShort(95, (int16_t)RR_Y.MAX - MED_Threshold);
        EEPROM.writeShort(100, (int16_t)RR_Y.MED);
        EEPROM.writeShort(105, (int16_t)RR_Y.MIN + MED_Threshold);

        EEPROM.end();
        delay(1000);
        break;
      }

    }
  }
}
void Read_Rocker_Parameter()//读摇杆的参数
{
  RL_X.MAX = (uint16_t)EEPROM.readShort(50);
  RL_X.MED = (uint16_t)EEPROM.readShort(55);
  RL_X.MIN = (uint16_t)EEPROM.readShort(60);

  RL_Y.MAX = (uint16_t)EEPROM.readShort(65);
  RL_Y.MED = (uint16_t)EEPROM.readShort(70);
  RL_Y.MIN = (uint16_t)EEPROM.readShort(75);

  RR_X.MAX = (uint16_t)EEPROM.readShort(80);
  RR_X.MED = (uint16_t)EEPROM.readShort(85);
  RR_X.MIN = (uint16_t)EEPROM.readShort(90);

  RR_Y.MAX = (uint16_t)EEPROM.readShort(95);
  RR_Y.MED = (uint16_t)EEPROM.readShort(100);
  RR_Y.MIN = (uint16_t)EEPROM.readShort(105);
}

void Push_Data()
{
  if (connected)
  {
    if (Push_EN)
      if (pRemoteCharacteristic->canWrite()) // 如果可以向特征值写数据
      {
        BLE_OUT[0] = RR_X.Equivalence;
        BLE_OUT[1] = RR_Y.Equivalence;
        BLE_OUT[2] = RL_X.Equivalence;
        BLE_OUT[3] = 200 - RL_Y.Equivalence;
        BLE_OUT[4] = (Key.A << 7) | (Key.B << 6) | (Key.L_K << 5) | (Key.R_K << 4) | (Key.L_U_K << 3) | (Key.L_D_K << 2) | (Key.R_U_K << 1) | (Key.R_D_K << 0);
        BLE_OUT[5] = (Key.Power_L << 7) | (Key.Power_R << 6) | (Key.UP << 5) | (Key.DO << 4) | (Key.TL << 3) | (Key.TR << 2) | (Key.X << 1) | (Key.Y << 0);

        if ((BLE_OUT[0] != BLE_OUT_Last[0]) || (BLE_OUT[1] != BLE_OUT_Last[1]) || (BLE_OUT[2] != BLE_OUT_Last[2]) || (BLE_OUT[3] != BLE_OUT_Last[3]) || (BLE_OUT[4] != BLE_OUT_Last[4]) || (BLE_OUT[5] != BLE_OUT_Last[5]))
        {
          pRemoteCharacteristic->writeValue(BLE_OUT, 6);

          Serial.print(BLE_OUT[0]);
          Serial.print(" ");
          Serial.print(BLE_OUT[1]);
          Serial.print(" ");
          Serial.print(BLE_OUT[2]);
          Serial.print(" ");
          Serial.print(BLE_OUT[3]);
          Serial.print(" ");
          Serial.print(BLE_OUT[4]); Serial.print(" ");
          Serial.print(BLE_OUT[5]); Serial.print(" ");
          Serial.println(Key.R_D_K);
          delay(20);//间隔较短时间发送
          BLE_OUT_Last[0] = BLE_OUT[0];
          BLE_OUT_Last[1] = BLE_OUT[1];
          BLE_OUT_Last[2] = BLE_OUT[2];
          BLE_OUT_Last[3] = BLE_OUT[3];
          BLE_OUT_Last[4] = BLE_OUT[4];
          BLE_OUT_Last[5] = BLE_OUT[5];
        }
      }
  }
}

void setup()
{
  // put your setup code here, to run once:
  pixels.begin();//初始化灯带
  pixels.clear();//清空灯带数组
//  pixels.setPixelColor(0, pixels.Color(0, 0, 0)); //注意是从0开始，第一个led对应0
//  pixels.show();//刷新
  Serial.begin(115200);
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(80);
  EEPROM.begin(200);
  Read_Rocker_Parameter();//读摇杆里的参数
  pinMode(R_U, INPUT);
  pinMode(R_D, INPUT);
  pinMode(L_U, INPUT);
  pinMode(L_D, INPUT);
  pinMode(L_M, INPUT_PULLUP);
  pinMode(R_M, INPUT_PULLUP);
  pinMode(POWER_K_IN, INPUT);
  pinMode(POWER_K_OUT, OUTPUT);
}

void loop() {
  Shutdown();
  Get_Key();
  Connect_ble();
  Read_Rocker();//读取摇杆
  Push_Data();
}
