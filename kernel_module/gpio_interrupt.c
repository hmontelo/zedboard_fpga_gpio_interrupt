/*
 ============================================================================
 Name        : gpio_interrupt.c
 Author      : Team3
 Version     :
 Copyright   : Your copyright notice
 Description : GPIO kernel module for Linux Xilinx Zedboard
 ============================================================================
 */

/* *************************** INCLUDES *********************************** */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <asm/errno.h>
#include <linux/signal.h>

/* ******************* MACROS AND DEFINITIONS ****************************** */

#ifdef DEBUG
  #undef DEBUG
#endif

#define TRUE                  1
#define FALSE                 0

#define GPIO_MODULE_VERSION   "1.0"
#define GPIO_MAJOR            243 // Need to mknod /dev/gpio_int c 243 0
#define GPIO_MODULE_NAME      "gpio-interrupt"
#define GPIO_CHAR_DEV_NAME    "gpio_int"
#define GPIO_PROC_ENTRY       GPIO_MODULE_NAME

/* ******************* STATIC AND GLOBAL VARIABLES  ************************ */
static unsigned int GPIO_interruptcount         = 0;
unsigned int GPIO_interrupt_number              = 0;
static struct proc_dir_entry *GPIO_proc_entry   = NULL;
static struct fasync_struct *GPIO_fasync_queue  = NULL;
static unsigned char platform_driver_registered = FALSE;
static unsigned char char_dev_registered        = FALSE;
static unsigned char proc_entry_created         = FALSE;
static unsigned char interrupt_requested        = FALSE;

/* ************************* FUNCTION PROPOTOTYPES ************************** */

/** @brief The GPIO initialization function
 *  The static keyword restricts the visibility of the function to within this
 *  C file. The __init macro means that for a built-in driver (not a GPIO) the
 *  function is only used at initialization time and that it can be discarded
 *  and its memory freed up after that point. In this case this function
 *  sets up the GPIOs and the IRQ
 *  @return returns 0 if successful
 */
static int __init GPIO_init(void);

/** @brief The GPIO cleanup function
 *  Similar to the initialization function, it is static. The __exit macro
 *  notifies that if this code is used for a built-in driver (not a GPIO) that
 *  this function is not required. Used to release the GPIOs and display
 *  cleanup messages.
 */
static void __exit GPIO_exit(void);

/** @brief The GPIO IRQ Handler function
 *  This function is a custom interrupt handler that is attached to the GPIO.
 *  The same interrupt handler cannot be invoked concurrently as the
 *  interrupt line is masked out until the function is complete. This function
 *  is static as it should not be invoked directly from outside of this file.
 *  @param irq    the IRQ number that is associated with the GPIO -- useful for
 *                logging.
 *  @param dev_id the *dev_id that is provided -- can be used to identify which
 *                device caused the interrupt. Not used in this case as NULL
 *                is passed.
 *  return returns IRQ_HANDLED if successful -- should return IRQ_NONE
 *  otherwise.
 */
static irqreturn_t GPIO_int_handler(int irq, void *dev_id);


/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int GPIO_open (struct inode *inode, struct file *filp);

/** @brief This function is called whenever device is being read from user space
 *  i.e. data is being sent from the device to the user. In this case is uses
 *  the copy_to_user() function to send the buffer string to the user and
 *  captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the
 *         data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t GPIO_read (struct file *filp,
                   char __user *buff, size_t count, loff_t *offp);

/** @brief This function is called whenever the device is being written to from
 *  user space i.e. data is sent to the device from the user. The data is copied
 *  to the message[] array in this LKM using the sprintf() function along with
 *  the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const
 *         char buffer
 *  @param offset The offset if required
 */
static ssize_t GPIO_write(struct file *filp,
                   const char __user *buf, size_t count,loff_t *f_pos);

/** @brief The device release function that is called whenever the device is
 *          closed/released by the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int GPIO_release(struct inode *inodep, struct file *filep);


/** @brief This is invoked by the kernel when the user program opens this
* input device and issues fcntl(F_SETFL) on the associated file
* descriptor. fasync_helper() ensures that if the driver issues a
* kill_fasync(), a SIGIO is dispatched to the owning application.
*/
static int GPIO_fasync (int fd, struct file *filp, int on);


static int GPIO_remove(struct platform_device *pdev);

