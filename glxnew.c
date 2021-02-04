#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/glx.h>

#define SQRT3_2 0.8660254037844386f
#define SQRT3 1.7320508075688772f
#define TWO_PI 6.283185307179586f
#define LOG_MAX_LENGTH 512

/* normally declared in math.h */
#ifndef M_PI_2
#define M_PI_2 1.5707963267948966f
#endif

#define Z_COORD(v, i) v[3 * (i) + 2]

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig,
                                                     GLXContext, Bool,
                                                     const int*);

static const GLchar *vshadersrc =
	"#version 330 core\n"
	"layout (location = 0) in vec3 position;\n"
	"out vec3 vnormal;\n"
	"uniform mat4 view;\n"
	"uniform mat4 projection;\n"
	"void main()\n"
	"{\n"
	"    gl_Position = projection * view * vec4(position, 1.0);\n"
	"}";

static const GLchar *gshadersrc =
	"#version 330 core\n"
	"layout (triangles) in;\n"
	"layout (triangle_strip, max_vertices=3) out;\n"
	"out vec3 normal;\n"
	"void main()\n"
	"{\n"
	"    normal = normalize(cross(gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz, gl_in[2].gl_Position.xyz - gl_in[0].gl_Position.xyz));\n"
	"    for (int i = 0; i < gl_in.length(); ++i) {\n"
	"        gl_Position = gl_in[i].gl_Position;\n"
	"        EmitVertex();\n"
	"    }\n"
	"}";

static const GLchar *fshadersrc =
	"#version 330 core\n"
	"in vec3 normal;\n"
	"out vec4 color;\n"
	"uniform vec3 light;\n"
	"void main()\n"
	"{\n"
	"    float a = -dot(normal, light);\n"
	"    color = vec4(0.5f * (1 + a), 0.375f * (1 + a), 0.0f, 1.0f);\n"
	"}";

static int sigclose = 0;

void
kill(int param)
{
	sigclose = 1;
}

float
sqr(const float x)
{
	return x * x;
}

size_t
numvert(const size_t width, const size_t height)
{
	return width * height;
}

size_t
numind(const size_t width, const size_t height)
{
	return 6 * (width - 1) * (height - 1);
}

void
initvert(const size_t width, const size_t height, GLfloat v[])
{
	const size_t n = numvert(width, height);
	const GLfloat side = fminf(2.0f / (width - 0.5f), 4.0f / (height - 1) * SQRT3);
	const GLfloat dy = side * SQRT3_2;
	const GLfloat offy = (height - 1) * dy / 2.0f;
	if (n) {
		for (size_t i = 0; i < n; ++i) {
			v[3 * i] = (i % width) * side - 1.0f;
			if (i / width % 2)
				v[3 * i] += side / 2.0f;
			v[3 * i + 1] = (i / width) * dy - offy;
			v[3 * i + 2] = 0.0f; //(i == 72)? 0.5f : 0.0f; //(GLfloat) (rand() % 32 - 16) / 256 - 0.75f;
		}
	}
}

void
initind(const size_t width, const size_t height, GLuint ind[])
{
	const size_t n = numvert(width, height);
	size_t i = 0;
	if (numind(width, height)) {
		for (size_t v = 0; v < n - width; ++v) {
			if (!((v + 1) % width))
				continue;
			ind[i++] = v;
			ind[i++] = v + width;
			if (i / width % 2) {
				ind[i++] = v + width + 1;
				ind[i++] = v;
				ind[i++] = v + width + 1;
				ind[i++] = v + 1;
			} else {
				ind[i++] = v + 1;
				ind[i++] = v + 1;
				ind[i++] = v + width;
				ind[i++] = v + width + 1;
			}
		}
	}
}

void
matproj(GLfloat mat[16], const float hfov, const float vfov,
        const float n, const float f)
{
	const GLfloat d = f - n;
	mat[0] = 1.0f / tan(hfov);
	mat[1] = 0.0f;
	mat[2] = 0.0f;
	mat[3] = 0.0f;
	mat[4] = 0.0f;
	mat[5] = 1.0f / tan(vfov);
	mat[6] = 0.0f;
	mat[7] = 0.0f;
	mat[8] = 0.0f;
	mat[9] = 0.0f;
	mat[10] = - (f + n) / d;
	mat[11] = -1.0f;
	mat[12] = 0.0f;
	mat[13] = 0.0f;
	mat[14] = -2 * f * n / d;
	mat[15] = 0.0f;
}

