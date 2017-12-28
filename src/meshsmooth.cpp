#define OCL_PLATFORM 0
#define OCL_DEVICE 0

#define DRAGON "res/dragon.obj"
#define CUBE "res/cube_example.obj"
#define HUMAN "res/human.obj"
#define HEAD "res/head.obj"
#define SUZANNE "res/suzanne.obj"

#include "ocl_boiler.h"
#include <iostream>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/gtx/string_cast.hpp>
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>

#define OCL_FILENAME "src/meshsmooth.ocl"

size_t preferred_wg_init;

void readOBJFile(std::string path, int &nels, float *& vertex4Array, unsigned int* &adjArray, size_t &adjmemsize){
	std::vector< glm::vec3 > obj_vertexArray;
	std::vector< unsigned int > faceVertexIndices;

	FILE * file = fopen(path.c_str(), "r");
	if( file == NULL ){
		printf("Impossible to open the file !\n");
		return;
	}
	while( 1 ){ // parsing
		char lineHeader[128];
		// read the first word of the line
		int res = fscanf(file, "%s", lineHeader);
		if (res == EOF) break; // EOF = End Of File. Quit the loop.
		// else : parse lineHeader
		if ( strcmp( lineHeader, "v" ) == 0 ){
			glm::vec3 vertex;
			fscanf(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z );
			obj_vertexArray.push_back(vertex);
		}
		else if ( strcmp( lineHeader, "f" ) == 0 ){
			unsigned int faceVertexIndex[3], faceUvIndex[3], faceNormalIndex[3];
			if(path == HUMAN || path == HEAD || path == SUZANNE){
				int matches = fscanf(file, "%d//%d %d//%d %d//%d\n", &faceVertexIndex[0], &faceNormalIndex[0], &faceVertexIndex[1], &faceNormalIndex[1], &faceVertexIndex[2], &faceNormalIndex[2] );
				if (matches != 6){
					printf("File can't be read by our simple parser : ( Try exporting with other options\n");
					return;
				}
			} else {
				int matches = fscanf(file, "%d/%d/%d %d/%d/%d %d/%d/%d\n", &faceVertexIndex[0], &faceUvIndex[0], &faceNormalIndex[0], &faceVertexIndex[1], &faceUvIndex[1], &faceNormalIndex[1], &faceVertexIndex[2], &faceUvIndex[2], &faceNormalIndex[2] );
				if (matches != 9){
					printf("File can't be read by our simple parser : ( Try exporting with other options\n");
					return;
				}				
			}
			faceVertexIndices.push_back(faceVertexIndex[0]);
			faceVertexIndices.push_back(faceVertexIndex[1]);
			faceVertexIndices.push_back(faceVertexIndex[2]);
		}
	}
	
	// Init number of vertex
	nels = obj_vertexArray.size();
	
	// Discover adjacents vertex for each vertex
	std::vector< unsigned int >* adjacents = new std::vector< unsigned int >[nels];

	for(int i=0; i<faceVertexIndices.size(); i+=3){
		unsigned int vertexID1 = faceVertexIndices[i] - 1;
		unsigned int vertexID2 = faceVertexIndices[i+1] - 1;
		unsigned int vertexID3 = faceVertexIndices[i+2] - 1;

		//glm::vec3 vertex1 = obj_vertexArray[ vertexID1 ];
		//glm::vec3 vertex2 = obj_vertexArray[ vertexID2 ];
		//glm::vec3 vertex3 = obj_vertexArray[ vertexID3 ];

		std::vector< unsigned int >* adjacent1 = &adjacents[vertexID1];
		std::vector< unsigned int >* adjacent2 = &adjacents[vertexID2];
		std::vector< unsigned int >* adjacent3 = &adjacents[vertexID3];
		
		if (std::find(adjacent1->begin(), adjacent1->end(), vertexID2) == adjacent1->end())
			adjacent1->push_back(vertexID2);
		if (std::find(adjacent1->begin(), adjacent1->end(), vertexID3) == adjacent1->end())
			adjacent1->push_back(vertexID3);

		if (std::find(adjacent2->begin(), adjacent2->end(), vertexID1) == adjacent2->end())
			adjacent2->push_back(vertexID1);
		if (std::find(adjacent2->begin(), adjacent2->end(), vertexID3) == adjacent2->end())
			adjacent2->push_back(vertexID3);

		if (std::find(adjacent3->begin(), adjacent3->end(), vertexID1) == adjacent3->end())
			adjacent3->push_back(vertexID1);
		if (std::find(adjacent3->begin(), adjacent3->end(), vertexID2) == adjacent3->end())
			adjacent3->push_back(vertexID2);
	}

	// Init vertex4Array
	vertex4Array = new float[4*nels];
	int currentAdjIndex = 0;
	//int maxadj = 0;
	
	for(int i=0; i<nels; i++) {
		glm::vec3 *vertex = &obj_vertexArray[i];
		vertex4Array[4*i] = vertex->x;
		vertex4Array[4*i+1] = vertex->y;
		vertex4Array[4*i+2] = vertex->z;

		unsigned int* adjIndexPtr = (unsigned int*)(&vertex4Array[4*i+3]);
		*adjIndexPtr = ((unsigned int)currentAdjIndex)<<6;
		*adjIndexPtr += (((unsigned int)adjacents[i].size())<<26)>>26;

		//printf("indexOfAdjs  ->  %d\n", (*adjIndexPtr)>>8);
		//printf("numOfAdjs  ->  %d\n", ((*adjIndexPtr)<<24)>>24);

		//if(adjacents[i].size() > maxadj) maxadj = adjacents[i].size();
		currentAdjIndex += adjacents[i].size();
	}
	//std::cout << "############# " << currentAdjIndex << std::endl;
	//std::cout << "############# " << maxadj << std::endl << std::endl;
	
	// Now, currentAdjIndex is the tolal adjacents numbers.
	adjmemsize = currentAdjIndex*sizeof(unsigned int);
	adjArray = new unsigned int[currentAdjIndex];
	int adjIndex = 0;
	for(int i=0; i<nels; i++) {
		for( unsigned int vertexIndex : adjacents[i])
			adjArray[adjIndex++] = vertexIndex;
	}	

	#if 0
	for(int i=0; i<obj_vertexArray.size(); i++) {
		std::cout << "adiacenti di " << i << " " << glm::to_string(obj_vertexArray[i]) << " => " << std::endl;

		for(unsigned int adj : adjacents[i])  std::cout << glm::to_string(obj_vertexArray[adj]) << std::endl;	
		std::cout << std::endl;	

	}
	#endif
}

