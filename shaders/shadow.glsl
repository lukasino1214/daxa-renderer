#include <shared.inl>


#include "utils/lighting.glsl"
#include "utils/core.glsl"

DAXA_USE_PUSH_CONSTANT(ShadowPush)

#define VERTEX deref(daxa_push_constant.face_buffer[gl_VertexIndex])
#define OBJECT deref(daxa_push_constant.object_buffer)
#define CAMERA deref(daxa_push_constant.camera_buffer)
#define MATERIAL deref(daxa_push_constant.material_info_buffer)
#define LIGHTS deref(daxa_push_constant.lights_buffer)

/*uniform mat4 lightSpaceMatrix;
uniform mat4 model;*/
#if defined(DRAW_VERT)
void main() {
    gl_Position = daxa_push_constant.light_matrix * OBJECT.model_matrix * vec4(VERTEX.position, 1.0);
}
#elif defined(DRAW_FRAG)
void main() {}
#endif