/*
 * Copyright © 2012 Intel Corporation
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

/**
 * \file common.cpp
 *
 * This file defines the functions which can be utilized to develop new
 * multisample test cases. Functions can be utilized to:
 *
 * - Draw a test image to default framebuffer.
 * - Initialize test_fbo with specified sample count.
 * - Draw a test image to test_fbo.
 * - Draw a reference image.
 * - Verify the accuracy of multisample antialiasing in FBO.
 *
 * Accuracy verification is done by rendering a scene consisting of
 * triangles that aren't perfectly aligned to pixel coordinates. Every
 * triangle in the scene is rendered using a solid color whose color
 * components are all 0.0 or 1.0.  The scene is renederd in two ways:
 *
 * - At normal resoluation, using MSAA.
 *
 * - At very high resolution ("supersampled" by a factor of 16 in both
 *   X and Y dimensions), without MSAA.
 *
 * Then, the supersampled image is scaled down to match the resolution
 * of the MSAA image, using a fragment shader to manually blend each
 * block of 16x16 pixels down to 1 pixel.  This produces a reference
 * image, which is then compared to the MSAA image to measure the
 * error introduced by MSAA.
 *
 * (Note: the supersampled image is actually larger than the maximum
 * texture size that GL 3.0 requires all implementations to support
 * (1024x1024), so it is actually done in 1024x1024 tiles that are
 * then stitched together to form the reference image).
 *
 * In the piglit window, the MSAA image appears on the left; the
 * reference image is on the right.
 *
 * For each color component of each pixel, if the reference image has
 * a value of exactly 0.0 or 1.0, that pixel is presumed to be
 * completely covered by a triangle, so the test verifies that the
 * corresponding pixel in the MSAA image is exactly 0.0 or 1.0.  Where
 * the reference image has a value between 0.0 and 1.0, we know there
 * is a triangle boundary that MSAA should smooth out, so the test
 * estimates the accuracy of MSAA rendering by computing the RMS error
 * between the reference image and the MSAA image for these pixels.
 *
 * In addition to the above test (the "color" test), there are functions
 * which can also verify the proper behavior of the stencil MSAA buffer.
 * This can be done in two ways:
 *
 * - "stencil_draw" test: after drawing the scene, we clear the MSAA
 *   color buffer and run a "manifest" pass which uses stencil
 *   operations to make a visual representation of the contents of the
 *   stencil buffer show up in the color buffer.  The rest of the test
 *   operates as usual.  This allows us to verify that drawing
 *   operations that use the stencil buffer operate correctly in MSAA
 *   mode.
 *
 * - "stencil_resolve" test: same as above, except that we blit the
 *   MSAA stencil buffer to a single-sampled FBO before running the
 *   "manifest" pass.  This allows us to verify that the
 *   implementation properly downsamples the MSAA stencil buffer.
 *
 * There are similar variants "depth_draw" and "depth_resolve" for
 * testing the MSAA depth buffer.
 *
 * Note that when downsampling the MSAA color buffer, implementations
 * are expected to blend the values of each of the color samples;
 * but when downsampling the stencil and depth buffers, they are
 * expected to just choose one representative sample (this is because
 * an intermediate stencil or depth value would not be meaningful).
 * Therefore, the pass threshold is relaxed for the "stencil_resolve"
 * and "depth_resolve" tests.
 *
 * Functions also accepts the following flags:
 *
 * - "small": Causes the MSAA image to be renedered in extremely tiny
 *   (16x16) tiles that are then stitched together.  This verifies
 *   that MSAA works properly on very small buffers (a critical corner
 *   case on i965).
 *
 * - "depthstencil": Causes the framebuffers to use a combined
 *   depth/stencil buffer (as opposed to separate depth and stencil
 *   buffers).  On some implementations (e.g. the nVidia proprietary
 *   driver for Linux) this is necessary for framebuffer completeness.
 *   On others (e.g. i965), this is an important corner case to test.
 */

#include "common.h"

/**
 * \param attach_texture, if true, means to use a texture as color
 * attachment instead of a renderbuffer.
 */
void
Fbo::init(int num_samples, int width, int height, bool combine_depth_stencil,
	  bool attach_texture)
{
	generate();
	this->width = width;
	this->height = height;
	this->combine_depth_stencil = combine_depth_stencil;
	this->attach_texture = attach_texture;
	set_samples(num_samples);
}

void
Fbo::generate(void)
{
	glGenFramebuffers(1, &handle);
}

void
Fbo::set_samples(int num_samples)
{
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, handle);

	this->color_tex = 0;

	/* Color buffer */
	if (!attach_texture) {
		GLuint rb;
		glGenRenderbuffers(1, &rb);
		glBindRenderbuffer(GL_RENDERBUFFER, rb);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, num_samples,
						 GL_RGBA, width, height);
		glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER,
					  GL_COLOR_ATTACHMENT0,
					  GL_RENDERBUFFER, rb);
	} else {
		glGenTextures(1, &color_tex);
		glBindTexture(GL_TEXTURE_2D, color_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
				GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D,
			     0 /* level */,
			     GL_RGBA /* internalformat */,
			     width,
			     height,
			     0 /* border */,
			     GL_RGBA /* format */,
			     GL_BYTE /* type */,
			     NULL /* data */);
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
				       GL_COLOR_ATTACHMENT0,
				       GL_TEXTURE_2D,
				       color_tex,
				       0 /* level */);
	}

	/* Depth/stencil buffer(s) */
	if (combine_depth_stencil) {
		GLuint depth_stencil;
		glGenRenderbuffers(1, &depth_stencil);
		glBindRenderbuffer(GL_RENDERBUFFER, depth_stencil);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, num_samples,
						 GL_DEPTH_STENCIL, width,
						 height);
		glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER,
					  GL_DEPTH_STENCIL_ATTACHMENT,
					  GL_RENDERBUFFER, depth_stencil);
	} else {
		GLuint stencil;
		glGenRenderbuffers(1, &stencil);
		glBindRenderbuffer(GL_RENDERBUFFER, stencil);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, num_samples,
						 GL_STENCIL_INDEX8,
						 width, height);
		glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER,
					  GL_STENCIL_ATTACHMENT,
					  GL_RENDERBUFFER, stencil);

		GLuint depth;
		glGenRenderbuffers(1, &depth);
		glBindRenderbuffer(GL_RENDERBUFFER, depth);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, num_samples,
						 GL_DEPTH_COMPONENT24,
						 width, height);
		glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER,
					  GL_DEPTH_ATTACHMENT,
					  GL_RENDERBUFFER, depth);
	}

	if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("Framebuffer not complete\n");
		if (!combine_depth_stencil) {
			/* Some implementations do not support
			 * separate depth and stencil attachments, so
			 * don't consider it an error if we fail to
			 * make a complete framebuffer using separate
			 * depth and stencil attachments.
			 */
			piglit_report_result(PIGLIT_SKIP);
		} else {
			piglit_report_result(PIGLIT_FAIL);
		}
	}

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}



