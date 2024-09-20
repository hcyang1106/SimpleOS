#ifndef DEV_H
#define DEV_H

#define DEV_NAME_SIZE 32

enum {
    DEV_UNKNOWN = 0,
    DEV_TTY,
    DEV_DISK,
};

struct _dev_desc_t;
// a specific device under dev_desc_t category
typedef struct _device_t {
    struct _dev_desc_t *desc;
    int mode; // read only, etc.
    int minor; // major and minor combined decribed this specific device
    void *data;
    int open_count; // times opened
}device_t;

// a category of device
typedef struct _dev_desc_t {
    char name[DEV_NAME_SIZE];
    int major;
    int (*open)(device_t *dev);
    int (*read)(device_t *dev, int addr, char *buf, int size); // buf is the destination
    int (*write)(device_t *dev, int addr, char *buf, int size);
    int (*control)(device_t *dev, int cmd, int arg0, int arg1);
    int (*close)(device_t *dev);
}dev_desc_t;

int dev_open(int major, int minor, void *data);
int dev_read(int dev_id, int addr, char *buf, int size);
int dev_write(int dev_id, int addr, char *buf, int size);
int dev_control(int dev_id, int cmd, int arg0, int arg1);
int dev_close(int dev_id);

#endif


// oop

// to my understanding, the more the user knows about the attributes,
// the more coupled user/provider are

// why encapsulation?
// 1. others won't modify my value, so it is easier to manage 
// (if met bugs then just check my implementation, no need to check others)
// 2. users don't need to know the detail of my implementation (e.g. int obj = create_obj();)
// no need to know its attribute

// how to achieve it in c? => define the struct in ".c" rather in ".h"
// so users can't access the attributes (if defined in .h then users could access)
// user might need to do forward declaration to make compilation successful
// (tells the compiler the definition is elsewhere, needed to be linked)
// (every function/variable needs to be declared before use)

// why inheritance?
// to reduce repeated code

// how to achieve it in c? => insert the base struct into the derived struct (so that base attr can be accessed)
// => create function that accepts derived struct as param, and inside call the function we wanted

// example:
// #include "human.h"
// #include <string.h>

// struct _Human {
//     struct _Animal animal;
//     int id;
// };

// char *humanGetName(struct _Human *human) {
//     return animalGetName(&(human->animal));
// }

// why polymorphism? => decoupling (no need to modify too much code if new subclass added), less code
// in c++ its virtual functions and classes that have only virtual functions are called interfaces
// e.g. os and devices, can add devices without modifying too much code
// e.g. could write algorithms that can apply to all subclasses

// to my understanding, the more abstract it is, the more decoupled they are 

// how to achieve it in c?
// similar to the code here. insert a function pointer to a struct (e.g. bark(void)), and when doing initialization,
// point it to the real implementation (e.g. dog_bark(void), cat_bark(void))
// so when user calls animal->bark(), it calls different functions according to which animal it is

// an example: https://stackoverflow.com/questions/4456685/polymorphism-in-c