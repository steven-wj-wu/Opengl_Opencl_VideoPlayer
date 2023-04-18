#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <windows.h>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <stdio.h>
#include <stdlib.h>
#include <CL/cl.h>
#include <CL/cl_gl.h>
#include <fstream>

#define BUFFER_NUM 2
const char* oclErrorString(cl_int error);
#define checkError(func, err) \
do { \
if (err != CL_SUCCESS) { \
    printf(func ": %s\n", oclErrorString(err)); \
    fgetc(stdin); \
    exit(0); \
}\
} while (0)\

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
    //#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
//#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
//#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
}

const char* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"layout (location = 1) in vec3 aColor;\n"
"layout (location = 2) in vec2 aTexCoord\n;"
"out vec3 ourColor;\n"
"out vec2 TexCoord;\n"
"void main()\n"
"{\n"
"  gl_Position = vec4(aPos, 1.0);\n"
"ourColor = aColor;\n"
"TexCoord = vec2(aTexCoord.x, aTexCoord.y)\n;"
"}\0";

const char* fragmentShaderSource = "#version 330 core\n"
"out vec4 FragColor;\n"
"in vec3 ourColor;\n"
"in vec2 TexCoord;\n"
"uniform sampler2D texture1;\n"
"void main()\n"
"{\n"
"   FragColor = texture(texture1, TexCoord);\n"
"}\n\0";

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

struct ReadBuffer {

    int index = 0;
    bool read = 0;
    bool image_write = 1;
    bool gpu_write = 0;
    AVFrame* codec_buffer;
    AVFrame* frame_buffer;
    uint8_t* internal_buffer;
    cl_mem data;
    AVFrame* tmp_frame = NULL;
    struct ReadBuffer* next_buffer;

};
typedef struct ReadBuffer buffer;
int buffer_num = BUFFER_NUM;
buffer buffer0, buffer1, buffer2, buffer3;
buffer buffer_head;
buffer* write_ptr = &buffer_head;
buffer* gpu_ptr = &buffer_head;
buffer* read_ptr = &buffer_head;

typedef struct {

    AVFormatContext* fmt_ctx;
    int stream_idx;
    AVStream* video_stream;
    AVCodecContext* codec_ctx;
    AVCodecParameters* codec_ctxpar;
    const AVCodec* decoder;
    AVPacket* packet;
    struct SwsContext* conv_ctx;
    enum AVHWDeviceType type;
}VideoData;
//static enum AVPixelFormat hw_pix_fmt;
//static AVBufferRef* hw_device_ctx = NULL;
int height, width;
void initialVideoData(VideoData* data);
void checkFile(const char* VideoPath, VideoData* data);
//static enum AVPixelFormat get_hw_format(AVCodecContext* ctx,
    //const enum AVPixelFormat* pix_fmts);
//static int hw_decoder_init(AVCodecContext* ctx, const enum AVHWDeviceType type);
void clearVideoData(VideoData* data);
void set_decoder(VideoData* data);
void set_frame_buffer(VideoData* data);


