#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define EQUILATERAL_Y 0.8660254037844386
#define SQRT3 1.7320508075688772

static const GLchar *vshadersrc =
	"#version 330 core\n"
	"layout (location = 0) in vec3 position;\n"
	"out vec3 vnormal;\n"
	"uniform mat4 projection;\n"
	"void main()\n"
	"{\n"
	"    gl_Position = projection * vec4(position, 1.0);\n"
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

GLfloat *
initclothvert(const size_t width, const size_t height, size_t *asize)
{
	const size_t numvert = width * height;
	size_t i;
	GLfloat *a;
	const GLfloat side = fminf(2.0f / (width - 0.5f), 4.0f / (height - 1) * SQRT3);
	const GLfloat dy = side * SQRT3 / 2.0f;
	const GLfloat offy = height * dy / 2;
	*asize = 3 * numvert;
	if ((a = malloc(*asize * sizeof(GLfloat)))) {
		for (i = 0; i < numvert; ++i) {
			a[3 * i] = (i % width) * side - 1.0f;
			if (i / width % 2)
				a[3 * i] += side / 2;
			a[3 * i + 1] = (i / width) * dy - offy;
			a[3 * i + 2] = (float) (rand() % 32) / 256 -1.0f;
		}
	}
	return a;
}

GLuint *
initclothtri(const size_t width, const size_t height, size_t *asize)
{
	const size_t numtri = 2 * (width - 1) * (height - 1);
	const size_t numvert = width * height;
	GLuint *a;
	size_t i, j = 0;
	if (width >= 2 && height >= 2) {
		*asize = 3 * numtri;
		if (!(a = malloc(*asize * sizeof(GLuint))))
			return NULL;
		for (i = 0; i + width < numvert; ++i) {
			if (!((i + 1) % width))
				continue;
			if (i / width % 2) {
				a[j++] = i;
				a[j++] = i + width;
				a[j++] = i + width + 1;
				a[j++] = i;
				a[j++] = i + width + 1;
				a[j++] = i + 1;
			} else {
				a[j++] = i;
				a[j++] = i + width;
				a[j++] = i + 1;
				a[j++] = i + 1;
				a[j++] = i + width;
				a[j++] = i + width + 1;
			}
		}
		return a;
	} else {
		return NULL;
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
			a[3 * (i * width + j) + 2] = (float) (rand() % 32) / 256 - 1.0;
	}
}

void
keycallb(GLFWwindow *window, int key, int scancode, int action, int mode)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}

int
main(void)
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

	GLFWwindow *window = glfwCreateWindow(1280, 720, "Cybweb", NULL, NULL);
	if (!window) {
		fputs("Failed to create GLFW window.\n", stderr);
		goto e_glfw;
	}
	glfwMakeContextCurrent(window);

	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK) {
		fputs("Failed to initialize GLEW.\n", stderr);
		goto e_glfw;
	}

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	glViewport(0, 0, width, height);

	size_t wwidth = 16;
	size_t wheight = 9;
	size_t nvert, nind;
	GLfloat *vertices = initclothvert(wwidth, wheight, &nvert);
	if (!vertices) {
		fputs("Failed to alocate vertices.\n", stderr);
		goto e_glfw;
	}
	GLuint *indices = initclothtri(wwidth, wheight, &nind);
	if (!indices) {
		fputs("Failed to alocate triangles.\n", stderr);
		goto e_vert;
	}

	GLint success;
	GLchar inflog[512];

	/* vertex shader */
	GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vshader, 1, &vshadersrc, NULL);
	glCompileShader(vshader);
	glGetShaderiv(vshader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vshader, 512, NULL, inflog);
		fprintf(stderr, "Error: vertex shader compilation failed.\n%s\n", inflog);
		goto e_vs;
	}

	/* geometry shader */
	GLuint gshader = glCreateShader(GL_GEOMETRY_SHADER);
	glShaderSource(gshader, 1, &gshadersrc, NULL);
	glCompileShader(gshader);
	glGetShaderiv(gshader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(gshader, 512, NULL, inflog);
		fprintf(stderr, "Error: geometry shader compilation failed.\n%s\n", inflog);
		goto e_gs;
	}

	/* fragment shader */
	GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fshader, 1, &fshadersrc, NULL);
	glCompileShader(fshader);
	glGetShaderiv(fshader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fshader, 512, NULL, inflog);
		fprintf(stderr, "Error: fragment shader compilation failed.\n%s\n", inflog);
		goto e_fs;
	}

	/* shader program */
	GLuint shaderp = glCreateProgram();
	glAttachShader(shaderp, vshader);
	glAttachShader(shaderp, gshader);
	glAttachShader(shaderp, fshader);
	glLinkProgram(shaderp);
	glGetProgramiv(shaderp, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(shaderp, 512, NULL, inflog);
		fprintf(stderr, "Error: shader program linking failed.\n%s\n", inflog);
		goto e_fs;
	}
	glDeleteShader(vshader);
	glDeleteShader(fshader);
	
	GLuint VBO, VAO, EBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	/* triangle */
	glBindVertexArray(VAO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, nvert * sizeof(GLfloat), vertices, GL_STREAM_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, nind * sizeof(GLuint), indices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *) 0);
	glEnableVertexAttribArray(0);

	GLfloat projection[16];
	matproj(projection, 1.0f, 9.0f / 16.0f, 0.01f, 1.0f);
	glfwSetKeyCallback(window, keycallb);
	while (!glfwWindowShouldClose(window)) {
		glClearColor(0.1f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		/* draw web */
		GLfloat time = glfwGetTime() * 2;
		const GLfloat langle = 0.5;
		GLuint projloc = glGetUniformLocation(shaderp, "projection");
		GLint llocation = glGetUniformLocation(shaderp, "light");
		glUniformMatrix4fv(projloc, 1, GL_FALSE, projection);
		glUniform3f(llocation, sin(langle) * cos(time), sin(langle) * sin(time), cos(langle));
		glUseProgram(shaderp);
		//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, nind, GL_UNSIGNED_INT, 0);

		glBindVertexArray(0);
		glfwSwapBuffers(window);
		//randomize(wwidth, wheight, vertices);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, nvert * sizeof(GLfloat), vertices, GL_STREAM_DRAW);
		glfwPollEvents();
	}
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	free(indices);
	free(vertices);
	glfwTerminate();
	puts("Success!");
	return 0;

	/* error cleanup */
	e_fs:
	glDeleteShader(fshader);
	e_gs:
	glDeleteShader(gshader);
	e_vs:
	glDeleteShader(vshader);
	free(indices);
	e_vert:
	free(vertices);
	e_glfw:
	glfwTerminate();
	return EXIT_FAILURE;
}
