// File:  ph.c
// Brief: Implements "Philip's GL utility API", or "ph" for short.

#include "quakedef.h"

#ifdef _WIN32
#include "winquake.h"
#include "ph.h"

const PHsurface *boundSurface;

int msaa_samples;

void phCreateSurface(char *identifier, PHsurface *surface, GLboolean depth, GLboolean fp, GLboolean linear)
{
    GLenum internalFormat = fp ? GL_RGBA16F_ARB : GL_RGBA;
    GLenum type = fp ? GL_HALF_FLOAT_ARB : GL_UNSIGNED_BYTE;
    GLenum filter = linear ? GL_LINEAR : GL_NEAREST;

	surface->depth = 0;


	// create the render buffers
	if (framebuffer_multisample_ext == true)
	{
		// we need to find out what the maximum supported samples is
		if(msaa_samples == 0)
			glGetIntegerv(GL_MAX_SAMPLES_EXT, &msaa_samples);



		// create the msaa color buffer
		glGenRenderbuffersEXT(1, &surface->texture_msaa);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, surface->texture_msaa);
		glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, (int)pow(2.0f, gl_framebuffer_msaa.value), GL_RGBA8, surface->width, surface->height);
		phCheckError("Creation of the msaa renderbuffer for the FBO");

		if (depth)
		{
			// create the msaa depth renderbuffer
			glGenRenderbuffersEXT(1, &surface->depth);
			glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, surface->depth);
			glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, (int)pow(2.0f, gl_framebuffer_msaa.value), GL_DEPTH_COMPONENT24, surface->width, surface->height);
			phCheckError("Creation of the msaa depth renderbuffer for the FBO");
		}
	}
	else
	{
		if (depth)
		{
			// create the depth renderbuffer
			glGenRenderbuffersEXT(1, &surface->depth);
			glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, surface->depth);
			glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, surface->width, surface->height);
			phCheckError("Creation of the depth renderbuffer for the FBO");
		}
	}



	// create the final color buffer
	surface->texture = R_FrameBuffer_Bind(identifier, surface->width, surface->height);
	glBindTexture(GL_TEXTURE_2D, surface->texture);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, surface->width, surface->height, 0, GL_RGBA, type, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	phCheckError("Creation of the color texture for the FBO");



	// attach the msaa renderbuffer to the msaa fbo
	if (framebuffer_multisample_ext == true)
	{
		glGenFramebuffersEXT(1, &surface->fbo_msaa);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, surface->fbo_msaa);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, surface->texture_msaa);
		if (depth)
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, surface->depth);
	}



	// attach the color buffer to the fbo
	glGenFramebuffersEXT(1, &surface->fbo);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, surface->fbo);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, surface->texture, 0);
	if (framebuffer_multisample_ext == false)
	{
		if (depth)
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, surface->depth);
	}
	phCheckFBO();
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    phCheckError("Creation of the FBO itself");
}

void phCheckFBO()
{
    char enums[][20] =
    {
        "attachment",         // GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT........... All framebuffer attachment points are 'framebuffer attachment complete'.
        "missing attachment", // GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT....There is at least one image attached to the framebuffer.
        "",                   //
        "dimensions",         // GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT............All attached images have the same width and height.
        "formats",            // GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT...............All images attached to the attachment points COLOR_ATTACHMENT0_EXT through COLOR_ATTACHMENTn_EXT must have the same internal format.
        "draw buffer",        // GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT...........The value of FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT must not be NONE for any color attachment point(s) named by DRAW_BUFFERi.
        "read buffer",        // GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT...........If READ_BUFFER is not NONE, then the value of FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT must not be NONE for the color attachment point named by READ_BUFFER.
        "unsupported format"  // GL_FRAMEBUFFER_UNSUPPORTED_EXT......................The combination of internal formats of the attached images does not violate an implementation-dependent set of restrictions.
    };

    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if (status == GL_FRAMEBUFFER_COMPLETE_EXT)
        return;

    status -= GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT;

	Cvar_Set("gl_framebuffer", "0");
	Sys_Error ("incomplete framebuffer object due to %s", enums[status]);
}

void phCheckError(const char *call)
{
    char enums[][20] =
    {
        "invalid enumeration", // GL_INVALID_ENUM
        "invalid value",       // GL_INVALID_VALUE
        "invalid operation",   // GL_INVALID_OPERATION
        "stack overflow",      // GL_STACK_OVERFLOW
        "stack underflow",     // GL_STACK_UNDERFLOW
        "out of memory"        // GL_OUT_OF_MEMORY
    };

    GLenum errcode = glGetError();
    if (errcode == GL_NO_ERROR)
        return;

    errcode -= GL_INVALID_ENUM;
	Cvar_Set("gl_framebuffer", "0");
	Sys_Error ("OpenGL %s in '%s'", enums[errcode], call);
}

void phBindSurface(const PHsurface *surface, qboolean msaa)
{
	if (msaa && framebuffer_multisample_ext == true)
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, surface->fbo_msaa);
	else
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, surface->fbo);

    glViewport(surface->viewport.x, surface->viewport.y, surface->viewport.width, surface->viewport.height);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(surface->projection);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(surface->modelview);
    boundSurface = surface;
}

void phClearSurface()
{
    const PHsurface *surface = boundSurface;
    glClearColor(surface->clearColor[0], surface->clearColor[1], surface->clearColor[2], surface->clearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT | (surface->depth ? GL_DEPTH_BUFFER_BIT : 0));
}

GLuint phCompile(const char *vert, const char *frag)
{
    GLchar buf[256];
    GLuint vertShader, fragShader, program;
    GLint success;

    vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertShader, 1, (const GLchar**) &vert, 0);
    glCompileShader(vertShader);
    glGetShaderiv(vertShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertShader, sizeof(buf), 0, buf);

		Con_DPrintf("%s\n", buf);
		Cvar_Set("gl_framebuffer", "0");
		Sys_Error ("Unable to compile vertex shader.\n\n%s", buf);
    }

    fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, (const GLchar**) &frag, 0);
    glCompileShader(fragShader);
    glGetShaderiv(fragShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragShader, sizeof(buf), 0, buf);

		Con_DPrintf("%s\n", buf);
		Cvar_Set("gl_framebuffer", "0");
		Sys_Error ("Unable to compile fragment shader.\n\n%s", buf);
    }

    program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(program, sizeof(buf), 0, buf);

		Con_DPrintf("%s\n", buf);
		Cvar_Set("gl_framebuffer", "0");
		Sys_Error ("Unable to link shaders.\n\n%s", buf);
    }

    return program;
}

void phNormalize(PHvec3 *v)
{
    GLfloat mag = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
    if (mag)
    {
        v->x /= mag;
        v->y /= mag;
        v->z /= mag;
    }
}

PHvec3 phAdd(const PHvec3 *a, const PHvec3 *b)
{
    PHvec3 c;
    c.x = a->x + b->x;
    c.y = a->y + b->y;
    c.z = a->z + b->z;
    return c;
}

PHvec3 phSub(const PHvec3 *a, const PHvec3 *b)
{
    PHvec3 c;
    c.x = a->x - b->x;
    c.y = a->y - b->y;
    c.z = a->z - b->z;
    return c;
}

#endif