void
Fbo::set_viewport()
{
	glViewport(0, 0, width, height);
}

void
DownsampleProg::compile(int supersample_factor)
{
	static const char *vert =
		"#version 130\n"
		"in vec2 pos;\n"
		"in vec2 texCoord;\n"
		"out vec2 texCoordVarying;\n"
		"void main()\n"
		"{\n"
		"  gl_Position = vec4(pos, 0.0, 1.0);\n"
		"  texCoordVarying = texCoord;\n"
		"}\n";

	static const char *frag =
		"#version 130\n"
		"uniform sampler2D samp;\n"
		"uniform int supersample_factor;\n"
		"in vec2 texCoordVarying;\n"
		"void main()\n"
		"{\n"
		"  vec4 sum = vec4(0.0);\n"
		"  ivec2 pixel = ivec2(texCoordVarying);\n"
		"  for (int i = 0; i < supersample_factor; ++i) {\n"
		"    for (int j = 0; j < supersample_factor; ++j) {\n"
		"      sum += texelFetch(\n"
		"          samp, pixel * supersample_factor + ivec2(i, j), 0);\n"
		"    }\n"
		"  }\n"
		"  gl_FragColor = sum / (supersample_factor * supersample_factor);\n"
		"}\n";

	/* Compile program */
	prog = glCreateProgram();
	GLint vs = piglit_compile_shader_text(GL_VERTEX_SHADER, vert);
	glAttachShader(prog, vs);
	GLint fs = piglit_compile_shader_text(GL_FRAGMENT_SHADER, frag);
	glAttachShader(prog, fs);
	glBindAttribLocation(prog, 0, "pos");
	glBindAttribLocation(prog, 1, "texCoord");
	glLinkProgram(prog);
	if (!piglit_link_check_status(prog)) {
		piglit_report_result(PIGLIT_FAIL);
	}

	/* Set up uniforms */
	glUseProgram(prog);
	glUniform1i(glGetUniformLocation(prog, "supersample_factor"),
		    supersample_factor);
	glUniform1i(glGetUniformLocation(prog, "samp"), 0);

	/* Set up vertex array object */
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	/* Set up vertex input buffer */
	glGenBuffers(1, &vertex_buf);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float),
			      (void *) 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float),
			      (void *) (2*sizeof(float)));

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
DownsampleProg::run(const Fbo *src_fbo, int dest_width, int dest_height)
{
	float w = dest_width;
	float h = dest_height;

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, src_fbo->color_tex);

	glUseProgram(prog);
	glBindVertexArray(vao);

	float vertex_data[4][4] = {
		{ -1, -1, 0, 0 },
		{ -1,  1, 0, h },
		{  1,  1, w, h },
		{  1, -1, w, 0 }
	};
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data,
		     GL_STREAM_DRAW);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void *) 0);
}

void
ManifestStencil::compile()
{
	static const char *vert =
		"#version 130\n"
		"in vec2 pos;\n"
		"void main()\n"
		"{\n"
		"  gl_Position = vec4(pos, 0.0, 1.0);\n"
		"}\n";

	static const char *frag =
		"#version 130\n"
		"uniform vec4 color;\n"
		"void main()\n"
		"{\n"
		"  gl_FragColor = color;\n"
		"}\n";

	/* Compile program */
	prog = glCreateProgram();
	GLint vs = piglit_compile_shader_text(GL_VERTEX_SHADER, vert);
	glAttachShader(prog, vs);
	GLint fs = piglit_compile_shader_text(GL_FRAGMENT_SHADER, frag);
	glAttachShader(prog, fs);
	glBindAttribLocation(prog, 0, "pos");
	glLinkProgram(prog);
	if (!piglit_link_check_status(prog)) {
		piglit_report_result(PIGLIT_FAIL);
	}

	/* Set up uniforms */
	glUseProgram(prog);
	color_loc = glGetUniformLocation(prog, "color");

	/* Set up vertex array object */
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	/* Set up vertex input buffer */
	float vertex_data[4][2] = {
		{ -1, -1 },
		{ -1,  1 },
		{  1,  1 },
		{  1, -1 }
	};
	glGenVertexArrays(1, &vertex_buf);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data,
		     GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_data[0]),
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
ManifestStencil::run()
{
	static float colors[8][4] = {
		{ 0.0, 0.0, 0.0, 1.0 },
		{ 0.0, 0.0, 1.0, 1.0 },
		{ 0.0, 1.0, 0.0, 1.0 },
		{ 0.0, 1.0, 1.0, 1.0 },
		{ 1.0, 0.0, 0.0, 1.0 },
		{ 1.0, 0.0, 1.0, 1.0 },
		{ 1.0, 1.0, 0.0, 1.0 },
		{ 1.0, 1.0, 1.0, 1.0 }
	};

	glUseProgram(prog);
	glBindVertexArray(vao);

	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	/* Clear the color buffer to 0, in case the stencil buffer
	 * contains any values outside the range 0..7
	 */
	glClear(GL_COLOR_BUFFER_BIT);

	for (int i = 0; i < 8; ++i) {
		glStencilFunc(GL_EQUAL, i, 0xff);
		glUniform4fv(color_loc, 1, colors[i]);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void *) 0);
	}

	glDisable(GL_STENCIL_TEST);
}

