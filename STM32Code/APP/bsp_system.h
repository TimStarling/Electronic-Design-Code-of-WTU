#ifndef _BSPSYSTEM__H_
#define _BSPSYSTEM__H_

#include "stdio.h"//基本库
#include "stdarg.h"//lcd相关
#include "string.h"//字符串处理
#include "main.h"
#include "usart.h"

typedef struct{
	char uart_buffer1[50];//串口接收区
}system_parameter;


void schedule_init(void);
void schedule_run(void);
void system_test();


#endif