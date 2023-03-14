#include "opencv2/opencv.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgcodecs/imgcodecs.hpp"
#include "opencv2/video/video.hpp"
#include <iostream>
#include <cmath>
#include<vector>

#include "xcl2.hpp"

int main(int argc, char** argv) {
	if (argc != 3) {
		fprintf(stderr, "Usage : <executable>  <xclbin> <input image>\n");
		return -1;
	}
    std::string imagePath = argv[2];	
    std::string binaryFile = argv[1];
    cv::Mat img;

    // reading in the grey image
    img = cv::imread(imagePath, 0);

    if (!img.data) {
        return -1;
    }

    int in_width = img.cols;
    int in_height = img.rows;
    int stride=img.cols;
    std::cout << "Input image height : " << in_height << std::endl;
    std::cout << "Input image width  : " << in_width << std::endl;

// OpenCL section:
    int nKeyPoints = 5000;
    size_t image_in_size_bytes = in_height * in_width * 1 * sizeof(unsigned char);
    size_t keyPoint_bytes = nKeyPoints* sizeof( int);
    std::vector< int> xKeys(nKeyPoints);
    std::vector<unsigned char> outImg(in_width*in_height);
//Declaration of variables that needs to be mapped from kernel to host
    std::vector< int> yKeys(nKeyPoints);
    std::vector< int> xKeys2(nKeyPoints);
    std::vector< int> xKeys3(nKeyPoints);
    std::vector< int> yKeys2(nKeyPoints);
    std::vector< int> yKeys3(nKeyPoints);
    cl_int err;
    std::cout << "INFO: Running OpenCL section." << std::endl;

    // Get the device:
    std::vector<cl::Device> devices = xcl::get_xil_devices();
    auto device = devices[0];

    // Context, command queue and device name:
    OCL_CHECK(err, cl::Context context(device, NULL, NULL, NULL, &err));
    OCL_CHECK(err, cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err));
    OCL_CHECK(err, std::string device_name = device.getInfo<CL_DEVICE_NAME>(&err));

    std::cout << "INFO: Device found - " << device_name << std::endl;

    // Load binary:
    std::cout << "Loading: '" << binaryFile << "'\n";
    std::ifstream bin_file(binaryFile, std::ifstream::binary);
    bin_file.seekg (0, bin_file.end);
    unsigned nb = bin_file.tellg();
    bin_file.seekg (0, bin_file.beg);
    char *buf = new char [nb];
    bin_file.read(buf, nb);
    
    // Creating Program from Binary File
    cl::Program::Binaries bins;
    bins.push_back({buf,nb});
    devices.resize(1);
    OCL_CHECK(err, cl::Program program(context, {device}, bins, NULL, &err));

    // Create a kernel:
    OCL_CHECK(err, cl::Kernel krnl(program, "mp_fast", &err));
    OCL_CHECK(err, cl::Kernel krnl2(program, "mp_fast", &err));
    OCL_CHECK(err, cl::Kernel krnl3(program, "mp_fast", &err));

    // Allocate the buffers:
    std::vector<cl::Memory> inBufVec, outBufVec;
    OCL_CHECK(err, cl::Buffer imageToDevice(context, CL_MEM_READ_ONLY, image_in_size_bytes, NULL, &err));
    OCL_CHECK(err, cl::Buffer imageToDevice2(context, CL_MEM_READ_ONLY, image_in_size_bytes, NULL, &err));
    OCL_CHECK(err, cl::Buffer imageToDevice3(context, CL_MEM_READ_ONLY, image_in_size_bytes, NULL, &err));
    OCL_CHECK(err, cl::Buffer xKeyPoint(context, CL_MEM_WRITE_ONLY, keyPoint_bytes, NULL, &err));
    OCL_CHECK(err, cl::Buffer xKeyPoint2(context, CL_MEM_WRITE_ONLY, keyPoint_bytes, NULL, &err));
    OCL_CHECK(err, cl::Buffer xKeyPoint3(context, CL_MEM_WRITE_ONLY, keyPoint_bytes, NULL, &err));
    OCL_CHECK(err, cl::Buffer yKeyPoint(context, CL_MEM_WRITE_ONLY, keyPoint_bytes, NULL, &err));
    OCL_CHECK(err, cl::Buffer yKeyPoint2(context, CL_MEM_WRITE_ONLY, keyPoint_bytes, NULL, &err));
    OCL_CHECK(err, cl::Buffer yKeyPoint3(context, CL_MEM_WRITE_ONLY, keyPoint_bytes, NULL, &err));

    // Deleted outImage
    // Set the kernel arguments
    OCL_CHECK(err, err = krnl.setArg(0, imageToDevice));
    OCL_CHECK(err, err = krnl.setArg(1, in_height));
    OCL_CHECK(err, err = krnl.setArg(2, in_width));
    OCL_CHECK(err, err = krnl.setArg(3, stride));
    OCL_CHECK(err, err = krnl.setArg(4, xKeyPoint));
    OCL_CHECK(err, err = krnl.setArg(5, yKeyPoint));

    //set the Kernel arguments for krnl2
    OCL_CHECK(err, err = krnl2.setArg(0, imageToDevice2));
    OCL_CHECK(err, err = krnl2.setArg(1, in_height));
    OCL_CHECK(err, err = krnl2.setArg(2, in_width));
    OCL_CHECK(err, err = krnl2.setArg(3, stride));
    OCL_CHECK(err, err = krnl2.setArg(4, xKeyPoint2));
    OCL_CHECK(err, err = krnl2.setArg(5, yKeyPoint2));

    //set the Kernel arguments for krnl3
    OCL_CHECK(err, err = krnl3.setArg(0, imageToDevice3));
    OCL_CHECK(err, err = krnl3.setArg(1, in_height));
    OCL_CHECK(err, err = krnl3.setArg(2, in_width));
    OCL_CHECK(err, err = krnl3.setArg(3, stride));
    OCL_CHECK(err, err = krnl3.setArg(4, xKeyPoint3));
    OCL_CHECK(err, err = krnl3.setArg(5, yKeyPoint3));
    /* Copy input vectors to memory */
    OCL_CHECK(err, q.enqueueWriteBuffer(imageToDevice,       // buffer on the FPGA
                                        CL_TRUE,             // blocking call
                                        0,                   // buffer offset in bytes
                                        image_in_size_bytes, // Size in bytes
                                        img.data));          // Pointer to the data to copy

    OCL_CHECK(err, q.enqueueWriteBuffer(imageToDevice2,       // buffer on the FPGA
                                        CL_TRUE,             // blocking call
                                        0,                   // buffer offset in bytes
                                        image_in_size_bytes, // Size in bytes
                                        img.data));          // Pointer to the data to copy
    OCL_CHECK(err, q.enqueueWriteBuffer(imageToDevice3,       // buffer on the FPGA
                                        CL_TRUE,             // blocking call
                                        0,                   // buffer offset in bytes
                                        image_in_size_bytes, // Size in bytes
                                        img.data));          // Pointer to the data to copy

    // Profiling Objects
    cl_ulong start = 0;
    cl_ulong end = 0;
    double diff_prof = 0.0f;
    cl::Event event_sp;
    cl::Event event_sp1;
    cl::Event event_sp2;

    // Execute the kernel:
    OCL_CHECK(err, err = q.enqueueTask(krnl, NULL, &event_sp));
    OCL_CHECK(err, err = q.enqueueTask(krnl2, NULL, &event_sp1));
    OCL_CHECK(err, err = q.enqueueTask(krnl3, NULL, &event_sp2));

    clWaitForEvents(1, (const cl_event*)&event_sp);
    clWaitForEvents(1, (const cl_event*)&event_sp1);
    clWaitForEvents(1, (const cl_event*)&event_sp2);

    event_sp.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
    event_sp.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
    diff_prof = end - start;
    std::cout<<"kernel 1 time"<<(diff_prof / 1000000) << "ms" <<std::endl;
    event_sp1.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
    event_sp1.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
    diff_prof = end - start;
    std::cout<<"kernel 2 time"<<(diff_prof / 1000000) << "ms" <<std::endl;
    event_sp2.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
    event_sp2.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
    diff_prof = end - start;
    std::cout<<"kernel 2 time"<<(diff_prof / 1000000) << "ms" <<std::endl;
    // diff_prof = end - start;
    //::cout << (diff_prof / 1000000) << "ms" << std::endl;



    // Copying Device result data to Host memory
    OCL_CHECK(err, q.enqueueReadBuffer(xKeyPoint,
                                       CL_TRUE,
                                       0,
									   keyPoint_bytes,
									   xKeys.data()));
    OCL_CHECK(err, q.enqueueReadBuffer(yKeyPoint,
                                       CL_TRUE,
                                       0,
									   keyPoint_bytes,
									   yKeys.data()));
    OCL_CHECK(err, q.enqueueReadBuffer(xKeyPoint2,
                                       CL_TRUE,
                                       0,
									   keyPoint_bytes,
									   xKeys2.data()));
    OCL_CHECK(err, q.enqueueReadBuffer(yKeyPoint2,
                                       CL_TRUE,
                                       0,
									   keyPoint_bytes,
									   yKeys2.data()));
    OCL_CHECK(err, q.enqueueReadBuffer(xKeyPoint3,
                                       CL_TRUE,
                                       0,
									   keyPoint_bytes,
									   xKeys3.data()));
    OCL_CHECK(err, q.enqueueReadBuffer(yKeyPoint3,
                                       CL_TRUE,
                                       0,
									   keyPoint_bytes,
									   yKeys3.data()));
    /* OCL_CHECK(err, q.enqueueReadBuffer(yKeyPoint,
                                       CL_TRUE,
                                       0,
                                       keyPoint_bytes,
                                       yKeyStore));*/


    q.finish();
/////////////////////////////////////// end of CL
//////////////////////////////////////////

    std::cout<<"first 20 key points of kernel 1"<<std::endl;
    for (size_t i = 0; i < 20; ++i)
    {
	    std::cout<<xKeys[i]<<","<<yKeys[i]<<std::endl;
    }
    std::cout<<"--------FINISH!---------\n";
    return 0;
}

