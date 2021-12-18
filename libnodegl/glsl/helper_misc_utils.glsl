#define ngli_sat(x) clamp(x, 0.0, 1.0)
#define ngli_linear(a, b, x) (((x) - (a)) / ((b) - (a)))

const vec3 ngli_luma_weights = vec3(.2126, .7152, .0722); // BT.709
const float ngli_pi = 3.14159265358979323846;
