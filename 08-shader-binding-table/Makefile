include ../Makeshared.mk

default: 08-shader-binding-table shaders

08-shader-binding-table: 08-shader-binding-table.o $(FRAMEWORK_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

shaders: normal.spv lambert.spv diffuse.spv phong.spv trace.spv miss.spv

clean:
	@find . -iregex '.*\.\(d\|o\)' -delete
	@find $(FRAMEWORK) -iregex '.*\.\(d\|o\)' -delete
	@rm -rf 08-shader-binding-table
