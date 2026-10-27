#include "stm32f10x.h"                  // 引入官方设备头文件

// 自定义一个简单的软件延时函数（不依赖外部库）
void My_Delay(uint32_t count) {
    while(count--) {
        // 空跑循环，用来消耗时间
    }
}

int main(void)
{
    // 1. 定义一个结构体，用来配置 GPIO
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 2. 开启 GPIOC 的时钟（相当于给 C 部门通电）
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    
    // 3. 配置 PC13 引脚
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;   // 推挽输出模式（驱动能力强）
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;         // 选中 13 号引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;  // 速度 50M
    GPIO_Init(GPIOC, &GPIO_InitStructure);             // 把配置交上去，正式生效
    
    // 4. 默认先关灯（板载 PC13 是低电平点亮，所以输出高电平是熄灭）
    GPIO_SetBits(GPIOC, GPIO_Pin_13);
    
    // 5. 进入无限死循环
    while(1)
    {
        // 【开灯】：输出低电平
        GPIO_ResetBits(GPIOC, GPIO_Pin_13);
        My_Delay(500000); // 延时一会儿
        
        // 【关灯】：输出高电平
        GPIO_SetBits(GPIOC, GPIO_Pin_13);
        My_Delay(500000); // 延时一会儿
    }
}