void
ManifestDepth::compile()
{
	static const char *vert =
		"#version 130\n"
		"in vec2 pos;\n"
		"uniform float depth;\n"
		"void main()\n"
		"{\n"
		"  gl_Position = vec4(pos, depth, 1.0);\n"
		"}\n";

	static const char *frag =
		"#version 130\n"
		"uniform vec4 color;\n"
		"void main()\n"
		"{\n"
		"  gl_FragColor = color;\n"
		"}\n";

	/* Compile program */
	prog = glCreateProgram();
	GLint vs = piglit_compile_shader_text(GL_VERTEX_SHADER, vert);
	glAttachShader(prog, vs);
	GLint fs = piglit_compile_shader_text(GL_FRAGMENT_SHADER, frag);
	glAttachShader(prog, fs);
	glBindAttribLocation(prog, 0, "pos");
	glLinkProgram(prog);
	if (!piglit_link_check_status(prog)) {
		piglit_report_result(PIGLIT_FAIL);
	}

	/* Set up uniforms */
	glUseProgram(prog);
	color_loc = glGetUniformLocation(prog, "color");
	depth_loc = glGetUniformLocation(prog, "depth");

	/* Set up vertex array object */
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	/* Set up vertex input buffer */
	float vertex_data[4][2] = {
		{ -1, -1 },
		{ -1,  1 },
		{  1,  1 },
		{  1, -1 }
	};
	glGenVertexArrays(1, &vertex_buf);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data,
		     GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_data[0]),
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
ManifestDepth::run()
{
	static float colors[8][4] = {
		{ 0.0, 0.0, 0.0, 1.0 },
		{ 0.0, 0.0, 1.0, 1.0 },
		{ 0.0, 1.0, 0.0, 1.0 },
		{ 0.0, 1.0, 1.0, 1.0 },
		{ 1.0, 0.0, 0.0, 1.0 },
		{ 1.0, 0.0, 1.0, 1.0 },
		{ 1.0, 1.0, 0.0, 1.0 },
		{ 1.0, 1.0, 1.0, 1.0 }
	};

	glUseProgram(prog);
	glBindVertexArray(vao);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
	glStencilFunc(GL_EQUAL, 0, 0xff);

	/* Clear the stencil buffer to 0, leaving depth and color
	 * buffers unchanged.
	 */
	glClear(GL_STENCIL_BUFFER_BIT);

	for (int i = 0; i < 8; ++i) {
		glUniform4fv(color_loc, 1, colors[i]);
		glUniform1f(depth_loc, float(7 - 2*i)/8);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void *) 0);
	}

	glDisable(GL_STENCIL_TEST);
	glDisable(GL_DEPTH_TEST);
}

void Triangles::compile()
{
	/* Triangle coords within (-1,-1) to (1,1) rect */
	static const float pos_within_tri[][2] = {
		{ -0.5, -1.0 },
		{  0.0,  1.0 },
		{  0.5, -1.0 }
	};

	/* Number of triangle instances across (and down) */
	int tris_across = 8;

	/* Total number of triangles drawn */
	num_tris = tris_across * tris_across;

	/* Scaling factor uniformly applied to triangle coords */
	float tri_scale = 0.8 / tris_across;

	/* Amount each triangle should be rotated compared to prev */
	float rotation_delta = M_PI * 2.0 / num_tris;

	/* Final scaling factor */
	float final_scale = 0.95;

	static const char *vert =
		"#version 130\n"
		"in vec2 pos_within_tri;\n"
		"uniform float tri_scale;\n"
		"uniform float rotation_delta;\n"
		"uniform int tris_across;\n"
		"uniform float final_scale;\n"
		"uniform mat4 proj;\n"
		"uniform int tri_num;\n"
		"\n"
		"void main()\n"
		"{\n"
		"  vec2 pos = tri_scale * pos_within_tri;\n"
		"  float rotation = rotation_delta * tri_num;\n"
		"  pos = mat2(cos(rotation), sin(rotation),\n"
		"             -sin(rotation), cos(rotation)) * pos;\n"
		"  int i = tri_num % tris_across;\n"
		"  int j = tris_across - 1 - tri_num / tris_across;\n"
		"  pos += (vec2(i, j) * 2.0 + 1.0) / tris_across - 1.0;\n"
		"  pos *= final_scale;\n"
		"  gl_Position = proj * vec4(pos, 0.0, 1.0);\n"
		"}\n";

	static const char *frag =
		"#version 130\n"
		"void main()\n"
		"{\n"
		"  gl_FragColor = vec4(1.0);\n"
		"}\n";

	/* Compile program */
	prog = glCreateProgram();
	GLint vs = piglit_compile_shader_text(GL_VERTEX_SHADER, vert);
	glAttachShader(prog, vs);
	GLint fs = piglit_compile_shader_text(GL_FRAGMENT_SHADER, frag);
	glAttachShader(prog, fs);
	glBindAttribLocation(prog, 0, "pos_within_tri");
	glLinkProgram(prog);
	if (!piglit_link_check_status(prog)) {
		piglit_report_result(PIGLIT_FAIL);
	}

	/* Set up uniforms */
	glUseProgram(prog);
	glUniform1f(glGetUniformLocation(prog, "tri_scale"), tri_scale);
	glUniform1f(glGetUniformLocation(prog, "rotation_delta"),
		    rotation_delta);
	glUniform1i(glGetUniformLocation(prog, "tris_across"), tris_across);
	glUniform1f(glGetUniformLocation(prog, "final_scale"), final_scale);
	proj_loc = glGetUniformLocation(prog, "proj");
	tri_num_loc = glGetUniformLocation(prog, "tri_num");

	/* Set up vertex array object */
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	/* Set up vertex input buffer */
	glGenBuffers(1, &vertex_buf);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(pos_within_tri), pos_within_tri,
		     GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, ARRAY_SIZE(pos_within_tri[0]), GL_FLOAT,
			      GL_FALSE, sizeof(pos_within_tri[0]), (void *) 0);
}

