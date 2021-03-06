#include "stm32f10x.h"
#include "delay.h"
#include "sys.h"
#include "usart.h"
#include "timer2_int.h"
#include "timer3_int.h"
#include "oled.h"
#include "bmp.h"
/* home->alarm->set->map */
#include "math.h"
#include "io.h"
#include "led.h"
#include "key.h"
#include "rtc.h"

#define TIME_NUM1_LEFT 2
#define TIME_NUM1_RIGHT 24
#define TIME_NUM2_LEFT 33
#define TIME_NUM2_RIGHT 55
#define TIME_NUM3_LEFT 73
#define TIME_NUM3_RIGHT 95
#define TIME_NUM4_LEFT 102
#define TIME_NUM4_RIGHT 124
//预定义显示中心位置
#define MID_ROW 63
#define MID_COL 31
//预定义指针长度
#define LEN_ARR_HOR 20
#define LEN_ARR_MIN 26 
#define LEN_ARR_SEC 32 

//时间显示模式，可以在设置中更改(默认0数字显示模式)
u8 Time_Mode = 0;

//时间界面冒号刷新标志位
u8 Flag_Time_Num_Dot = 0;
//一秒刷新时间标志位
u8 Flag_Time_IT_1S = 0;
//五秒刷新时间标志位
//u8 Flag_Time_IT_5S = 0;
//记录定时器中断标志位更新标志位
u8 key = 0;
//现在时间分量静态变量，用于判断何时滚屏
u8 Time_Now_Min_G;
u8 Time_Now_Min_S;
u8 Time_Now_Hor_G;
u8 Time_Now_Hor_S;

//现在时间静态变量，用于判断何时更新指针
u8 Time_Now_Sec = 0;
u8 Time_Now_Min = 0;
u8 Time_Now_Hor = 0;

//表盘角度变量0-360用u16
//竖直向上为0°顺时针递增
//u16 Time_Dish_Arrow_Angle_H = 0;
//u16 Time_Dish_Arrow_Angle_M = 0;
//u16 Time_Dish_Arrow_Angle_S = 0;

void Time_Num_Dot_Display(void)
{
    //冒号显示，通过定时器周期显示
    if(Flag_Time_Num_Dot == 1)
    {
        //冒号显示区域局部刷新
        OLED_Fill(58,14,68,54,0);
    }
    else if (Flag_Time_Num_Dot == 2)
    {
        OLED_DrawFillCircle(63,19,2);
        OLED_DrawFillCircle(63,52,2);
    }
}
void Time_Num_Display(void)
{
    OLED_ShowPicture(TIME_NUM1_LEFT,8,22,56,gImage_0[calendar.hour/10],1);
    OLED_ShowPicture(TIME_NUM2_LEFT,8,22,56,gImage_0[calendar.hour%10],1);
    OLED_ShowPicture(TIME_NUM3_LEFT,8,22,56,gImage_0[calendar.min/10],1);
    OLED_ShowPicture(TIME_NUM4_LEFT,8,22,56,gImage_0[calendar.min%10],1);
}
void Time_Dish_Display(void)
{
    //表盘上会在0度方向奇怪的多出一条一次性的线用这条给它刷掉
	OLED_DrawLine(MID_ROW,0,MID_ROW,MID_COL,0);
	//表盘
    OLED_DrawEllipse(MID_ROW,MID_COL,62,31);
    //刻度
    OLED_DrawLine( 64, 0, 64, 3,1);
    OLED_DrawLine( 64,64, 64,61,1);
    OLED_DrawLine(  0,32,  3,32,1);
    OLED_DrawLine(128,32,123,32,1);
    //中心点
    OLED_DrawFillCircle(MID_ROW,MID_COL,3);
	
    //秒针刷新 
    OLED_DrawAngleLine(MID_ROW,MID_COL,calendar.sec * 6,LEN_ARR_SEC,1);
    //秒针清除上次
	if (calendar.sec == 0)
	{
		OLED_DrawAngleLine(MID_ROW,MID_COL,59 * 6,LEN_ARR_SEC,0);
	}
	else
	{
		OLED_DrawAngleLine(MID_ROW,MID_COL,(calendar.sec - 1) * 6,LEN_ARR_SEC,0);
	}
    
	//分针刷新
    OLED_DrawAngleLine(MID_ROW,MID_COL,calendar.min * 6,LEN_ARR_MIN,1);
	//分针清除上次
	if (calendar.min == 0)
	{
		OLED_DrawAngleLine(MID_ROW,MID_COL,59 * 6,LEN_ARR_MIN,0);
	}
	else
	{
		OLED_DrawAngleLine(MID_ROW,MID_COL,(calendar.min - 1) * 6,LEN_ARR_MIN,0);
	}
    
	//时针刷新
    OLED_DrawAngleLine(MID_ROW,MID_COL,calendar.hour % 12 * 30,LEN_ARR_HOR,1);
	//时针清除上次
	if (calendar.hour % 12 == 0)
	{
		OLED_DrawAngleLine(MID_ROW,MID_COL,11 * 30,LEN_ARR_HOR,0);
	}
	else
	{
		OLED_DrawAngleLine(MID_ROW,MID_COL,(calendar.hour - 1) % 12 * 30,LEN_ARR_HOR,0);
	}
}


