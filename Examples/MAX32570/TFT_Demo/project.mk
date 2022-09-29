# This file can be used to set build configuration
# variables.  These variables are defined in a file called 
# "Makefile" that is located next to this one.

# For instructions on how to use this system, see
# https://github.com/Analog-Devices-MSDK/VSCode-Maxim/tree/develop#build-configuration



# **********************************************************

# Add resources folder to the build
VPATH += resources
IPATH += resources

# Disable TFT display
PROJ_CFLAGS += -DDISABLE_EVKIT_DISPLAY