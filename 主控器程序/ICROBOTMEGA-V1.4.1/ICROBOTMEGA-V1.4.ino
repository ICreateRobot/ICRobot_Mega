#include <Wire.h>

#include "MeMegaPi.h"
#include <FastLED.h>


#define LED   10
//灯带
#define NUM_LEDS 6             // LED灯珠数量
#define DATA_PIN A10              // Arduino输出控制信号引脚
#define LED_TYPE WS2812         // LED灯带型号
#define COLOR_ORDER GRB         // RGB灯珠中红色、绿色、蓝色LED的排列顺序
uint8_t max_bright = 100;       // LED亮度控制变量，可使用数值为 0 ～ 255， 数值越大则光带亮度越高
CRGB leds[NUM_LEDS];            // 建立灯带leds
//超声波探头灯带
#define NUM1_LEDS 6             // LED灯珠数量
#define DATA_PIN1 A13              // Arduino输出控制信号引脚
CRGB led[NUM1_LEDS];            // 建立灯带leds
//超声波
#define tirg A11
#define echo A12
float backtime;

int val = 0;
uint8_t data[8];
uint8_t Y_data[8];
uint8_t Bullet_angle = 70;    //水弹舵机
uint8_t Bullet_angledata = 90; //水弹舵机
uint8_t Machine_angledata = 90; //机械手舵机
uint8_t Machine_angle = 50;     //机械手舵机
//
MeMegaPiDCMotor motor1(PORT1A);  //左前

MeMegaPiDCMotor motor2(PORT1B);  //右前

MeMegaPiDCMotor motor3(PORT2A);  //左后

