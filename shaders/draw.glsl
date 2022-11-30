#include <shared.inl>
#include <core.glsl>

DAXA_USE_PUSH_CONSTANT(DrawPush)

#if defined(DRAW_VERT)
layout(location = 0) out f32vec2 v_uv;
layout(location = 1) out f32vec3 v_normal;
layout(location = 2) out f32vec3 v_position;
layout(location = 3) out f32vec3 v_camera_position;

void main() {
    DrawVertex vertex = push_constant.face_buffer.vertices[gl_VertexIndex];
    ObjectInfo object = push_constant.object_info_buffer.info;
    CameraInfo camera = push_constant.camera_info_buffer.info;
    f32vec3 position = (object.model_matrix * f32vec4(vertex.position.xyz, 1)).xyz;
    gl_Position = camera.projection_matrix * camera.view_matrix * f32vec4(position.xyz, 1);
    //gl_Position = push_constant.vp * f32vec4(position.xyz, 1);
    //gl_Position = push_constant.mvp * f32vec4(vertex.position.xyz, 1);
    v_uv = vertex.uv.xy;
    v_normal = f32mat3x3(object.normal_matrix) * vertex.normal.xyz;
    v_position = position.xyz;
    v_camera_position = camera.position;
    //v_camera_position = push_constant.camera_position;
}

#elif defined(DRAW_FRAG)
layout(location = 0) in f32vec2 v_uv;
layout(location = 1) in f32vec3 v_normal;
layout(location = 2) in f32vec3 v_position;
layout(location = 3) in f32vec3 v_camera_position;

layout(location = 0) out f32vec4 out_color;

const f32 PI = 3.14159265359;