cl_event init(cl_command_queue queue, cl_kernel init_k, cl_mem cl_vertex4Array, cl_mem cl_adjArray, cl_mem cl_result, cl_int nels, cl_float factor) {
	size_t gws[] = { round_mul_up(nels, preferred_wg_init) };
	cl_event init_evt;
	cl_int err;

	printf("init gws: %d | %zu => %zu\n", nels, preferred_wg_init, gws[0]);

	// Setting arguments
	err = clSetKernelArg(init_k, 0, sizeof(cl_vertex4Array), &cl_vertex4Array);
	ocl_check(err, "set init arg 0");
	err = clSetKernelArg(init_k, 1, sizeof(cl_adjArray), &cl_adjArray);
	ocl_check(err, "set init arg 1");
	err = clSetKernelArg(init_k, 2, sizeof(cl_result), &cl_result);
	ocl_check(err, "set init arg 2");
	err = clSetKernelArg(init_k, 3, sizeof(nels), &nels);
	ocl_check(err, "set init arg 3");
	err = clSetKernelArg(init_k, 4, sizeof(factor), &factor);
	ocl_check(err, "set init arg 4");

	err = clEnqueueNDRangeKernel(queue, init_k,
		1, NULL, gws, NULL, /* griglia di lancio */
		0, NULL, /* waiting list */
		&init_evt);
	ocl_check(err, "enqueue kernel init");
	return init_evt;
}

int main(int argc, char *argv[]) {

	int nels;
	float* vertex4Array;
	unsigned int* adjArray;
	size_t adjmemsize;

	// ###################################################################################################################
	readOBJFile(SUZANNE, nels, vertex4Array, adjArray, adjmemsize);
	std::cout<<"OBJ loaded"<<std::endl;
	const size_t memsize = nels*4*sizeof(float);

	// Hic sunt leones
	cl_int err;
	cl_platform_id platformID = select_platform();
	cl_device_id deviceID = select_device(platformID);
	cl_context context = create_context(platformID, deviceID);
	cl_command_queue queue = create_queue(context, deviceID);
	cl_program program = create_program(OCL_FILENAME, context, deviceID);
	
	// Create Buffers
	cl_mem cl_vertex4Array = clCreateBuffer(context, CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, memsize, vertex4Array, &err);
	cl_mem cl_adjArray = clCreateBuffer(context, CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, adjmemsize, adjArray, &err);
	cl_mem cl_result = clCreateBuffer(context, CL_MEM_READ_WRITE, memsize, NULL, &err); // TEST

	// Extract kernels
	cl_kernel init_k = clCreateKernel(program, "init", &err);
	ocl_check(err, "create kernel init");

	// Set preferred_wg size from device info
	err = clGetKernelWorkGroupInfo(init_k, deviceID, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(preferred_wg_init), &preferred_wg_init, NULL);
	
	cl_event init_evt = init(queue, init_k, cl_vertex4Array, cl_adjArray, cl_result, nels, 0.5f);
	err = clWaitForEvents(1, &init_evt);
	ocl_check(err, "clWaitForEvents");

	cl_event taubin_evt = init(queue, init_k, cl_result, cl_adjArray, cl_vertex4Array, nels, -0.6f);
	err = clWaitForEvents(1, &taubin_evt);
	ocl_check(err, "clWaitForEvents");
	
	printf("init time:\t%gms\t%gGB/s\n", runtime_ms(init_evt), (2.0*memsize+12*memsize)/runtime_ns(init_evt));
	printf("init time:\t%gms\t%gGB/s\n", runtime_ms(taubin_evt), (2.0*memsize+12*memsize)/runtime_ns(taubin_evt));
	return 0;
}
