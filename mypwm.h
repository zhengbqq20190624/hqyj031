#ifndef __MYPWM_H__
#define __MYPWM_H__
#define FAN_ON _IOWR('F', 1, int)
#define FAN_OFF _IOWR('F', 0, int)
#define BUZZ_ON _IOWR('B', 1, int)
#define BUZZ_OFF _IOWR('B', 0, int)
#define MOTOR_ON _IOWR('M', 1, int)
#define MOTOR_OFF _IOWR('M', 0, int)
#define GET_CMD_SIZE(code) ((code >> 16) & 0x3FFF)
#endif//mypwm.h