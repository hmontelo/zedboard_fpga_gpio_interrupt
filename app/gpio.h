/*
 * gpio.h
 *
 *  Created on: Mar 31, 2018
 *      Author: Team 3
 */

#ifndef _GPIO_H_
#define _GPIO_H_

/* -----------------------------------------------------------------------------
 *
 * Opens a memory map. It return its file descriptor.
 *
 */
int
gpio_open_memory_map (char *mem_device);

/** @brief gpio_set_pin routine: This routine sets and clears a single bit
 * in a GPIO register.
 *  @param target_addr GPIO register address
 *  @param pin_number GPIO pin used to read/write
 *  @return bit_val   value used to set the GPIO pin
 */
int
gpio_set_pin (int fd, unsigned int target_addr, unsigned int pin_number,
              unsigned int bit_val);


/* -----------------------------------------------------------------------------
 *
 * Closes a memory map.
 *
 */
int
gpio_close_memory_map (int fd);

#endif /* _GPIO_H_ */
