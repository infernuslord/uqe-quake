// File:  ph.h
// Brief: Declares "Philip's GL utility API", or "ph" for short.

//#define PH_PI (3.14159265358979323846f)


#ifndef __PH_H__
#define __PH_H__

typedef struct PHviewportRec
{
    GLint x;
    GLint y;
    GLsizei width;
    GLsizei height;
}PHviewport;

struct PHsurfaceRec
{
    GLsizei width;
    GLsizei height;
    struct PHviewportRec viewport;
    GLfloat clearColor[4];
    GLfloat modelview[16];
    GLfloat projection[16];
    GLuint texture;
	GLuint texture_msaa;
    GLuint depth;
    GLuint fbo;
	GLuint fbo_msaa;
};

struct PHvec3Rec
{
    GLfloat x;
    GLfloat y;
    GLfloat z;
};

#endif


typedef struct PHviewportRec PHviewport;
typedef struct PHsurfaceRec PHsurface;
typedef struct PHvec3Rec PHvec3;

extern int msaa_samples;

void phCreateSurface(char *identifier, PHsurface *, GLboolean depth, GLboolean fp, GLboolean linear);

void phBindSurface(const PHsurface *surface, qboolean msaa);
void phClearSurface();
void phCheckFBO();
void phCheckError(const char *);
GLuint phCompile(const char *vert, const char *frag);
void phNormalize(PHvec3 *);
PHvec3 phAdd(const PHvec3 *, const PHvec3 *);
PHvec3 phSub(const PHvec3 *, const PHvec3 *);