void Triangles::draw(float (*proj)[4])
{
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(prog);
	glUniformMatrix4fv(proj_loc, 1, GL_TRUE, &proj[0][0]);
	glBindVertexArray(vao);
	for (int tri_num = 0; tri_num < num_tris; ++tri_num) {
		glUniform1i(tri_num_loc, tri_num);
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}
}

void Lines::compile()
{
	/* Line coords within (-1,-1) to (1,1) rect */
	static const float pos_line[][2] = {
		{ -0.8, -0.5 },
		{  0.8, -0.5 }
	};

	/* Number of line instances across (and down) */
	int lines_across = 4;

	/* Total number of lines drawn */
	num_lines = lines_across * lines_across;

	/* Amount each line should be rotated compared to prev */
	float rotation_delta = M_PI * 2.0 / num_lines;

	/* Scaling factor uniformly applied to line coords */
	float line_scale = 0.8 / lines_across;

	/* Final scaling factor */
	float final_scale = 0.95;

	static const char *vert =
		"#version 130\n"
		"in vec2 pos_line;\n"
		"uniform float line_scale;\n"
		"uniform float rotation_delta;\n"
		"uniform int lines_across;\n"
		"uniform float final_scale;\n"
		"uniform mat4 proj;\n"
		"uniform int line_num;\n"
		"\n"
		"void main()\n"
		"{\n"
		"  vec2 pos = line_scale * pos_line;\n"
		"  float rotation = rotation_delta * line_num;\n"
		"  pos = mat2(cos(rotation), sin(rotation),\n"
		"             -sin(rotation), cos(rotation)) * pos;\n"
		"  int i = line_num % lines_across;\n"
		"  int j = lines_across - 1 - line_num / lines_across;\n"
		"  pos += (vec2(i, j) * 2.0 + 1.0) / lines_across - 1.0;\n"
		"  pos *= final_scale;\n"
		"  gl_Position = proj * vec4(pos, 0.0, 1.0);\n"
		"}\n";

	static const char *frag =
		"#version 130\n"
		"void main()\n"
		"{\n"
		"  gl_FragColor = vec4(1.0);\n"
		"}\n";

	/* Compile program */
	prog = glCreateProgram();
	GLint vs = piglit_compile_shader_text(GL_VERTEX_SHADER, vert);
	glAttachShader(prog, vs);
	GLint fs = piglit_compile_shader_text(GL_FRAGMENT_SHADER, frag);
	glAttachShader(prog, fs);
	glBindAttribLocation(prog, 0, "pos_line");
	glLinkProgram(prog);
	if (!piglit_link_check_status(prog)) {
		piglit_report_result(PIGLIT_FAIL);
	}

	/* Set up uniforms */
	glUseProgram(prog);
	glUniform1f(glGetUniformLocation(prog, "line_scale"), line_scale);
	glUniform1f(glGetUniformLocation(prog, "rotation_delta"),
		    rotation_delta);
	glUniform1i(glGetUniformLocation(prog, "lines_across"), lines_across);
	glUniform1f(glGetUniformLocation(prog, "final_scale"), final_scale);
	proj_loc = glGetUniformLocation(prog, "proj");
	line_num_loc = glGetUniformLocation(prog, "line_num");

	/* Set up vertex array object */
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	/* Set up vertex input buffer */
	glGenBuffers(1, &vertex_buf);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(pos_line), pos_line,
		     GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, ARRAY_SIZE(pos_line[0]), GL_FLOAT,
			      GL_FALSE, sizeof(pos_line[0]), (void *) 0);
}

void Lines::draw(float (*proj)[4])
{
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(prog);
	glUniformMatrix4fv(proj_loc, 1, GL_TRUE, &proj[0][0]);
	glBindVertexArray(vao);
	for (int line_num = 0; line_num < num_lines; ++line_num) {
		glLineWidth(1 + line_num / 4);
		glUniform1i(line_num_loc, line_num);
		glDrawArrays(GL_LINES, 0, 2);
	}
}

void Points::compile()
{
	/* Point coords within (-1,-1) to (1,1) rect */
	static const float pos_point[2] = { -0.5, -0.5 };

	/* Number of point instances across (and down) */
	int points_across = 4;

	/* Total number of points drawn */
	num_points = points_across * points_across;

	/* Scaling factor uniformly applied to point coords */
	float point_scale = 0.8 / points_across;

	/* Final scaling factor */
	float final_scale = 0.95;

	static const char *vert =
		"#version 130\n"
		"in vec2 pos_point;\n"
		"uniform float point_scale;\n"
		"uniform int points_across;\n"
		"uniform float final_scale;\n"
		"uniform mat4 proj;\n"
		"uniform int point_num;\n"
		"uniform float depth;\n"
		"\n"
		"void main()\n"
		"{\n"
		"  vec2 pos = point_scale * pos_point;\n"
		"  int i = point_num % points_across;\n"
		"  int j = points_across - 1 - point_num / points_across;\n"
		"  pos += (vec2(i, j) * 2.0 + 1.0) / points_across - 1.0;\n"
		"  pos *= final_scale;\n"
		"  gl_Position = proj * vec4(pos, depth, 1.0);\n"
		"}\n";

	static const char *frag =
		"#version 130\n"
		"void main()\n"
		"{\n"
		"  gl_FragColor = vec4(1.0);\n"
		"}\n";

	/* Compile program */
	prog = glCreateProgram();
	GLint vs = piglit_compile_shader_text(GL_VERTEX_SHADER, vert);
	glAttachShader(prog, vs);
	GLint fs = piglit_compile_shader_text(GL_FRAGMENT_SHADER, frag);
	glAttachShader(prog, fs);
	glBindAttribLocation(prog, 0, "pos_point");
	glLinkProgram(prog);
	if (!piglit_link_check_status(prog)) {
		piglit_report_result(PIGLIT_FAIL);
	}

	/* Set up uniforms */
	glUseProgram(prog);
	glUniform1f(glGetUniformLocation(prog, "point_scale"), point_scale);
	glUniform1i(glGetUniformLocation(prog, "points_across"), points_across);
	glUniform1f(glGetUniformLocation(prog, "final_scale"), final_scale);
	proj_loc = glGetUniformLocation(prog, "proj");
	point_num_loc = glGetUniformLocation(prog, "point_num");
	depth_loc = glGetUniformLocation(prog, "depth");

	/* Set up vertex array object */
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	/* Set up vertex input buffer */
	glGenBuffers(1, &vertex_buf);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(pos_point), pos_point,
		     GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, ARRAY_SIZE(pos_point), GL_FLOAT,
			      GL_FALSE, 0, (void *) 0);
}