cl_context context = 0;
cl_command_queue command_queue = 0;
cl_program program = 0;
cl_device_id device = 0;
cl_kernel kernel = 0;
cl_mem memObject[3] = { 0,0,0 };
cl_int err_num;
cl_int err, err_d;
cl_event event;
cl_mem cl_read = 0;
cl_mem cl_write = 0;
const char* oclErrorString(cl_int error)
{
    static const char* errorString[] = {
        "CL_SUCCESS",
        "CL_DEVICE_NOT_FOUND",
        "CL_DEVICE_NOT_AVAILABLE",
        "CL_COMPILER_NOT_AVAILABLE",
        "CL_MEM_OBJECT_ALLOCATION_FAILURE",
        "CL_OUT_OF_RESOURCES",
        "CL_OUT_OF_HOST_MEMORY",
        "CL_PROFILING_INFO_NOT_AVAILABLE",
        "CL_MEM_COPY_OVERLAP",
        "CL_IMAGE_FORMAT_MISMATCH",
        "CL_IMAGE_FORMAT_NOT_SUPPORTED",
        "CL_BUILD_PROGRAM_FAILURE",
        "CL_MAP_FAILURE",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "CL_INVALID_VALUE",
        "CL_INVALID_DEVICE_TYPE",
        "CL_INVALID_PLATFORM",
        "CL_INVALID_DEVICE",
        "CL_INVALID_CONTEXT",
        "CL_INVALID_QUEUE_PROPERTIES",
        "CL_INVALID_COMMAND_QUEUE",
        "CL_INVALID_HOST_PTR",
        "CL_INVALID_MEM_OBJECT",
        "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR",
        "CL_INVALID_IMAGE_SIZE",
        "CL_INVALID_SAMPLER",
        "CL_INVALID_BINARY",
        "CL_INVALID_BUILD_OPTIONS",
        "CL_INVALID_PROGRAM",
        "CL_INVALID_PROGRAM_EXECUTABLE",
        "CL_INVALID_KERNEL_NAME",
        "CL_INVALID_KERNEL_DEFINITION",
        "CL_INVALID_KERNEL",
        "CL_INVALID_ARG_INDEX",
        "CL_INVALID_ARG_VALUE",
        "CL_INVALID_ARG_SIZE",
        "CL_INVALID_KERNEL_ARGS",
        "CL_INVALID_WORK_DIMENSION",
        "CL_INVALID_WORK_GROUP_SIZE",
        "CL_INVALID_WORK_ITEM_SIZE",
        "CL_INVALID_GLOBAL_OFFSET",
        "CL_INVALID_EVENT_WAIT_LIST",
        "CL_INVALID_EVENT",
        "CL_INVALID_OPERATION",
        "CL_INVALID_GL_OBJECT",
        "CL_INVALID_BUFFER_SIZE",
        "CL_INVALID_MIP_LEVEL",
        "CL_INVALID_GLOBAL_WORK_SIZE",
    };

    const int errorCount = sizeof(errorString) / sizeof(errorString[0]);

    const int index = -error;

    return (index >= 0 && index < errorCount) ? errorString[index] : "";

}
char* ReadKernelSourceFile(const char* filename, size_t* length);
cl_context CreateContext(cl_device_id* device);
cl_command_queue CreateCommandQueue(cl_context context, cl_device_id device);
cl_program CreateProgram(cl_context context, cl_device_id device, const char* filename);
void CleanUp(cl_context context, cl_command_queue command_queue, cl_program program, cl_kernel kernel, cl_mem memObjects[3]);

//void read(VideoData* data);
std::condition_variable cond_var;
std::mutex m;
void read_multi_buffer(VideoData* data);
void gpu_write_muilti_buffer(VideoData* data);
void clearImageBuffer(ReadBuffer* head);

double time_decode, time_transfer, time_write, time_swap;
bool stop = 1;
VideoData  my_video;

std::ofstream ofs;