static int GPIO_probe(struct platform_device *pdev);



/* ************************ STRUCTURES AND TYPEDEFS ************************* */

/*
* Define which file operations are supported
*
*/
struct file_operations gpio_fops = {
  .owner          = THIS_MODULE,  // Pointer to the GPIO that owns the structure
  .llseek         = NULL,         // Change current read/write position in a file
  .read           = GPIO_read,    // Used to retrieve data from the device
  .write          = GPIO_write,   // Used to send data to the device
  .poll           = NULL,         // Does a read or write block?
  .unlocked_ioctl = NULL,         // Called by the ioctl system call
  .mmap           = NULL,         // Called by mmap system call
  .open           = GPIO_open,    // first operation performed on a device file
  .flush          = NULL,         // called when a process closes its copy of the descriptor
  .release        = GPIO_release, // called when a file structure is being released
  .fsync          = NULL,         // notify device of change in its FASYNC flag
  .fasync         = GPIO_fasync,  // asynchronous notify device of change in its FASYNC flag
  .lock           = NULL,         // used to implement file locking
};

static const struct of_device_id gpio_of_match[] = {
  { .compatible = "xlnx,gpio-interrupt-1.0" },
  { /* end of table */ }
};

static struct platform_driver gpio_driver = {
  .driver = {
    .name           = GPIO_MODULE_NAME,
    .of_match_table = gpio_of_match,
  },
  .probe  = GPIO_probe,
  .remove = GPIO_remove,
};

MODULE_DEVICE_TABLE(of, gpio_of_match);


/* ************************ FUNCTION IMPLEMENTATION ************************* */

int GPIO_open (struct inode *inode, struct file *filp)
{
  printk("GPIO_KMOD: gpio_open\n");
  return 0; /* success */
}

ssize_t GPIO_read (struct file *filp,
                   char __user *buff, size_t count, loff_t *offp)
{
  return 0;
}

ssize_t GPIO_write (struct file *filp,
                   const char __user *buf, size_t count,loff_t *f_pos)
{
  return 0;
}

/* ===================================================================
* gpio_probe - Initialization method for a zynq_gpio device
* Return: 0 on success, negative error otherwise.
*/
static int GPIO_probe(struct platform_device *pdev)
{
  struct resource *res;

  printk("GPIO_KMOD: Starting probe\n");
  // This code gets the IRQ number by probing the system.
  res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
  if (!res)
  {
    printk("GPIO_KMOD: No IRQ found\n");
    return 0;
  }
  // Get interrupt number
  GPIO_interrupt_number = res->start;
  printk("GPIO_KMOD: IRQ found: %d\n", GPIO_interrupt_number);
  return 0;
}

/* ===================================================================
* function: gpio_int_handler
*
* This function is the interrupt handler for interrupt latency test
*/
static irqreturn_t GPIO_int_handler(int irq, void *dev_id)
{
  GPIO_interruptcount++;
  #ifdef DEBUG
    printk("GPIO_KMOD: Interrupt detected in kernel \n"); // DEBUG
  #endif
  /* Signal the user application that an interrupt occurred */
  kill_fasync(&GPIO_fasync_queue, SIGIO, POLL_IN);
  return IRQ_HANDLED;
}

/* ===================================================================
* function: gpio_fasync
*
* This is invoked by the kernel when the user program opens this
* input device and issues fcntl(F_SETFL) on the associated file
* descriptor. fasync_helper() ensures that if the driver issues a
* kill_fasync(), a SIGIO is dispatched to the owning application.
*/
static int GPIO_fasync (int fd, struct file *filp, int on)
{
  #ifdef DEBUG
    printk("GPIO_KMOD: Inside gpio_fasync \n"); // DEBUG
  #endif
  return fasync_helper(fd, filp, on, &GPIO_fasync_queue);
};

static int GPIO_release(struct inode *inodep, struct file *filep)
{
  return 0;
}

