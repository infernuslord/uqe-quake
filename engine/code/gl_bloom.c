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
// gl_bloom.c: 2D lighting post process effect

// Developed by Jacques Krige
// Ultimate Quake Engine
// http://www.corvinstein.com


#include "quakedef.h"
#include "winquake.h"


#define BLOOM_FILTER_COUNT  3
#define KERNEL_SIZE   3
#define STRINGIFY(A)  #A

#include "../shaders/combine4f.glsl"
#include "../shaders/row3f.glsl"


PHsurface pass0[BLOOM_FILTER_COUNT];
PHsurface pass1[BLOOM_FILTER_COUNT];
GLuint combineProg;
GLuint filterProg;

cvar_t gl_bloom = {"gl_bloom", "1", true};

qboolean bloom_initialized = false;
float kernel[KERNEL_SIZE] = { 5, 6, 5 };


void blur(PHsurface *sources, PHsurface *dests, int count, float offset, Direction dir)
{
    GLint loc;
    int p;


	// set up the filter
    glUseProgram(filterProg);
    loc = glGetUniformLocation(filterProg, "source");
    glUniform1i(loc, 0);
    loc = glGetUniformLocation(filterProg, "coefficients");
    glUniform1fv(loc, KERNEL_SIZE, kernel);
    loc = glGetUniformLocation(filterProg, "offsetx");
    glUniform1f(loc, 0);
    loc = glGetUniformLocation(filterProg, "offsety");
    glUniform1f(loc, 0);
    if (dir == HORIZONTAL)
        loc = glGetUniformLocation(filterProg, "offsetx");


    // perform the blurring
    for (p = 0; p < count; p++)
    {
        float offset2;
		
		if (dir == HORIZONTAL)
			offset2 = offset / (float)sources[p].width;

		if (dir == VERTICAL)
			offset2 = offset / (float)sources[p].height;

        glUniform1f(loc, offset2);
        phBindSurface(dests + p, false);
		GL_Bind(sources[p].texture);

		glBegin(GL_QUADS);
        glTexCoord2i(0, 0); glVertex2i(-1, -1);
		glTexCoord2i(0, 1); glVertex2i(-1, 1);
		glTexCoord2i(1, 1); glVertex2i(1, 1);
        glTexCoord2i(1, 0); glVertex2i(1, -1);
        glEnd();
    }
}

void R_Bloom_Init(void)
{
	int p, c;
	float sum;
    GLsizei width;
    GLsizei height;

	if(framebuffer_ext == false)
		return;

	if(!framebuffer_initialized)
		return;

	if(bloom_initialized == true)
		return;


	// normalize kernel coefficients
    sum = 0;
    for (c = 0; c < KERNEL_SIZE; c++)
        sum += kernel[c];
    for (c = 0; c < KERNEL_SIZE; c++)
        kernel[c] /= sum;


	width = glwidth >> 1;
    height = glheight >> 1;


	// create pass surfaces
    for (p = 0; p < BLOOM_FILTER_COUNT; p++)
    {
        pass0[p].width = width;
        pass0[p].height = height;
        pass0[p].viewport.x = 0;
        pass0[p].viewport.y = 0;
        pass0[p].viewport.width = width;
        pass0[p].viewport.height = height;
        glGetFloatv(GL_MODELVIEW_MATRIX, pass0[p].modelview);
        glGetFloatv(GL_MODELVIEW_MATRIX, pass0[p].projection);
        phCreateSurface(va("bloompass0tex%i", p), pass0 + p, GL_FALSE, GL_FALSE, GL_TRUE);
        width = width >> 1;
        height = height >> 1;
    }

	width = glwidth;
	height = glheight;
	for (p = 0; p < BLOOM_FILTER_COUNT; p++)
    {
        pass1[p].width = width;
        pass1[p].height = height;
        pass1[p].viewport.x = 0;
        pass1[p].viewport.y = 0;
        pass1[p].viewport.width = width;
        pass1[p].viewport.height = height;
        glGetFloatv(GL_MODELVIEW_MATRIX, pass1[p].modelview);
        glGetFloatv(GL_MODELVIEW_MATRIX, pass1[p].projection);
		phCreateSurface(va("bloompass1tex%i", p), pass1 + p, GL_FALSE, GL_FALSE, GL_TRUE);
        width = width >> 1;
        height = height >> 1;
    }


	// compile shaders
	combineProg = phCompile(passv, combine4f);
	filterProg = phCompile(passv, row3f);

	bloom_initialized = true;
}

void R_Bloom_Process(void)
{
	int p;
    GLint loc;

	if(!bloom_initialized)
		return;

	if(!gl_bloom.value)
		return;


	// pass filter into pass0[0]
    glUseProgram(passProg);
    loc = glGetUniformLocation(passProg, "source");
    glUniform1i(loc, 0);
    glEnable(GL_TEXTURE_2D);
	GL_Bind(scenebase.texture);
    phBindSurface(pass0, false);
	glBegin(GL_QUADS);
    glTexCoord2i(0, 0); glVertex2i(-1, -1);
	glTexCoord2i(0, 1); glVertex2i(-1, 1);
	glTexCoord2i(1, 1); glVertex2i(1, 1);
    glTexCoord2i(1, 0); glVertex2i(1, -1);
    glEnd();
    glUseProgram(0);


	// downsample the scene into the source surfaces
	glEnable(GL_TEXTURE_2D);
	GL_Bind(pass0[0].texture);

	for (p = 1; p < BLOOM_FILTER_COUNT; p++)
    {
        phBindSurface(pass0 + p, false);

		glBegin(GL_QUADS);
        glTexCoord2i(0, 0); glVertex2i(-1, -1);
		glTexCoord2i(0, 1); glVertex2i(-1, 1);
		glTexCoord2i(1, 1); glVertex2i(1, 1);
        glTexCoord2i(1, 0); glVertex2i(1, -1);
        glEnd();
    }
	

    // perform the horizontal blurring pass.
    blur(pass0, pass1, BLOOM_FILTER_COUNT, 1.0f, HORIZONTAL);

	// perform the vertical blurring pass.
    blur(pass1, pass0, BLOOM_FILTER_COUNT, 1.0f, VERTICAL);


	glUseProgram(combineProg);
    for (p = 0; p < BLOOM_FILTER_COUNT; p++)
    {
        char name[] = "Pass#";

        glActiveTexture(GL_TEXTURE0 + p);
		glEnable(GL_TEXTURE_2D);
		GL_Bind(pass0[p].texture);

        sprintf(name, "Pass%d", p);
        loc = glGetUniformLocation(combineProg, name);
        glUniform1i(loc, p);
    }


	// combine original scene
	glActiveTexture(GL_TEXTURE0 + BLOOM_FILTER_COUNT);

	if(cl.worldmodel && r_viewleaf->contents <= CONTENTS_WATER)
		GL_Bind(scenepass0.texture);
	else
		GL_Bind(scenebase.texture);

	glEnable(GL_TEXTURE_2D);
    loc = glGetUniformLocation(combineProg, "Scene");
    glUniform1i(loc, BLOOM_FILTER_COUNT);
}

void R_Bloom_Clear(void)
{
	int p;


	if(!bloom_initialized)
		return;

	if(!gl_bloom.value)
		return;

	for (p = 0; p < BLOOM_FILTER_COUNT; p++)
    {
        glActiveTexture(GL_TEXTURE0 + p);
        glDisable(GL_TEXTURE_2D);
    }

    glActiveTexture(GL_TEXTURE0 + BLOOM_FILTER_COUNT);
    glDisable(GL_TEXTURE_2D);
}