void Points::draw(float (*proj)[4])
{
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(prog);
	glUniformMatrix4fv(proj_loc, 1, GL_TRUE, &proj[0][0]);
	glBindVertexArray(vao);
	glUniform1f(depth_loc, 0.0);
	for (int point_num = 0; point_num < num_points; ++point_num) {
		glPointSize(2 + point_num);
		glUniform1i(point_num_loc, point_num);
		glDrawArrays(GL_POINTS, 0, 1);
	}
}

void Spiral::compile()
{
	/* Point coords within (-1,-1) to (1,1) rect */
	static const float pos_point[2] = { 0.7, 0.7 };

	num_points = 15;
	num_circles = 3;

	static const char *vert =
		"#version 130\n"
		"in vec2 pos_point;\n"
		"uniform float rotation;\n"
		"uniform float length;\n"
		"uniform float depth;\n"
		"uniform mat4 proj;\n"
		"\n"
		"void main()\n"
		"{\n"
		"  vec2 pos = pos_point * length;\n"
		"  pos = mat2(cos(rotation), sin(rotation),\n"
		"             -sin(rotation), cos(rotation)) * pos;\n"
		"  gl_Position = proj * vec4(pos, depth, 1.0);\n"
		"}\n";

	static const char *frag =
		"#version 130\n"
		"void main()\n"
		"{\n"
		"  gl_FragColor = vec4(0.0);\n"
		"}\n";

	/* Compile program */
	prog = glCreateProgram();
	GLint vs = piglit_compile_shader_text(GL_VERTEX_SHADER, vert);
	glAttachShader(prog, vs);
	GLint fs = piglit_compile_shader_text(GL_FRAGMENT_SHADER, frag);
	glAttachShader(prog, fs);
	glBindAttribLocation(prog, 0, "pos_point");
	glLinkProgram(prog);
	if (!piglit_link_check_status(prog)) {
		piglit_report_result(PIGLIT_FAIL);
	}

	/* Set up uniforms */
	glUseProgram(prog);
	rotation_loc = glGetUniformLocation(prog, "rotation");
	length_loc = glGetUniformLocation(prog, "length");
	depth_loc = glGetUniformLocation(prog, "depth");
	glUniform1f(depth_loc, 0.0);
	proj_loc = glGetUniformLocation(prog, "proj");

	/* Set up vertex array object */
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	/* Set up vertex input buffer */
	glGenBuffers(1, &vertex_buf);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(pos_point), pos_point,
		     GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, ARRAY_SIZE(pos_point), GL_FLOAT,
			      GL_FALSE, 0, (void *) 0);
}

void
StencilSpiral::draw(float (*proj)[4])
{
	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glUseProgram(prog);
	glUniformMatrix4fv(proj_loc, 1, GL_TRUE, &proj[0][0]);
	glUniform1f(depth_loc, 0.0);
	glBindVertexArray(vao);

	/* Total number of points drawn */
	int total_points = num_circles * num_points;

	for (int i = 0; i < total_points; ++i) {
		/* Set the stencil value of point between 0 - 7 */
		glStencilFunc(GL_ALWAYS, i % 8, 0xff);
		glPointSize(1 + 2 * ((total_points - i) / num_circles ));

		glUniform1f(length_loc, (1.0 - i * 1.0 / total_points));
		glUniform1f(rotation_loc, M_PI * 2.0 * i / (num_points));

		glDrawArrays(GL_POINTS, 0, 1);
	}

	glDisable(GL_STENCIL_TEST);
}

void
DepthSpiral::draw(float (*proj)[4])
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(prog);
	glUniformMatrix4fv(proj_loc, 1, GL_TRUE, &proj[0][0]);
	glBindVertexArray(vao);

	/* Total number of points drawn */
	int total_points = num_circles * num_points;

	for (int i = 0; i < total_points; ++i) {
		glPointSize(1 + 2 * ((total_points - i) / num_circles ));

		/* Draw points in a haphazard order so we can verify that
		 * depth comparisons sort them out properly.
		 */
		int point_to_draw = (i * 10) % total_points;

		glUniform1f(length_loc, (1.0 - i * 1.0 / total_points));

		/* Draw points in a depth range of -1 to +1 */
		float depth = float(total_points - point_to_draw * 2 - 1)
			      / (total_points + 1);
		glUniform1f(depth_loc, depth);
		glUniform1f(rotation_loc, M_PI * 2.0 * i / num_points);

		glDrawArrays(GL_POINTS, 0, 1);
	}

	glDisable(GL_DEPTH_TEST);
}

