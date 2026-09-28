#!/bin/bash
gdbserver :2345 ./intercom_gpio_demon > /dev/null 2>&1 &
