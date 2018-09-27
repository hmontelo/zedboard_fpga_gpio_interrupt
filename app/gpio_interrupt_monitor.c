/*
 ============================================================================
 Name        : gpio_interrupt_monitor.c
 Author      : Advanced MCU - Spring 2018 - Team3
 Version     :
 Copyright   : Your copyright notice
 Description : Monitor interrupts from GPIO Kernel Module for Linux Xilinx
 Zedboard
 ============================================================================
 */

/* *************************** INCLUDES *********************************** */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <math.h>
#include <limits.h>
#include "gpio.h"

/* ******************* MACROS AND TYPE DEFINITIONS ************************* */

#ifdef DEBUG
#undef DEBUG
#endif

//#define READING_PROC
#define TRUE              1
#define FALSE             0

#define GPIO_DEVICE       "/dev/gpio_int"
#define PROC_FS_FILENAME  "/proc/interrupts"
#define MEM_DEVICE        "/dev/mem"
#define CSV_FILENAME      "latency.csv"

#define NUM_SETS          1
#define NUM_SAMPLES       10000

#define INT_LATENCY_ADDR  0x43C10000

/* ************************* FUNCTION PROPOTOTYPES ************************ */

/** @brief The signal handler function
 *  This function is used to capture an asynchronous I/O Linux signal sent by
 *  the GPIO kernel module drive.
 *  @return none
 */
void
sighandler (int signo);

/** @brief Creates a .CSV file
 *  This function creates a .csv file used to record and review the latency
 *  information.
 *  @param filename The name of the csv file
 *  @return A FILE pointer with the descriptor of the opened/created file.
 */
FILE*
create_csv_file (char *filename);

/** @brief Update a .CSV file
 *  This function updates the values in the opened .csv file
 *  @param data The array of data to be recorded in the csv file
 *  @param size The maximum array data size
 *  @return none
 */
void
update_csv_file (FILE *fp, unsigned long min, unsigned long max);

/** @brief Closes a .CSV file
 *  This function closes the previously opened .csv file
 *  @param fp It is the file descriptor of the opened/created file.
 *  @return none
 */
void
close_csv_file (FILE *fp);

/* ******************* STATIC AND GLOBAL VARIABLES  *********************** */

static int det_int = 0;
static int num_int = 0;
static int KeepRunning = TRUE;
static FILE *fd_proc = NULL;
static struct timeval GPIO_t1;
static struct timeval GPIO_t2;
unsigned long buff[NUM_SAMPLES];

/* ********************** FUNCTION IMPLEMENTATION ************************* */

/* ============================ Signal Handler ============================ */
void
sighandler (int signo)
{
  switch (signo)
    {
    case SIGIO:
      {
        //printf ("GPIO_MONITOR: Interrupt captured by SIGIO\n");
        det_int = 1;
        gettimeofday (&GPIO_t2, NULL);

        break;
      }
    case SIGHUP:
    case SIGINT:
      {
        KeepRunning = FALSE;
        break;
      }
    }
  return; /* Return to main loop */
}

/* ==================== Functions to Handle CSV files ====================== */

FILE*
create_csv_file (char *filename)
{
  FILE *fp = NULL;

  fp = fopen (filename, "a+");
  return fp;

}

void
update_csv_file (FILE *fp, unsigned long min, unsigned long max)
{
  fprintf (fp, "%lu,%lu\n", min, max);
}

void
close_csv_file (FILE *fp)
{
  fclose (fp);
}

/* *************************** MAIN FUNCTION ****************************** */

