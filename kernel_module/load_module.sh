#!/bin/bash

MODULE_NAME=$1
DEV_FILE=/dev/gpio_int
GPIO_MAJOR=243

if [ ! -f $DEV_FILE ]; then
  echo "GPIO_INT_MOD: Creating device node: "$DEV_FILE
  echo "GPIO_INT_MOD: Command = mknod "$DEV_FILE" c "$GPIO_MAJOR" 0"
  mknod $DEV_FILE c $GPIO_MAJOR 0
fi

echo "Inserting kernel module = " $MODULE_NAME
insmod $MODULE_NAME