void
matcam(GLfloat mat[16], const float d, const float r, const float a)
{
	const float x = r * cosf(a);
	const float y = r * sinf(a);
	const float n = sqrtf(sqr(d) + sqr(r));
	const float root = sqrtf(1.0f - sqr(y / d));
	mat[0] = (d + sqr(y) / d) / (n * root);
	mat[1] = 0.0f;
	mat[2] = x / n;
	mat[3] = 0.0f;
	
	mat[4] = -x * y / (n * root * d);
	mat[5] = 1 / root;
	mat[6] = y / n;
	mat[7] = 0.0f;

	mat[8] = -x / (n * root);
	mat[9] = -y / (root * d);
	mat[10] = d / n;
	mat[11] = 0.0f;

	mat[12] = -x * mat[0] - y * mat[4] - d * mat[8];
	mat[13] = -x * mat[5] - d * mat[9];
	mat[14] = -(sqr(r) + sqr(d)) / n;
	mat[15] = 1.0f;
}

void
matid(GLfloat mat[16])
{
	size_t i, j;
	for (i = 0; i < 4; ++i) {
		for (j = 0; j < 4; ++j)
			mat[4 * i + j] = i == j;
	}
}

void
randomize(const size_t width, const size_t height, GLfloat *const a)
{
	size_t i, j;
	for (i = 0; i < height; ++i) {
		for (j = 0; j < width; ++j)
			a[3 * (i * width + j) + 2] = (float) (rand() % 128 - 64) / 256 - 0.75f;
	}
}

float
force(const size_t width, const size_t height,
      float vcur[], float vprev[], const size_t i)
{
	const float k = 0.125;
	const float p = 0.1;
	const float f = 0.5;
	const float z = Z_COORD(vcur, i);
	float a = 0.0f;

	/* left */
	if (i % width) {
		a -= z - Z_COORD(vcur, i - 1);
		if (i / width % 2) {
			/* row 0 is never odd */
			a -= z - Z_COORD(vcur, i - width);
			if (i / width + 1 < height)
				a -= z - Z_COORD(vcur, i + width);
		} else {
			if (i > width)
				a -= z - Z_COORD(vcur, i - width - 1);
			if (i / width + 1 < height)
				a -= z - Z_COORD(vcur, i + width - 1);
		}
	}

	/* right */
	if ((i + 1) % width) {
		a -= z - Z_COORD(vcur, i + 1);
		if (i / width % 2) {
			/* row 0 is never odd */
			a -= z - Z_COORD(vcur, i - width + 1);
			if (i / width + 1 < height)
				a -= z - Z_COORD(vcur, i + width + 1);
		} else {
			if (i > width)
				a -= z - Z_COORD(vcur, i - width);
			if (i / width + 1 < height)
				a -= z - Z_COORD(vcur, i + width);
		}
	}
	if (!(i % width || rand() % 128))
		return 2.0f;
	else
		return p * a - k * z - f * (z - Z_COORD(vprev, i));
}

void
move(const size_t width, const size_t height, float vcur[], float vprev[])
{
	const size_t n = numvert(width, height);
	const float h = 0.1;
	size_t v;
	float temp[3 * n];
	memcpy(temp, vcur, 3 * n * sizeof(float));
	for (v = 0; v < n; ++v)
		Z_COORD(vcur, v) = 2 * Z_COORD(vcur, v) - Z_COORD(vprev, v) + force(width, height, temp, vprev, v) * h * h;
	memcpy(vprev, temp, 3 * n * sizeof(float));
}

/*
 * NOTE: This is the dirtiest function, and the only one using Xlib too.
 * Coincidence? Definitely not.
 */
void
setbkg(Display *const disp, Screen *const scr,
       char buffer[3 * scr->width * scr->height])
{
	const int depth = DefaultDepth(disp, DefaultScreen(disp));
	Window root = RootWindow(disp, DefaultScreen(disp));
	XImage *img;
	Pixmap pix;
	XGCValues gcval;
	GC gc;
	img = XCreateImage(disp, CopyFromParent, depth, ZPixmap, 0,
	                   buffer, scr->width, scr->height, 32, 0);
	pix = XCreatePixmap(disp, root, scr->width, scr->height, depth); 
	gc = XCreateGC(disp, pix, 0, &gcval);
	XPutImage(disp, pix, gc, img, 0, 0, 0, 0, scr->width, scr->height);
	XSetWindowBackgroundPixmap(disp, root, pix);
	XFreeGC(disp, gc);
	XFreePixmap(disp, pix);
	/* FIXME: XDestroyImage is unlinkable and causes SIGABRT. Fuck Xlib! */
	/* XDestroyImage(img); */
	XClearWindow(disp, root);
	XFlush(disp);
}