void Star::compile()
{
	/* Triangle coords within (-1,-1) to (1,1) rect */
	static const float pos_line[][2] = {
		{ -0.3, -0.8 },
		{  0.3,  0.8 }
	};

	/* Total number of lines drawn */
	num_lines = 7;

	static const char *vert =
		"#version 130\n"
		"in vec2 pos_line;\n"
		"uniform float rotation;\n"
		"uniform float depth;\n"
		"uniform mat4 proj;\n"
		"\n"
		"void main()\n"
		"{\n"
		"  vec2 pos = pos_line;\n"
		"  pos = mat2(cos(rotation), sin(rotation),\n"
		"             -sin(rotation), cos(rotation)) * pos;\n"
		"  gl_Position = proj * vec4(pos, depth, 1.0);\n"
		"}\n";

	static const char *frag =
		"#version 130\n"
		"void main()\n"
		"{\n"
		"  gl_FragColor = vec4(0.0);\n"
		"}\n";

	/* Compile program */
	prog = glCreateProgram();
	GLint vs = piglit_compile_shader_text(GL_VERTEX_SHADER, vert);
	glAttachShader(prog, vs);
	GLint fs = piglit_compile_shader_text(GL_FRAGMENT_SHADER, frag);
	glAttachShader(prog, fs);
	glBindAttribLocation(prog, 0, "pos_line");
	glLinkProgram(prog);
	if (!piglit_link_check_status(prog)) {
		piglit_report_result(PIGLIT_FAIL);
	}

	/* Set up uniforms */
	glUseProgram(prog);
	rotation_loc = glGetUniformLocation(prog, "rotation");
	depth_loc = glGetUniformLocation(prog, "depth");
	glUniform1f(depth_loc, 0.0);
	proj_loc = glGetUniformLocation(prog, "proj");

	/* Set up vertex array object */
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	/* Set up vertex input buffer */
	glGenBuffers(1, &vertex_buf);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(pos_line), pos_line,
		     GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, ARRAY_SIZE(pos_line[0]), GL_FLOAT,
			      GL_FALSE, sizeof(pos_line[0]), (void *) 0);
}

void
StencilStar::draw(float (*proj)[4])
{
	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glUseProgram(prog);
	glUniformMatrix4fv(proj_loc, 1, GL_TRUE, &proj[0][0]);
	glBindVertexArray(vao);
	for (int i = 0; i < num_lines; ++i) {
		glStencilFunc(GL_ALWAYS, i+1, 0xff);
		glUniform1f(rotation_loc, M_PI * 2.0 * i / num_lines);
		glDrawArrays(GL_LINES, 0, 2);
	}

	glDisable(GL_STENCIL_TEST);
}

void
DepthStar::draw(float (*proj)[4])
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(prog);
	glUniformMatrix4fv(proj_loc, 1, GL_TRUE, &proj[0][0]);
	glBindVertexArray(vao);
	for (int i = 0; i < num_lines; ++i) {
		/* Draw lines in a haphazard order so we can verify
		 * that depth comparisons sort them out properly.
		 */
		int line_to_draw = (i * 3) % num_lines;

		/* Note: with num_lines == 7, this causes us to draw
		 * lines at depths of 3/4, 1/2, -1/4, 0, 1/4, 1/2,
		 * and 3/4.
		 */
		glUniform1f(depth_loc,
			    float(num_lines - line_to_draw * 2 - 1)
			    / (num_lines + 1));

		glUniform1f(rotation_loc,
			    M_PI * 2.0 * line_to_draw / num_lines);
		glDrawArrays(GL_LINES, 0, 2);
	}

	glDisable(GL_DEPTH_TEST);
}

void Sunburst::compile()
{
	/* Triangle coords within (-1,-1) to (1,1) rect */
	static const float pos_within_tri[][2] = {
		{ -0.3, -0.8 },
		{  0.0,  1.0 },
		{  0.3, -0.8 }
	};

	/* Total number of triangles drawn */
	num_tris = 7;

	static const char *vert =
		"#version 130\n"
		"in vec2 pos_within_tri;\n"
		"uniform float rotation;\n"
		"uniform float depth;\n"
		"uniform mat4 proj;\n"
		"\n"
		"void main()\n"
		"{\n"
		"  vec2 pos = pos_within_tri;\n"
		"  pos = mat2(cos(rotation), sin(rotation),\n"
		"             -sin(rotation), cos(rotation)) * pos;\n"
		"  gl_Position = proj * vec4(pos, depth, 1.0);\n"
		"}\n";

	static const char *frag =
		"#version 130\n"
		"void main()\n"
		"{\n"
		"  gl_FragColor = vec4(0.0);\n"
		"}\n";

	/* Compile program */
	prog = glCreateProgram();
	GLint vs = piglit_compile_shader_text(GL_VERTEX_SHADER, vert);
	glAttachShader(prog, vs);
	GLint fs = piglit_compile_shader_text(GL_FRAGMENT_SHADER, frag);
	glAttachShader(prog, fs);
	glBindAttribLocation(prog, 0, "pos_within_tri");
	glLinkProgram(prog);
	if (!piglit_link_check_status(prog)) {
		piglit_report_result(PIGLIT_FAIL);
	}

	/* Set up uniforms */
	glUseProgram(prog);
	rotation_loc = glGetUniformLocation(prog, "rotation");
	depth_loc = glGetUniformLocation(prog, "depth");
	glUniform1f(depth_loc, 0.0);
	proj_loc = glGetUniformLocation(prog, "proj");

	/* Set up vertex array object */
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	/* Set up vertex input buffer */
	glGenBuffers(1, &vertex_buf);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(pos_within_tri), pos_within_tri,
		     GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, ARRAY_SIZE(pos_within_tri[0]), GL_FLOAT,
			      GL_FALSE, sizeof(pos_within_tri[0]), (void *) 0);
}

void
StencilSunburst::draw(float (*proj)[4])
{
	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glUseProgram(prog);
	glUniformMatrix4fv(proj_loc, 1, GL_TRUE, &proj[0][0]);
	glBindVertexArray(vao);
	for (int i = 0; i < num_tris; ++i) {
		glStencilFunc(GL_ALWAYS, i+1, 0xff);
		glUniform1f(rotation_loc, M_PI * 2.0 * i / num_tris);
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}

	glDisable(GL_STENCIL_TEST);
}