void Time_Num_Updata(void);
/*****************************UI切换函数声明*****************************/
void BMP_HomeToAlarm(void);



int main(void)
{
    //u8 SET[2][4] = {{"ON "},{"OFF"}};
    delay_init();	    	 	//延时函数初始化
    //设置NVIC中断分组2:2位抢占优先级，2位响应优先级
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    uart_init(115200);	 		//串口初始化为115200
    delay_ms(1000);				//上电延时，外设上电准备阶段
    TIM2_Int_Init(4999,7199);	//72000000/(7199+1)/(5000) = 2hz
    TIM3_Int_Init(499,7199);	//72000000/(7199+1)/(500) = 20hz
    RTC_Init();	  				//RTC初始化
    //IO_Init();
    LED_Init();			     	//LED端口初始化
    KEY_Init();          		//初始化与按键连接的硬件接口
    OLED_Init();
    OLED_Refresh();				//显示一组字符等之后需要用这个刷新显存
    OLED_Clear();
	//Upadta Flag
	Time_Now_Min_G = calendar.min % 10;
	Time_Now_Min_S = calendar.min / 10;
	Time_Now_Hor_G = calendar.hour % 10;
	Time_Now_Hor_S = calendar.hour / 10;
//	printf("%d-%d-%d-%d\r\n",Time_Now_Min_G,Time_Now_Min_S,Time_Now_Hor_G,Time_Now_Hor_S);
//	while(1);
	
	//自己用代码
    while(1)
    {
		//主界面
		OLED_DrawBMP(0,0,128,8,gImage_home);
		switch (key)
        {
        case 1:
			OLED_DrawBMP(0,0,128,8,gImage_set);
            break;
        case 2:
            //过渡动画
            BMP_HomeToAlarm();
            //延时一段时间加载时间信息
            delay_ms(800);
			Time_Num_Display();
            OLED_Fill(0,0,128,64,0);
            while(1)
            {
                if(Time_Mode == 0)
                {
                    OLED_Refresh();
                    //数字界面
                    Time_Num_Updata();
                    Time_Num_Display();
                    Time_Num_Dot_Display();
                }
                else
                {
                    OLED_Refresh();
                    //表盘界面
                    Time_Dish_Display();
                }
				if(key == 1)
				{
					break;
				}
            }
            break;
        case 3:
            break;
        }


    }
}

