#ifndef MV4D_PROJECTOR_H
#define MV4D_PROJECTOR_H

#define MVIO                                          0x0A
#define MVPROJECTOR_IOCTL_COMMAND(mode,cmd)           mode(MVIO, cmd, char*)
#define MVPROJECTOR_IOCTL_GET_COMMAND(cmd)            MVPROJECTOR_IOCTL_COMMAND(_IOR,cmd)
#define MVPROJECTOR_IOCTL_SET_COMMAND(cmd)            MVPROJECTOR_IOCTL_COMMAND(_IOW,cmd)

#include <stddef.h>

typedef enum {
    GPIO_PROJECTOR_ENABLE   = 0x0085,
    GPIO_POWER_ENABLE       = 0x0086
} mvprojector_gpio_address_t;

typedef enum {
    GPIO_INPUT,
    GPIO_OUTPUT_HIGH,
    GPIO_OUTPUT_LOW
} mvprojector_gpio_command_t;

typedef enum {
    //read commands
    COMMAND_GET_FW_VERSION           = 0x3C,
    COMMAND_GET_PULSE_WIDTH          = 0x41,

    //write commands
    COMMAND_SET_DELAY                = 0x60,
    COMMAND_SET_PULSE_WIDTH          = 0x61,
    LASER_CMD_SET_CURRENT            = 0x62,
} mvprojector_i2c_command_t;


#endif
