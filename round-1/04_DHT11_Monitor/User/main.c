/* =========================================================================
   任务四：终极极简版 (板载灯控制 + 温湿度实时显示 + 自动刷新)
   ========================================================================= */

#include "stm32f10x.h"
#include "Delay.h"
#include "Serial.h"
#include "DHT11.h"     // 包含我们刚才自己写的温湿度驱动
#include <stdio.h>
#include <string.h>

// ==================== 全局变量区 ====================
// 为什么加 volatile？因为这些变量在中断（如串口接收）和主循环中都会变，防止编译器自作聪明把变量缓存进寄存器，导致主循环变成“瞎子”。
volatile uint8_t RxData;               // 临时接收串口发来的单个字符，占一个字节，带有 _t 结尾的，都是标准类型
volatile char RxBuffer[256];           // 存放手机浏览器发来的长长一串请求
volatile uint8_t RxCnt = 0;            // 记录接收了几个字符

uint8_t temp = 0, humi = 0;            // 存放温湿度的变量（0~255）
char html_buffer[512];                 // 【关键】用来存放拼接好的网页代码（512字节足够长了）

// ==================== 辅助函数：发送网页 ====================
void Send_Web_Page(int conn_id, char* html) {
    int len = strlen(html);                            // 自动算出网页代码有多少个字节
    printf("AT+CIPSEND=%d,%d\r\n", conn_id, len);      // 告诉 WiFi 模块：要在 X 号通道发 Y 个字节，赶紧准备
    Delay_ms(500);                                     // 等模块回复 '>' 符号（必须等）
    printf("%s", html);                                // 把网页内容真正发出去
    Delay_ms(500);                                     // 等数据发完
    printf("AT+CIPCLOSE=%d\r\n", conn_id);             // 【必须做】发完立马挂断连接，不然手机浏览器会一直转圈
    Delay_ms(200);                                     // 稍作停顿，准备迎接下一次请求
}

// ==================== 主函数 ====================
int main(void)
{
    // 1. GPIO 初始化（配置板载蓝灯 PC13）
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); // 给 GPIOC 部门供电
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;      // 推挽输出（能推能拉，驱动能力强）
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;            // 选中 13 号引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    GPIO_SetBits(GPIOC, GPIO_Pin_13);                     // 【细节】板载灯是低电平亮，所以这里输出高电平，默认关灯

    // 2. 模块初始化
    DHT11_Init();     // 唤醒 DHT11 传感器（引脚在 DHT11.h 里配置的 PA5）
    Serial_Init();    // 初始化串口（配置波特率、收发模式）
    Delay_ms(2000);   // 等所有硬件模块上电稳定

    // 3. 连接 WiFi 和开启服务器
    printf("AT+RST\r\n"); Delay_ms(5000);                 // 强制重启 WiFi 模块，清空历史状态
    printf("AT+CWMODE=1\r\n"); Delay_ms(1000);            // 设置为 Station 模式（去连手机热点）
    // ⚠️【重点】一定要改成你自己的手机热点名字和密码！
    printf("AT+CWJAP=\"zx123\",\"123456zx\"\r\n"); Delay_ms(12000); // 连网（给足12秒）
    printf("AT+CIPMUX=1\r\n"); Delay_ms(1000);            // 开启多连接（允许手机发多次请求）
    printf("AT+CIPSERVER=1,80\r\n"); Delay_ms(1000);      // 开启 80 端口服务器（网页默认端口）

    // ==================== 死循环 ====================
    while(1)
    {
        // 如果串口收到了手机发来的数据
        if (Serial_GetRxFlag() == 1) 
        {
            RxData = Serial_GetRxData(); // 一次读一个字符

            // 把字符装进缓存箱子（防止溢出）
            if(RxCnt < 250) { 
                RxBuffer[RxCnt++] = RxData; 
                RxBuffer[RxCnt] = '\0'; 
            } else {
                RxCnt = 0; memset((char*)RxBuffer, 0, sizeof(RxBuffer)); // 满了就倒掉重来
            }

            // 只要收到回车换行符 '\n'，说明浏览器的一整句话发完了，可以开始解析了
            if (RxData == '\n') 
            {
                int conn_id = 0; // 默认连接号
                
                // 【高级技巧】自动识别通道号：ESP8266 返回的格式是 "+IPD,0:..."，把 0 提取出来
                if (strstr((char*)RxBuffer, "+IPD,") != 0) {
                    conn_id = RxBuffer[5] - '0'; 
                }

                // ---------- 情况 1：收到 /ON 指令 (开灯) ----------
                if (strstr((char*)RxBuffer, "/ON") != 0) {
                    GPIO_ResetBits(GPIOC, GPIO_Pin_13); // 【细节】输出低电平，点亮板载蓝灯
                    // 返回一个带按钮的简单网页
                    Send_Web_Page(conn_id, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>Light ON</h1><a href=\"/OFF\">OFF</a><br><a href=\"/\">HOME</a>");
                }
                
                // ---------- 情况 2：收到 /OFF 指令 (关灯) ----------
                else if (strstr((char*)RxBuffer, "/OFF") != 0) {
                    GPIO_SetBits(GPIOC, GPIO_Pin_13);   // 【细节】输出高电平，熄灭板载蓝灯
                    Send_Web_Page(conn_id, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>Light OFF</h1><a href=\"/ON\">ON</a><br><a href=\"/\">HOME</a>");
                }
                
                // ---------- 情况 3：收到主页请求 (读取温湿度) ----------
                else if (strstr((char*)RxBuffer, "GET / HTTP") != 0) {
                    
                    // 1. 呼叫传感器，把温度装进 temp，湿度装进 humi
                    DHT11_Read_Data(&temp, &humi); 
                    
                    // 2. 【魔法时刻】拼装网页
                    // <meta http-equiv="refresh" content="3"> 意思是：让浏览器每 3 秒自动刷新一次页面！
                    // %d 是占位符，会被后面的 temp 和 humi 替换掉，变成实际数字。
                    snprintf(html_buffer, sizeof(html_buffer), 
                        "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                        "<meta http-equiv=\"refresh\" content=\"3\">"
                        "<h1>Temp: %d C</h1>"
                        "<h1>Humi: %d %%</h1>"
                        "<hr>"
                        "<a href=\"/ON\">ON</a> | <a href=\"/OFF\">OFF</a>",
                        temp, humi);
                        
                    // 3. 把拼好的长网页发出去
                    Send_Web_Page(conn_id, html_buffer);
                }
                
                // 打扫战场，清空缓存箱，准备迎接下一次点击
                RxCnt = 0; 
                memset((char*)RxBuffer, 0, sizeof(RxBuffer));
            }
        }
    }
}