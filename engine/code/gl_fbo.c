/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// gl_fbo.c: 2D framebuffer object management

// Developed by Jacques Krige
// Ultimate Quake Engine
// http://www.corvinstein.com


#include "quakedef.h"
#include "winquake.h"


#define STRINGIFY(A)  #A

#include "../shaders/passv.glsl"
#include "../shaders/passf.glsl"


PHsurface window;
PHsurface scenebase;
PHsurface scenepass0;
PHsurface scenepass1;
GLuint passProg;

GLboolean compile_status;

cvar_t gl_framebuffer = {"gl_framebuffer", "1", true};
cvar_t gl_framebuffer_msaa = {"gl_framebuffer_msaa", "2", true};

qboolean framebuffer_initialized = false;


GLint R_FrameBuffer_Bind(char *identifier, int width, int height)
{
	int TexNum;
	byte	*data;

	data = malloc(width * height * 4);
	TexNum = GL_LoadTexture(identifier, "framebuffer", width, height, data, false, false, 4);
	free ( data );

	return TexNum;
}

void R_FrameBuffer_Init(void)
{
    GLsizei width;
    GLsizei height;

	if(framebuffer_ext == false)
		return;

	if(framebuffer_initialized == true)
		return;

	if (!GLEW_VERSION_2_0)
	{
		Con_Printf("OpenGL 2.0 is required\n");
		Cvar_Set("gl_framebuffer", "0");

		return;
	}

	if (!glewIsSupported("GL_EXT_framebuffer_object"))
	{
		Con_Printf("GL_EXT_framebuffer_object is required\n");
		Cvar_Set("gl_framebuffer", "0");

		return;
	}


	glGetBooleanv(GL_SHADER_COMPILER, &compile_status);
	if (!compile_status && gl_framebuffer.value)
	{
		Con_Printf("Shader compilation support is required\n");
		Cvar_Set("gl_framebuffer", "0");

		return;
	}


	if (!gl_framebuffer.value)
		return;


	// create window surface
    window.fbo = 0;
    window.depth = 0;
    window.width = glwidth;
    window.height = glheight;
    window.clearColor[0] = 0.0f;
    window.clearColor[1] = 0.0f;
    window.clearColor[2] = 0.0f;
    window.clearColor[3] = 0.0f;
    window.viewport.x = 0;
    window.viewport.y = 0;
    window.viewport.width = window.width;
    window.viewport.height = window.height;
    glLoadIdentity();
    glGetFloatv(GL_MODELVIEW_MATRIX, window.modelview);
	glOrtho(0, window.width, window.height, 0, -99999, 99999);
    glGetFloatv(GL_MODELVIEW_MATRIX, window.projection);
    glLoadIdentity();


	// create 3D scene surface
    width = glwidth;
    height = glheight;
    scenebase.width = width;
    scenebase.height = height;
    scenebase.clearColor[0] = 0;
    scenebase.clearColor[1] = 0;
    scenebase.clearColor[2] = 0;
    scenebase.clearColor[3] = 0;
    scenebase.viewport.x = 0;
    scenebase.viewport.y = 0;
    scenebase.viewport.width = width;
    scenebase.viewport.height = height;
    glGetFloatv(GL_MODELVIEW_MATRIX, scenebase.modelview);
    glGetFloatv(GL_MODELVIEW_MATRIX, scenebase.projection);
	phCreateSurface("sourcetex", &scenebase, GL_TRUE, GL_FALSE, GL_TRUE);


	width = width >> 1;
    height = height >> 1;

	// create 3D scene surface (first pass)
    scenepass0.width = width;
    scenepass0.height = height;
    scenepass0.clearColor[0] = 0;
    scenepass0.clearColor[1] = 0;
    scenepass0.clearColor[2] = 0;
    scenepass0.clearColor[3] = 0;
    scenepass0.viewport.x = 0;
    scenepass0.viewport.y = 0;
    scenepass0.viewport.width = width;
    scenepass0.viewport.height = height;
    glGetFloatv(GL_MODELVIEW_MATRIX, scenepass0.modelview);
    glGetFloatv(GL_MODELVIEW_MATRIX, scenepass0.projection);
	phCreateSurface("scenepass0", &scenepass0, GL_FALSE, GL_FALSE, GL_TRUE);

	// create 3D scene surface (second pass)
    scenepass1.width = width;
    scenepass1.height = height;
    scenepass1.clearColor[0] = 0;
    scenepass1.clearColor[1] = 0;
    scenepass1.clearColor[2] = 0;
    scenepass1.clearColor[3] = 0;
    scenepass1.viewport.x = 0;
    scenepass1.viewport.y = 0;
    scenepass1.viewport.width = width;
    scenepass1.viewport.height = height;
    glGetFloatv(GL_MODELVIEW_MATRIX, scenepass1.modelview);
    glGetFloatv(GL_MODELVIEW_MATRIX, scenepass1.projection);
	phCreateSurface("scenepass1", &scenepass1, GL_FALSE, GL_FALSE, GL_TRUE);


	// compile shaders
	passProg = phCompile(passv, passf);

	framebuffer_initialized = true;
}