int
mkshader(GLuint *const s, const GLint type, char *const inflog,
         const char **const src, const char *const name)
{
	int success;
	*s = glCreateShader(type);
	glShaderSource(*s, 1, src, NULL);
	glCompileShader(*s);
	glGetShaderiv(*s, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(*s, LOG_MAX_LENGTH, NULL, inflog);
		fprintf(stderr, "Error: %s compilation failed.\n%s\n",
		        name, inflog);
		return 0;
	}
	printf("%s compilation succeeded.\n", name);
	return 1;
}

#define MAKE_SHADER(s, TYPE, src, name, lbl) \
	s = glCreateShader(GL_##TYPE##_SHADER); \
	glShaderSource(s, 1, &src, NULL); \
	glCompileShader(s); \
	glGetShaderiv(s, GL_COMPILE_STATUS, &success); \
	if (!success) { \
		glGetShaderInfoLog(s, LOG_MAX_LENGTH, NULL, inflog); \
		fprintf(stderr, "Error: "name" compilation failed.\n%s\n", inflog); \
		goto lbl; \
	}

int
mkpgr(GLuint *const sp, const GLchar *vs, const GLchar *gs, const GLchar *fs)
{
	GLchar inflog[LOG_MAX_LENGTH];
	GLuint v, g, f;
	GLint success;
	if (!mkshader(&v, GL_VERTEX_SHADER, inflog, &vs, "vertex shader"))
		goto errv;
	if (!mkshader(&g, GL_GEOMETRY_SHADER, inflog, &gs, "geometry shader"))
		goto errg;
	if (!mkshader(&f, GL_FRAGMENT_SHADER, inflog, &fs, "fragment shader"))
		goto errf;
	*sp = glCreateProgram();
	glAttachShader(*sp, v);
	glAttachShader(*sp, g);
	glAttachShader(*sp, f);
	glLinkProgram(*sp);
	glGetProgramiv(*sp, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(*sp, LOG_MAX_LENGTH, NULL, inflog);
		fprintf(stderr, "Error: shader program linking failed.\n%s\n", inflog);
	}
	errf:
	glDeleteShader(f);
	errg:
	glDeleteShader(g);
	errv:
	glDeleteShader(v);
	return success;
}

int
mkcontext(Display *const disp, const int msaa, GLXContext *const retcontext)
{
	int visattr[] = {
		GLX_RGBA,
		GLX_DOUBLEBUFFER,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DEPTH_SIZE, 1,
		GLX_SAMPLE_BUFFERS, 1,
		GLX_SAMPLES, msaa,
		None
	};
	int contextattr[] = {
		GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
		GLX_CONTEXT_MINOR_VERSION_ARB, 3,
		None
	};
	glXCreateContextAttribsARBProc glXCreateContextAttribsARB;
	GLXContext context;
	GLXFBConfig fbconfig;
	XVisualInfo *vis = glXChooseVisual(disp, DefaultScreen(disp), visattr);
	if (!vis) {
		fprintf(stderr,
		        "Error: failed to get a %dx MSAA RGB visual.\n", msaa);
		return 0;
	}
	/* TODO: get best framebuffer config */
	int numfbconfig;
	GLXFBConfig *fbconfigs = glXChooseFBConfig(disp, DefaultScreen(disp), visattr, &numfbconfig);
	fbconfig = NULL;
	for (int i = 0; i < numfbconfig; ++i) {
		XVisualInfo *vis = glXGetVisualFromFBConfig(disp, fbconfigs[i]);
		if (!vis)
			continue;
		XRenderPictFormat *fmt = XRenderFindVisualFormat(disp, vis->visual);
		if (!fmt)
			continue;
		fbconfig = fbconfigs[i];
		if (fmt->direct.alphaMask > 0)
			break;
	}
	if (!fbconfig) {
		fputs("Error: no FB config found.\n", stderr);
		return 0;
	}
	glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc) glXGetProcAddressARB((const GLubyte*) "glXCreateContextAttribsARB");
	if (!glXCreateContextAttribsARB) {
		fputs("Error: could not load glXCreateContextAttribsARB\n", stderr);
		//XFree(vis);
		return 0;
	}
	context = glXCreateContextAttribsARB(disp, fbconfig, NULL, True, contextattr);
	if (!context) {
		fputs("Error: failed to create an OpenGL context.\n", stderr);
		//XFree(vis);
		return 0;
	}
	//XFree(vis);
	*retcontext = context;
	return 1;
}

