#
# this file was created by a computer. trust it.
#

# compiler tools
XILINX_VITIS ?= /home/settysha/Downloads/Vitis/2021.2
XILINX_XRT ?= /opt/xilinx/xrt
XILINX_HLS ?= $(XILINX_VITIS)/Vivado_HLS
XILINX_XRT ?= /opt/xilinx/xrt
OPENCV=/home/settysha/opencv
VITIS_LIBRARIES=/home/settysha/Vitis_github/Vitis_Libraries
TARGET = sw_emu

host_file_input=mp_fast_host.cpp
kernel_file_input=mp_fast.cpp

#host executable
HOST_EXE = mp_fast_v4
xclbin=mp_fast.xclbin


##user needs to change untill here 


#Linker tools
VPP_LINKER ?= ${XILINX_VITIS}/bin/v++

HOST_CXX ?= g++
VPP ?= ${XILINX_VITIS}/bin/v++

RM = rm -f
RMDIR = rm -rf

VITIS_PLATFORM = xilinx_u280_xdma_201920_3
VITIS_PLATFORM_PATH = $(VITIS_PLATFORM)

VPP_OPTS = --target $(TARGET)

# host compiler global settings
CXXFLAGS += -std=c++1y -DVITIS_PLATFORM=$(VITIS_PLATFORM) -D__USE_XOPEN2K8 -I$(OPENCV)/opencv-4.4.0/install/include/opencv4 -I$(OPENCV)/opencv-4.4.0/install/lib -I$(VITIS_LIBRARIES)/vision/ext/xcl2 -I$(XILINX_XRT)/include/ -I$(XILINX_HLS)/2021.2/include/ -O2 -g -Wall -c -fmessage-length=0
LDFLAGS += -lopencv_highgui -lopencv_calib3d -lopencv_videoio -lopencv_flann -lopencv_features2d -lopencv_imgproc -lopencv_core -lopencv_imgcodecs -lopencv_video -lxilinxopencl -lpthread -lrt -lstdc++ -L/usr/local/lib/ -L$(XILINX_XRT)/lib/ -Wl,-rpath-link,$(XILINX_XRT)/lib

#
# host files
#

HOST_OBJECTS += src/host_file.o
HOST_OBJECTS += src/xcl2.o



##kernel files
BUILD_SUBDIRS += build
HW_KERNEL_OBJS += build/kernel_file.xo
SRC_KERNEL_OBJS += build/kernel_file.xo
BUILD_SUBDIRS += src/
ALL_MESSAGE_FILES = $(subst .xo,.o,.mdb,$(SRC_KERNEL_OBJS))
#
##binary containers
BINARY_CONTAINERS += binary_container_1.xclbin
BUILD_SUBDIRS += binary_container_1.build
BINARY_CONTAINER_1_OBJS += build/kernel_file.xo

ALL_MESSAGE_FILES = $(subst .xclbin,.mdb,$(BINARY_CONTAINERS))


# primary build targets
#

.PHONY: all clean
all:  $(HOST_EXE) $(HW_KERNEL_OBJS) $(BINARY_CONTAINERS) package

clean:
	-$(RM) $(HOST_EXE) $(HOST_OBJECTS)
	-$(RM) $(SRC_KERNEL_OBJS) $(ALL_MESSAGE_FILES) 
	-$(RMDIR) $(BUILD_SUBDIRS)
	-$(RMDIR) .Xil
	-$(RM) $(BINARY_CONTAINERS) $(ALL_MESSAGE_FILES) 
	-$(RM) *.xclbin.sh *.xclbin.info *.xclbin.link_summary*
	-$(RMDIR) $(BUILD_SUBDIRS)
	-$(RMDIR) .Xil
	-$(RMDIR) ./package.build
	-${RMDIR} package

.PHONY: incremental
incremental: all


nothing:

#
# host rules
#

src/host_file.o: ../src/$(host_file_input) $(OPENCV)/opencv-4.4.0/install/include/opencv4/opencv2/opencv.hpp $(OPENCV)/opencv-4.4.0/install/include/opencv4/opencv2/imgproc/imgproc.hpp  $(OPENCV)/opencv-4.4.0/install/include/opencv4/opencv2/highgui/highgui.hpp  $(OPENCV)/opencv-4.4.0/install/include/opencv4/opencv2/imgcodecs/imgcodecs.hpp  $(OPENCV)/opencv-4.4.0/install/include/opencv4/opencv2/video/video.hpp $(VITIS_LIBRARIES)/vision/ext/xcl2/xcl2.hpp
	-@mkdir -p $(@D)
	$(HOST_CXX) $(CXXFLAGS) -o "$@" "$<"

src/xcl2.o: ../src/xcl2.cpp $(VITIS_LIBRARIES)/vision/ext/xcl2/xcl2.hpp
	-@mkdir -p $(@D)
	$(HOST_CXX) $(CXXFLAGS) -o "$@" "$<"

$(HOST_EXE): $(HOST_OBJECTS)
	$(HOST_CXX) -o "$@" $(+) $(LDFLAGS)


#
# Hw Kernels compile;
#

build/kernel_file.xo: ../src/$(kernel_file_input) mp_fast-compile.cfg
	-@mkdir -p $(@D)
	-@$(RM) $@
	$(VPP) $(VPP_OPTS) --compile -I"$(<D)" --config mp_fast-compile.cfg -o"$@" "$<"


##binary container generation(xclbin)
ifeq ($(MAKECMDGOALS),incremental)
.PHONY: $(xclbin).xclbin
.PHONY: build/kernel_file.xo
endif

binary_container_1.xclbin: $(BINARY_CONTAINER_1_OBJS) binary_container_1-link.cfg
	-@echo $(VPP_LINKER) $(VPP_OPTS) --link --config binary_container_1-link.cfg -o"$@" $(BINARY_CONTAINER_1_OBJS) > binary_container_1.xclbin.sh
	$(VPP_LINKER) $(VPP_OPTS) --link --config binary_container_1-link.cfg -o"$@" $(BINARY_CONTAINER_1_OBJS)



package:
	-@echo $(VPP) --package --config package.cfg binary_container_1.xclbin -o $(xclbin)
	-@$(VPP) --package --config package.cfg binary_container_1.xclbin -o $(xclbin)


