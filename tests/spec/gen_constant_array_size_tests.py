from builtin_function import *
import os.path

this_file = os.path.basename(__file__)

for test_suite_name, test_cases in test_suites.items():
    for shader_type in ('vert', 'frag'):
	if shader_type == 'vert':
	    output_var = 'gl_Position '
	else:
	    output_var = 'gl_FragColor'
	filename = 'glsl-1.20/compiler/built-in-functions/const-{0}.{1}'.format(
	    test_suite_name, shader_type)
	with open(filename, 'w') as f:
	    f.write('/* [config]\n')
	    f.write(' * expect_result: pass\n')
	    f.write(' * glsl_version: 1.20\n')
	    f.write(' * [end config]\n')
	    f.write(' * Automatically generated by {0}\n'.format(this_file))
	    f.write(' */\n')
	    f.write('#version 120\n')
	    f.write('\n')
	    for i, test_case in enumerate(test_cases):
		function_name, arguments, expected_result = test_case
		expected_str = make_constant(expected_result)
		arg_str = ', '.join(make_constant(arg) for arg in arguments)
		residual = 'length({0} - {1}({2}))'.format(
		    expected_str, function_name, arg_str)
		f.write('float[{0} < 0.001 ? 1 : -1] array{1};\n'.format(
			residual, i))
	    f.write('\n')
	    f.write('main()\n')
	    f.write('{\n')
	    array_lengths = [
		'array{0}.length()'.format(i) for i in xrange(len(test_cases))]
	    array_length_sum = '\n                      + '.join(array_lengths)
	    f.write('  {0} = vec4({1});\n'.format(
		    output_var, array_length_sum))
	    f.write('}\n')
