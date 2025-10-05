include ../Makeshared.mk

default: 02-transform shaders

02-transform: 02-transform.o $(FRAMEWORK_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

shaders: trace.spv hit.spv miss.spv

clean:
	@find . -iregex '.*\.\(d\|o\)' -delete
	@find $(FRAMEWORK) -iregex '.*\.\(d\|o\)' -delete
	@rm -rf 02-transform
