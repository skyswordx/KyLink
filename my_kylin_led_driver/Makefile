obj-m += my_kylin_led_driver.o

# Allow KDIR override on the command line.
KDIR ?= ../../kernel

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
