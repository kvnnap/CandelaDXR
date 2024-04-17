#ifndef ACC_SHADER
#define ACC_SHADER

#define ACC_NONE			(0U)
#define ACC_CLEAR			(1U << 0)
#define ACC_ACCUMULATE		(1U << 1)
#define ACC_EXPOSURE		(1U << 2)
#define ACC_TONEMAP			(1U << 3)
#define ACC_TONEMAP_ACES	(1U << 4)
#define ACC_LINEARTOSRGB	(1U << 5)

#define ACC_IS_SET(value, flags) ((value) & (flags))

#endif