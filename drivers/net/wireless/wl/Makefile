obj-m		+= wl.o

wl-objs		:= src/shared/linux_osl.o
wl-objs		+= src/wl/sys/wl_linux.o
wl-objs		+= src/wl/sys/wl_iw.o

EXTRA_CFLAGS	+= -I$(src)/src/include
EXTRA_CFLAGS	+= -I$(src)/src/wl/sys
EXTRA_LDFLAGS	:= $(srctree)/$(src)/lib/wlc_hybrid.o_shipped
