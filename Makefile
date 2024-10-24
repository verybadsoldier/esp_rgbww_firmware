#####################################################################
#### Please don't change this file. Use component.mk instead ####
#####################################################################


# removed to use global sming according to SMING_HOME
#override SMING_HOME := $(CURDIR)/Sming/Sming

ifndef SMING_HOME
$(error SMING_HOME is not set: please configure it as an environment variable)
endif

include $(SMING_HOME)/project.mk
