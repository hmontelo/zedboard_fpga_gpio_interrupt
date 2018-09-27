#!/bin/bash

MODULE_NAME=$1

echo "Removing kernel module = " $MODULE_NAME
rmmod $MODULE_NAME