vec3 getNormalFromMap(TextureId normal_map) {
    vec3 tangentNormal = sample_texture(normal_map, v_uv).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(v_position);
    vec3 Q2  = dFdy(v_position);
    vec2 st1 = dFdx(v_uv);
    vec2 st2 = dFdy(v_uv);

    vec3 N   = normalize(v_normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
/*vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    if(cosTheta>1.0)
        cosTheta=1.0;
    float p=pow(1.0 - cosTheta, 5.0);
    return F0 + (1.0 - F0) * p;
}*/
// ----------------------------------------------------------------------------
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

f32vec3 calculate_point_light(PointLight light, f32vec3 frag_color, f32vec3 normal, f32vec3 frag_position, f32vec3 camera_position) {
    #if defined(SETTINGS_SHADING_MODEL_LAMBERTIAN)
    f32vec3 light_dir = normalize(light.position - frag_position);
    f32 distance = length(light.position.xyz - frag_position);
    f32 attenuation = 1.0f / (distance * distance);
    f32vec3 diffuse = frag_color * light.color * max(dot(normal, light_dir), 0.0) * attenuation * light.intensity;
    return diffuse;
    #endif
    #if defined(SETTINGS_SHADING_MODEL_PHONG)
    f32vec3 light_dir = normalize(light.position - frag_position);
    f32vec3 view_dir = normalize(camera_position - frag_position);
    f32 distance = length(light.position.xyz - frag_position);
    f32 attenuation = 1.0f / (distance * distance);
    f32 diffuse = max(dot(normal, light_dir), 0.0);
    f32 specular = max(dot(view_dir, reflect(-light_dir, normal)), 0.0);
    return frag_color * light.color * (diffuse + specular) * attenuation * light.intensity;
    #endif
    #if defined(SETTINGS_SHADING_MODEL_BLINN_PHONG)
    f32vec3 light_dir = normalize(light.position - frag_position);
    f32vec3 view_dir = normalize(camera_position - frag_position);
    f32vec3 halfway_dir = normalize(light_dir + view_dir);
    f32 distance = length(light.position.xyz - frag_position);
    f32 attenuation = 1.0f / (distance * distance);
    f32 diffuse = max(dot(normal, light_dir), 0.0);
    f32 specular = max(dot(view_dir, halfway_dir), 0.0);
    return frag_color * light.color * (diffuse + specular) * attenuation * light.intensity;
    #endif
    #if defined(SETTINGS_SHADING_MODEL_GAUSSIAN)
    f32vec3 light_dir = normalize(light.position - frag_position);
    f32vec3 view_dir = normalize(camera_position - frag_position);
    f32vec3 halfway_dir = normalize(light_dir + view_dir);
    f32 distance = length(light.position.xyz - frag_position);
    f32 attenuation = 1.0f / (distance * distance);
    f32 diffuse = max(dot(normal, light_dir), 0.0);
    f32 normal_half = acos(dot(halfway_dir, normal));
    f32 exponent = normal_half / 1.0;
    exponent = -(exponent * exponent);
    return frag_color * light.color * (diffuse + exp(exponent)) * attenuation * light.intensity;
    #endif
    return f32vec3(1.0);
}

void main() {
    #if defined(SETTINGS_TEXTURING_NONE)
    f32vec3 color = f32vec3(1.0, 1.0, 1.0);
    #elif defined(SETTINGS_TEXTURING_VERTEX_COLOR)
    f32vec3 color = f32vec3(0.5, 0.5, 0.5);
    #elif defined(SETTINGS_TEXTURING_ALBEDO)
    MaterialInfo material_info = push_constant.material_info.info;
    f32vec3 color = sample_texture(material_info.albedo, v_uv).rgb;
    #else
    f32vec3 color = f32vec3(0.0, 0.0, 0.0);
    #endif

    #if defined(SETTINGS_SHADING_MODEL_NONE)

    #elif defined(SETTINGS_SHADING_MODEL_LAMBERTIAN)
    f32vec3 normal = normalize(v_normal);
    LightsInfo lights = push_constant.lights_info_buffer.info;
    //for(uint i = 0; i < lights.num_dir_lights; i++) {}
    for(uint i = 0; i < lights.num_point_lights; i++) {
        color += calculate_point_light(lights.point_lights[i], color, normal, v_position, v_camera_position);
    }
    //color = f32vec3(1.0, 0.0, 0.0);
    //for(uint i = 0; i < lights.num_spot_lights; i++) {}


    /*f32vec3 normal = normalize(v_normal);
    f32vec3 light_dir = normalize(f32vec3(0.0, 260.0, 0.0) - v_position);
    float diffuse = max(dot(normal, light_dir), 0.0);
    color = diffuse * color;*/
    #elif defined(SETTINGS_SHADING_MODEL_PHONG)
    f32vec3 normal = normalize(v_normal);
    LightsInfo lights = push_constant.lights_info_buffer.info;
    //for(uint i = 0; i < lights.num_dir_lights; i++) {}
    for(uint i = 0; i < lights.num_point_lights; i++) {
        color += calculate_point_light(lights.point_lights[i], color, normal, v_position, v_camera_position);
    }

    #elif defined(SETTINGS_SHADING_MODEL_BLINN_PHONG)
    f32vec3 normal = normalize(v_normal);
    LightsInfo lights = push_constant.lights_info_buffer.info;
    //for(uint i = 0; i < lights.num_dir_lights; i++) {}
    for(uint i = 0; i < lights.num_point_lights; i++) {
        color += calculate_point_light(lights.point_lights[i], color, normal, v_position, v_camera_position);
    }

    #elif defined(SETTINGS_SHADING_MODEL_GAUSSIAN)
    f32vec3 normal = normalize(v_normal);
    LightsInfo lights = push_constant.lights_info_buffer.info;
    //for(uint i = 0; i < lights.num_dir_lights; i++) {}
    for(uint i = 0; i < lights.num_point_lights; i++) {
        color += calculate_point_light(lights.point_lights[i], color, normal, v_position, v_camera_position);
    }

    #else
    color = f32vec3(0.0, 0.0, 0.0);
    #endif

    out_color = vec4(color, 1.0);
}

#endif