#ifndef USER_INCLUDE_APPDEF_H_
#define USER_INCLUDE_APPDEF_H_

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef enum {
    GPIO0 = 0,
    GPIO1,
    GPIO2,
    GPIO3,
    GPIO4,
    GPIO5,
    GPIO6,
    GPIO7,
    GPIO8,
    GPIO9,
    GPIO10,
    GPIO11,
    GPIO12,
    GPIO13,
    GPIO14,
    GPIO15,
    GPIOMAX
} gpio_num_t;

extern const int32_t pin_name[GPIOMAX];
extern const int32_t func[GPIOMAX];

#endif /* USER_INCLUDE_APPDEF_H_ */
