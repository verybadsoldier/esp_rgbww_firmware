#####################################################################
#### Please don't change this file. Use component.mk instead ####
#####################################################################

override SMING_HOME := $(CURDIR)/Sming/Sming

ifndef SMING_HOME
$(error SMING_HOME is not set: please configure it as an environment variable)
endif

include $(SMING_HOME)/project.mk
