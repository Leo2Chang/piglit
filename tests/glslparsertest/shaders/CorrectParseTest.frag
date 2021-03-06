// [config]
// expect_result: pass
// glsl_version: 1.10
//
// # NOTE: Config section was auto-generated from file
// # NOTE: 'glslparser.tests' at git revision
// # NOTE: 6cc17ae70b70d150aa1751f8e28db7b2a9bd50f0
// [end config]

struct s{
    float f;
    vec3  v;
} s1 ;
void main()
{
    vec4 v = vec4(float(vec2(1,2)), 5,6,7);  // 1, 5, 6, 7
    vec4 v1 = vec4(3, vec2(ivec2(1,2)), 4);  // 3, 1, 2, 4
    vec4 v2 = vec4(8, 9, vec4(ivec4(1,2,3,4))); // 8,9, 1,2 
    vec2 v3 = vec2(v2);  // 8,9
    vec4 v4 = vec4(v3, v2.z, v2.w);  // 8,9,1,2

    const vec4 v5 = vec4(2.0, s(2.0, vec3(3,4,5)).v); // 2,3,4,5
    gl_FragColor = v5 + v + v1 + v4 ;  // 14, 18, 13, 18
}