MeMegaPiDCMotor motor4(PORT2B);  //右后
//水弹舵机
int servopin = A9;    //定义舵机接口数字接口7 ,接舵机信号线，这个IO口随便定义
void servopulse(int angle)//定义一个脉冲函数,这个函数的频率是50hz的，20000us=20ms=1/50hz
{
  int pulsewidth = (angle * 11) + 500; //将角度转化为500-2480的脉宽值
  digitalWrite(servopin, HIGH);   //将舵机接口电平至高,反过来也是可以的
  delayMicroseconds(pulsewidth);  //延时脉宽值的微秒数
  digitalWrite(servopin, LOW);    //将舵机接口电平至低
  delayMicroseconds(20000 - pulsewidth);
}
//机械手舵机
int servopin1 = A8;    //定义舵机接口数字接口7 ,接舵机信号线，这个IO口随便定义
void servopulse1(int angle)//定义一个脉冲函数,这个函数的频率是50hz的，20000us=20ms=1/50hz
{
  int pulsewidth = (angle * 11) + 500; //将角度转化为500-2480的脉宽值
  digitalWrite(servopin1, HIGH);   //将舵机接口电平至高,反过来也是可以的
  delayMicroseconds(pulsewidth);  //延时脉宽值的微秒数
  digitalWrite(servopin1, LOW);    //将舵机接口电平至低
  delayMicroseconds(20000 - pulsewidth);
}
//电机
uint8_t motorSpeed = 100;
//灯带
void Light_Strip(void)
{
  // Red
  for (int i = 0; i < 9; i++) {
    leds[i] = CRGB ( 255, 0, 0);
    FastLED.show();
    delay(2);
  }
  // Green
  for (int i = 0; i < 9; i++) {
    leds[i] = CRGB ( 0, 255, 0);
    FastLED.show();
    delay(2);
  }
  //  Blue
  for (int i = 0; i < 9; i++) {
    leds[i] = CRGB ( 0, 0, 255);
    FastLED.show();
    delay(2);
  }
}
//超声波
void Ultrasonic_Wave(void)
{
  digitalWrite(tirg, LOW);
  delayMicroseconds(2);
  digitalWrite(tirg, HIGH);
  delayMicroseconds(10);
  digitalWrite(tirg, LOW);
  backtime = pulseIn(echo, HIGH) * 340.0 / 1000.0 / 2.0; //毫米
  //Serial.println(backtime);  //串口绘图器 必须用println
}
//水弹舵机 A9
void Bullet_Servo()
{
  if (data[4] == 0x23)
  {
    Bullet_angledata++;
  }
  else if (data[4] == 0x25)
  {
    Bullet_angledata = Bullet_angledata + 3;
  }
  else if (data[4] == 0x22)
  {
    Bullet_angledata--;
  }
  else if (data[4] == 0x24)
  {
    Bullet_angledata = Bullet_angledata - 3;
  }
  if (Bullet_angledata <= 10)
  {
    Bullet_angledata = 10;
  }
  if (Bullet_angledata > 140)
  {
    Bullet_angledata = 140;
  }
  Bullet_angle = 160 - Bullet_angledata ;
  servopulse(Bullet_angle);   //引用脉冲函数  20-160舵机角度
  Serial.println(1);
}
//机械手舵机 A8
void Machine_Servo()
{
  if (data[5] == 0x30)
  {
    Machine_angledata = Machine_angledata + 3;
  }
  else if (data[5] == 0x31)
  {
    Machine_angledata = Machine_angledata - 3;
  }
  if (Machine_angledata >= 85)
  {
    Machine_angledata = 85;
  }
  if (Machine_angledata <= 15)
  {
    Machine_angledata = 15;
  }
  Serial.println(551);
  Machine_angle = 90 - Machine_angledata ;
  servopulse1(Machine_angle);  //引用脉冲函数  30-80舵机角度
  data[5] = 0;
  Y_data[5] = 0;
}
//电机
void Motor_Control(void)
{
  if (data[1] == 0x10) //前进
  {
    motor1.run(-data[2]);
    motor2.run(data[2]);
    motor3.run(data[2]);
    motor4.run(-data[2]);
    Serial.println(33);
  }
  else if (data[1] == 0x11) //后退
  {
    motor1.run(data[2]);
    motor2.run(-data[2]);
    motor3.run(-data[2]);
    motor4.run(data[2]);
    Serial.println(66);
  }
  else  if (data[1] == 0x13) //左移
  {
    motor1.run(-data[2]);
    motor2.run(-data[2]);
    motor3.run(data[2]);
    motor4.run(data[2]);
  }
  else  if (data[1] == 0x12) //右移
  {
    motor1.run(data[2]);
    motor2.run(data[2]);
    motor3.run(-data[2]);
    motor4.run(-data[2]);
  }
  else if (data[1] == 0x14) //左转
  {
    motor1.run(-data[2]);
    motor2.run(data[2]);
    motor3.run(-data[2]);
    motor4.run(data[2]);
  }
  else if (data[1] == 0x15) //右转
  {
    motor1.run(data[2]);
    motor2.run(-data[2]);
    motor3.run(data[2]);
    motor4.run(-data[2]);
  }
  if ((data[1] == 0x20) || (data[1] == 0)) //停止电机
  {
    motor1.stop();
    motor2.stop();
    motor3.stop();
    motor4.stop();
    //    motor1.run(0); /* value: between -255 and 255. */
    //    motor2.run(0); /* value: between -255 and 255. */
    //    motor3.run(0);
    //    motor4.run(0);
    Serial.println(99);
  }
}
//
void setup()
{
  Serial.begin(9600);
  Serial3.begin(9600);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  pinMode(servopin, OUTPUT); //设定舵机接口为输出接口
  pinMode(servopin1, OUTPUT); //设定舵机接口为输出接口
  //灯带
  LEDS.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);  // 初始化灯带
  //超声波探头灯带
  LEDS.addLeds<LED_TYPE, DATA_PIN1, COLOR_ORDER>(led, NUM1_LEDS);  // 初始化灯带
  FastLED.setBrightness(max_bright);
  //超声波
  pinMode(tirg, OUTPUT);
  pinMode(echo, INPUT);
  //IIC
  //  Wire.begin(50); // join i2c bus (address optional for master)
}
int num = 0;
void loop()
{
  while (Serial3.available() > 0)
  {
    for (uint8_t i = 0; i < 8; i++)
    {
      Y_data[i] = (Serial3.read());
      Serial.print(Y_data[i]);
      Serial.print(",");
    }
    Serial.println("");
  }
  if ((Y_data[0] == 0xfe) && (Y_data[7] == 0xfc))
  {
    num = 0;
    for (uint8_t i = 1; i < 7; i++)
    {
      data[i] = Y_data[i];
      Serial.print(data[i]);
      Serial.print(",");
    }
    Serial.println("");
  }
  num++;
  if (num > 50)
  {
    num = 0;
    for (uint8_t i = 1; i < 6; i++)
    {
      Y_data[i] = 0;
      data[i] = 0;
    }
  }
  if (data[3] == 0x10)
  {
    digitalWrite(LED, HIGH);
    delay(10);
    data[3] = 0;
    Y_data[3] = 0;
  }
  else
  {
    digitalWrite(LED, LOW);
  }
  //超声波
  //Ultrasonic_Wave();
  //超声波探头灯带
  // fill_rainbow(led, 6, 0, 100);
  //灯带
  //Light_Strip();
  //电机控制
  Motor_Control();
  //  水弹舵机
  if ((data[4] == 0x22) || (data[4] == 0x23) || (data[4] == 0x24) || (data[4] == 0x25))
  {
    Bullet_Servo();  //引用脉冲函数  20-160舵机角度 //30--80机械手
    Y_data[4] = 0;
    data[4] = 0;
  }
  // servopulse(80);
  //  servopulse1(30);
  //  机械手舵机
  if (data[5] != 0)
  {
    // Machine_Servo();
  }
}
