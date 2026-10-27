#include "stm32f10x.h"                  // Device header
#include "Delay.h"                      // 延时函数
#include "Serial.h"                     // 串口函数

uint8_t RxData;                         // 用于接收数据的变量

int main(void)
{
    // 1. 初始化串口（波特率在 Serial.c 里写死的，115200）
    Serial_Init();
    
    // 2. 延时两秒，给 ESP8266 足够的开机准备时间
    Delay_ms(2000); 
    
    // 3. 发送指令测试模块是否活着
    Serial_SendString("AT\r\n"); 
    Delay_ms(1000); // 等它回复 OK
    
    // 4. 设置为 Station 模式（去连别人 WiFi）
    Serial_SendString("AT+CWMODE=1\r\n");
    Delay_ms(1000);
    
    // 5. 连接我的手机的热点
    Serial_SendString("AT+CWJAP=\"zx123\",\"123456zx\"\r\n"); 
    
    // 6. 进入死循环，持续监听并打印 ESP8266 的回复
    while(1)
    {
        // 检查串口是否收到了数据（ESP8266 回复的话）
        if (Serial_GetRxFlag() == 1) 
        {
            RxData = Serial_GetRxData();  // 把数据读出来
            Serial_SendByte(RxData);      // 原封不动打印到电脑的串口助手屏幕上
        }
    }
}