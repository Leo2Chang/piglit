/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/** \file builtin-gl-num-samples.cpp
 *  This test verifies that using gl_NumSamples in fragment shader
 *  program works as per ARB_sample_shading specification.
 *
 **/

#include "piglit-fbo.h"
using namespace piglit_util_fbo;

const int pattern_width = 128; const int pattern_height = 128;

PIGLIT_GL_TEST_CONFIG_BEGIN

	config.supports_gl_compat_version = 10;

	config.window_width = pattern_width;
	config.window_height = pattern_height;
	config.window_visual = PIGLIT_GL_VISUAL_DOUBLE | PIGLIT_GL_VISUAL_RGBA;

PIGLIT_GL_TEST_CONFIG_END

static int  num_samples;
static unsigned prog, vao, vertex_buf;
static Fbo multisampled_fbo, singlesampled_fbo;

static void
print_usage_and_exit(char *prog_name)
{
	printf("Usage: %s <num_samples>\n", prog_name);
	piglit_report_result(PIGLIT_FAIL);
}

void
compile_shader(void)
{
	static const char *vert =
		"#version 130\n"
		"uniform mat4 proj;\n"
		"in vec2 pos;\n"
		"void main()\n"
		"{\n"
		"  gl_Position = vec4(pos, 0.0, 1.0);\n"
		"}\n";
	static const char *frag =
		"#version 130\n"
		"#extension GL_ARB_sample_shading : require\n"
		"uniform int samples;\n"
		"out vec4 out_color;\n"
		"void main()\n"
		"{\n"
		"  if (gl_NumSamples == samples)\n"
		"    out_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		"  else\n"
		"    out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
		"}\n";
	/* Compile program */
	prog = glCreateProgram();
	GLint vs = piglit_compile_shader_text(GL_VERTEX_SHADER, vert);
	glAttachShader(prog, vs);
	piglit_check_gl_error(GL_NO_ERROR);
	GLint fs = piglit_compile_shader_text(GL_FRAGMENT_SHADER, frag);
	glAttachShader(prog, fs);
	glBindAttribLocation(prog, 0, "pos");
	glLinkProgram(prog);
	if (!piglit_link_check_status(prog)) {
		piglit_report_result(PIGLIT_FAIL);
	}

	/* Set up vertex array object */
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	/* Set up vertex input buffer */
	glGenBuffers(1, &vertex_buf);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float),
			      (void *) 0);

	/* Set up element input buffer to tesselate a quad into
	 * triangles
	 */
	unsigned int indices[6] = { 0, 1, 2, 0, 2, 3 };
	GLuint element_buf;
	glGenBuffers(1, &element_buf);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buf);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
		     GL_STATIC_DRAW);
}

void
draw_pattern_glsl(const Fbo *src_fbo, GLint samples)
{
	float vertex_data[4][4] = {
		{ -1, -1 },
		{ -1,  1 },
		{  1,  1 },
		{  1, -1 }};

	glUseProgram(prog);
	glBindVertexArray(vao);

	/* Set up uniforms */
	glUseProgram(prog);
	glUniform1i(glGetUniformLocation(prog, "samples"), samples);

	glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data,
		     GL_STREAM_DRAW);
	glDrawElements(GL_TRIANGLES, 6 , GL_UNSIGNED_INT, (void *) 0);
}

void
piglit_init(int argc, char **argv)
{
	if (argc != 2)
		print_usage_and_exit(argv[0]);

	/* 1st arg: num_samples */
	char *endptr = NULL;
	num_samples = strtol(argv[1], &endptr, 0);
	if (endptr != argv[1] + strlen(argv[1]))
		print_usage_and_exit(argv[0]);

	piglit_require_gl_version(21);
	piglit_require_extension("GL_ARB_vertex_array_object");
	piglit_require_extension("GL_ARB_sample_shading");

	/* Skip the test if num_samples > GL_MAX_SAMPLES */
	GLint max_samples;
	glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
	if (num_samples > max_samples)
		piglit_report_result(PIGLIT_SKIP);

	singlesampled_fbo.setup(FboConfig(0,
					  pattern_width,
					  pattern_height));

	FboConfig msConfig(num_samples, pattern_width, pattern_height);
	multisampled_fbo.setup(msConfig);

	compile_shader();
	if (!piglit_check_gl_error(GL_NO_ERROR)) {
		piglit_report_result(PIGLIT_FAIL);
	}
}

bool test_ms_blit_scaled(Fbo ms_fbo)
{
	GLint samples;
        GLfloat expected[4] = {0.0, 1.0, 0.0, 1.0};
	bool pass = true;

	/* Draw the test pattern into the framebuffer with texture
	 * attachment.
	 */
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ms_fbo.handle);
	glGetIntegerv(GL_SAMPLES, &samples);

	glClear(GL_COLOR_BUFFER_BIT);
	draw_pattern_glsl(&ms_fbo, samples);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, ms_fbo.handle);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, singlesampled_fbo.handle);
	glClear(GL_COLOR_BUFFER_BIT);
	glBlitFramebuffer(0, 0,
			  pattern_width, pattern_height,
			  0, 0,
			  pattern_width, pattern_height,
			  GL_COLOR_BUFFER_BIT,
			  GL_NEAREST);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, singlesampled_fbo.handle);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, piglit_winsys_fbo);

	 pass = piglit_probe_rect_rgba(0, 0, pattern_width, pattern_width,
                                       expected)  && pass;
	glClear(GL_COLOR_BUFFER_BIT);
	glBlitFramebuffer(0, 0,
			  pattern_width, pattern_height,
			  0, 0,
			  pattern_width, pattern_height,
			  GL_COLOR_BUFFER_BIT,
			  GL_NEAREST);

	piglit_present_results();
	return pass;
}

enum piglit_result
piglit_display()
{
	bool pass = true;
	pass = test_ms_blit_scaled(multisampled_fbo)
               && pass;
	return pass ? PIGLIT_PASS : PIGLIT_FAIL;
}