int main()
{
    time_decode = 0;
    time_transfer = 0;
    time_write = 0;
    time_swap = 0;



    avdevice_register_all();
    avformat_network_init();
    initialVideoData(&my_video);
    checkFile("4k output sbs-6.avi", &my_video);
    //checkFile("battle.mp4", &my_video);
    //checkFile("sample_6.mp4", &my_video);
    set_decoder(&my_video);
    my_video.packet = (AVPacket*)av_malloc(sizeof(AVPacket));
    //set_frame_buffer(&my_video);

    height = my_video.codec_ctx->height;
    width = my_video.codec_ctx->width;


    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    GLFWwindow* window = glfwCreateWindow(width, height, "CLG Player", glfwGetPrimaryMonitor(), NULL);
    //GLFWwindow* window = glfwCreateWindow(width, height, "CLG Player", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); //vsync
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // build and compile our shader program
    // vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // link shaders
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);


    // set up vertex data (and buffer(s)) and configure vertex attributes
    float vertices[] = {
        // positions          // colors           // texture coords
         1.0f,  1.0f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 0.0f, // top right
         1.0f, -1.0f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 1.0f, // bottom right
         -1.0f, -1.0f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f, // bottom left
        -1.0f,  1.0f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 0.0f  // top left 
    };
    unsigned int indices[] = {
    0, 1, 3, // first triangle
    1, 2, 3  // second triangle
    };

    GLuint PBO_dst, PBO_src;
    GLuint texture_src, texture_dst;
    unsigned int buffer_size = (width * height * 3) * sizeof(GLubyte);
    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // texture coord attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glGenBuffers(1, &PBO_src);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, PBO_src);
    glBufferData(GL_PIXEL_PACK_BUFFER, buffer_size, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    glGenTextures(1, &texture_dst);
    glBindTexture(GL_TEXTURE_2D, texture_dst);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenBuffers(1, &PBO_dst);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, PBO_dst);
    glBufferData(GL_PIXEL_PACK_BUFFER, buffer_size, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO_dst);
    glBindTexture(GL_TEXTURE_2D, texture_dst);


    /*
    size_t maxWorkGroupSize;
    clGetKernelWorkGroupInfo(kernel,
        device,
        CL_KERNEL_WORK_GROUP_SIZE,
        sizeof(size_t),
        &maxWorkGroupSize,
        NULL);
    std::cout << "MAX GROUP SIZE:" << maxWorkGroupSize << std::endl;
    */

    context = CreateContext(&device);
    if (context == NULL) {
        printf("context build fail\n");
        CleanUp(context, command_queue, program, kernel, memObject);
        return 1;
    }
    command_queue = CreateCommandQueue(context, device);
    if (command_queue == NULL) {
        printf("command queue build fail\n");
        CleanUp(context, command_queue, program, kernel, memObject);
        return 1;
    }
    program = CreateProgram(context, device, "MyCL.cl");
    if (program == NULL) {
        CleanUp(context, command_queue, program, kernel, memObject);
        return 1;
    }
    kernel = clCreateKernel(program, "pixel_extract", NULL);
    if (kernel == NULL) {
        printf("fail to build kernel\n");
        CleanUp(context, command_queue, program, kernel, memObject);
        return 1;
    }


    int RGBFramesize;
    RGBFramesize = av_image_get_buffer_size(AV_PIX_FMT_BGR24, width, height, 1);
    unsigned char* gl_texture_bytes = (unsigned char*)malloc(sizeof(unsigned char) * height * width * 3);

    buffer_head.index = 0;
    buffer_head.data = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(unsigned char) * height * width * 3, gl_texture_bytes, NULL);
    buffer_head.frame_buffer = av_frame_alloc();
    buffer_head.codec_buffer = av_frame_alloc();
    buffer_head.tmp_frame = av_frame_alloc();
    buffer_head.internal_buffer = (uint8_t*)av_malloc(RGBFramesize * sizeof(uint8_t));
    av_image_fill_arrays(buffer_head.frame_buffer->data, buffer_head.frame_buffer->linesize, buffer_head.internal_buffer, AV_PIX_FMT_BGR24, width, height, 1);
    buffer_head.next_buffer = NULL;
    buffer* current = &buffer_head;

    for (int i = 0; i < buffer_num - 1; i++) {
        current->next_buffer = new ReadBuffer();
        current = current->next_buffer;
        current->index = i + 1;
        current->data = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(unsigned char) * height * width * 3, gl_texture_bytes, NULL);
        current->frame_buffer = av_frame_alloc();
        current->codec_buffer = av_frame_alloc();
        current->tmp_frame = av_frame_alloc();
        current->internal_buffer = (uint8_t*)av_malloc(RGBFramesize * sizeof(uint8_t));
        av_image_fill_arrays(current->frame_buffer->data, current->frame_buffer->linesize, current->internal_buffer, AV_PIX_FMT_BGR24, width, height, 1);
        current->next_buffer = NULL;
    }
    current->next_buffer = &buffer_head;

    cl_write = clCreateFromGLBuffer(context, CL_MEM_WRITE_ONLY, PBO_dst, &err);
    checkError("clCreateFromGLBuffer(dest)", err);
    size_t global_item_size[] = { width , height };
    size_t local_item_size[] = { 32,8 }; //256
    err = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&cl_write);
    checkError("clSetKernelArg1", err);
    err = clSetKernelArg(kernel, 2, sizeof(width), (void*)&width);
    checkError("clSetKernelArg1", err);
    err = clEnqueueAcquireGLObjects(command_queue, 1, &cl_write, 0, NULL, NULL);
    checkError("acquireGLObjects", err);

    std::thread reader(read_multi_buffer, &my_video);
    std::thread gpu_write(gpu_write_muilti_buffer, &my_video);
    std::unique_lock<std::mutex> lock(m);
    std::cout << "wait video decode\n";
    cond_var.wait(lock);
    std::cout << "video decode end\n";


    glUseProgram(shaderProgram);
    // render loop

    glfwSetKeyCallback(window, key_callback);
    while (!glfwWindowShouldClose(window))
    {
        // input 
        glfwPollEvents();

        processInput(window);
        if (read_ptr->data == NULL) {
            printf("no frame\n");
            break;
        }

        //std::cout << "read:" << read_ptr->index << std::endl;

        if (read_ptr->read && stop) {

            err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&read_ptr->data);
            checkError("clSetKernelArg0", err);
            err = clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL,
                global_item_size,
                local_item_size,
                0, NULL, &event);
            checkError("clEnqueueNDRangeKernel", err);


            glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                width, height,
                GL_RGB, GL_UNSIGNED_BYTE, NULL);
            glBindVertexArray(VAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
            glfwSwapBuffers(window);

            read_ptr->read = 0;
            read_ptr = read_ptr->next_buffer;

        }

    }

    reader.detach();
    gpu_write.detach();

    clEnqueueReleaseGLObjects(command_queue, 1, &cl_write, 0, NULL, NULL);
    checkError("releaseGLObjects", err);
    clFlush(command_queue);

    CleanUp(context, command_queue, program, kernel, memObject);
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteBuffers(1, &PBO_src);
    glDeleteBuffers(1, &PBO_dst);
    glfwTerminate();

    clearImageBuffer(&buffer_head);

    return 0;
}