void
DepthSunburst::draw(float (*proj)[4])
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(prog);
	glUniformMatrix4fv(proj_loc, 1, GL_TRUE, &proj[0][0]);
	glBindVertexArray(vao);
	for (int i = 0; i < num_tris; ++i) {
		/* Draw triangles in a haphazard order so we can
		 * verify that depth comparisons sort them out
		 * properly.
		 */
		int triangle_to_draw = (i * 3) % num_tris;

		/* Note: with num_tris == 7, this causes us to draw
		 * triangles at depths of 3/4, 1/2, -1/4, 0, 1/4, 1/2,
		 * and 3/4.
		 */
		glUniform1f(depth_loc,
			    float(num_tris - triangle_to_draw * 2 - 1)
			    / (num_tris + 1));

		glUniform1f(rotation_loc,
			    M_PI * 2.0 * triangle_to_draw / num_tris);
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}

	glDisable(GL_DEPTH_TEST);
}

Stats::Stats()
	: count(0), sum_squared_error(0.0)
{
}

void
Stats::summarize()
{
	printf("  count = %d\n", count);
	if (count != 0) {
		if (sum_squared_error != 0.0) {
			printf("  RMS error = %f\n",
			       sqrt(sum_squared_error / count));
		} else {
			printf("  Perfect output\n");
		}
	}
}

bool
Stats::is_perfect()
{
	return sum_squared_error == 0.0;
}

bool
Stats::is_better_than(double rms_error_threshold)
{
	return sqrt(sum_squared_error / count) < rms_error_threshold;
}

Test::Test(TestPattern *pattern, ManifestProgram *manifest_program,
	   bool test_resolve, GLbitfield blit_type)
	: pattern(pattern),
	  manifest_program(manifest_program),
	  test_resolve(test_resolve),
	  blit_type(blit_type)
{
}

void
Test::init(int num_samples, bool small, bool combine_depth_stencil,
	   int pattern_width, int pattern_height, int supersample_factor)
{
	this->num_samples = num_samples;
	this->pattern_width = pattern_width;
	this->pattern_height = pattern_height;
	this->supersample_factor = supersample_factor;

	test_fbo.init(0,
		      small ? 16 : pattern_width,
		      small ? 16 : pattern_height,
		      combine_depth_stencil,
		      false);

	multisample_fbo.init(num_samples,
			     small ? 16 : pattern_width,
			     small ? 16 : pattern_height,
			     combine_depth_stencil,
			     false);
	resolve_fbo.init(0,
			 small ? 16 : pattern_width,
			 small ? 16 : pattern_height,
			 combine_depth_stencil,
			 false);
	supersample_fbo.init(0 /* num_samples */,
			     1024, 1024, combine_depth_stencil, true);
	downsample_fbo.init(0 /* num_samples */,
			    1024 / supersample_factor,
			    1024 / supersample_factor,
			    combine_depth_stencil, false);

	pattern->compile();
	downsample_prog.compile(supersample_factor);
	if (manifest_program)
		manifest_program->compile();

	/* Only do depth testing in those parts of the test where we
	 * explicitly want it
	 */
	glDisable(GL_DEPTH_TEST);
}

/**
 * Blit the data from multisample_fbo to resolve_fbo, forcing the
 * implementation to do an MSAA resolve.
 */
void
Test::resolve(Fbo *fbo, GLbitfield which_buffers)
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo->handle);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolve_fbo.handle);
	resolve_fbo.set_viewport();
	glBlitFramebuffer(0, 0, fbo->width, fbo->height,
			  0, 0, resolve_fbo.width, resolve_fbo.height,
			  which_buffers, GL_NEAREST);
}

/**
 * Use downsample_prog to blend 16x16 blocks of samples in
 * supersample_fbo, to produce a reference image in downsample_fbo.
 */
void
Test::downsample_color(int downsampled_width, int downsampled_height)
{

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, downsample_fbo.handle);
	downsample_fbo.set_viewport();
	downsample_prog.run(&supersample_fbo,
			    downsample_fbo.width, downsample_fbo.height);
}

/**
 * Blit the color data from src_fbo to the given location in the
 * windowsystem buffer, so that the user can see it and we can read it
 * using glReadPixels.
 */
void
Test::show(Fbo *src_fbo, int x_offset, int y_offset)
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, src_fbo->handle);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glViewport(0, 0, piglit_width, piglit_height);
	glBlitFramebuffer(0, 0, src_fbo->width, src_fbo->height,
			  x_offset, y_offset,
			  x_offset + src_fbo->width,
			  y_offset + src_fbo->height,
			  GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

/**
 * Draw a portion of the test pattern by setting up an appropriate
 * projection matrix to map that portion of the test pattern to the
 * full FBO.
 */
void
Test::draw_pattern(int x_offset, int y_offset, int width, int height)
{
	/* Need a projection matrix such that:
	 * xc = ((xe + 1) * pattern_width/2 - x_offset) * 2/width - 1
	 * yc = ((ye + 1) * pattern_height/2 - y_offset) * 2/height - 1
	 * zc = ze
	 * wc = we = 1.0
	 *
	 * Therefore
	 * xc = pattern_width / width * xe
	 *    + pattern_width / width - x_offset * 2 / width - 1
	 * yc = pattern_height / height * ye
	 *    + pattern_height / height - y_offset * 2 / height - 1
	 * zc = ze
	 * wc = we = 1.0
	 */
	float x_scale = float(pattern_width) / width;
	float x_delta = x_scale - x_offset * 2.0 / width - 1.0;
	float y_scale = float(pattern_height) / height;
	float y_delta = y_scale - y_offset * 2.0 / height - 1.0;
	float proj[4][4] = {
		{ x_scale, 0, 0, x_delta },
		{ 0, y_scale, 0, y_delta },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 }
	};

	pattern->draw(proj);
}

/**
 * Draw the entire test image, rendering it a piece at a time if
 * multisample_fbo is very small.
 */