void R_FrameBuffer_Begin(void)
{
	if(framebuffer_ext == false)
		return;

	if(!gl_framebuffer.value)
		return;

	R_FrameBuffer_Init();
	R_Bloom_Init();

	if(!framebuffer_initialized)
		return;


	if (!compile_status && gl_framebuffer.value)
	{
		Con_Printf("Shader compilation support is required\n");
		Cvar_Set("gl_framebuffer", "0");

		return;
	}

	
	if(msaa_samples > 0)
	{
		if (gl_framebuffer_msaa.value > (int)floor((log((float)msaa_samples)/log(2.0f)) + 0.5f))
			Cvar_Set("gl_framebuffer_msaa", va("%i", (int)floor((log((float)msaa_samples)/log(2.0f)) + 0.5f)));
	}
	else
	{
		if (gl_framebuffer_msaa.value > 0.0f)
			Cvar_Set("gl_framebuffer_msaa", "0");
	}

	if (gl_framebuffer_msaa.value < 0.0f)
		Cvar_Set("gl_framebuffer_msaa", "0");


    phBindSurface(&scenebase, true);
    phClearSurface();
}

void R_FrameBuffer_Process(void)
{
    GLint		loc;

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnable(GL_TEXTURE_2D);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);


	if (framebuffer_multisample_ext == true)
	{
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, scenebase.fbo_msaa);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, scenebase.fbo);
		glBlitFramebufferEXT(0, 0, scenebase.width, scenebase.height, 0, 0, scenebase.width, scenebase.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); // unbind all (render to back buffer)
	}


	// pass filter into scenepass0
    glUseProgram(passProg);
    loc = glGetUniformLocation(passProg, "source");
    glUniform1i(loc, 0);
    glEnable(GL_TEXTURE_2D);
	GL_Bind(scenebase.texture);
    phBindSurface(&scenepass0, false);
	glBegin(GL_QUADS);
    glTexCoord2i(0, 0); glVertex2i(-1, -1);
	glTexCoord2i(0, 1); glVertex2i(-1, 1);
	glTexCoord2i(1, 1); glVertex2i(1, 1);
    glTexCoord2i(1, 0); glVertex2i(1, -1);
    glEnd();
    glUseProgram(0);


	if(r_viewleaf->contents <= CONTENTS_WATER)
	{
		// perform the horizontal blurring pass.
		blur(&scenepass0, &scenepass1, 1, 1.5f, HORIZONTAL);

		// perform the vertical blurring pass.
		blur(&scenepass1, &scenepass0, 1, 1.5f, VERTICAL);
	}

	glUseProgram(0);

	
	if (gl_bloom.value == 0.0f)
	{
		glActiveTexture(GL_TEXTURE0);

		if(r_viewleaf->contents <= CONTENTS_WATER)
			GL_Bind(scenepass0.texture);
		else
			GL_Bind(scenebase.texture);

		glEnable(GL_TEXTURE_2D);
	}
}

void R_FrameBuffer_End(void)
{
	int p;
	GLint loc;

	if(framebuffer_ext == false)
		return;

	if(!gl_framebuffer.value)
		return;

	if(!framebuffer_initialized)
		return;

	R_FrameBuffer_Process();
	R_Bloom_Process();


	// draw to original window surface
	phBindSurface(&window, false);
	glBegin(GL_QUADS);
	glTexCoord2i(0, 1); glVertex2i(0, 0);
    glTexCoord2i(1, 1); glVertex2i(scenebase.width, 0);
    glTexCoord2i(1, 0); glVertex2i(scenebase.width, scenebase.height);
    glTexCoord2i(0, 0); glVertex2i(0, scenebase.height);
    glEnd();


	glUseProgram(0);

	R_Bloom_Clear();
	
	// back to normal window-system-provided framebuffer (unbind)
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

    glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D);

	//we probably need to delete the framebuffers somewhere using the glDeleteFramebuffersEXT function
}






















