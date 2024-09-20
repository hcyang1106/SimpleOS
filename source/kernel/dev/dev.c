#include "dev/dev.h"
#include "cpu/cpu.h"

#define DEV_TABLE_SIZE 128

extern dev_desc_t dev_tty_desc;
extern dev_desc_t dev_disk_desc;

static dev_desc_t *dev_desc_table[] = {
    &dev_tty_desc,
    &dev_disk_desc,
};

static device_t dev_table[DEV_TABLE_SIZE];

static int is_device_id_bad(int dev_id) {
    if (dev_id < 0 || dev_id >= DEV_TABLE_SIZE) {
        return 1;
    }
    if (dev_table[dev_id].desc == (dev_desc_t*)0) {
        return 1;
    }
    return 0;
}

int dev_open(int major, int minor, void *data) {
    irq_state_t state = irq_enter_protection();

    // return opened device or free struct
    // if a free one is returned, it will return the largest number of the free ones
    device_t *free_dev = (device_t*)0;
    for (int i = 0; i < DEV_TABLE_SIZE; i++) {
        if (dev_table[i].desc->major == major &&
            dev_table[i].minor == minor) {
            dev_table[i].open_count++;
            irq_leave_protection(state);
            return i;
        } else if (dev_table[i].open_count == 0) {
            free_dev = &dev_table[i];
        }
    }
    if (!free_dev) {
        return -1;
    }

    dev_desc_t *desc = (dev_desc_t*)0;
    for (int i = 0; i < sizeof(dev_desc_table) / sizeof(dev_desc_table[0]); i++) {
        if (dev_desc_table[i]->major == major) {
            desc = dev_desc_table[i];
            break;
        }
    }
    if (!desc) {
        return -1;
    }

    free_dev->data = data;
    free_dev->desc = desc;
    free_dev->minor = minor;
    int err = desc->open(free_dev);
    if (!err) {
        free_dev->open_count = 1;
        irq_leave_protection(state);
        return free_dev - dev_table;
    }
    
    irq_leave_protection(state);
    return -1;
}

int dev_read(int dev_id, int addr, char *buf, int size) {
    if (is_device_id_bad(dev_id)) {
        return -1;
    }

    device_t *device = dev_table + dev_id;
    return device->desc->read(device, addr, buf, size);
}

int dev_write(int dev_id, int addr, char *buf, int size) {
    if (is_device_id_bad(dev_id)) {
        return -1;
    }

    device_t *device = dev_table + dev_id;
    return device->desc->write(device, addr, buf, size);
}

int dev_control(int dev_id, int cmd, int arg0, int arg1) {
    if (is_device_id_bad(dev_id)) {
        return -1;
    }

    device_t *device = dev_table + dev_id;
    return device->desc->control(device, cmd, arg0, arg1);
}

int dev_close(int dev_id) {
    if (is_device_id_bad(dev_id)) {
        return -1;
    }

    device_t *device = dev_table + dev_id;
    irq_state_t state = irq_enter_protection();

    if (device->open_count > 1) {
        device->open_count--;
        irq_leave_protection(state);
        return 0;
    }

    int err = device->desc->close(device);
    if (!err) {
        device->open_count = 0;
        irq_leave_protection(state);
        return 0;
    }

    irq_leave_protection(state);
    return -1;
}