#
# Makefile for the NXP TFA9894 smart amplifier driver.
#

ccflags-y += -I$(src)/inc

snd-soc-tfa9894-objs += src/tfa98xx.o
snd-soc-tfa9894-objs += src/tfa_container.o
snd-soc-tfa9894-objs += src/tfa_dsp.o
snd-soc-tfa9894-objs += src/tfa_init.o

obj-$(CONFIG_SND_SOC_TFA9894) := snd-soc-tfa9894.o
