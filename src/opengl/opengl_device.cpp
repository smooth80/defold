#include <string.h>
#include <assert.h>
#include <dlib/log.h>
#include <dlib/profile.h>
#include <vectormath/cpp/vectormath_aos.h>

#include "../graphics_device.h"
#include "opengl_device.h"

#include "../glfw/include/GL/glfw.h"

#ifdef __linux__
#include <GL/glext.h>

#elif defined (__MACH__)

#elif defined (_WIN32)

#ifdef GL_GLEXT_PROTOTYPES

#undef GL_GLEXT_PROTOTYPES
#include "win32/glext.h"
#define GL_GLEXT_PROTOTYPES

#else
#include "win32/glext.h"
#endif

// VBO Extension for OGL 1.4.1
typedef void (APIENTRY * PFNGLGENPROGRAMARBPROC) (GLenum, GLuint *);
typedef void (APIENTRY * PFNGLBINDPROGRAMARBPROC) (GLenum, GLuint);
typedef void (APIENTRY * PFNGLDELETEPROGRAMSARBPROC) (GLsizei, const GLuint*);
typedef void (APIENTRY * PFNGLPROGRAMSTRINGARBPROC) (GLenum, GLenum, GLsizei, const GLvoid *);
typedef void (APIENTRY * PFNGLVERTEXPARAMFLOAT4ARBPROC) (GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (APIENTRY * PFNGLVERTEXATTRIBSETPROC) (GLuint);
typedef void (APIENTRY * PFNGLVERTEXATTRIBPTRPROC) (GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid *);
typedef void (APIENTRY * PFNGLTEXPARAM2DPROC) (GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const GLvoid *);
typedef void (APIENTRY * PFNGLBINDBUFFERPROC) (GLenum, GLuint);
typedef void (APIENTRY * PFNGLBUFFERDATAPROC) (GLenum, GLsizeiptr, const GLvoid*, GLenum);
typedef void (APIENTRY * PFNGLGENRENDERBUFFERSPROC) (GLenum, GLuint *);
typedef void (APIENTRY * PFNGLBINDRENDERBUFFERPROC) (GLenum, GLuint);
typedef void (APIENTRY * PFNGLRENDERBUFFERSTORAGEPROC) (GLenum, GLenum, GLsizei, GLsizei);
typedef void (APIENTRY * PFNGLRENDERBUFFERTEXTURE2DPROC) (GLenum, GLenum, GLenum, GLuint, GLint);
typedef void (APIENTRY * PFNGLFRAMEBUFFERRENDERBUFFERPROC) (GLenum, GLenum, GLenum, GLuint);
typedef void (APIENTRY * PFNGLGENFRAMEBUFFERSPROC) (GLenum, GLuint *);
typedef void (APIENTRY * PFNGLBINDFRAMEBUFFERPROC) (GLenum, GLuint);
typedef void (APIENTRY * PFNGLDELETEFRAMEBUFFERSPROC) (GLsizei, GLuint*);
typedef void (APIENTRY * PFNGLDELETERENDERBUFFERSPROC) (GLsizei, GLuint*);
typedef void (APIENTRY * PFNGLBUFFERSUBDATAPROC) (GLenum, GLintptr, GLsizeiptr, const GLvoid*);
typedef void* (APIENTRY * PFNGLMAPBUFFERPROC) (GLenum, GLenum);
typedef GLboolean (APIENTRY * PFNGLUNMAPBUFFERPROC) (GLenum);
typedef void (APIENTRY * PFNGLACTIVETEXTURE) (GLenum);

PFNGLGENPROGRAMARBPROC glGenProgramsARB = NULL;
PFNGLBINDPROGRAMARBPROC glBindProgramARB = NULL;
PFNGLDELETEPROGRAMSARBPROC glDeleteProgramsARB = NULL;
PFNGLPROGRAMSTRINGARBPROC glProgramStringARB = NULL;
PFNGLVERTEXPARAMFLOAT4ARBPROC glProgramLocalParameter4fARB = NULL;
PFNGLVERTEXATTRIBSETPROC glEnableVertexAttribArray = NULL;
PFNGLVERTEXATTRIBSETPROC glDisableVertexAttribArray = NULL;
PFNGLVERTEXATTRIBPTRPROC glVertexAttribPointer = NULL;
PFNGLTEXPARAM2DPROC glCompressedTexImage2D = NULL;
PFNGLGENBUFFERSPROC glGenBuffersARB = NULL;
PFNGLDELETEBUFFERSPROC glDeleteBuffersARB = NULL;
PFNGLBINDBUFFERPROC glBindBufferARB = NULL;
PFNGLBUFFERDATAPROC glBufferDataARB = NULL;
PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements = NULL;
PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers = NULL;
PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer = NULL;
PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage = NULL;
PFNGLRENDERBUFFERTEXTURE2DPROC glFramebufferTexture2D = NULL;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer = NULL;
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = NULL;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = NULL;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = NULL;
PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers = NULL;
PFNGLBUFFERSUBDATAPROC glBufferSubDataARB = NULL;
PFNGLMAPBUFFERPROC glMapBufferARB = NULL;
PFNGLUNMAPBUFFERPROC glUnmapBufferARB = NULL;
PFNGLACTIVETEXTURE glActiveTexture = NULL;
#else
#error "Platform not supported."
#endif

using namespace Vectormath::Aos;

namespace dmGraphics
{
#define CHECK_GL_ERROR \
    { \
        GLint err = glGetError(); \
        if (err != 0) \
        { \
            dmLogError("gl error %d: %s\n", err, gluErrorString(err)); \
            assert(0); \
        } \
    }\

#define CHECK_GL_FRAMEBUFFER_ERROR \
    { \
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT); \
        if (status != GL_FRAMEBUFFER_COMPLETE_EXT) \
        { \
            switch (status) \
            { \
                case GL_FRAMEBUFFER_UNDEFINED: \
                    dmLogError("gl error %d: %s\n", GL_FRAMEBUFFER_UNDEFINED, "GL_FRAMEBUFFER_UNDEFINED"); \
                    break; \
                case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT: \
                    dmLogError("gl error %d: %s\n", GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT, "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT"); \
                    break; \
                case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT: \
                    dmLogError("gl error %d: %s\n", GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT, "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT"); \
                    break; \
                case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT: \
                    dmLogError("gl error %d: %s\n", GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT, "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER"); \
                    break; \
                case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT: \
                    dmLogError("gl error %d: %s\n", GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT, "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER"); \
                    break; \
                case GL_FRAMEBUFFER_UNSUPPORTED_EXT: \
                    dmLogError("gl error %d: %s\n", GL_FRAMEBUFFER_UNSUPPORTED_EXT, "GL_FRAMEBUFFER_UNSUPPORTED"); \
                    break; \
                case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT: \
                    dmLogError("gl error %d: %s\n", GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT, "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE"); \
                    break; \
                case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT: \
                    dmLogError("gl error %d: %s\n", GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT, "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS"); \
                    break; \
            } \
            assert(0); \
        } \
    } \

    extern BufferType BUFFER_TYPES[MAX_BUFFER_TYPE_COUNT];
    extern GLenum TEXTURE_UNIT_NAMES[32];

    Device gdevice;
    Context gcontext;

    HContext GetContext()
    {
        return (HContext)&gcontext;
    }

    HDevice NewDevice(int* argc, char** argv, CreateDeviceParams *params )
    {
        assert(params);

        glfwInit(); // We can do this twice


        if( !glfwOpenWindow( params->m_DisplayWidth, params->m_DisplayHeight, 8,8,8,8, 32,0, GLFW_WINDOW ) )
        {
            glfwTerminate();
            return 0;
        }

        glfwSetWindowTitle(params->m_AppTitle);
        glfwSwapInterval(1);
        CHECK_GL_ERROR

        gdevice.m_DisplayWidth = params->m_DisplayWidth;
        gdevice.m_DisplayHeight = params->m_DisplayHeight;

        if (params->m_PrintDeviceInfo)
        {
            printf("GL_RENDERER   = %s\n", (char *) glGetString(GL_RENDERER));
            printf("GL_VERSION    = %s\n", (char *) glGetString(GL_VERSION));
            printf("GL_VENDOR     = %s\n", (char *) glGetString(GL_VENDOR));
            printf("GL_EXTENSIONS = %s\n", (char *) glGetString(GL_EXTENSIONS));
        }

    #if defined (_WIN32)
        glGenProgramsARB = (PFNGLGENPROGRAMARBPROC) wglGetProcAddress("glGenProgramsARB");
        glBindProgramARB = (PFNGLBINDPROGRAMARBPROC) wglGetProcAddress("glBindProgramARB");
        glDeleteProgramsARB = (PFNGLDELETEPROGRAMSARBPROC) wglGetProcAddress("glDeleteProgramsARB");
        glProgramStringARB = (PFNGLPROGRAMSTRINGARBPROC) wglGetProcAddress("glProgramStringARB");
        glProgramLocalParameter4fARB = (PFNGLVERTEXPARAMFLOAT4ARBPROC) wglGetProcAddress("glProgramLocalParameter4fARB");
        glEnableVertexAttribArray = (PFNGLVERTEXATTRIBSETPROC) wglGetProcAddress("glEnableVertexAttribArray");
        glDisableVertexAttribArray = (PFNGLVERTEXATTRIBSETPROC) wglGetProcAddress("glDisableVertexAttribArray");
        glVertexAttribPointer = (PFNGLVERTEXATTRIBPTRPROC) wglGetProcAddress("glVertexAttribPointer");
        glCompressedTexImage2D = (PFNGLTEXPARAM2DPROC) wglGetProcAddress("glCompressedTexImage2D");
        glGenBuffersARB = (PFNGLGENBUFFERSPROC) wglGetProcAddress("glGenBuffersARB");
        glDeleteBuffersARB = (PFNGLDELETEBUFFERSPROC) wglGetProcAddress("glDeleteBuffersARB");
        glBindBufferARB = (PFNGLBINDBUFFERPROC) wglGetProcAddress("glBindBufferARB");
        glBufferDataARB = (PFNGLBUFFERDATAPROC) wglGetProcAddress("glBufferDataARB");
        glDrawRangeElements = (PFNGLDRAWRANGEELEMENTSPROC) wglGetProcAddress("glDrawRangeElements");
        glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC) wglGetProcAddress("glGenRenderbuffers");
        glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC) wglGetProcAddress("glBindRenderbuffer");
        glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC) wglGetProcAddress("glRenderbufferStorage");
        glFramebufferTexture2D = (PFNGLRENDERBUFFERTEXTURE2DPROC) wglGetProcAddress("glFramebufferTexture2D");
        glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC) wglGetProcAddress("glFramebufferRenderbuffer");
        glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC) wglGetProcAddress("glGenFramebuffers");
        glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC) wglGetProcAddress("glBindFramebuffer");
        glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC) wglGetProcAddress("glDeleteFramebuffers");
        glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC) wglGetProcAddress("glDeleteRenderbuffers");
        glBufferSubDataARB = (PFNGLBUFFERSUBDATAPROC) wglGetProcAddress("glBufferSubDataARB");
        glMapBufferARB = (PFNGLMAPBUFFERPROC) wglGetProcAddress("glMapBufferARB");
        glUnmapBufferARB = (PFNGLUNMAPBUFFERPROC) wglGetProcAddress("glUnmapBufferARB");
    #endif

        return (HDevice)&gdevice;
    }

    void DeleteDevice(HDevice device)
    {
        glfwTerminate();
    }

    void Clear(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {
        assert(context);
        DM_PROFILE(Graphics, "Clear");

        float r = ((float)red)/255.0f;
        float g = ((float)green)/255.0f;
        float b = ((float)blue)/255.0f;
        float a = ((float)alpha)/255.0f;
        glClearColor(r, g, b, a);
        CHECK_GL_ERROR

        glClearDepth(depth);
        CHECK_GL_ERROR

        glClearStencil(stencil);
        CHECK_GL_ERROR

        glClear(flags);
        CHECK_GL_ERROR
    }

    void Flip()
    {
        DM_PROFILE(Graphics, "Flip");
        glfwSwapBuffers();
        CHECK_GL_ERROR
    }

    HVertexBuffer NewVertexBuffer(uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        uint32_t buffer = 0;
        glGenBuffersARB(1, &buffer);
        CHECK_GL_ERROR
        SetVertexBufferData(buffer, size, data, buffer_usage);
        return buffer;
    }

    void DeleteVertexBuffer(HVertexBuffer buffer)
    {
        glDeleteBuffersARB(1, &buffer);
        CHECK_GL_ERROR
    }

    void SetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        DM_PROFILE(Graphics, "SetVertexBufferData");
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
        CHECK_GL_ERROR
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, size, data, buffer_usage);
        CHECK_GL_ERROR
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR
    }

    void SetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        DM_PROFILE(Graphics, "SetVertexBufferSubData");
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
        CHECK_GL_ERROR
        glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, offset, size, data);
        CHECK_GL_ERROR
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR
    }

    void* MapVertexBuffer(HVertexBuffer buffer, BufferAccess access)
    {
        DM_PROFILE(Graphics, "MapVertexBuffer");
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
        CHECK_GL_ERROR
        void* result = glMapBufferARB(GL_ARRAY_BUFFER_ARB, access);
        CHECK_GL_ERROR
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR
        return result;
    }

    bool UnmapVertexBuffer(HVertexBuffer buffer)
    {
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
        CHECK_GL_ERROR
        bool result = glUnmapBufferARB(GL_ARRAY_BUFFER_ARB) == GL_TRUE;
        CHECK_GL_ERROR
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR
        return result;
    }

    HIndexBuffer NewIndexBuffer(uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        uint32_t buffer = 0;
        glGenBuffersARB(1, &buffer);
        CHECK_GL_ERROR
        SetIndexBufferData(buffer, size, data, buffer_usage);
        return buffer;
    }

    void DeleteIndexBuffer(HIndexBuffer buffer)
    {
        glDeleteBuffersARB(1, &buffer);
        CHECK_GL_ERROR
    }

    void SetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        DM_PROFILE(Graphics, "SetIndexBufferData");
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffer);
        CHECK_GL_ERROR
        glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, size, data, buffer_usage);
        CHECK_GL_ERROR
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR
    }

    void SetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        DM_PROFILE(Graphics, "SetIndexBufferSubData");
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffer);
        CHECK_GL_ERROR
        glBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, offset, size, data);
        CHECK_GL_ERROR
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR
    }

    void* MapIndexBuffer(HIndexBuffer buffer, BufferAccess access)
    {
        DM_PROFILE(Graphics, "MapIndexBuffer");
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffer);
        CHECK_GL_ERROR
        void* result = glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, access);
        CHECK_GL_ERROR
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR
        return result;
    }

    bool UnmapIndexBuffer(HIndexBuffer buffer)
    {
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffer);
        CHECK_GL_ERROR
        bool result = glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB) == GL_TRUE;
        CHECK_GL_ERROR
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR
        return result;
    }

    static uint32_t GetTypeSize(Type type)
    {
        uint32_t size = 0;
        switch (type)
        {
            case TYPE_BYTE:
            case TYPE_UNSIGNED_BYTE:
                size = 1;
                break;

            case TYPE_SHORT:
            case TYPE_UNSIGNED_SHORT:
                size = 2;
                break;

            case TYPE_INT:
            case TYPE_UNSIGNED_INT:
            case TYPE_FLOAT:
                size = 4;
                break;

            default:
                assert(0);
        }
        return size;
    }

    HVertexDeclaration NewVertexDeclaration(VertexElement* element, uint32_t count)
    {
        VertexDeclaration* vd = new VertexDeclaration;

        vd->m_Stride = 0;
        assert(count < (sizeof(vd->m_Streams) / sizeof(vd->m_Streams[0]) ) );

        for (uint32_t i=0; i<count; i++)
        {
            vd->m_Streams[i].m_Index = i;
            vd->m_Streams[i].m_Size = element[i].m_Size;
            vd->m_Streams[i].m_Usage = element[i].m_Usage;
            vd->m_Streams[i].m_Type = element[i].m_Type;
            vd->m_Streams[i].m_UsageIndex = element[i].m_UsageIndex;
            vd->m_Streams[i].m_Offset = vd->m_Stride;

            vd->m_Stride += element[i].m_Size * GetTypeSize(element[i].m_Type);
        }
        vd->m_StreamCount = count;
        return vd;
    }

    void DeleteVertexDeclaration(HVertexDeclaration vertex_declaration)
    {
        delete vertex_declaration;
    }

    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer)
    {
        assert(context);
        assert(vertex_buffer);
        assert(vertex_declaration);
        #define BUFFER_OFFSET(i) ((char*)0x0 + (i))

        glBindBufferARB(GL_ARRAY_BUFFER, vertex_buffer);
        CHECK_GL_ERROR

        for (uint32_t i=0; i<vertex_declaration->m_StreamCount; i++)
        {
            glEnableVertexAttribArray(vertex_declaration->m_Streams[i].m_Index);
            CHECK_GL_ERROR
            glVertexAttribPointer(
                    vertex_declaration->m_Streams[i].m_Index,
                    vertex_declaration->m_Streams[i].m_Size,
                    vertex_declaration->m_Streams[i].m_Type,
                    false,
                    vertex_declaration->m_Stride,
            BUFFER_OFFSET(vertex_declaration->m_Streams[i].m_Offset) );   //The starting point of the VBO, for the vertices

            CHECK_GL_ERROR
        }

        #undef BUFFER_OFFSET
    }

    void DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration)
    {
        assert(context);
        assert(vertex_declaration);

        for (uint32_t i=0; i<vertex_declaration->m_StreamCount; i++)
        {
            glDisableVertexAttribArray(i);
            CHECK_GL_ERROR
        }

        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR

        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR
    }

    void SetVertexStream(HContext context, uint16_t stream, uint16_t size, Type type, uint16_t stride, const void* vertex_buffer)
    {
        assert(context);
        assert(vertex_buffer);
        DM_PROFILE(Graphics, "SetVertexStream");

        glEnableVertexAttribArray(stream);
        CHECK_GL_ERROR
        glVertexAttribPointer(stream, size, type, false, stride, vertex_buffer);
        CHECK_GL_ERROR
    }

    void DisableVertexStream(HContext context, uint16_t stream)
    {
        assert(context);

        glDisableVertexAttribArray(stream);
        CHECK_GL_ERROR
    }

    void DrawRangeElements(HContext context, PrimitiveType prim_type, uint32_t start, uint32_t count, Type type, HIndexBuffer index_buffer)
    {
        assert(context);
        assert(index_buffer);
        DM_PROFILE(Graphics, "DrawRangeElements");

        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
        CHECK_GL_ERROR

        glDrawRangeElements(prim_type, start, start + count, count * 3, type, 0);
        CHECK_GL_ERROR
    }

    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t count, Type type, const void* index_buffer)
    {
        assert(context);
        assert(index_buffer);
        DM_PROFILE(Graphics, "DrawElements");

        glDrawElements(prim_type, count, type, index_buffer);
        CHECK_GL_ERROR
    }

    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count)
    {
        assert(context);
        DM_PROFILE(Graphics, "Draw");
        glDrawArrays(prim_type, first, count);
        CHECK_GL_ERROR
    }

    static uint32_t CreateProgram(GLenum type, const void* program, uint32_t program_size)
    {
        glEnable(type);
        GLuint shader[1];
        glGenProgramsARB(1, shader);
        CHECK_GL_ERROR

        glBindProgramARB(type, shader[0]);
        CHECK_GL_ERROR

        glProgramStringARB(type, GL_PROGRAM_FORMAT_ASCII_ARB, program_size, program);
        CHECK_GL_ERROR

        glDisable(type);
        CHECK_GL_ERROR

        return shader[0];
    }

    HVertexProgram NewVertexProgram(const void* program, uint32_t program_size)
    {
        assert(program);

        return CreateProgram(GL_VERTEX_PROGRAM_ARB, program, program_size);
    }

    HFragmentProgram NewFragmentProgram(const void* program, uint32_t program_size)
    {
        assert(program);

        return CreateProgram(GL_FRAGMENT_PROGRAM_ARB, program, program_size);
    }

    void DeleteVertexProgram(HVertexProgram program)
    {
        assert(program);
        glDeleteProgramsARB(1, &program);
        CHECK_GL_ERROR
    }

    void DeleteFragmentProgram(HFragmentProgram program)
    {
        assert(program);
        glDeleteProgramsARB(1, &program);
        CHECK_GL_ERROR
    }

    static void SetProgram(GLenum type, uint32_t program)
    {
        glEnable(type);
        CHECK_GL_ERROR
        glBindProgramARB(type, program);
        CHECK_GL_ERROR
    }

    void SetVertexProgram(HContext context, HVertexProgram program)
    {
        assert(context);

        SetProgram(GL_VERTEX_PROGRAM_ARB, program);
    }

    void SetFragmentProgram(HContext context, HFragmentProgram program)
    {
        assert(context);

        SetProgram(GL_FRAGMENT_PROGRAM_ARB, program);
    }

    void SetViewport(HContext context, int width, int height)
    {
        assert(context);

        glViewport(0, 0, width, height);
        CHECK_GL_ERROR
    }

    static void SetProgramConstantBlock(HContext context, GLenum type, const Vector4* data, int base_register, int num_vectors)
    {
        assert(context);
        assert(data);
        assert(base_register >= 0);
        assert(num_vectors >= 0);

        const float *f = (const float*)data;
        int reg = 0;
        for (int i=0; i<num_vectors; i++, reg+=4)
        {
            glProgramLocalParameter4fARB(type, base_register+i,  f[0+reg], f[1+reg], f[2+reg], f[3+reg]);
            CHECK_GL_ERROR
        }

    }

    void SetFragmentConstant(HContext context, const Vector4* data, int base_register)
    {
        assert(context);

        SetProgramConstantBlock(context, GL_FRAGMENT_PROGRAM_ARB, data, base_register, 1);
        CHECK_GL_ERROR
    }

    void SetVertexConstantBlock(HContext context, const Vector4* data, int base_register, int num_vectors)
    {
        assert(context);

        SetProgramConstantBlock(context, GL_VERTEX_PROGRAM_ARB, data, base_register, num_vectors);
        CHECK_GL_ERROR
    }

    void SetFragmentConstantBlock(HContext context, const Vector4* data, int base_register, int num_vectors)
    {
        assert(context);

        SetProgramConstantBlock(context, GL_FRAGMENT_PROGRAM_ARB, data, base_register, num_vectors);
        CHECK_GL_ERROR
    }

    HRenderTarget NewRenderTarget(uint32_t buffer_type_flags, const TextureParams params[MAX_BUFFER_TYPE_COUNT])
    {
        RenderTarget* rt = new RenderTarget;

        glGenFramebuffers(1, &rt->m_Id);
        CHECK_GL_ERROR
        glBindFramebuffer(GL_FRAMEBUFFER_EXT, rt->m_Id);
        CHECK_GL_ERROR

        GLenum buffer_attachments[MAX_BUFFER_TYPE_COUNT] = {GL_COLOR_ATTACHMENT0_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_STENCIL_ATTACHMENT_EXT};
        for (uint32_t i = 0; i < MAX_BUFFER_TYPE_COUNT; ++i)
        {
            if (buffer_type_flags & BUFFER_TYPES[i])
            {
                rt->m_BufferTextures[i] = NewTexture(params[i]);
                // attach the texture to FBO color attachment point
                glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, buffer_attachments[i], GL_TEXTURE_2D, rt->m_BufferTextures[i]->m_Texture, 0);
                CHECK_GL_ERROR
            }
        }
        // Disable color buffer
        if ((buffer_type_flags & BUFFER_TYPE_COLOR) == 0)
        {
            glDrawBuffer(GL_NONE);
            CHECK_GL_ERROR
        }

        glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
        CHECK_GL_ERROR

        CHECK_GL_FRAMEBUFFER_ERROR

        return rt;
    }


    void DeleteRenderTarget(HRenderTarget render_target)
    {
        glDeleteFramebuffers(1, &render_target->m_Id);
        for (uint32_t i = 0; i < MAX_BUFFER_TYPE_COUNT; ++i)
            DeleteTexture(render_target->m_BufferTextures[i]);
        delete render_target;
    }

    void EnableRenderTarget(HContext context, HRenderTarget render_target)
    {
        glBindFramebuffer(GL_FRAMEBUFFER_EXT, render_target->m_Id);
        CHECK_GL_ERROR
        CHECK_GL_FRAMEBUFFER_ERROR
    }

    void DisableRenderTarget(HContext context, HRenderTarget render_target)
    {
        glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
        CHECK_GL_ERROR
        CHECK_GL_FRAMEBUFFER_ERROR
    }

    HTexture GetRenderTargetTexture(HRenderTarget render_target, BufferType buffer_type)
    {
        return render_target->m_BufferTextures[GetBufferTypeIndex(buffer_type)];
    }

    HTexture NewTexture(const TextureParams& params)
    {
        GLuint t;
        glGenTextures( 1, &t );
        CHECK_GL_ERROR

        Texture* tex = new Texture;
        tex->m_Texture = t;

        SetTexture(tex, params);

        return (HTexture) tex;
    }

    void SetTexture(HTexture texture, const TextureParams& params)
    {
        glBindTexture(GL_TEXTURE_2D, texture->m_Texture);
        CHECK_GL_ERROR

        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE );
        CHECK_GL_ERROR

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, params.m_MinFilter);
        CHECK_GL_ERROR

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, params.m_MagFilter);
        CHECK_GL_ERROR

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, params.m_UWrap);
        CHECK_GL_ERROR

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, params.m_VWrap);
        CHECK_GL_ERROR

        GLenum gl_format;
        GLenum gl_type = GL_UNSIGNED_BYTE;
        GLint internal_format;

        switch (params.m_Format)
        {
        case TEXTURE_FORMAT_LUMINANCE:
            gl_format = GL_LUMINANCE;
            internal_format = 1;
            break;
        case TEXTURE_FORMAT_RGB:
            gl_format = GL_RGB;
            internal_format = 3;
            break;
        case TEXTURE_FORMAT_RGBA:
            gl_format = GL_RGBA;
            internal_format = 4;
            break;
        case TEXTURE_FORMAT_RGB_DXT1:
            gl_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
            break;
        case TEXTURE_FORMAT_RGBA_DXT1:
            gl_format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
            break;
        case TEXTURE_FORMAT_RGBA_DXT3:
            gl_format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
            break;
        case TEXTURE_FORMAT_RGBA_DXT5:
            gl_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            CHECK_GL_ERROR
            break;
        case TEXTURE_FORMAT_DEPTH:
            gl_format = GL_DEPTH_COMPONENT;
            internal_format = GL_DEPTH_COMPONENT;
            gl_type = GL_FLOAT;
            break;
        default:
            assert(0);
        }
        switch (params.m_Format)
        {
        case TEXTURE_FORMAT_LUMINANCE:
        case TEXTURE_FORMAT_RGB:
        case TEXTURE_FORMAT_RGBA:
        case TEXTURE_FORMAT_DEPTH:
            glTexImage2D(GL_TEXTURE_2D, params.m_MipMap, internal_format, params.m_Width, params.m_Height, 0, gl_format, gl_type, params.m_Data);
            CHECK_GL_ERROR
            break;

        case TEXTURE_FORMAT_RGB_DXT1:
        case TEXTURE_FORMAT_RGBA_DXT1:
        case TEXTURE_FORMAT_RGBA_DXT3:
        case TEXTURE_FORMAT_RGBA_DXT5:
            if (params.m_DataSize > 0)
                glCompressedTexImage2D(GL_TEXTURE_2D, params.m_MipMap, gl_format, params.m_Width, params.m_Height, 0, params.m_DataSize, params.m_Data);
            CHECK_GL_ERROR
            break;
        default:
            assert(0);
        }
    }

    void DeleteTexture(HTexture texture)
    {
        assert(texture);

        glDeleteTextures(1, &texture->m_Texture);
        CHECK_GL_ERROR

        delete texture;
    }

    void SetTextureUnit(HContext context, uint32_t unit, HTexture texture)
    {
        assert(context);

        glEnable(GL_TEXTURE_2D);
        CHECK_GL_ERROR

        glActiveTexture(TEXTURE_UNIT_NAMES[unit]);

        GLuint texture_id = 0;
        if (texture)
            texture_id = texture->m_Texture;
        glBindTexture(GL_TEXTURE_2D, texture_id);
        CHECK_GL_ERROR
    }

    void EnableState(HContext context, State state)
    {
        assert(context);
        glEnable(state);
        CHECK_GL_ERROR
    }

    void DisableState(HContext context, State state)
    {
        assert(context);
        glDisable(state);
        CHECK_GL_ERROR
    }

    void SetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
        assert(context);
        glBlendFunc((GLenum) source_factor, (GLenum) destinaton_factor);
        CHECK_GL_ERROR
    }

    void SetColorMask(HContext context, bool red, bool green, bool blue, bool alpha)
    {
        assert(context);
        glColorMask(red, green, blue, alpha);
        CHECK_GL_ERROR
    }

    void SetDepthMask(HContext context, bool mask)
    {
        assert(context);
        glDepthMask(mask);
        CHECK_GL_ERROR
    }

    void SetIndexMask(HContext context, uint32_t mask)
    {
        assert(context);
        glIndexMask(mask);
        CHECK_GL_ERROR
    }

    void SetStencilMask(HContext context, uint32_t mask)
    {
        assert(context);
        glStencilMask(mask);
        CHECK_GL_ERROR
    }

    void SetCullFace(HContext context, FaceType face_type)
    {
        assert(context);
        glCullFace(face_type);
        CHECK_GL_ERROR
    }

    void SetPolygonOffset(HContext context, float factor, float units)
    {
        assert(context);
        glPolygonOffset(factor, units);
        CHECK_GL_ERROR
    }

    uint32_t GetWindowParam(WindowParam param)
    {
        return glfwGetWindowParam(param);
    }

    uint32_t GetWindowWidth()
    {
        return gdevice.m_DisplayWidth;
    }

    uint32_t GetWindowHeight()
    {
        return gdevice.m_DisplayHeight;
    }

    BufferType BUFFER_TYPES[MAX_BUFFER_TYPE_COUNT] = {BUFFER_TYPE_COLOR, BUFFER_TYPE_DEPTH, BUFFER_TYPE_STENCIL};

    GLenum TEXTURE_UNIT_NAMES[32] =
    {
        GL_TEXTURE0,
        GL_TEXTURE1,
        GL_TEXTURE2,
        GL_TEXTURE3,
        GL_TEXTURE4,
        GL_TEXTURE5,
        GL_TEXTURE6,
        GL_TEXTURE7,
        GL_TEXTURE8,
        GL_TEXTURE9,
        GL_TEXTURE10,
        GL_TEXTURE11,
        GL_TEXTURE12,
        GL_TEXTURE13,
        GL_TEXTURE14,
        GL_TEXTURE15,
        GL_TEXTURE16,
        GL_TEXTURE17,
        GL_TEXTURE18,
        GL_TEXTURE19,
        GL_TEXTURE20,
        GL_TEXTURE21,
        GL_TEXTURE22,
        GL_TEXTURE23,
        GL_TEXTURE24,
        GL_TEXTURE25,
        GL_TEXTURE26,
        GL_TEXTURE27,
        GL_TEXTURE28,
        GL_TEXTURE29,
        GL_TEXTURE30,
        GL_TEXTURE31
    };
}
