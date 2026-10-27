#include "stm32f10x.h"
#include "Delay.h"
#include "Serial.h"
#include <stdio.h>
#include <string.h>

/* =========================================================================
   【任务三：非阻塞流水灯 + 网页双向控制】
   核心思想：
   1. 使用【volatile】防止编译器优化导致的变量假死。
   2. 使用【定时器中断】在后台偷偷跑流水灯，完全不占用主循环。
   3. 主循环使用【状态机 (Mode)】处理网络请求，实现“边跑马灯边秒回网页”。
   ========================================================================= */

// ==================== 极其关键：必须加 volatile ====================
// 告诉编译器：这些变量随时会在中断里被改变，每次用都要去内存里重新读！
volatile uint8_t RxData;               
volatile char RxBuffer[256];           
volatile uint8_t RxCnt = 0;            
volatile uint8_t Mode = 0;             // 状态机核心：0为全关，1为单亮，2为流水灯
volatile uint8_t FlowStep = 0;         // 记录流水灯当前走到第几个灯了 (0~4)
uint16_t LedPins[5] = {GPIO_Pin_0, GPIO_Pin_1, GPIO_Pin_2, GPIO_Pin_3, GPIO_Pin_4}; // 引脚数组，方便循环调用

// ==================== 自动发送网页函数 ====================
// 为什么传入了 conn_id？因为浏览器每次连接，ESP8266 分配的通道号（0或1）是随机的。
// 如果写死发到0号通道，手机可能是在1号通道等，就会无限转圈。这个函数完美解决了这个问题。
void Send_Web_Page(int conn_id, char* html) {
    int len = strlen(html); // 电脑自动计算网页代码的精确字节数，比人手数靠谱一万倍
    printf("AT+CIPSEND=%d,%d\r\n", conn_id, len); // 告诉模块：要在X号通道发Y个字节
    Delay_ms(500); // 给模块时间准备，等它回复 '>' 符号
    printf("%s", html); // 正式发送网页数据
    Delay_ms(500); // 等待发送完毕
    printf("AT+CIPCLOSE=%d\r\n", conn_id); // 【关键】发完立刻挂断！不挂断浏览器就会一直转圈
    Delay_ms(200);
}

// ==================== 5个LED硬件初始化 ====================
void LED5_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // 开启GPIOA时钟
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;      // 推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // 【已修正】你的接线是长脚接引脚，所以低电平(Reset)是灭，高电平(Set)是亮
    GPIO_ResetBits(GPIOA, GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4); 
}

// ==================== 定时器初始化 ====================
void TIM2_Init(void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    
    // 定时时间计算：72MHz / 7200 = 10kHz，数 5000 个数 = 0.5秒
    TIM_TimeBaseStructure.TIM_Period = 5000 - 1;       
    TIM_TimeBaseStructure.TIM_Prescaler = 7200 - 1;    
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE); // 开启更新中断
    
    // 配置中断优先级
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    TIM_Cmd(TIM2, ENABLE); // 启动定时器
}

// ==================== 定时器2中断服务函数 ====================
// 只要时间一到（0.5秒），单片机会立刻暂停主循环，跳到这里执行。
void TIM2_IRQHandler(void) {
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update); // 清除标志位，防止死循环
        
        // 如果当前模式是 2（流水灯），就切换灯的状态
        if (Mode == 2) {
            // 【已修正】全灭（低电平），单亮（高电平）
            GPIO_ResetBits(GPIOA, GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4); 
            GPIO_SetBits(GPIOA, LedPins[FlowStep]); 
            FlowStep++;
            if (FlowStep >= 5) FlowStep = 0; // 循环播放
        }
    }
}

// ==================== 主函数 ====================
int main(void)
{
    LED5_Init();
    TIM2_Init(); 
    Serial_Init();
    Delay_ms(2000); // 等模块上电
    
    // 🚨 强制模块重启，清除一切历史残留状态！
    printf("AT+RST\r\n"); Delay_ms(5000); 
    
    // ⚠️ 务必改成你自己的热点！
    printf("AT+CWMODE=1\r\n"); Delay_ms(1000); // 设为 Station 模式
    printf("AT+CWJAP=\"zx123\",\"123456zx\"\r\n"); Delay_ms(12000); // 连网（给12秒容错）
    printf("AT+CIPMUX=1\r\n"); Delay_ms(1000); // 开启多连接
    printf("AT+CIPSERVER=1,80\r\n"); Delay_ms(1000); // 开启 80 端口服务器
    
    while(1)
    {
        // 只要收到任何一个字符
        if (Serial_GetRxFlag() == 1) 
        {
            RxData = Serial_GetRxData();
            
            // 存入缓存箱
            if(RxCnt < 250) { 
                RxBuffer[RxCnt++] = RxData; 
                RxBuffer[RxCnt] = '\0'; 
            } else {
                // 防溢出死锁
                RxCnt = 0; memset((char*)RxBuffer, 0, sizeof(RxBuffer));
            }

            // 收到换行符，说明浏览器的一整句话发完了
            if (RxData == '\n') 
            {
                int conn_id = 0; // 默认连接号
                
                // 【自动识别通道号】：如果发现数据头是 "+IPD,x"，提取那个 x
                if (strstr((char*)RxBuffer, "+IPD,") != 0) {
                    conn_id = RxBuffer[5] - '0'; 
                }

                // 状态机：解析 URL 指令
                if (strstr((char*)RxBuffer, "/ON") != 0) {
                    Mode = 1;
                    GPIO_ResetBits(GPIOA, GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4); 
                    GPIO_SetBits(GPIOA, GPIO_Pin_0); 
                    Send_Web_Page(conn_id, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>ON</h1><a href=\"/OFF\">OFF</a><a href=\"/FLOW\">FLOW</a>");
                }
                else if (strstr((char*)RxBuffer, "/OFF") != 0) {
                    Mode = 0;
                    GPIO_ResetBits(GPIOA, GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4); 
                    Send_Web_Page(conn_id, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>OFF</h1><a href=\"/ON\">ON</a><a href=\"/FLOW\">FLOW</a>");
                }
                else if (strstr((char*)RxBuffer, "/FLOW") != 0) {
                    Mode = 2;
                    FlowStep = 0;
                    Send_Web_Page(conn_id, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>FLOW</h1><a href=\"/ON\">ON</a><a href=\"/OFF\">OFF</a>");
                }
                else if (strstr((char*)RxBuffer, "GET / HTTP") != 0) {
                    Send_Web_Page(conn_id, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>Control</h1><a href=\"/ON\">ON</a> <a href=\"/OFF\">OFF</a> <a href=\"/FLOW\">FLOW</a>");
                }
                
                // 打扫战场，清空缓存箱，迎接下一次点击
                RxCnt = 0; 
                memset((char*)RxBuffer, 0, sizeof(RxBuffer));
            }
        }
    }
}