/* =======================================================
*
* zynq_gpio_remove - Driver removal function
*
* Return: 0 always
*/
static int GPIO_remove(struct platform_device *pdev)
{
  return 0;
}
/* ===================================================================
* function: cleanup_gpio_interrupt
*
* This function frees interrupt then removes the /proc directory entry
* gpio_interrupt.
*/
static void __exit GPIO_exit(void)
{
  if(platform_driver_registered != FALSE)
    unregister_chrdev(GPIO_MAJOR, GPIO_CHAR_DEV_NAME); // Release character device
  if(char_dev_registered != FALSE)
    platform_driver_unregister(&gpio_driver); // Unregister the driver
  if(proc_entry_created != FALSE)
    remove_proc_entry(GPIO_PROC_ENTRY, NULL); // Remove process entry
  if(interrupt_requested != FALSE)
    free_irq(GPIO_interrupt_number,NULL); // Release IRQ

  printk(KERN_INFO "GPIO_KMOD: %s %s removed\n", GPIO_MODULE_NAME, GPIO_MODULE_VERSION);
}

/* ===================================================================
* function: init_gpio_int
*
* This function creates the /proc directory entry gpio_interrupt.
*/
static int __init GPIO_init(void)
{
  int err = 0;

  GPIO_interruptcount   = 0;
  GPIO_interrupt_number = 0;
  GPIO_proc_entry       = NULL;
  GPIO_fasync_queue     = NULL;

  platform_driver_registered = FALSE;
  char_dev_registered        = FALSE;
  proc_entry_created         = FALSE;
  interrupt_requested        = FALSE;

  platform_driver_unregister(&gpio_driver);
  printk("GPIO_KMOD: ZED Interrupt Module\n");
  printk("GPIO_KMOD: ZED Interrupt Driver Loading.\n");
  printk("GPIO_KMOD: Using Major Number %d on %s\n", GPIO_MAJOR, GPIO_CHAR_DEV_NAME);

  err = platform_driver_register(&gpio_driver);
  if(err !=0)
  {
    printk("GPIO_KMOD: Driver register error with number %d\n",err);
    goto no_gpio_interrupt;
  }
  printk("GPIO_KMOD: Success to register the GPIO Driver\n");

  platform_driver_registered = TRUE;

  err = register_chrdev(GPIO_MAJOR, GPIO_CHAR_DEV_NAME, &gpio_fops);

  if(err != 0)
  {
    printk("GPIO_KMOD: Unable to get major %d. ABORTING!\n", GPIO_MAJOR);
    goto no_gpio_interrupt;
  }
  printk("GPIO_KMOD: Success to register %s with major %d\n", GPIO_CHAR_DEV_NAME,GPIO_MAJOR);

  char_dev_registered = TRUE;

  // Create the proc entry
  GPIO_proc_entry = proc_create(GPIO_PROC_ENTRY, 0444, NULL, &gpio_fops );
  if(GPIO_proc_entry == NULL)
  {
    printk("GPIO_KMOD: Create /proc/%s entry returned NULL. ABORTING!\n",GPIO_PROC_ENTRY);
    goto no_gpio_interrupt;
  }
  printk("GPIO_KMOD: Success to create /proc/%s\n", GPIO_PROC_ENTRY);

  proc_entry_created = TRUE;

  // request interrupt number from linux
  err = request_irq(GPIO_interrupt_number,
                   GPIO_int_handler,
                   IRQF_TRIGGER_RISING,
                   GPIO_MODULE_NAME,
                   NULL);
  if ( err )
  {
    printk("GPIO_KMOD: Can't get interrupt %d with error code: %d\n", GPIO_interrupt_number, err);
    goto no_gpio_interrupt;
  }
  printk("GPIO_KMOD: %s %s Initialized\n",GPIO_MODULE_NAME, GPIO_MODULE_VERSION);

  interrupt_requested = TRUE;

  return 0;
  // remove the proc entry on error
no_gpio_interrupt:
  if(platform_driver_registered != FALSE)
    unregister_chrdev(GPIO_MAJOR, GPIO_CHAR_DEV_NAME);
  if(char_dev_registered != FALSE)
    platform_driver_unregister(&gpio_driver);
  if(proc_entry_created != FALSE)
    remove_proc_entry(GPIO_PROC_ENTRY, NULL);
  if(interrupt_requested != FALSE)
    free_irq(GPIO_interrupt_number,NULL); // Release IRQ
  return -EBUSY;
};

/* REQUIRED */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Team 3");
MODULE_DESCRIPTION("GPIO Interrupt Kernel Module");
MODULE_VERSION(GPIO_MODULE_VERSION);
module_init(GPIO_init);
module_exit(GPIO_exit);


