#ifndef __THREAD_FIXED_POINT_H
#define __THREAD_FIXED_POINT_H

/* 浮点数的基本定义。*/
typedef int fixed_t;
/* 用于小数部分的 16 位。*/
#define FP_SHIFT_AMOUNT 16
/* 将一个值转换为浮点数。*/
#define FP_CONST(A) ((fixed_t)(A << FP_SHIFT_AMOUNT))
/* 添加两个浮点数值。*/
#define FP_ADD(A,B) (A + B)
/* 将浮点数值 A 和整数值 B 相加。*/
#define FP_ADD_MIX(A,B) (A + (B << FP_SHIFT_AMOUNT))
/* 减去两个浮点数值。*/
#define FP_SUB(A,B) (A - B)
/* 从浮点数值 A 减去整数值 B。*/
#define FP_SUB_MIX(A,B) (A - (B << FP_SHIFT_AMOUNT))
/* 将浮点数值 A 乘以整数值 B。*/
#define FP_MULT_MIX(A,B) (A * B)
/* 将浮点数值 A 除以整数值 B。*/
#define FP_DIV_MIX(A,B) (A / B)
/* 将两个浮点数值相乘。*/
#define FP_MULT(A,B) ((fixed_t)(((int64_t) A) * B >> FP_SHIFT_AMOUNT))
/* 将两个浮点数值相除。*/
#define FP_DIV(A,B) ((fixed_t)((((int64_t) A) << FP_SHIFT_AMOUNT) / B))
/* 获取浮点数值的整数部分。*/
#define FP_INT_PART(A) (A >> FP_SHIFT_AMOUNT)
/* 获取浮点数值的四舍五入整数部分。*/
#define FP_ROUND(A) (A >= 0 ? ((A + (1 << (FP_SHIFT_AMOUNT - 1))) >> FP_SHIFT_AMOUNT) \
        : ((A - (1 << (FP_SHIFT_AMOUNT - 1))) >> FP_SHIFT_AMOUNT))

#endif /* thread/fixed_point.h */
