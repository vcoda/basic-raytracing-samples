include ../Makeshared.mk

default: 03-procedural-intersection shaders

03-procedural-intersection: 03-procedural-intersection.o $(FRAMEWORK_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

shaders: trace.spv miss.spv hit.spv raySphere.spv

clean:
	@find . -iregex '.*\.\(d\|o\)' -delete
	@find $(FRAMEWORK) -iregex '.*\.\(d\|o\)' -delete
	@rm -rf 03-procedural-intersection