void
Test::draw_test_image(Fbo *fbo)
{
	int num_h_tiles = pattern_width / fbo->width;
	int num_v_tiles = pattern_height / fbo->height;
	for (int h = 0; h < num_h_tiles; ++h) {
		for (int v = 0; v < num_v_tiles; ++v) {
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
					  fbo->handle);
			fbo->set_viewport();
			int x_offset = h * fbo->width;
			int y_offset = v * fbo->height;
			draw_pattern(x_offset, y_offset,
				     fbo->width,
				     fbo->height);
			if (test_resolve) {
				resolve(fbo, blit_type);
				if (manifest_program)
					manifest_program->run();
			} else {
				if (manifest_program)
					manifest_program->run();
				resolve(fbo,
					GL_COLOR_BUFFER_BIT);
			}

			show(&resolve_fbo, x_offset, y_offset);
		}
	}
}

/**
 * Draw the test image to the default framebuffer
 */
void
Test::draw_to_default_framebuffer()
{
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glViewport(0, 0, pattern_width, pattern_height);
	draw_pattern(0, 0, pattern_width, pattern_height);
}

/**
 * Draw the entire test image, rendering it a piece at a time.
 */
void
Test::draw_reference_image()
{
	int downsampled_width = supersample_fbo.width / supersample_factor;
	int downsampled_height = supersample_fbo.height / supersample_factor;
	int num_h_tiles = pattern_width / downsampled_width;
	int num_v_tiles = pattern_height / downsampled_height;
	for (int h = 0; h < num_h_tiles; ++h) {
		for (int v = 0; v < num_v_tiles; ++v) {
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
					  supersample_fbo.handle);
			supersample_fbo.set_viewport();
			int x_offset = h * downsampled_width;
			int y_offset = v * downsampled_height;
			draw_pattern(x_offset, y_offset,
				     downsampled_width, downsampled_height);

			if (manifest_program)
				manifest_program->run();

			downsample_color(downsampled_width, downsampled_height);
			show(&downsample_fbo,
			     pattern_width + x_offset, y_offset);
		}
	}
}

/**
 * Measure the accuracy of MSAA downsampling.  Pixels that are fully
 * on or off in the reference image are required to be fully on or off
 * in the test image.  Pixels that are not fully on or off in the
 * reference image may be at any grayscale level; we mesaure the RMS
 * error between the reference image and the test image.
 */
bool
Test::measure_accuracy()
{
	bool pass = true;

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glViewport(0, 0, piglit_width, piglit_height);

	float *reference_data = new float[pattern_width * pattern_height * 4];
	glReadPixels(pattern_width, 0, pattern_width, pattern_height, GL_RGBA,
		     GL_FLOAT, reference_data);

	float *test_data = new float[pattern_width * pattern_height * 4];
	glReadPixels(0, 0, pattern_width, pattern_height, GL_RGBA,
		     GL_FLOAT, test_data);

	Stats unlit_stats;
	Stats partially_lit_stats;
	Stats totally_lit_stats;
	for (int y = 0; y < pattern_height; ++y) {
		for (int x = 0; x < pattern_width; ++x) {
			for (int c = 0; c < 4; ++c) {
				int pixel_pos = 4*(y*pattern_width + x) + c;
				float ref = reference_data[pixel_pos];
				float test = test_data[pixel_pos];
				if (ref <= 0.0)
					unlit_stats.record(test - ref);
				else if (ref >= 1.0)
					totally_lit_stats.record(test - ref);
				else
					partially_lit_stats.record(test - ref);
			}
		}
	}

	printf("Pixels that should be unlit\n");
	unlit_stats.summarize();
	pass = unlit_stats.is_perfect() && pass;
	printf("Pixels that should be totally lit\n");
	totally_lit_stats.summarize();
	pass = totally_lit_stats.is_perfect() && pass;
	printf("Pixels that should be partially lit\n");
	partially_lit_stats.summarize();

	double error_threshold;
	if (test_resolve) {
		/* For depth and stencil resolves, the implementation
		 * typically just picks one of the N multisamples, so
		 * we have to allow for a generous amount of error.
		 */
		error_threshold = 0.4;
	} else {
		/* Empirically, the RMS error for no oversampling is
		 * about 0.25, and each additional factor of 2
		 * overampling reduces the error by a factor of about
		 * 0.6.  Leaving some room for variation, we'll set
		 * the error threshold to 0.333 * 0.6 ^
		 * log2(num_samples).
		 */
		int effective_num_samples = num_samples == 0 ? 1 : num_samples;
		error_threshold = 0.333 *
			pow(0.6, log((double)effective_num_samples) / log(2.0));
	}
	printf("The error threshold for this test is %f\n", error_threshold);
	pass = partially_lit_stats.is_better_than(error_threshold) && pass;
	// TODO: deal with sRGB.
	return pass;
}

bool
Test::run()
{
	draw_test_image(&multisample_fbo);
	draw_reference_image();
	return measure_accuracy();
}


Test *
create_test(test_type_enum test_type, int n_samples, bool small,
	    bool combine_depth_stencil, int pattern_width, int pattern_height,
	    int supersample_factor)
{
	Test *test = NULL;
	switch (test_type) {
	case TEST_TYPE_COLOR:
		test = new Test(new Triangles(), NULL, false, 0);
		break;
	case TEST_TYPE_STENCIL_DRAW:
		test = new Test(new StencilSunburst(),
				new ManifestStencil(),
				false, 0);
		break;
	case TEST_TYPE_STENCIL_RESOLVE:
		test = new Test(new StencilSunburst(),
				new ManifestStencil(),
				true,
				GL_STENCIL_BUFFER_BIT);
		break;
	case TEST_TYPE_DEPTH_DRAW:
		test = new Test(new DepthSunburst(),
				new ManifestDepth(),
				false, 0);
		break;
	case TEST_TYPE_DEPTH_RESOLVE:
		test = new Test(new DepthSunburst(),
				new ManifestDepth(),
				true,
				GL_DEPTH_BUFFER_BIT);
		break;
	default:
		printf("Unrecognized test type\n");
		piglit_report_result(PIGLIT_FAIL);
		break;
	}

	test->init(n_samples, small, combine_depth_stencil, pattern_width,
		   pattern_height, supersample_factor);
	return test;
}
