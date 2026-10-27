#include "stm32f10x.h"
#include "Delay.h"
#include "DHT11.h"

// 微秒级延时
#define DHT11_Delay_us(xus) Delay_us(xus)

// 配置引脚为输出模式
static void DHT11_Set_Output(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = DHT11_GPIO_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStructure);
}

// 配置引脚为输入模式（上拉输入，保证空闲时为高电平）
static void DHT11_Set_Input(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; 
    GPIO_InitStructure.GPIO_Pin = DHT11_GPIO_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStructure);
}

// 初始化
void DHT11_Init(void) {
    RCC_APB2PeriphClockCmd(DHT11_GPIO_CLK, ENABLE);
    DHT11_Set_Output();
    GPIO_SetBits(DHT11_GPIO_PORT, DHT11_GPIO_PIN); // 默认拉高
}

// 读取数据
uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi) {
    uint8_t i, j;
    uint8_t buf[5];
    
    // 1. 主机发送起始信号
    DHT11_Set_Output();
    GPIO_ResetBits(DHT11_GPIO_PORT, DHT11_GPIO_PIN); // 拉低
    Delay_ms(20);                                    // 至少拉低 18ms
    GPIO_SetBits(DHT11_GPIO_PORT, DHT11_GPIO_PIN);   // 拉高
    DHT11_Delay_us(30);                              // 等待 20-40us

    // 2. 检查 DHT11 响应
    DHT11_Set_Input(); // 变成输入模式准备读取
    if (GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN) == 0) {
        while (GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN) == 0); // 等待 DHT11 拉高
        while (GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN) == 1); // 等待 DHT11 拉低
    } else {
        return 1; // 响应失败
    }

    // 3. 读取 40 位数据 (5个字节)
    for (i = 0; i < 5; i++) {
        buf[i] = 0;
        for (j = 0; j < 8; j++) {
            while (GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN) == 0); // 等待低电平过去
            DHT11_Delay_us(40); // 延时 40us 判断高电平持续时间 (数据0高电平26-28us，数据1高电平70us)
            
            if (GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN) == 1) {
                buf[i] |= (1 << (7 - j)); // 如果还是高电平，说明是数据 1
                while (GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN) == 1); // 等待高电平结束
            }
        }
    }

    // 4. 校验和验证：前四个字节相加必须等于第五个字节
    if (buf[0] + buf[1] + buf[2] + buf[3] == buf[4]) {
        *humi = buf[0]; // 湿度整数部分
        *temp = buf[2]; // 温度整数部分
        return 0; // 成功
    }
    return 2; // 校验失败
}