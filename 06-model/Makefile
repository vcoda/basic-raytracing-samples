include ../Makeshared.mk

default: 06-model shaders

06-model: 06-model.o $(FRAMEWORK_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

shaders: trace.spv hit.spv miss.spv

clean:
	@find . -iregex '.*\.\(d\|o\)' -delete
	@find $(FRAMEWORK) -iregex '.*\.\(d\|o\)' -delete
	@rm -rf 06-model