void initialVideoData(VideoData* data) {
    data->fmt_ctx = NULL;
    data->stream_idx = -1;
    data->video_stream = NULL;
    data->codec_ctx = NULL;
    data->codec_ctxpar = NULL;
    data->decoder = NULL;
    data->conv_ctx = NULL;
}
void checkFile(const char* VideoPath, VideoData* data) {
    /*
    data->type = av_hwdevice_find_type_by_name("cuda");
    if (data->type == AV_HWDEVICE_TYPE_NONE) {
        fprintf(stderr, "Device type %s is not supported.\n", "cuda");
        fprintf(stderr, "Available device types:");
        while ((data->type = av_hwdevice_iterate_types(data->type)) != AV_HWDEVICE_TYPE_NONE)
            fprintf(stderr, " %s", av_hwdevice_get_type_name(data->type));
        fprintf(stderr, "\n");
    }
    */


    if (avformat_open_input(&data->fmt_ctx, VideoPath, NULL, NULL) < 0) {
        std::cout << "failed to load file" << std::endl;
        clearVideoData(data);
    }

    if (avformat_find_stream_info(data->fmt_ctx, NULL) < 0) {
        std::cout << "fail to get stream info" << std::endl;
        clearVideoData(data);
    }
    av_dump_format(data->fmt_ctx, 0, VideoPath, 0);

    for (unsigned int i = 0; i < data->fmt_ctx->nb_streams; ++i) {

        if (data->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            data->stream_idx = i;
            break;
        }
    }
    if (data->stream_idx == -1) {
        std::cout << "fail to find video stream" << std::endl;
        clearVideoData(data);

    }
}
void set_decoder(VideoData* data) {

    data->video_stream = data->fmt_ctx->streams[data->stream_idx];
    data->codec_ctxpar = data->video_stream->codecpar;
    data->decoder = avcodec_find_decoder(data->codec_ctxpar->codec_id);//use cpu decoder

    /*
    for (int i = 0;; i++) {
        const AVCodecHWConfig* config = avcodec_get_hw_config(data->decoder, i);
        if (!config) {
            fprintf(stderr, "Decoder %s does not support device type %s.\n",
                data->decoder->name, av_hwdevice_get_type_name(data->type));
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == data->type) {
            hw_pix_fmt = config->pix_fmt;
            break;
        }
    }
    */
    //--------------------------------------------------------------------

    data->codec_ctx = avcodec_alloc_context3(data->decoder);
    avcodec_parameters_to_context(data->codec_ctx, data->codec_ctxpar);

    //data->codec_ctx->get_format = get_hw_format;

    /*
    if (hw_decoder_init(data->codec_ctx, data->type) < 0)
        printf("hw_decoder_error");
        */
    data->codec_ctx->thread_count = 0;
    data->codec_ctx->thread_type = FF_THREAD_FRAME;
    //----------------------------------------------------

    if (data->decoder == NULL) {
        std::cout << "faild to find decorder" << std::endl;
        clearVideoData(data);
    }

    //open dcoder
    if (avcodec_open2(data->codec_ctx, data->decoder, NULL) < 0) {
        std::cout << "fail to  open decoder" << std::endl;
        clearVideoData(data);
    }
}
void clearVideoData(VideoData* data) {
    if (data->packet) av_free(data->packet);
    if (data->codec_ctx) avcodec_close(data->codec_ctx);
    if (data->codec_ctxpar) avcodec_parameters_free(&data->codec_ctxpar);
    if (data->fmt_ctx) avformat_free_context(data->fmt_ctx);
    //initialVideoData(data);
}
void clearImageBuffer(buffer* head) {
    buffer* ptr = head;

    for (int i = 0; i < buffer_num; i++) {
        av_free(ptr->internal_buffer);
        av_free(ptr->frame_buffer);
        av_free(ptr->codec_buffer);
        clReleaseMemObject(ptr->data);
        ptr = ptr->next_buffer;
    }
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_BACKSPACE && action == GLFW_PRESS)
        stop = stop ^ 1;
    else if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
        av_seek_frame(my_video.fmt_ctx, my_video.stream_idx, 0, 8);
}
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

