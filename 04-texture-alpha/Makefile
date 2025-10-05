include ../Makeshared.mk

default: 04-texture-alpha shaders

04-texture-alpha: 04-texture-alpha.o $(FRAMEWORK_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

shaders: trace.spv hit.spv miss.spv

clean:
	@find . -iregex '.*\.\(d\|o\)' -delete
	@find $(FRAMEWORK) -iregex '.*\.\(d\|o\)' -delete
	@rm -rf 04-texture-alpha