int
graphics(const size_t wwidth, const size_t wheight,
         Display *const disp, Screen *const scr, const float t)
{
	const size_t numv = numvert(wwidth, wheight);
	const size_t numi = numind(wwidth, wheight);
	struct tm *localt;
	GLfloat vcur[3 * numv];
	GLfloat vprev[3 * numv];
	GLuint ind[numi];
	GLfloat view[16];
	GLfloat projection[16];
	GLuint sp;
	GLuint vbo, vao, ebo;
	Window root = RootWindow(disp, DefaultScreen(disp));
	GLXContext context;

	/* OpenGL context */
	if (!mkcontext(disp, 8, &context))
		return EXIT_FAILURE;
	if (!glXMakeCurrent(disp, root, context)) {
		fputs("Error: failed to make root current window.\n", stderr);
		return EXIT_FAILURE;
	}
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK) {
		fputs("Failed to initialize GLEW.\n", stderr);
		goto errcontext;
	}

	/* vertices and tris */
	initvert(wwidth, wheight, vcur);
	memcpy(vprev, vcur, 3 * numv * sizeof(GLfloat));
	//initvert(wwidth, wheight, vprev);
	initind(wwidth, wheight, ind);

	glViewport(0, 0, scr->width, scr->height);
	//glEnable(GL_DEPTH_TEST);
	//glEnable(GL_MULTISAMPLE);

	if (!mkpgr(&sp, vshadersrc, gshadersrc, fshadersrc))
		goto errcontext;

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);

	/* web */
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, 3 * numv * sizeof(GLfloat), vcur, GL_STREAM_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, numi * sizeof(GLuint), ind, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void *) 0);
	glEnableVertexAttribArray(0);

	matproj(projection, 0.75f, 0.75f * 9.0f / 16.0f, 0.01f, 4.0f);
	signal(SIGTERM, kill);
	while (!sigclose) {
		glClearColor(0.1f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		/* draw web */
		const time_t tempt = time(NULL);
		localt = localtime(&tempt);
		const GLfloat lrot = TWO_PI * (localt->tm_hour + (localt->tm_min + localt->tm_sec / 60.0f) / 60.0f) / 24.0f - M_PI_2;
		GLfloat time = (GLfloat) clock() / CLOCKS_PER_SEC;
		const GLfloat langle = 0.5;
		matcam(view, 0.5f, 0.05f, time / 2.0f);
		GLuint viewloc = glGetUniformLocation(sp, "view");
		GLuint projloc = glGetUniformLocation(sp, "projection");
		GLint llocation = glGetUniformLocation(sp, "light");
		glUniformMatrix4fv(viewloc, 1, GL_FALSE, view);
		glUniformMatrix4fv(projloc, 1, GL_FALSE, projection);
		glUniform3f(llocation, sin(langle) * cos(lrot), sin(langle) * sin(lrot), cos(langle));
		glUseProgram(sp);
		//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glBindVertexArray(vao);
		glDrawElements(GL_TRIANGLES, numi, GL_UNSIGNED_INT, 0);

		glBindVertexArray(0);
		glXSwapBuffers(disp, root);

		/* transfer to root */
		//char buffer[4 * scr->width * scr->height];
		//glReadPixels(0, 0, scr->width, scr->height, GL_BGRA, GL_UNSIGNED_BYTE, buffer);
		//setbkg(disp, scr, buffer);


		/* movements */
		move(wwidth, wheight, vcur, vprev);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, 3 * numv * sizeof(GLfloat), vcur, GL_STREAM_DRAW);
		sleep(t);
	}
	signal(SIGTERM, SIG_DFL);
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);
	glXMakeCurrent(disp, None, NULL);
	glXDestroyContext(disp, context);
	puts("Success!");
	return EXIT_SUCCESS;

	errcontext:
	glXMakeCurrent(disp, None, NULL);
	glXDestroyContext(disp, context);
	return EXIT_FAILURE;
}

int
main(void)
{
	Display *const disp = XOpenDisplay(NULL);
	if (disp) {
		Screen *const scr = ScreenOfDisplay(disp, DefaultScreen(disp));
		const int r = graphics(16, 9, disp, scr, 1.0f / 15);
		XCloseDisplay(disp);
		return r;
	} else {
		fputs("Error: failed to open X display.\n", stderr);
		return EXIT_FAILURE;
	}
}
