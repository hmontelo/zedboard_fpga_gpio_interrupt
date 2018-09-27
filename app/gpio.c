#include <sys/mman.h>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>
#include "gpio.h"

#define ONE_BIT_MASK(_bit)    (0x00000001 << (_bit))
#define MAP_SIZE              4096UL
#define MAP_MASK              (MAP_SIZE - 1)

/* -----------------------------------------------------------------------------
 *
 * Opens a memory map. It return its file descriptor.
 *
 */

int
gpio_open_memory_map (char *mem_device)
{
  int fd = open (mem_device, O_RDWR | O_SYNC);

  if (fd == -1)
  {
    return -1;
  }

  return fd;

}


/* -----------------------------------------------------------------------------
 *
 * gpio_set_pin routine: This routine sets and clears a single bit
 * in a GPIO register.
 *
 */

int
gpio_set_pin (int fd, unsigned int target_addr, unsigned int pin_number,
              unsigned int bit_val)
{
  unsigned int reg_data;

  volatile unsigned int *regs, *address;

  regs = (unsigned int *) mmap (NULL, MAP_SIZE, PROT_READ | PROT_WRITE,
                                MAP_SHARED, fd, target_addr & ~MAP_MASK);

  address = regs + (((target_addr) & MAP_MASK) >> 2);

#ifdef DEBUG1
  printf("REGS           = 0x%.8x\n", regs);
  printf("Target Address = 0x%.8x\n", target_addr);
  printf("Address        = 0x%.8x\n", address);     // display address value
  //printf("Mask           = 0x%.8x\n", ONE_BIT_MASK(pin_number));  // Display mask value
#endif

  /* Read register value to modify */

  reg_data = *address;

  if (bit_val == 0)
  {

    /* Deassert output pin in the target port's DR register*/

    reg_data &= ~ONE_BIT_MASK(pin_number);
    *address = reg_data;
  }
  else
  {

    /* Assert output pin in the target port's DR register*/

    reg_data |= ONE_BIT_MASK(pin_number);
    *address = reg_data;
  }

  return 0;

}

/* -----------------------------------------------------------------------------
 *
 * Closes a memory map.
 *
 */

int
gpio_close_memory_map (int fd)
{
  int temp = close (fd);
  if (temp == -1)
  {
    return -1;
  }
  munmap (NULL, MAP_SIZE);
  return 0;
}