/***********
  * @BRIEF :[ 数字时间更新函数 ]
  * @NOTE  :-
			-
  * @INPUT :>
			>
  * @RETURN:void
  *
  * @DATE  :2021/01/29
  * @DESIGN:
***********/
void Time_Num_Updata(void)
{
    u8 i;
    //四位一起变
    if (Time_Now_Min_G != calendar.min % 10 &&
		Time_Now_Min_S != calendar.min / 10 &&
		Time_Now_Hor_G != calendar.hour % 10 &&
		Time_Now_Hor_S != calendar.hour / 10 )
    {
        //开始滚动,更新
        for(i = 60; i > 8; i -= 3)
        {
            //填充下区域清空上区域，注意先后
            OLED_ShowPicture(TIME_NUM4_LEFT,i,22,56,gImage_0[calendar.min %10],1);
            OLED_ShowPicture(TIME_NUM3_LEFT,i,22,56,gImage_0[calendar.min /10],1);
            OLED_ShowPicture(TIME_NUM2_LEFT,i,22,56,gImage_0[calendar.hour%10],1);
            OLED_ShowPicture(TIME_NUM1_LEFT,i,22,56,gImage_0[calendar.hour/10],1);
            OLED_Fill(TIME_NUM4_LEFT,0,TIME_NUM4_RIGHT,i - 1,0);
            OLED_Fill(TIME_NUM3_LEFT,0,TIME_NUM3_RIGHT,i - 1,0);
            OLED_Fill(TIME_NUM2_LEFT,0,TIME_NUM2_RIGHT,i - 1,0);
            OLED_Fill(TIME_NUM1_LEFT,0,TIME_NUM1_RIGHT,i - 1,0);
            OLED_Refresh();
        }
    }
    //三位一起变
    else if(Time_Now_Min_G != calendar.min % 10 &&
            Time_Now_Min_S != calendar.min / 10 &&
            Time_Now_Hor_G != calendar.hour % 10)
    {
        //开始滚动,更新
        for(i = 60; i > 8; i -= 3)
        {
            //填充下区域清空上区域，注意先后
            OLED_ShowPicture(TIME_NUM4_LEFT,i,22,56,gImage_0[calendar.min%10],1);
            OLED_ShowPicture(TIME_NUM3_LEFT,i,22,56,gImage_0[calendar.min/10],1);
            OLED_ShowPicture(TIME_NUM2_LEFT,i,22,56,gImage_0[calendar.hour%10],1);
            OLED_Fill(TIME_NUM4_LEFT,0,TIME_NUM4_RIGHT,i - 1,0);
            OLED_Fill(TIME_NUM3_LEFT,0,TIME_NUM3_RIGHT,i - 1,0);
            OLED_Fill(TIME_NUM2_LEFT,0,TIME_NUM2_RIGHT,i - 1,0);
            OLED_Refresh();
        }
    }
    //二位一起变
    else if(Time_Now_Min_G != calendar.min % 10 &&
            Time_Now_Min_S != calendar.min / 10)
    {
        //开始滚动,更新
        for(i = 60; i > 8; i -= 3)
        {
            //填充下区域清空上区域，注意先后
            OLED_ShowPicture(TIME_NUM4_LEFT,i,22,56,gImage_0[calendar.min%10],1);
            OLED_ShowPicture(TIME_NUM3_LEFT,i,22,56,gImage_0[calendar.min/10],1);
            OLED_Fill(TIME_NUM4_LEFT,0,TIME_NUM4_RIGHT,i - 1,0);
            OLED_Fill(TIME_NUM3_LEFT,0,TIME_NUM3_RIGHT,i - 1,0);
            OLED_Refresh();
        }
    }
    //一位变
    else if(Time_Now_Min_G != calendar.min % 10)
    {
        //开始滚动,更新
        for(i = 60; i > 8; i -= 3)
        {
            //填充下区域清空上区域，注意先后
            OLED_ShowPicture(TIME_NUM4_LEFT,i,22,56,gImage_0[calendar.min%10],1);
            OLED_Fill(TIME_NUM4_LEFT,0,TIME_NUM4_RIGHT,i - 1,0);
            OLED_Refresh();
        }
    }
	Time_Now_Min_G = calendar.min % 10;
	Time_Now_Min_S = calendar.min / 10;
	Time_Now_Hor_G = calendar.hour % 10;
	Time_Now_Hor_S = calendar.hour / 10;
}

/***********
  * @BRIEF :[图标UI切换函数]
  * @NOTE  :-切换动画有待优化
			-
  * @INPUT :>
			>
  * @RETURN:void
  *
  * @DATE  :2021/01/29
  * @DESIGN:
***********/
void BMP_HomeToAlarm(void)
{
    u8 i;
	OLED_Fill(0,0,128,64,0);
	OLED_Refresh();
    //开始滚动,更新
    for(i = 128; i > 0; i -= 4)
    {
        /*原图下移，新图右移，填充原图上面区域
        if(i % 2 == 0)
        {
            OLED_ShowPicture(0,64 - i / 2,128,64,gImage_home,1);
        }
        OLED_ShowPicture(i,0,128,64,gImage_alarm,1);
        OLED_Fill(0,0,i,64 - i / 2,0);
        OLED_Refresh();
        */
        //原图不动，新图右移，填充原图上面区域
        OLED_Fill(0,0,i -1,8,0);
        OLED_ShowPicture(i,0,128,64,gImage_alarm,1);
        OLED_Refresh();
    }
}




