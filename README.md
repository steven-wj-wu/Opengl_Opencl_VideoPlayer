# Opengl_Opencl_VideoPlayer
this project is about the sample code of using GlFW+OpenCL to make a non UI video player based on window OS

The pipeline is follow:

  ffmpeg hardware(CUDA)decoder->OpenGL Texture Buffer->OpenCL Image Process->OpenGl Render with vsync

Include Lib:
  
  GLFW 3.3.8 WIN64

  FFmpeg 5.1.2

  GLAD 0.1.36 

  OpenCL (OCL_SDK)

IDE:

  Visual Studio 2019
