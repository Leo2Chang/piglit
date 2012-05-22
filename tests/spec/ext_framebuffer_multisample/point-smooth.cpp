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

#include "common.h"

/**
 * \file point-smooth.cpp
 *
 * Page 122 (in the PDF) of the OpenGL 3.0 spec says:
 * "If MULTISAMPLE is enabled, and the value of SAMPLE BUFFERS is one,
 * then points are rasterized using the following algorithm, regardless
 * of whether point antialias-ing (POINT SMOOTH) is enabled or disabled".
 *
 * This test operates by drawing a test image with GL_POINT_SMOOTH
 * enabled in an MSAA buffer. Blit it in to left half of window system
 * framebuffer.
 *
 * Draw same image second time with GL_POINT_SMOOTH disabled. Blit it
 * in to right half of window system framebuffer. This is the reference
 * image.
 *
 * To verify that GL_POINT_SMOOTH doesn't affect MSAA, compare the two
 * halves of default framebuffer. There should be no difference.
 *
 */
int piglit_width = 512; int piglit_height = 256;
int piglit_window_mode =
	GLUT_DOUBLE | GLUT_RGBA | GLUT_ALPHA | GLUT_DEPTH | GLUT_STENCIL;

const int pattern_width = 256; const int pattern_height = 256;

Fbo test_fbo;
TestPattern *test_pattern = NULL;
ManifestProgram *manifest_program = NULL;
GLbitfield buffer_to_test;

void
print_usage_and_exit(char *prog_name)
{
	printf("Usage: %s <num_samples> <buffer_type>\n"
	       "  where <buffer_type> is one of:\n"
	       "    color\n"
	       "    stencil\n"
	       "    depth\n",
	       prog_name);
	piglit_report_result(PIGLIT_FAIL);
}

void
piglit_init(int argc, char **argv)
{
	int num_samples;
	if (argc < 3)
		print_usage_and_exit(argv[0]);
	{
		char *endptr = NULL;
		num_samples = strtol(argv[1], &endptr, 0);
		if (endptr != argv[1] + strlen(argv[1]))
			print_usage_and_exit(argv[0]);
	}

	piglit_require_gl_version(30);
	piglit_require_GLSL_version(130);

	glClear (GL_COLOR_BUFFER_BIT);

	/* Skip the test if num_samples > GL_MAX_SAMPLES */
	GLint max_samples;
	glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
	if (num_samples > max_samples)
		piglit_report_result(PIGLIT_SKIP);

	if (strcmp(argv[2], "color") == 0) {
		test_pattern = new Points();
		buffer_to_test = GL_COLOR_BUFFER_BIT;
	} else if (strcmp(argv[2], "depth") == 0) {
		test_pattern = new DepthSpiral();
		manifest_program = new ManifestDepth();
		buffer_to_test = GL_DEPTH_BUFFER_BIT;
	} else if (strcmp(argv[2], "stencil") == 0) {
		test_pattern = new StencilSpiral();
		manifest_program = new ManifestStencil();
		buffer_to_test = GL_STENCIL_BUFFER_BIT;
	} else {
		print_usage_and_exit(argv[0]);
	}

	test_pattern->compile();
	if (manifest_program)
		manifest_program->compile();

	test_fbo.init(num_samples, pattern_width, pattern_height,
		     true /* combine_depth_stencil */,
		     false /* attach_texture */);

	/* Blending is required to test smooth points */
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA_SATURATE, GL_ONE);
}

enum piglit_result
piglit_display()
{
	bool pass = true;
	float proj[4][4] = {
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 } };
	/* Draw test pattern in  multisample test_fbo with GL_POINT_SMOOTH
	 * disabled.
	 */
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, test_fbo.handle);
	glClear(buffer_to_test);
	test_fbo.set_viewport();
	test_pattern->draw(proj);

	/* Blit test_fbo to the right half of window system framebuffer.
	 * This is the reference image.
	 */
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, test_fbo.handle);
	test_fbo.set_viewport();
	if (manifest_program)
		manifest_program->run();
	glBindFramebuffer(GL_READ_FRAMEBUFFER, test_fbo.handle);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0, 0, pattern_width, pattern_height,
			  pattern_width, 0, 2*pattern_width, pattern_height,
			  GL_COLOR_BUFFER_BIT, GL_NEAREST);

	/* Draw test pattern in mulisample test_fbo with GL_POINT_SMOOTH
	 * enabled
	 */
	glEnable (GL_POINT_SMOOTH);
	glHint( GL_POINT_SMOOTH_HINT, GL_NICEST );
	glDisable (GL_DEPTH_TEST);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, test_fbo.handle);
	test_fbo.set_viewport();
	test_pattern->draw(proj);

	glDisable(GL_POINT_SMOOTH);

	/* Now blit test_fbo to the left half of window system framebuffer.
	 * This is the test image.
	 */
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, test_fbo.handle);
	test_fbo.set_viewport();
	if (manifest_program)
		manifest_program->run();
	glBindFramebuffer(GL_READ_FRAMEBUFFER, test_fbo.handle);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0, 0, pattern_width, pattern_height,
			  0, 0, pattern_width, pattern_height,
			  GL_COLOR_BUFFER_BIT, GL_NEAREST);

	/* Check that the left and right halves of the screen match. If they
	 * don't, then GL_POINT_SMOOTH is not ignored with multisample
	 * rendering.
	 */
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	pass = piglit_probe_rect_halves_equal_rgba(0, 0, piglit_width,
						   piglit_height) && pass;

	pass = piglit_check_gl_error(GL_NO_ERROR) && pass;

	piglit_present_results();

	return pass ? PIGLIT_PASS : PIGLIT_FAIL;
}
