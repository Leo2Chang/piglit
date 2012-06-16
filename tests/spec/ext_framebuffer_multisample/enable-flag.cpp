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
 * \file enable-flag.cpp
 *
 * This test verifies the proper functioning of the GL_MULTISAMPLE
 * flag.  It operates by performing the following operations:
 *
 * 1. Verify that the default state of GL_MULTISAMPLE is enabled.
 *
 * 2. Draw a test image into a multisampled buffer, with
 *    GL_MULTISAMPLE disabled.
 *
 * 3. Blit this image to the left half of the piglit window (which is
 *    not multisampled) to resolve it.
 *
 * 4. Draw the same test image into a single-sampled buffer.
 *
 * 5. Blit this image to the right half of the piglit window.
 *
 * 6. Verify that the two halves of the piglit window match.  If they
 *    don't, then presumably the disabling of GL_MULTISAMPLE failed to
 *    take effect.
 */

#include "common.h"

int piglit_width = 512; int piglit_height = 256;
int piglit_window_mode = GLUT_DOUBLE | GLUT_RGBA | GLUT_ALPHA;

namespace {

const int pattern_width = 256; const int pattern_height = 256;

Fbo singlesampled_fbo;
Fbo multisampled_fbo;
Triangles triangles;

extern "C" void
piglit_init(int argc, char **argv)
{
	piglit_require_gl_version(30);

	GLint max_samples;
	glGetIntegerv(GL_MAX_SAMPLES, &max_samples);

	singlesampled_fbo.setup(FboConfig(0, pattern_width,
					  pattern_height));
	multisampled_fbo.setup(FboConfig(max_samples, pattern_width,
					 pattern_height));
	triangles.compile();
}

extern "C" enum piglit_result
piglit_display()
{
	bool pass = true;

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glViewport(0, 0, piglit_width, piglit_height);
	glClear(GL_COLOR_BUFFER_BIT);

	/* Verify that the default state of GL_MULTISAMPLE is
	 * enabled.
	 */
	pass = glIsEnabled(GL_MULTISAMPLE) && pass;

	/* Draw a test image into a multisampled buffer, with
	 * GL_MULTISAMPLE disabled.
	 */
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, multisampled_fbo.handle);
	multisampled_fbo.set_viewport();
	glDisable(GL_MULTISAMPLE);
	triangles.draw(TestPattern::no_projection);
	glEnable(GL_MULTISAMPLE);

	/* Blit this image to the left half of the piglit window
	 * (which is not multisampled) to resolve it.
	 */
	glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampled_fbo.handle);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0, 0, pattern_width, pattern_height,
			  0, 0, pattern_width, pattern_height,
			  GL_COLOR_BUFFER_BIT, GL_NEAREST);

	/* Draw the same test image into a single-sampled buffer. */
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, singlesampled_fbo.handle);
	singlesampled_fbo.set_viewport();
	triangles.draw(TestPattern::no_projection);

	/* Blit this image to the right half of the piglit window. */
	glBindFramebuffer(GL_READ_FRAMEBUFFER, singlesampled_fbo.handle);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0, 0, pattern_width, pattern_height,
			  pattern_width, 0, 2*pattern_width, pattern_height,
			  GL_COLOR_BUFFER_BIT, GL_NEAREST);

	/* Verify that the two halves of the piglit window match.  If
	 * they don't, then presumably the disabling of GL_MULTISAMPLE
	 * failed to take effect.
	 */
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	pass = piglit_probe_rect_halves_equal_rgba(0, 0, 2*pattern_width,
						   pattern_height) && pass;

	piglit_present_results();

	return pass ? PIGLIT_PASS : PIGLIT_FAIL;
}

} /* Anonymous namespace */