int
main (int argc, char **argv)
{
  struct sigaction action;
  fd_proc = NULL;
  time_t current_time;
  FILE *fp;
  int fd;
  int rc;
  int fc;
  int fd_mem;
  unsigned long max, min, sum;
  float avg, std_dev;
  int set, i;

  // Print pid, so that we can send signals from other shells
  printf ("GPIO_MONITOR: Process Id (Pid) is: %d\n", getpid ());

  /*
   * Open /proc filesystem
   */
  fd_proc = fopen(PROC_FS_FILENAME,"r");
  if(fp == NULL)
  {
    printf("GPIO_MONITOR: Unable to open %s\n", PROC_FS_FILENAME);
    exit(-1);
  }

  /*
   * Creating CSV used file for review.
   */
  printf ("GPIO_MONITOR: Creating %s file...\n", CSV_FILENAME);
  fp = create_csv_file (CSV_FILENAME);
  if (fp == NULL)
  {
    printf ("GPIO_MONITOR: Unable to open/create %s\n", CSV_FILENAME);
    exit (-1);
  }

  /*
   * Registering a Linux signal and Handler
   */
  //sigemptyset (&action.sa_mask);
  //sigaddset (&action.sa_mask, SIG_GPIO);
  // Setup the signal handle
  action.sa_handler = sighandler;

  // Restart the system call, if at all possible
  action.sa_flags = SA_RESTART;
  if (sigaction (SIGIO, &action, NULL) == -1)
  {
    perror ("Error: cannot handle SIGIO"); // Should not happen
  }
  /*  if(sigaction(SIGHUP, &action, NULL) == -1)
   {
   perror("Error: cannot handle SIGHUP"); // Should not happen
   }*/
  /*  if(sigaction(SIGINT, &action, NULL) == -1)
   {
   perror("Error: cannot handle SIGINT"); // Should not happen
   }*/

  // Block every signal during the handler
  sigfillset (&action.sa_mask);

  /*
   * Opening the gpio device that was created by the command
   * mknod /dev/gpio_int c 243 0 during the kernel module development
   */
  fd = open (GPIO_DEVICE, O_RDWR);

  if (fd == -1)
  {
    printf ("GPIO_MONITOR: Unable to open %s\n", GPIO_DEVICE);
    rc = fd;
    exit (-1);
  }
  printf ("GPIO_MONITOR: %s opened successfully\n", GPIO_DEVICE);

  /*
   * Now, The process associated with the opened GPIO device is configured to
   * owner of the device and handle the SIG_GPIO signals.
   */
  fc = fcntl (fd, F_SETOWN, getpid ());

  if (fc == -1)
  {
    printf ("GPIO_MONITOR: SETOWN failed\n");
    rc = fd;
    exit (-1);
  }
  printf ("GPIO_MONITOR: SETOWN configured successfully\n");

  /*
   * Set the file status for the opened and owned GPIO device.
   * It obtains the current file status and append the flag O_ASYNC to enable
   * the generation of signals through the opened device.
   */
  fc = fcntl (fd, F_SETFL, fcntl (fd, F_GETFL) | O_ASYNC);

  if (fc == -1)
  {
    printf ("GPIO_MONITOR: SETFL failed\n");
    rc = fd;
    exit (-1);
  }
  printf ("GPIO_MONITOR: SETFL configured successfully\n");

  fd_mem = gpio_open_memory_map(MEM_DEVICE);
  if (fd_mem == -1)
  {
    printf ("GPIO_MONITOR: Unable to open %s.  Ensure it exists (major=1, minor=1)\n", MEM_DEVICE);
    exit (-1);
  }
  printf ("GPIO_MONITOR: Memory Map %s opened successfully\n", MEM_DEVICE);

  /*
   * This while loop emulates a program running the main loop i.e. sleep().
   * The main loop is interrupted when the Linux SIG_GPIO signal is received
   */
  KeepRunning = TRUE;
  rc = gpio_set_pin (fd_mem, INT_LATENCY_ADDR, 0, 0);  // Clear output pin
  if (rc != 0)
  {
    perror ("gpio_set_pin() failed");
    return -1;
  }

  for (set = 0; set < NUM_SETS; set++)
  {
    max = 0;
    min = ULONG_MAX;
    sum = 0;
    avg = 0;
    std_dev = 0;
    int hi = 1;
    int lo = 0;

    for (i = 0; i < NUM_SAMPLES; i++)
    {
      gettimeofday (&GPIO_t1, NULL);
      rc = gpio_set_pin (fd_mem, INT_LATENCY_ADDR, 0, 1);  // Set output pin
      if (rc != 0)
      {
        perror ("gpio_set_pin() failed");
        return -1;
      }

      while (det_int == 0) ;
      rc = gpio_set_pin (fd_mem, INT_LATENCY_ADDR, 0, 0);  // Clear output pin
      if (rc != 0)
      {
        perror ("gpio_set_pin() failed");
        return -1;
      }

      unsigned long diff = (GPIO_t2.tv_sec - GPIO_t1.tv_sec) * 1000000
          + (GPIO_t2.tv_usec - GPIO_t1.tv_usec);
      buff[i] = diff;
      sum += diff;
      if (max < diff)
        max = diff;
      if (min > diff)
        min = diff;
      det_int = 0;
    }

    avg = sum / NUM_SAMPLES;
    for (i = 0; i < NUM_SAMPLES; i++)
    {
      std_dev += pow (buff[i] - avg, 2);
    }
    std_dev = sqrt (std_dev / NUM_SAMPLES);

    printf ("Minimum Latency:    %luus\n", min);
    printf ("Maximum Latency:    %luus\n", max);
    printf ("Average Latency:    %fus\n", avg);
    printf ("Standard Deviation: %fus\n", std_dev);
    printf ("Number of samples: %d\n", NUM_SAMPLES);

    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    int num_line = 0;
    while ((read = getline (&line, &len, fd_proc)) != -1)
    {
      if (line[0] == '1' && line[1] == '6' && line[2] == '4')
        printf ("%s", line);
      num_line++;
    }
    fseek(fd_proc, 0, SEEK_SET);
    update_csv_file (fp, min, max);

  }
  gpio_close_memory_map(fd_mem);
  fclose (fd_proc);
  close_csv_file (fp);
  printf ("\nGPIO_MONITOR: Monitoring GPIO interrupt has finished.\n");
}

