/********************************************************************************
 *                            USDK(ATS350B_linux)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-8-31-上午11:59:01             1.0             build this file
 ********************************************************************************/
/*!
 * \file     test_uart_console.c
 * \brief
 * \author
 * \version  1.0
 * \date  2018-8-31-上午11:59:01
 *******************************************************************************/
#include <sdk.h>
#include <greatest_api.h>

TEST uart_print(void){
	char *str = "test";

    //type 类型
    printf("%i\n",123);                                                       //输出  123
    printf("0%o\n",123);                                                      //输出  0173
    printf("%u\n",123);                                                       //输出  123
    printf("0x%x 0x%X\n",123,123);                                            //输出  0x7b 0x7B
    printf("%c\n",65);                                                        //输出  A
    printf("%s\n",str);                                                       //输出  test
    printf("%p\n",str);                                 					  //输出  0xxxxxx
    printf("%%\n");                                                           //输出  %

    //flags 标志
    printf("%5d\n",1000);               //默认右对齐,左边补空格               //输出  _1000
    printf("%-5d\n",1000);              //左对齐,右边补空格                   //输出  1000_
    printf("%+d %+d\n",1000,-1000);     //输出正负号                          //输出  +1000 -1000
    printf("% d % d\n",1000,-1000);     //正号用空格替代，负号输出            //输出  _1000 -1000
    printf("%x %#x\n",1000,1000);       //输出0x                              //输出  3e8 0x3e8
    printf("%05d\n",1000);              //前面补0                             //输出  01000

    //width 最小宽度
    printf("%06d\n",1000);            //输出  001000
    printf("%0*d\n",6,1000);          //输出  001000

    //precision 精度
    printf("%.8s\n","abcdefghijkl");  //超过指定长度截断                      //输出  abcdefgh

    //length 类型长度
    printf("%hhd\n",'A');               //输出有符号char                      //输出  65
    printf("%hhu\n",'A'+128);           //输出无符号char                      //输出  193
    printf("%hd\n",32767);              //输出有符号短整型short int           //输出  32767
    printf("%hu\n",65535);              //输出无符号短整型unsigned short int  //输出  65535
    printf("%ld\n",(long int)0x7fffffff);         //输出有符号长整型long int            //输出  2147483647
    printf("%lu\n",(long unsigned int)0xffffffff);         //输出无符号长整型unsigned long int   //输出  4294967295
    printf("%llx,%d,%llx\n",0x8765432112345678ull,100,0x1234567887654321ull);
                                        //输出无符号长长整型unsigned long long//输出  0x8765432112345678,100,0x1234567887654321
    printf("%d,%lld\n",200,0x1234567887654321ll);
                                        //输出有符号长长整型long long int     //输出  200,0x1234567887654321

}

extern void trace_init(void);

RUN_TEST_CASE("uart_console")
{
	trace_init();
    //测试物理层函数
    //RUN_TEST(uart_console_sync_out);
    //RUN_TEST(uart_console_async_out);
    RUN_TEST(uart_print);
}