cl_context CreateContext(cl_device_id* device) {

    cl_int err_num;
    cl_uint number_of_platform;
    cl_platform_id first_platform;
    cl_context context = NULL;

    err_num = clGetPlatformIDs(1, &first_platform, &number_of_platform);

    //error situation
    if (err_num != CL_SUCCESS || number_of_platform <= 0) {

        printf("Failed to find platforms.\n");
        return NULL;
    }

    err_num = clGetDeviceIDs(first_platform, CL_DEVICE_TYPE_GPU, 1, device, NULL);
    if (err_num != CL_SUCCESS) {
        printf("There is no GPU device\n");
        err_num = clGetDeviceIDs(first_platform, CL_DEVICE_TYPE_CPU, 1, device, NULL);
        if (err_num != CL_SUCCESS) {
            printf("There is no CPU device\n");
            return NULL;
        }
    }

    cl_context_properties properties[7];

    properties[0] = CL_GL_CONTEXT_KHR;
    properties[1] = (cl_context_properties)wglGetCurrentContext();
    properties[2] = CL_WGL_HDC_KHR;
    properties[3] = (cl_context_properties)wglGetCurrentDC();
    properties[4] = CL_CONTEXT_PLATFORM;
    properties[5] = (cl_context_properties)first_platform;
    properties[6] = 0;

    context = clCreateContext(properties, 1, device, NULL, NULL, &err_num);
    if (err_num != CL_SUCCESS) {
        printf("can't build context\n");
        return NULL;
    }

    return context;

}
cl_command_queue CreateCommandQueue(cl_context context, cl_device_id device) {

    cl_int err_num;
    cl_command_queue command_queue = NULL;

    command_queue = clCreateCommandQueueWithProperties(context, device, 0, NULL);
    if (command_queue == NULL) {
        printf("fail to create Queue");
        return NULL;
    }

    return command_queue;

}
cl_program CreateProgram(cl_context context, cl_device_id device, const char* filename) {

    cl_int err_num;
    cl_program  program;

    size_t program_length;

    char* const source_code = ReadKernelSourceFile(filename, &program_length);
    program = clCreateProgramWithSource(context, 1, (const char**)&source_code, NULL, NULL);



    if (program == NULL) {
        printf("failed to build program by cl code\n");
        return NULL;
    }

    err_num = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (err_num != CL_SUCCESS) {
        char build_log[16384];
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(build_log), build_log, NULL);
        printf("Error in kernel: %s \n", build_log);
        clReleaseProgram(program);
        return NULL;
    }

    return program;
}
void CleanUp(cl_context context, cl_command_queue command_queue, cl_program program, cl_kernel kernel, cl_mem memObjects[3]) {

    for (int i = 0; i < 3; i++) {
        if (memObjects[i] != 0) {
            clReleaseMemObject(memObjects[i]);
        }
    }

    if (command_queue != 0) {
        clReleaseCommandQueue(command_queue);
    }
    if (kernel != 0) {
        clReleaseKernel(kernel);
    }
    if (program != 0) {
        clReleaseProgram(program);
    }
    if (context != 0) {
        clReleaseContext(context);
    }

}
char* ReadKernelSourceFile(const char* filename, size_t* length) {
    FILE* file = NULL;
    errno_t err;
    size_t source_length;
    char* source_string;// code in cl file

    int ret;
    err = fopen_s(&file, filename, "rb");
    if (err != 0) {
        printf("%s at %d: cl file %s is opened failed\n", __FILE__, __LINE__ - 2, filename);
        return NULL;
    }

    //get cl file length
    fseek(file, 0, SEEK_END);
    source_length = ftell(file);

    //load cl file content
    fseek(file, 0, SEEK_SET);
    source_string = (char*)malloc(source_length + 1);
    source_string[0] = '\0';
    ret = fread(source_string, source_length, 1, file);
    if (ret == 0) {
        printf("fail to read content");
    }

    //free space
    fclose(file);
    if (length != 0) {
        *length = source_length; //get cl code length
    }

    source_string[source_length] = '\0';

    return source_string;

}
void read_multi_buffer(VideoData* data) {

    int err = 0;
    int num_write = 0;
    double last_time = 0;
    double start = 0;
    double end = 0;
    double time = 0;
    double max = 0;
    double min = 999999;

    ofs.open("CPU_1500frame_decode_time_record_4K_60hz.txt");
    if (!ofs.is_open()) {
        std::cout << "Failed to open file.\n";
    }

    if (!data->conv_ctx) {
        data->conv_ctx = sws_getContext(data->codec_ctx->width,
            data->codec_ctx->height, data->codec_ctx->pix_fmt,
            data->codec_ctx->width, data->codec_ctx->height, AV_PIX_FMT_BGR24, NULL, NULL, NULL, NULL);
    }


    while (1) {
        while (1) {
            if (write_ptr->image_write) {
                if (!(av_read_frame(data->fmt_ctx, data->packet) >= 0))
                    break;
                if (data->packet->stream_index == data->stream_idx) {

                    start = glfwGetTime();
                    if (avcodec_send_packet(data->codec_ctx, data->packet) < 0) {
                        break;
                    }
                    //if (!(avcodec_receive_frame(data->codec_ctx, data->av_frame) < 0)) {
                    if (!(avcodec_receive_frame(data->codec_ctx, write_ptr->codec_buffer) < 0)) {
                        end = glfwGetTime();  
                        ofs << (end-start)*1000 << "\n";
               
                        write_ptr->image_write = 0;
                        write_ptr->gpu_write = 1;
                        write_ptr = write_ptr->next_buffer;
                    }
                }
                av_packet_unref(data->packet);
            }
        }
        //av_seek_frame(data->fmt_ctx, data->stream_idx, 0, 8);
        break;
    }
    ofs.close();
    write_ptr->codec_buffer = NULL;
}
void gpu_write_muilti_buffer(VideoData* data) {
    int wait = 1;
    bool start_wait = 1;
    int er = 0;
    int num_write = 0;

    while (1)
    {
        if (gpu_ptr->codec_buffer == NULL) {
            gpu_ptr->data = NULL;
            break;
        }

        if (gpu_ptr->gpu_write && gpu_ptr->read == 0) {

            sws_scale(data->conv_ctx, gpu_ptr->codec_buffer->data, gpu_ptr->codec_buffer->linesize, 0, data->codec_ctx->height, gpu_ptr->frame_buffer->data, gpu_ptr->frame_buffer->linesize);

            er = clEnqueueWriteBuffer(command_queue, gpu_ptr->data, CL_TRUE, 0, height * width * 3 * sizeof(unsigned char), gpu_ptr->frame_buffer->data[0], 0, NULL, NULL);
            checkError("clSetKernelArg0", er);

            if (start_wait) {
                if (wait != buffer_num) {
                    wait++;
                }
                else {
                    cond_var.notify_one();
                    start_wait = 0;
                }
            }

            gpu_ptr->image_write = 1;
            gpu_ptr->gpu_write = 0;
            gpu_ptr->read = 1;
            gpu_ptr = gpu_ptr->next_buffer;
        }

    }

}

