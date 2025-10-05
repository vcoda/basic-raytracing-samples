include ../Makeshared.mk

default: 05-mesh shaders

05-mesh: 05-mesh.o $(FRAMEWORK_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

shaders: trace.spv hit.spv miss.spv

clean:
	@find . -iregex '.*\.\(d\|o\)' -delete
	@find $(FRAMEWORK) -iregex '.*\.\(d\|o\)' -delete
	@rm -rf 05-mesh
