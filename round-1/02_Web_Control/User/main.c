#include "stm32f10x.h"
#include "Delay.h"
#include "Serial.h"
#include <stdio.h>
#include <string.h>

uint8_t RxData;
char RxBuffer[256]; 
uint8_t RxCnt = 0;  

int main(void)
{
    // 1. 配置 PC13 引脚 (推挽输出)
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN; 
    GPIOC->CRH &= ~(0x0F << 20); 
    GPIOC->CRH |=  (0x02 << 20); 
    GPIOC->BSRR = (1 << 13); // 默认关灯

    Serial_Init();
    Delay_ms(2000); 
    
    // 2. 连 WiFi
    printf("AT+CWMODE=1\r\n"); 
    Delay_ms(1000);
    printf("AT+CWJAP=\"zx123\",\"123456zx\"\r\n"); 
    Delay_ms(10000); 
    
    // 3. 开服务器
    printf("AT+CIPMUX=1\r\n"); 
    Delay_ms(500);
    printf("AT+CIPSERVER=1,80\r\n"); 
    Delay_ms(500);
    
    while(1)
    {
        if (Serial_GetRxFlag() == 1) 
        {
            RxData = Serial_GetRxData();
            
            if(RxCnt < 250) {
                RxBuffer[RxCnt++] = RxData;
                RxBuffer[RxCnt] = '\0'; 
            }

            // 收到完整的一行指令后执行
            if (RxData == '\n') 
            {
                // ================= 1. 收到 /OFF (关灯) =================
                if (strstr(RxBuffer, "/OFF") != 0) 
                {
                    GPIOC->BSRR = (1 << 13); // 关灯
                    
                    printf("AT+CIPSEND=0,82\r\n"); 
                    Delay_ms(500); 
                    printf("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>Light OFF</h1><a href=\"/ON\">ON</a>");
                    
                    Delay_ms(500);
                    printf("AT+CIPCLOSE=0\r\n");
                    
                    RxCnt = 0;
                    memset(RxBuffer, 0, sizeof(RxBuffer));
                }
                // ================= 2. 收到 /ON (开灯) =================
                else if (strstr(RxBuffer, "/ON") != 0) 
                {
                    GPIOC->BRR = (1 << 13); // 开灯
                    
                    printf("AT+CIPSEND=0,83\r\n"); 
                    Delay_ms(500); 
                    printf("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>Light ON</h1><a href=\"/OFF\">OFF</a>");
                    
                    Delay_ms(500);
                    printf("AT+CIPCLOSE=0\r\n");
                    
                    RxCnt = 0;
                    memset(RxBuffer, 0, sizeof(RxBuffer));
                }
                // ================= 3. 收到主页请求 (GET / HTTP) =================
                else if (strstr(RxBuffer, "GET / HTTP") != 0) 
                {
                    printf("AT+CIPSEND=0,103\r\n"); 
                    Delay_ms(500); 
                    printf("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>Control</h1><a href=\"/ON\">ON</a> <a href=\"/OFF\">OFF</a>");
                    
                    Delay_ms(500);
                    printf("AT+CIPCLOSE=0\r\n");
                    
                    RxCnt = 0;
                    memset(RxBuffer, 0, sizeof(RxBuffer));
                }
                else 
                {
                    // 忽略无关请求
                    RxCnt = 0;
                    memset(RxBuffer, 0, sizeof(RxBuffer));
                }
            }
        }
    }
}