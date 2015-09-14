TARGET	:=power-domain-test
obj-m += $(TARGET).o

$(TARGET)-objs	:= device.o domain.o

MODULE_DIR	:= "$(PWD)"

all:
	CROSS_COMPILE= ARCH=x86 make -C /lib/modules/$(shell uname -r)/build M=$(MODULE_DIR) modules 

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(MODULE_DIR) clean

install:
	sudo rmmod $(TARGET); \
		sudo mkdir -p /lib/modules/$(KVERSION)/kernel/drivers/$(TARGET)
	sudo cp $(TARGET).ko /lib/modules/$(KVERSION)/kernel/drivers/$(TARGET)
	sudo depmod -a
	sudo modprobe $(TARGET)

.PHONY:remove
test:	remove all
	sudo insmod ./$(TARGET).ko n_devs=3 debug=2

.PHONY:test
remove:
	sudo rmmod $(TARGET)
