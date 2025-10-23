THIRD_PARTY=third-party
MAGMA=$(THIRD_PARTY)/magma
QUADRIC=$(THIRD_PARTY)/quadric

basic-raytracing-samples:
	$(MAKE) -C $(MAGMA)
	$(MAKE) -C 01-triangle
	$(MAKE) -C 02-transform
	$(MAKE) -C 03-procedural-intersection
	$(MAKE) -C 04-texture-alpha
	$(MAKE) -C 05-mesh
	$(MAKE) -C 06-model
	$(MAKE) -C 07-texture-mapping
	$(MAKE) -C 08-shader-binding-table
	$(MAKE) -C 09-ray-differentials

magma:
	$(MAKE) -C $(MAGMA)

clean:
	$(MAKE) -C $(MAGMA) clean
	$(MAKE) -C 01-triangle clean
	$(MAKE) -C 02-transform clean
	$(MAKE) -C 03-procedural-intersection clean
	$(MAKE) -C 04-texture-alpha clean
	$(MAKE) -C 05-mesh clean
	$(MAKE) -C 06-model clean
	$(MAKE) -C 07-texture-mapping clean
	$(MAKE) -C 08-shader-binding-table clean
	$(MAKE) -C 09-ray-differentials clean
