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


/** @file clear-depth.c
 *
 * Section 4.4.7(Framebuffer Objects) From GL spec 3.2 core:
 * When the Clear or ClearBuffer* commands are used to clear a layered
 * framebuffer attachment, all layers of the attachment are cleared.
 *
 * Test Layout
 *         Tex1     Tex2
 *	*--------*--------*
 *      | layer4 | layer4 |
 *      *--------*--------*    Each Layer for both tex1 and tex2 will be
 *      | layer3 | layer3 |   different depths.
 *      *--------*--------*
 *      | layer2 | layer2 |    Tex1 will be cleared using glClear()
 *      *--------*--------*
 *      | layer1 | layer1 |    Tex2 will be cleared using glClearBuffer()
 *      *--------*--------*
 *
 *      Result:
 *        Layer 1-4 of both tex1 and tex2 should be the clearDepth
 */

#include "piglit-util-gl-common.h"

PIGLIT_GL_TEST_CONFIG_BEGIN

	config.supports_gl_compat_version = 32;
	config.supports_gl_core_version = 32;

PIGLIT_GL_TEST_CONFIG_END

const char *vs_source = {
	"#version 150\n"
	"in vec4 piglit_vertex;\n"
	"void main()\n"
	"{\n"
	"	gl_Position = piglit_vertex;\n"
	"}\n"
};

bool
probe_texture_layered_depth(GLuint texture, int x, int y, int z,
			    int w, int h, int d, float *expected)
{
	GLint prev_read_fbo;
	GLint prev_draw_fbo;

	GLuint fbo;
	int i, j, k;

	GLfloat *probe;
	GLfloat *pixels = malloc(w*h*sizeof(float));

	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fbo);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read_fbo);

	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	for(k = 0; k < d; k++ ) {

		glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
					  texture, 0, k+z);

		glReadPixels(x, y, w, h, GL_DEPTH_COMPONENT, GL_FLOAT, pixels);

		for(j = 0; j < h; j++) {
			for(i = 0; i < w; i++) {
				probe = &pixels[j*w+i];
				if (fabs(*probe - expected[k]) >= 0.01) {
					printf("Probe depth at (%i,%i,%i)\n",
					       x+i, y+j,z+k);
					printf("  Expected: %f\n", expected[k]);
					printf("  Observed: %f\n", *probe);

					free(pixels);
					return false;
				}
			}
		}
	}
	free(pixels);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_draw_fbo);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_read_fbo);
	return true;
}

void
piglit_init(int argc, char **argv)
{

	int i, j;
	GLenum fbstatus;
	bool pass = true;
	GLuint fbo[2], texture[2];
	GLint program;

	float depths[4] = {
		0.25, 0.5, 0.75, 1.0
	};

	GLfloat clearDepth = 0.0;
	float expected[4] = { 0.0, 0.0, 0.0, 0.0 };

	program = piglit_build_simple_program(vs_source, NULL);
	glUseProgram(program);

	glGenTextures(2, texture);
	glGenFramebuffers(2, fbo);
	for(i = 0; i < 2; i++) {
		glBindTexture(GL_TEXTURE_2D_ARRAY, texture[i]);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER,
				GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER,
				GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S,
				GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T,
				GL_REPEAT);
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32,
			     10, 10, 4, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);
		glDepthRange(0, 1);
		glDrawBuffer(GL_NONE);

		glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);

		for(j = 0; j < 4; j++) {
			glFramebufferTextureLayer(GL_FRAMEBUFFER,
						  GL_DEPTH_ATTACHMENT,
					          texture[i], 0, j);


			fbstatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if(fbstatus != GL_FRAMEBUFFER_COMPLETE){
				printf("%s\n", piglit_get_gl_enum_name(fbstatus));
				piglit_report_result(PIGLIT_FAIL);
			}

			piglit_check_gl_error(GL_NO_ERROR);
			piglit_draw_rect_z(depths[j], -1, -1, 2, 2);
			piglit_check_gl_error(GL_NO_ERROR);
		}

		glDisable(GL_DEPTH_TEST);

		/* Once values are set, attach the texture to the fbo
		 * as a layered texture
		 */
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
				     texture[i], 0);


		fbstatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(fbstatus != GL_FRAMEBUFFER_COMPLETE){
			printf("%s\n", piglit_get_gl_enum_name(fbstatus));
			piglit_report_result(PIGLIT_FAIL);
		}

		if(!piglit_check_gl_error(GL_NO_ERROR))
			piglit_report_result(PIGLIT_FAIL);
	}


	glBindFramebuffer(GL_FRAMEBUFFER, fbo[0]);
	/* Clear fbo 0 with glClear() */
	glClearDepth(clearDepth);
	glClear(GL_DEPTH_BUFFER_BIT);

	if(!probe_texture_layered_depth(texture[0], 0, 0, 0, 10,
					10, 4, expected)) {
		printf("Incorrect depth values recieved with glClear()\n");
		pass = false;
	}

	/* Clear fbo 1 with glClearBuffer() */
	glBindFramebuffer(GL_FRAMEBUFFER, fbo[1]);
	glClearBufferfv(GL_DEPTH, 0, &clearDepth);

	if(!probe_texture_layered_depth(texture[1], 0, 0, 0, 10,
					10, 4, expected)) {
		printf("Incorrect depth values recieved with glClearBuffer()\n");
		pass = false;
	}

	pass = piglit_check_gl_error(GL_NO_ERROR) && pass;

	piglit_report_result(pass ? PIGLIT_PASS : PIGLIT_FAIL);
}


enum piglit_result
piglit_display(void)
{
	/* UNREACHABLE */
	return PIGLIT_FAIL;
}
