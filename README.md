-- Design:

For this system, it was developed a hardware block design and then wrote source code to implement a kernel module and 
application program. 

It was used Vivado (on Xilinx/2017.4) to create the block design given in the lab instructions. The AXI_GPIO module was removed and 
created a new AXI slave IP block. This new block was named as int_latency_0 and mapped LEDS[7:0] to the physical LEDs on the 
Zed Board via the updated constrants file given on Canvas. Additionally it was mapped the interrupt_out output wire from the 
int_latency_0 module to the PS as an IRQ_F2P[0:0] input. This required us to configure and enable interrupts on the PS.

A kernel module by making use of the gpio_interrupt.c code based on given on the course web page. The module dynamically probe 
the system to find the IRQ number like in the code given in the lecture slides. In the interrupt handler of this module, 
a SIGIO signal is raised and inserted into the fasync_fpga_queue so that the user application can be notified correctly 
of interrupts. 

For the user application, a file called gpio_interrupt_monitor.c was created. An important note about the user application is that
the function gpio_set_pin() was created, that function takes in an address, a register number, and a value and performs a mmap() 
and munmap() accordingly to map and unmap the location of the interrupt assertion pin into physical memory. 
Initially in the main function, the program sets up to listen for SIGIO signals that come from the kernel module and the 
interrupt pin is de-asserted. Then in the main body of the 10,000 iteration loop, a time measurement is taken, the interrupt 
pin is asserted, a busy-wait occurs on a flag which is set in the signal handler of the user program when a signal arrives 
from the kernel module, and then the interrupt pin is de-asserted. Minimum Latency and Maximum Latency metrics are accumulated 
in a buffer. Average Latency, Standard Deviation, Number of Samples, and Number of Interrupts registered in the Kernel are 
metrics that are computed after the 10,000 iteration loop finishes.

-- To Run:

1. In the repository directory, first make the kernel module and the app by going into each subdirectory and running "make"
2. To load the kernel module into the kernel, run "./loadModule.sh <path to kernel module .ko file>"
3. To test one iteration of the application (10,000 samples), simply run ./gpio_interrupt_monitor
3. To test 300 iterations of the application without load, run "./test300.sh". To run with a workload in the background, first run "./workload.sh &" and then run "./test300.sh" 
