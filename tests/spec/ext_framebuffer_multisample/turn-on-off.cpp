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
 * @file turn-on-off.c
 *
 * Algorithm for this test case:
 * 1. Render the geometry to left half of default framebuffer and read color
 *    buffer in to a local buffer.
 *
 * 2. Create a FBO with MSAA turned OFF. Compare the rendered geometry with
 *    buffer in Step 1. It is expected to match.
 *
 * 3. Turn ON MSAA in FBO. Render the scene, draw reference image and measure
 *    the accuracy of MSAA. This varifies if MSAA is turned ON.
 *
 * 4. Turn OFF MSAA in FBO. Render the scene and compare to the buffer in
 *    step 1. It is expected to match.
 *
 * Test image is rendered in to left half of framebuffer.
 * Reference image is rendered in to right half of framebuffer.
 *
 * TODO: Add testing for depth and stencil buffers.
 */

int piglit_width = 512; int piglit_height = 256;
int piglit_window_mode = GLUT_DOUBLE | GLUT_RGBA | GLUT_ALPHA;

int num_samples = 0;
bool small = false;
bool combine_depth_stencil = false;
Test *test = NULL;

const int pattern_width = 256; const int pattern_height = 256;
const int supersample_factor = 16;

enum piglit_result
piglit_display(void)
{
	bool pass = true;
	GLint w = pattern_width, h = pattern_height;
	GLfloat *color_buffer = NULL;

	test->draw_to_default_framebuffer();

	/* Read color buffer */
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	color_buffer = (GLfloat *) malloc(w * h * 4 * sizeof(float));
	glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, color_buffer);

	/* Draw to FBO with single sample render buffer. i.e. MSAA turned OFF.
	 * Set use_render_buffer flag to verify glRenderbufferStorageMultisample()
	 * with zero sample count turns off MSAA.
	 */
	test->test_fbo.set_samples(0);

	test->draw_test_image(&test->test_fbo);
	/* Compare rendered scene with color_buffer */
	glBindFramebuffer(GL_READ_FRAMEBUFFER, test->test_fbo.handle);
	pass = piglit_probe_image_rgba(0, 0, w, h, color_buffer)
	       && pass;

	/* Switch ON MSAA in this FBO by passing num_samples > 0 */
	test->test_fbo.set_samples(num_samples);
	/* Draw test image in multisample FBO */
	test->draw_test_image(&test->test_fbo);
	/* Draw a reference image for MSAA */
	test->draw_reference_image();
	/* Measure the accuracy of MSAA in multisample FBO by comparing the
	 * test image to reference image. This varifies if MSAA is actually
	 * switched on.
	 */
	pass = test->measure_accuracy() && pass;

	/* Switch OFF MSAA again in this FBO */
	test->test_fbo.set_samples(0);
	test->draw_test_image(&test->test_fbo);

	/* Compare rendered scene with color_buffer */
	glBindFramebuffer(GL_READ_FRAMEBUFFER, test->test_fbo.handle);
	pass = piglit_probe_image_rgba(0, 0, w, h, color_buffer)
	       && pass;

	if (!piglit_automatic)
		piglit_present_results();
	return (pass ? PIGLIT_PASS : PIGLIT_FAIL);
}

void
print_usage_and_exit(char *prog_name)
{
	printf("Usage: %s <num_samples> <test_type> [options]\n"
	       "  where <test_type> is one of:\n"
	       "    color: test turning MSAA on/off in color attachment of FBO\n"
	       "Available options:\n"
	       "    small: use a very small (16x16) MSAA buffer\n"
	       "    depthstencil: use a combined depth/stencil buffer\n",
	       prog_name);

	piglit_report_result(PIGLIT_FAIL);
}

void
piglit_init(int argc, char **argv)
{
	GLint i, max_samples;
	if (argc < 3)
		print_usage_and_exit(argv[0]);
	{
		char *endptr = NULL;
		num_samples = strtol(argv[1], &endptr, 0);
		if (endptr != argv[1] + strlen(argv[1]))
			print_usage_and_exit(argv[0]);
	}

	for (i = 3; i < argc; ++i) {
		if (strcmp(argv[i], "small") == 0) {
			small = true;
		} else if (strcmp(argv[i], "depthstencil") == 0) {
			combine_depth_stencil = true;
		} else {
			print_usage_and_exit(argv[0]);
		}
	}

	piglit_require_gl_version(30);
	piglit_require_GLSL_version(130);

	/* Skip the test if num_samples > GL_MAX_SAMPLES */
	glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
	if (num_samples > max_samples) {
		printf("Sample count not supported : %d\n", num_samples);
		piglit_report_result(PIGLIT_SKIP);
	}

	test = create_test(TEST_TYPE_COLOR, num_samples, small,
			   combine_depth_stencil,
			   pattern_width, pattern_height, supersample_factor);
}