/*
void set_frame_buffer(VideoData* data) {
    //allocate frame to memory
    int RGBFramesize;
    RGBFramesize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, data->codec_ctx->width, data->codec_ctx->height, 1);
    uint8_t* internal_buffer = (uint8_t*)av_malloc(RGBFramesize * sizeof(uint8_t));
    av_image_fill_arrays(data->gl_frame->data, data->gl_frame->linesize, internal_buffer, AV_PIX_FMT_RGB24, data->codec_ctx->width, data->codec_ctx->height, 1);
    data->packet = (AVPacket*)av_malloc(sizeof(AVPacket));
}
*/
/*
void read(VideoData* data) {
    int err = 0;

    while (av_read_frame(data->fmt_ctx, data->packet) >= 0) {

        if (data->packet->stream_index == data->stream_idx) {

            if (!data->conv_ctx) {

                data->conv_ctx = sws_getContext(data->codec_ctx->width,
                    data->codec_ctx->height, data->codec_ctx->pix_fmt,
                    data->codec_ctx->width, data->codec_ctx->height, AV_PIX_FMT_RGB24, NULL, NULL, NULL, NULL);
            }

            if (avcodec_send_packet(data->codec_ctx, data->packet) < 0) {
                break;
            }

            if (!(avcodec_receive_frame(data->codec_ctx, data->av_frame) < 0)) {
                sws_scale(data->conv_ctx, data->av_frame->data, data->av_frame->linesize, 0,
                    data->codec_ctx->height, data->gl_frame->data, data->gl_frame->linesize);
            }
        }
        av_packet_unref(data->packet);

        cond_var.notify_one();
        //image_buffer = data->gl_frame->data[0];

        err = clEnqueueWriteBuffer(command_queue, cl_read, CL_TRUE, 0, height * width * 3 * sizeof(unsigned char), data->gl_frame->data[0], 0, NULL, NULL);
        checkError("clSetKernelArg0", err);
    }

}
*/



/*
static enum AVPixelFormat get_hw_format(AVCodecContext* ctx,
    const enum AVPixelFormat* pix_fmts)
{
    const enum AVPixelFormat* p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt)
            return *p;
    }

    fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}
*/

/*
static int hw_decoder_init(AVCodecContext* ctx, const enum AVHWDeviceType type)
{
    int err = 0;

    if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type,
        NULL, NULL, 0)) < 0) {
        fprintf(stderr, "Failed to create specified HW device.\n");
        return err;
    }
    ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

    return err;
}
*/
