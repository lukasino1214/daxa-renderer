#version 450 core

#include <shared.inl>
#include <core.glsl>

DAXA_USE_PUSH_CONSTANT(DrawPush)

#if defined(DRAW_VERT)
layout(location = 0) out f32vec2 v_uv;
layout(location = 1) out f32vec3 v_normal;
layout(location = 2) out f32vec3 v_position;
layout(location = 3) out f32vec3 v_camera_position;

void main() {
    DrawVertex vertex = read_buffer(DrawVertex, push_constant.face_buffer + gl_VertexIndex * (4 * 8));
    ObjectInfo object = read_buffer(ObjectInfo, push_constant.object_info_buffer);
    //CameraInfo camera = read_buffer(CameraInfo, push_constant.camera_info_buffer);
    f32vec3 position = (object.model_matrix * f32vec4(vertex.position.xyz, 1)).xyz;
    gl_Position = push_constant.vp * f32vec4(position.xyz, 1);
    //gl_Position = push_constant.mvp * f32vec4(vertex.position.xyz, 1);
    v_uv = vertex.uv.xy;
    v_normal = f32mat3x3(object.normal_matrix) * vertex.normal.xyz;
    v_position = position.xyz;
    //v_camera_position = camera.position;
    v_camera_position = push_constant.camera_position;
}

#elif defined(DRAW_FRAG)
layout(location = 0) in f32vec2 v_uv;
layout(location = 1) in f32vec3 v_normal;
layout(location = 2) in f32vec3 v_position;
layout(location = 3) in f32vec3 v_camera_position;

layout(location = 0) out f32vec4 out_color;

const f32 PI = 3.14159265359;

vec3 getNormalFromMap() {
    vec3 tangentNormal = get_texture(push_constant.normal_map, v_uv).xyz * 2.0 - 1.0;

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
// ----------------------------------------------------------------------------

void main() {
    //out_color = f32vec4(get_texture(push_constant.albedo, v_uv).rgb, 1);
    LightsInfo lights = read_buffer(LightsInfo, push_constant.lights_info_buffer);

    vec3 N = getNormalFromMap();

    f32vec3 albedo = get_texture(push_constant.albedo, v_uv).rgb;
    f32vec2 metallic_roughness = get_texture(push_constant.metallic_roughness, v_uv).gb;
    f32 metallic  = metallic_roughness.y;
    f32 roughness = metallic_roughness.x;
    f32 ao = 1.0;

    //vec3 N = v_normal;
    vec3 V = normalize(v_camera_position - v_position);

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
    for(int i = 0; i < lights.num_point_lights; i++)  {
        vec3 L = normalize(lights.point_lights[i].position.xyz - v_position);
        vec3 H = normalize(V + L);
        float distance = length(lights.point_lights[i].position.xyz - v_position);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = lights.point_lights[i].color.xyz * lights.point_lights[i].intensity * attenuation;

        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
           
        vec3 numerator    = NDF * G * F; 
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;

        kD *= 1.0 - metallic;	  

        float NdotL = max(dot(N, L), 0.0);        

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    
    vec3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + vec3(1.0));

    out_color = vec4(color, 1.0);
}

#endif