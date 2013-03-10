
BUILTIN_DTB := $(patsubst "%",%,$(CONFIG_BUILTIN_DTB)).dtb.o
ifneq ($(CONFIG_BUILTIN_DTB),"")
obj-y += $(BUILTIN_DTB)
endif

targets += dtbs
targets += $(dtb-y)

dtbs: $(addprefix $(obj)/, $(dtb-y))

clean-files := *.dtb *.dtb.S
