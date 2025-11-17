#include "bsp_system.h"
extern system_parameter sp;
uint16_t rx_cnt = 0;

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	uint16_t temp_num = Size;
	HAL_UART_Transmit_DMA(&huart1,(uint8_t*)sp.uart_buffer1,Size);
	if(huart == &huart1)
	{
		/*接收数据处理*/
		printf("%s",sp.uart_buffer1);
		memset(sp.uart_buffer1, 0, Size);
		HAL_UARTEx_ReceiveToIdle_DMA(&huart1,(uint8_t*)sp.uart_buffer1,50);
	}
}	

void system_test()
{
	/*LED测试*/
	  HAL_GPIO_WritePin(GPIOC,GPIO_PIN_14,GPIO_PIN_RESET);
	  HAL_Delay(500);
	  HAL_GPIO_WritePin(GPIOC,GPIO_PIN_14,GPIO_PIN_SET);
	  HAL_Delay(500);
		
	  /*串口测试*/
	  printf("Hellow World!");
}
