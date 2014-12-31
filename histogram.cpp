/******************************************************************************

 * * Filename: 		histogram.cpp
 * * Description: 	OpenCL version (include kernel function)
 * * 
 * * Version: 		2.0
 * * Created:		2014/12/22
 * * Revision:		2014/12/30
 * * Compiler: 		g++
 * *
 * * Author: 		Wayne
 * * Organization:	CoDesign 

 ******************************************************************************/

#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <limits>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

#define MAX_SIZE 1000000

unsigned int image[MAX_SIZE];
unsigned int histogram_results[256*3];

const char *histogramKernelSource = 
	"__kernel void histogram(__global unsigned int *image,							\n" \
	"						__global unsigned int *histogram_results,				\n" \
	"						__const unsigned int n)									\n" \
	"{																				\n" \
	"	unsigned int global_id = get_global_id(0);									\n" \
	"																				\n" \
	"	histogram_results[global_id] = 0;											\n" \
	"	barrier(CLK_LOCAL_MEM_FENCE);												\n" \
	"																				\n" \
	"	while(global_id < n)														\n" \
	"	{																			\n" \
	"		atom_inc(histogram_results + image[global_id] + (global_id%3) * 256);	\n" \
	"		global_id += get_global_size(0);										\n" \
	"	}																			\n" \
	"}																				\n" \
	"\n";

cl_program load_program(cl_context context);

int main(int argc, char const *argv[])
{
	unsigned int i, a, input_size;

	// OpenCL
	cl_context context;
	cl_command_queue queue;
	cl_program program;
	cl_kernel histogram;
	size_t cb;
	cl_int err;
	cl_uint num;
	cl_uint num_devices;
	std::string devname;
	std::string devver;
	size_t global_work_size = 1024;
	size_t local_work_size = 256;

	// get the number of supporting OpenCL platforms
	err = clGetPlatformIDs(0, 0, &num);
	if(err != CL_SUCCESS)
	{
		std::cerr << "Unable to get platforms\n";
		return 0;
	}

	// create a vector to store the platforms id
	std::vector<cl_platform_id> platforms(num);
	err = clGetPlatformIDs(num, &platforms[0], &num);
	if(err != CL_SUCCESS)
	{
		std::cerr << "Unable to get platform ID\n";
		return 0;
	}

	// create an OpenCL context
	cl_context_properties prop[] = { CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(platforms[0]), 0 };
	context = clCreateContextFromType(prop, CL_DEVICE_TYPE_DEFAULT, NULL, NULL, NULL);
	if(context == 0)
	{
		std::cerr << "Can't create OpenCL context\n";
		return 0;
	}

	// get list of context info
	clGetContextInfo(context, CL_CONTEXT_DEVICES, 0, NULL, &cb);
	std::vector<cl_device_id> devices(cb / sizeof(cl_device_id));
	clGetContextInfo(context, CL_CONTEXT_DEVICES, cb, &devices[0], 0);

	// construct command queue
	queue = clCreateCommandQueue(context, devices[0], 0, 0);
	if(queue == 0)
	{
		std::cerr << "Can't create command queue\n";
		clReleaseContext(context);
		return 0;
	}

	// get input
	std::fstream inFile("input", std::ios_base::in);
	std::ofstream outFile("output", std::ios_base::out);

	inFile >> input_size;
	i = 0;
	while( inFile >> a )
		image[i++] = a;

	// create cl buffers
	cl_mem cl_image = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(unsigned int) * input_size, &image[0], NULL);
	cl_mem cl_histogram_results = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned int) * 256 * 3, NULL, NULL);
	if(cl_image == 0 || cl_histogram_results == 0)
	{
		std::cerr << "Can't create OpenCL buffer\n";
		clReleaseCommandQueue(queue);
		clReleaseContext(context);
		return 0;
	}

	// create and compile the program object
	program = load_program(context);
	if(program == 0)
	{
		std::cerr << "Can't load or build program\n";
		clReleaseMemObject(cl_image);
		clReleaseMemObject(cl_histogram_results);
		clReleaseCommandQueue(queue);
		clReleaseContext(context);
		return 0;
	}

	// create kernel objects from compiled program
	histogram = clCreateKernel(program, "histogram", NULL);
	if(histogram == 0)
	{
		std::cerr << "Can't load kernel histogram\n";
		clReleaseProgram(program);
		clReleaseMemObject(cl_image);
		clReleaseMemObject(cl_histogram_results);
		clReleaseCommandQueue(queue);
		clReleaseContext(context);
		return 0;
	}
	clSetKernelArg(histogram, 0, sizeof(cl_mem), &cl_image);
	clSetKernelArg(histogram, 1, sizeof(cl_mem), &cl_histogram_results);
	clSetKernelArg(histogram, 2, sizeof(unsigned int), &input_size);

	// enqueue kernel histogram
	err = clEnqueueNDRangeKernel(queue, histogram, 1, NULL, &global_work_size, &local_work_size, 0, NULL, NULL);
	if(err == CL_SUCCESS);
	else
		std::cerr << "Can't enqueue kernel histogram\n";

	clEnqueueReadBuffer(queue, cl_histogram_results, CL_TRUE, 0, sizeof(unsigned int) * 256 * 3, &histogram_results[0], 0, NULL, NULL);

	for(i = 0; i < 256 * 3; ++i)
	{
		if(i % 256 == 0 && i != 0)
			outFile << std::endl;
		outFile << histogram_results[i] << ' ';
	}

	inFile.close();
	outFile.close();

	clReleaseKernel(histogram);
	clReleaseProgram(program);
	clReleaseMemObject(cl_image);
	clReleaseMemObject(cl_histogram_results);
	clReleaseCommandQueue(queue);
	clReleaseContext(context);

	return 0;
}

cl_program load_program(cl_context context)
{
	// create and build program object
	cl_program program = clCreateProgramWithSource(context, 1, (const char **) &histogramKernelSource, NULL, NULL);
	if(program == 0)
		return 0;

	// compile program
	if(clBuildProgram(program, 0, NULL, NULL, NULL, NULL) != CL_SUCCESS)
		return 0;

	return program;
}
