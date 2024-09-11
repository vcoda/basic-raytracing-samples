# Basic ray-tracing samples using Magma library and Vulkan graphics API

## Cloning

To clone this repository with external submodules:
```
git clone --recursive https://github.com/vcoda/basic-raytracing-samples.git
``` 
or clone this repository only and update submodules manually:
```
git clone https://github.com/vcoda/basic-raytracing-samples.git
cd basic-raytracing-samples
git submodule update --init --recursive
``` 

## Build Tools and SDK

## Windows

* [Microsoft Visual Studio Community](https://www.visualstudio.com/downloads/)<br>
* [Git for Windows](https://git-scm.com/download)<br>
* [LunarG Vulkan SDK](https://www.lunarg.com/vulkan-sdk/)<br>

Check that VK_SDK_PATH environment variable is present after SDK installation:
```
echo %VK_SDK_PATH%
```
Shaders are automatically compiled using glslangValidator as Custom Build Tool.
If you use Visual Studio newer than 2017, change the SDK version in the project property pages or by right-clicking the solution and selecting "Retarget solution".
</br>

## Linux (Ubuntu)

Install GCC and Git (if not available):
```
sudo apt update
sudo apt install gcc
sudo apt install g++
sudo apt install make
sudo apt install git
```
Install XCB headers and libraries:
```
sudo apt install xcb
sudo apt install libxcb-icccm4-dev
```
For Xlib, install X11 headers and libraries (optional):
```
sudo apt install libx11-dev
```

* [LunarG Vulkan SDK](https://www.lunarg.com/vulkan-sdk/)<br>

Go to the directory where .run file was saved:
```
chmod ugo+x vulkansdk-linux-x86_64-<version>.run
./vulkansdk-linux-x86_64-<version>.run
cd VulkanSDK/<version>/
source ./setup-env.sh
```
Check that Vulkan environment variables are present:
```
printenv | grep Vulkan
```

### Systems with AMD graphics hardware

Check whether AMDGPU-PRO stack is installed:
```
dpkg -l amdgpu-pro
```
If not, download and install it from an official web page:

* [AMD Radeon™ Software AMDGPU-PRO Driver for Linux](https://support.amd.com/en-us/kb-articles/Pages/AMDGPU-PRO-Install.aspx)<br>

NOTE: You have to make sure that graphics driver is compatible with current Linux kernel. Some of the driver's libraries are ***compiled*** against installed kernel headers, and if kernel API changed since driver release, compilation will fail and driver become malfunction after reboot. I used a combination of AMDGPU-PRO Driver Version 17.10 and Ubuntu 16.04.2 with kernel 4.8.0-36. Also disable system update as it may upgrade kernel to the version that is incompatible with installed graphics driver. 
Successfull AMDGPU-PRO installation should look like this:
```
Loading new amdgpu-pro-17.10-446706 DKMS files...
First Installation: checking all kernels...
Building only for 4.8.0-36-generic
Building for architecture x86_64
Building initial module for 4.8.0-36-generic
Done.
Forcing installation of amdgpu-pro
```
After reboot check that driver stack is installed:
```
dpkg -l amdgpu-pro
Desired=Unknown/Install/Remove/Purge/Hold
| Status=Not/Inst/Conf-files/Unpacked/halF-conf/Half-inst/trig-aWait/Trig-pend
|/ Err?=(none)/Reinst-required (Status,Err: uppercase=bad)
||/ Name                            Version              Architecture         Description
+++-===============================-====================-====================-====================================================================
ii  amdgpu-pro                      17.10-446706         amd64                Meta package to install amdgpu Pro components.
```
### Systems with Nvidia graphics hardware

TODO

### Hardware Requirements

These demos require GPU with hardware ray-tracing support. Graphics driver and Vulkan SDK should support the following extensions:

* VK_KHR_acceleration_structure
* VK_KHR_buffer_device_address
* VK_EXT_descriptor_indexing
* VK_KHR_ray_query
* VK_KHR_ray_tracing_pipeline
* VK_KHR_spirv_1_4

### Building

To build all samples, go to the repo root directory and run make script:
```
make -j<N>
```
where N is the number of threads to run with multi-threaded compilation. First, magma and quadric libraries will be built, then samples.
To build a particular sample, you can build dependencies separately, then go to the sample's directory and run make script:
```
cd 01-triangle
make
./01-triangle
```
There is debug build by default. For release build, set DEBUG variable to 0, e. g.:
```
make magma DEBUG=0 -j<N>
```

## Samples

### [01 - Hello, triangle!](01-triangle/)
<img src="./screenshots/01.png" height="128px" align="left">
This very first sample renders a triangle using hardware ray-tracing instead of conventional rasterization.
It shows how to allocate top- and bottom-level acceleration structures, build them on device, and how to run 
ray-tracing shader to find closest intersection of ray with geometry.

### [02 - Perspective transformation](02-transform/)
<img src="./screenshots/02.png" height="128px" align="left">
Calculates origin and direction of the ray using inverted view and projection matrices. Triangle's world transformation
stored in the instance buffer bound to top-level acceleration structure. To apply transformation, acceleration structure 
is updated every frame.

## Credits
This framework uses a few third-party libraries:

* [Microsoft DirectXMath](https://github.com/Microsoft/DirectXMath)<br>
  Linear algebra library with fantastic CPU optimizations using SSE2/SSE3/SSE4/AVX/AVX2 intrinsics.<br>
  I wrote a simple [wrapper](https://github.com/vcoda/rapid) over it to make its usage more OOP friendly.

* [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)<br>
  Tiny but powerful single file wavefront obj loader by Syoyo Fujita.

* [stb](https://github.com/nothings/stb)<br>
  Sean T. Barrett's single-header image loading library.
