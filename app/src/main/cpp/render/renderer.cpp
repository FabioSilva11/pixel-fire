#include "renderer.h"

#include <algorithm>
#include <cmath>

#include "../core/log.h"
#include "../core/random.h"
#include "../world/block_textures.h"

namespace zz {

namespace {

constexpr int kBillboardFloats = 9;    // pos(3) cor(4) uv(2)
constexpr float kExposure = 0.8f;      // evita superexposição das cores das texturas

std::string shaderSource(const char* defines, const char* body) {
    std::string s = "#version 300 es\n";
    s += defines;
    s += body;
    return s;
}

const char* kVoxelVs = R"(
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_meta;
uniform mat4 u_viewProj;
uniform float u_time;
uniform vec2 u_chunkOrigin;
out vec3 v_uvl;
out float v_light;
out vec3 v_world;
flat out float v_flags;
void main() {
    vec3 p0 = a_pos * 0.125 + vec3(u_chunkOrigin.x,0.0,u_chunkOrigin.y);
    vec3 p = p0;
    float flags = a_meta.z + 0.5;
    float water = mod(floor(flags), 2.0);
    float sway = mod(floor(flags / 4.0), 2.0);
    float leaf = mod(floor(flags / 8.0), 2.0);
    // Vento: rajadas lentas que atravessam o mapa + oscilação rápida.
    float gust = 0.55 + 0.45 * sin(u_time * 0.55 + p0.x * 0.021 + p0.z * 0.017);
    if (sway > 0.5) {
        float w = sin(u_time * 2.2 + p0.x * 0.55 + p0.z * 0.35) + 0.35 * sin(u_time * 3.4 + p0.x * 1.3 - p0.z * 0.9);
        p.xz += vec2(0.83, 0.55) * (w * 0.12 + 0.1) * gust;
        p.y -= abs(w) * 0.05 * gust;
    }
    if (leaf > 0.5) {
        p += vec3(sin(u_time * 1.9 + p0.y * 0.8 + p0.z * 0.6),
                  0.6 * sin(u_time * 2.6 + p0.x * 0.7 + p0.z * 0.3),
                  cos(u_time * 1.6 + p0.x * 0.5 + p0.y * 0.7)) * (0.045 * gust);
    }
    vec2 uv = a_uv;
    if (water > 0.5) {
        p.y += sin(u_time * 1.6 + p0.x * 0.7 + p0.z * 0.5) * 0.035 - 0.03;
        uv += vec2(u_time * 0.05, u_time * 0.035);
    }
    v_uvl = vec3(uv, a_meta.x);
    v_light = a_meta.y / 255.0;
    v_flags = a_meta.z;
    v_world = p;
    gl_Position = u_viewProj * vec4(p, 1.0);
}
)";

// Luzes pontuais e flash do disparo.
const char* kLightingCommon = R"(
uniform int u_lightCount;
uniform vec4 u_lights[8];
uniform vec3 u_lightColors[8];
uniform vec4 u_muzzle;
vec3 pointLights(vec3 p) {
    vec3 l = vec3(0.0);
    for (int i = 0; i < 8; ++i) {
        if (i >= u_lightCount) break;
        float d = distance(p, u_lights[i].xyz);
        float att = clamp(1.0 - d / u_lights[i].w, 0.0, 1.0);
        l += u_lightColors[i] * att * att * 1.3;
    }
    float fd = distance(p, u_muzzle.xyz);
    float fatt = clamp(1.0 - fd / 7.0, 0.0, 1.0);
    l += vec3(1.0, 0.85, 0.55) * fatt * fatt * u_muzzle.w;
    return l;
}
)";

const char* kVoxelFs = R"(
precision highp float;
precision mediump sampler2DArray;
uniform sampler2DArray u_tex;
uniform vec3 u_dayLight;
uniform vec3 u_fogColor;
uniform vec2 u_fogRange;
uniform vec3 u_camPos;
uniform float u_alphaTest;
uniform float u_alpha;
uniform float u_time;
in vec3 v_uvl;
in float v_light;
in vec3 v_world;
flat in float v_flags;
out vec4 o_color;
)";

const char* kVoxelFsMain = R"(
void main() {
    vec4 tex = texture(u_tex, v_uvl);
    if (tex.a < u_alphaTest) discard;
    float flags = v_flags + 0.5;
    float emissive = mod(floor(flags / 2.0), 2.0);
    float grass = mod(floor(flags / 16.0), 2.0);
    vec3 albedo = tex.rgb;
    // Ondas de vento correndo pelos campos de grama.
    float wave = sin(dot(v_world.xz, vec2(0.13, 0.08)) - u_time * 1.8) * 0.5 + 0.5;
    albedo *= 1.0 + grass * (wave * wave * 0.14 - 0.05);
    float dist = distance(v_world, u_camPos);
    vec3 light = (u_dayLight + pointLights(v_world)) * v_light;
    vec3 color = albedo * light;
    color = mix(color, tex.rgb * 1.1, emissive);
    float fog = smoothstep(u_fogRange.x, u_fogRange.y, dist);
    o_color = vec4(mix(color, u_fogColor, fog), u_alpha * max(tex.a, 0.0));
}
)";

const char* kModelVs = R"(
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
uniform mat4 u_model;
uniform mat4 u_viewProj;
out vec3 v_normal;
out vec2 v_uv;
out vec3 v_world;
void main() {
    vec4 w = u_model * vec4(a_pos, 1.0);
    v_world = w.xyz;
    v_normal = mat3(u_model) * a_normal;
    v_uv = a_uv;
    gl_Position = u_viewProj * w;
}
)";

const char* kModelFs = R"(
precision highp float;
uniform sampler2D u_tex;
uniform float u_useTex;
uniform vec4 u_color;
uniform vec3 u_tint;
uniform float u_flash;
uniform vec3 u_flashColor;
uniform float u_emissive;
uniform vec3 u_sunDir;
uniform vec3 u_sunColor;
uniform vec3 u_ambient;
uniform vec3 u_fogColor;
uniform vec2 u_fogRange;
uniform vec3 u_camPos;
uniform float u_studio;
in vec3 v_normal;
in vec2 v_uv;
in vec3 v_world;
out vec4 o_color;

// Tom ACES do three.js (ACESFilmicToneMapping), com a exposição 1,55 da página de teste.
vec3 acesFilmic(vec3 color) {
    const mat3 inputMat = mat3(vec3(0.59719, 0.07600, 0.02840), vec3(0.35458, 0.90834, 0.13383), vec3(0.04823, 0.01566, 0.83777));
    const mat3 outputMat = mat3(vec3(1.60475, -0.10208, -0.00327), vec3(-0.53108, 1.10813, -0.07276), vec3(-0.07367, -0.00605, 1.07602));
    color = inputMat * (color * (1.55 / 0.6));
    vec3 a = color * (color + 0.0245786) - 0.000090537;
    vec3 b = color * (0.983729 * color + 0.4329510) + 0.238081;
    return clamp(outputMat * (a / b), 0.0, 1.0);
}

// Luz de estúdio da página (tools/personagem-voxel): céu/chão, principal, preenchimento e contorno, presas à
// câmera para o personagem sempre aparecer igual; a luz do mundo (dia/noite) só regula o brilho geral.
vec3 studioLight(vec3 albedo, vec3 n) {
    vec3 lin = pow(albedo, vec3(2.2));
    vec3 fwd = normalize(v_world - u_camPos);
    vec3 right = normalize(cross(fwd, vec3(0.0, 1.0, 0.0)));
    vec3 up = cross(right, fwd);
    vec3 keyDir = normalize(-fwd * 0.55 + up * 0.75 - right * 0.4);
    vec3 fillDir = normalize(fwd * 0.75 + up * 0.25 + right * 0.45);
    vec3 rimDir = normalize(right * 0.8 + up * 0.25 + fwd * 0.35);
    float hemi = clamp(n.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 irradiance = mix(vec3(0.267, 0.2, 0.129), vec3(0.906, 0.937, 1.0), hemi) * 0.70
        + vec3(1.0, 0.878, 0.71) * max(dot(n, keyDir), 0.0) * 1.02
        + vec3(0.667, 0.776, 0.898) * max(dot(n, fillDir), 0.0) * 0.51
        + vec3(1.0) * max(dot(n, rimDir), 0.0) * 0.32;
    float world = clamp(dot(u_ambient + u_sunColor * 0.6, vec3(0.333)) * 1.6, 0.35, 1.0);
    return pow(acesFilmic(lin * irradiance * world), vec3(1.0 / 2.2));
}
)";

const char* kModelFsMain = R"(
void main() {
    vec4 base = u_useTex > 0.5 ? texture(u_tex, v_uv) : vec4(1.0);
    base *= u_color;
    base.rgb *= u_tint;
    vec3 n = normalize(v_normal);
    float ndl = max(dot(n, u_sunDir), 0.0);
    float sh = 1.0;
    vec3 light = u_ambient + u_sunColor * ndl * sh + vec3(0.06) * max(-n.y, 0.0) + pointLights(v_world) * 0.7;
    vec3 color = base.rgb * min(light, vec3(1.25));
    color = min(color, vec3(1.25));
    color = mix(min(color, vec3(0.9)), color, step(0.05, u_emissive + u_flash));
    if (u_studio > 0.5) color = studioLight(base.rgb, n);
    color = mix(color, base.rgb, u_emissive);
    color = mix(color, u_flashColor, u_flash);
    float fog = smoothstep(u_fogRange.x, u_fogRange.y, distance(v_world, u_camPos));
    o_color = vec4(mix(color, u_fogColor, fog), base.a);
}
)";

const char* kSkyVs = R"(
out vec2 v_ndc;
void main() {
    vec2 pts[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    v_ndc = pts[gl_VertexID];
    gl_Position = vec4(v_ndc, 0.0, 1.0);
}
)";

const char* kSkyFs = R"(
precision highp float;
uniform mat4 u_invViewProj;
uniform vec3 u_camPos;
uniform vec3 u_zenith;
uniform vec3 u_horizon;
uniform vec3 u_fog;
uniform vec3 u_sunDir;
uniform vec3 u_sunColor;
uniform float u_night;
uniform float u_time;
in vec2 v_ndc;
out vec4 o_color;
void main() {
    vec4 far = u_invViewProj * vec4(v_ndc, 1.0, 1.0);
    vec3 dir = normalize(far.xyz / far.w - u_camPos);
    float h = dir.y;
    vec3 col = mix(u_horizon, u_zenith, smoothstep(0.0, 0.55, h));
    col = mix(col, u_fog * 0.85, smoothstep(0.02, -0.25, h));
    vec3 s = normalize(u_sunDir);
    vec3 t1 = normalize(cross(abs(s.y) > 0.9 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0), s));
    vec3 t2 = cross(s, t1);
    float ds = dot(dir, s);
    if (ds > 0.0) {
        vec2 q = vec2(dot(dir, t1), dot(dir, t2)) / ds;
        float m = max(abs(q.x), abs(q.y));
        vec3 disc = u_night > 0.5 ? vec3(0.92, 0.94, 1.0) : u_sunColor * 1.35;
        col = mix(col, disc, step(m, 0.075));
        col += u_sunColor * 0.22 * pow(ds, 24.0);
    }
    if (u_night > 0.01 && h > 0.02) {
        vec3 cell = floor(dir * 160.0);
        float r = fract(sin(dot(cell, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
        if (r > 0.9955) col += vec3(0.85) * u_night * (0.6 + 0.4 * sin(u_time * 2.5 + r * 90.0));
    }
    o_color = vec4(col, 1.0);
}
)";

const char* kCloudVs = R"(
layout(location = 0) in vec2 a_pos;
uniform mat4 u_viewProj;
uniform vec3 u_center;
uniform float u_size;
uniform float u_time;
out vec2 v_uv;
out vec3 v_world;
void main() {
    vec3 w = vec3(u_center.x + a_pos.x * u_size, u_center.y, u_center.z + a_pos.y * u_size);
    v_world = w;
    v_uv = (w.xz + vec2(u_time * 1.2, 0.0)) / 768.0;
    gl_Position = u_viewProj * vec4(w, 1.0);
}
)";

const char* kCloudFs = R"(
precision highp float;
uniform sampler2D u_tex;
uniform vec3 u_cloudColor;
uniform vec3 u_camPos;
uniform float u_fade;
in vec2 v_uv;
in vec3 v_world;
out vec4 o_color;
void main() {
    float m = texture(u_tex, v_uv).r;
    if (m < 0.5) discard;
    float d = distance(v_world.xz, u_camPos.xz);
    float fade = 1.0 - smoothstep(u_fade * 0.5, u_fade, d);
    o_color = vec4(u_cloudColor, 0.78 * fade);
}
)";

const char* kBillboardVs = R"(
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec2 a_uv;
uniform mat4 u_viewProj;
out vec4 v_color;
out vec2 v_uv;
out vec3 v_world;
void main() {
    v_color = a_color;
    v_uv = a_uv;
    v_world = a_pos;
    gl_Position = u_viewProj * vec4(a_pos, 1.0);
}
)";

const char* kBillboardFs = R"(
precision highp float;
uniform sampler2D u_tex;
uniform int u_mode;
uniform float u_time;
uniform vec3 u_fogColor;
uniform vec2 u_fogRange;
uniform vec3 u_camPos;
in vec4 v_color;
in vec2 v_uv;
in vec3 v_world;
out vec4 o_color;
void main() {
    vec4 c;
    if (u_mode == 0) {
        c = vec4(v_color.rgb, v_color.a * texture(u_tex, v_uv).r);
    } else if (u_mode == 1) {
        c = v_color;
    } else if (u_mode == 3) {
        // Anel de aviso pulsante (onde um projétil vai cair).
        float r = length(v_uv - 0.5) * 2.0;
        float pulse = 0.65 + 0.35 * sin(u_time * 11.0);
        float ring = smoothstep(0.76, 0.86, r) * (1.0 - smoothstep(0.92, 1.0, r));
        float fill = (1.0 - smoothstep(0.0, 0.95, r)) * 0.32;
        c = vec4(v_color.rgb, v_color.a * (ring + fill) * pulse);
    } else {
        vec2 p = v_uv - 0.5;
        float r = length(p * vec2(1.0, 0.75));
        float ang = atan(p.y, p.x);
        float sw = sin(ang * 3.0 + r * 16.0 - u_time * 4.0) * 0.5 + 0.5;
        vec3 col = mix(vec3(0.32, 0.08, 0.7), vec3(0.88, 0.55, 1.0), sw);
        c = vec4(col, v_color.a * (0.72 + 0.28 * sw));
    }
    float fog = smoothstep(u_fogRange.x, u_fogRange.y, distance(v_world, u_camPos));
    c.rgb = mix(c.rgb, u_fogColor, fog * 0.8);
    o_color = c;
}
)";


const float kBoxVerts[] = {
    // +X
    0.5f, -0.5f, 0.5f, 1, 0, 0, 0, 1,  0.5f, -0.5f, -0.5f, 1, 0, 0, 1, 1,  0.5f, 0.5f, -0.5f, 1, 0, 0, 1, 0,  0.5f, 0.5f, 0.5f, 1, 0, 0, 0, 0,
    // -X
    -0.5f, -0.5f, -0.5f, -1, 0, 0, 0, 1,  -0.5f, -0.5f, 0.5f, -1, 0, 0, 1, 1,  -0.5f, 0.5f, 0.5f, -1, 0, 0, 1, 0,  -0.5f, 0.5f, -0.5f, -1, 0, 0, 0, 0,
    // +Y
    -0.5f, 0.5f, 0.5f, 0, 1, 0, 0, 1,  0.5f, 0.5f, 0.5f, 0, 1, 0, 1, 1,  0.5f, 0.5f, -0.5f, 0, 1, 0, 1, 0,  -0.5f, 0.5f, -0.5f, 0, 1, 0, 0, 0,
    // -Y
    -0.5f, -0.5f, -0.5f, 0, -1, 0, 0, 1,  0.5f, -0.5f, -0.5f, 0, -1, 0, 1, 1,  0.5f, -0.5f, 0.5f, 0, -1, 0, 1, 0,  -0.5f, -0.5f, 0.5f, 0, -1, 0, 0, 0,
    // +Z
    -0.5f, -0.5f, 0.5f, 0, 0, 1, 0, 1,  0.5f, -0.5f, 0.5f, 0, 0, 1, 1, 1,  0.5f, 0.5f, 0.5f, 0, 0, 1, 1, 0,  -0.5f, 0.5f, 0.5f, 0, 0, 1, 0, 0,
    // -Z
    0.5f, -0.5f, -0.5f, 0, 0, -1, 0, 1,  -0.5f, -0.5f, -0.5f, 0, 0, -1, 1, 1,  -0.5f, 0.5f, -0.5f, 0, 0, -1, 1, 0,  0.5f, 0.5f, -0.5f, 0, 0, -1, 0, 0,
};

void pushVertex(std::vector<float>& out, const Vec3& p, const Vec4& c, float u, float v) {
    out.insert(out.end(), {p.x, p.y, p.z, c.x, c.y, c.z, c.w, u, v});
}

void pushQuad(std::vector<float>& out, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec4& color) {
    // Dois triângulos: a b c / a c d (uv a=(0,1) b=(1,1) c=(1,0) d=(0,0))
    pushVertex(out, a, color, 0, 1);
    pushVertex(out, b, color, 1, 1);
    pushVertex(out, c, color, 1, 0);
    pushVertex(out, a, color, 0, 1);
    pushVertex(out, c, color, 1, 0);
    pushVertex(out, d, color, 0, 0);
}

void extractPlanes(const Mat4& vp, Vec4* out) {
    const float* m = vp.m;
    Vec4 rows[4] = {{m[0], m[4], m[8], m[12]}, {m[1], m[5], m[9], m[13]}, {m[2], m[6], m[10], m[14]}, {m[3], m[7], m[11], m[15]}};
    for (int i = 0; i < 3; ++i) {
        out[i * 2] = {rows[3].x + rows[i].x, rows[3].y + rows[i].y, rows[3].z + rows[i].z, rows[3].w + rows[i].w};
        out[i * 2 + 1] = {rows[3].x - rows[i].x, rows[3].y - rows[i].y, rows[3].z - rows[i].z, rows[3].w - rows[i].w};
    }
}

}  // namespace

void Renderer::onSurfaceCreated() {
    ++contextId_;
    gpuBytes_ = 0;  // os VBOs do contexto antigo se perderam
    // Handles antigos ficaram inválidos com o contexto; recria tudo sem chamar glDelete.
    quadIbo_ = 0;
    quadIboCapacity_ = 0;
    billboardCapacity_ = 0;
    themeDirty_ = true;
    garbage_.clear();
    createPrograms();
    createStaticBuffers();
    LOGI("GL: %s | %s", glGetString(GL_RENDERER), glGetString(GL_VERSION));
}

void Renderer::onSurfaceChanged(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    glViewport(0, 0, width_, height_);
}

void Renderer::setTheme(const Theme& theme) {
    theme_ = theme;
    themeDirty_ = true;
}

void Renderer::createPrograms() {
    ShaderProgram::invalidateAll();  // cache de locais de uniformes
    const std::string vs = shaderSource("", kVoxelVs);
    voxel_.id = compileProgram(vs.c_str(), (shaderSource("", kVoxelFs) + kLightingCommon + kVoxelFsMain).c_str(), "voxel");
    const std::string mvs = shaderSource("", kModelVs);
    model_.id = compileProgram(mvs.c_str(), (shaderSource("", kModelFs) + kLightingCommon + kModelFsMain).c_str(), "model");
    sky_.id = compileProgram(shaderSource("", kSkyVs).c_str(), shaderSource("", kSkyFs).c_str(), "sky");
    billboard_.id = compileProgram(shaderSource("", kBillboardVs).c_str(), shaderSource("", kBillboardFs).c_str(), "billboard");
    cloud_.id = compileProgram(shaderSource("", kCloudVs).c_str(), shaderSource("", kCloudFs).c_str(), "cloud");
}

void Renderer::createStaticBuffers() {
    glGenVertexArrays(1, &skyVao_);

    // Cubo unitário (armas, coletáveis, itens dos zumbis).
    glGenVertexArrays(1, &boxVao_);
    glGenBuffers(1, &boxVbo_);
    glGenBuffers(1, &boxIbo_);
    glBindVertexArray(boxVao_);
    glBindBuffer(GL_ARRAY_BUFFER, boxVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kBoxVerts), kBoxVerts, GL_STATIC_DRAW);
    uint16_t idx[36];
    for (int f = 0; f < 6; ++f) {
        uint16_t b = static_cast<uint16_t>(f * 4);
        uint16_t q[6] = {b, static_cast<uint16_t>(b + 1), static_cast<uint16_t>(b + 2), b, static_cast<uint16_t>(b + 2), static_cast<uint16_t>(b + 3)};
        std::copy(q, q + 6, idx + f * 6);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, boxIbo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, reinterpret_cast<void*>(12));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, reinterpret_cast<void*>(24));
    glBindVertexArray(0);

    // Plano das nuvens.
    const float cloud[] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
    glGenVertexArrays(1, &cloudVao_);
    glGenBuffers(1, &cloudVbo_);
    glBindVertexArray(cloudVao_);
    glBindBuffer(GL_ARRAY_BUFFER, cloudVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cloud), cloud, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
    glBindVertexArray(0);

    // Máscara de nuvens em blocos.
    std::vector<uint8_t> mask(64 * 64);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            float n = 0;
            float amp = 0.5f, freq = 1.0f / 8.0f, norm = 0;
            for (int o = 0; o < 3; ++o) {
                // ruído periódico (tileável) em 64
                int period = static_cast<int>(64 * freq);
                float fx = x * freq, fy = y * freq;
                int x0 = static_cast<int>(fx), y0 = static_cast<int>(fy);
                float tx = fx - x0, ty = fy - y0;
                auto h = [&](int a, int b) { return hashFloat(((a % period) + period) % period, ((b % period) + period) % period, 900u + o); };
                float v = lerpf(lerpf(h(x0, y0), h(x0 + 1, y0), tx), lerpf(h(x0, y0 + 1), h(x0 + 1, y0 + 1), tx), ty);
                n += v * amp;
                norm += amp;
                amp *= 0.5f;
                freq *= 2.0f;
            }
            mask[y * 64 + x] = (n / norm) > 0.56f ? 255 : 0;
        }
    glGenTextures(1, &cloudTexture_);
    glBindTexture(GL_TEXTURE_2D, cloudTexture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 64, 64, 0, GL_RED, GL_UNSIGNED_BYTE, mask.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Círculo suave para partículas, sombras e decalques.
    std::vector<uint8_t> soft(32 * 32);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) {
            float dx = (x + 0.5f) / 16.0f - 1.0f, dy = (y + 0.5f) / 16.0f - 1.0f;
            float d = std::sqrt(dx * dx + dy * dy);
            soft[y * 32 + x] = static_cast<uint8_t>(clampf(1.0f - d, 0, 1) * clampf(1.0f - d, 0, 1) * 255.0f);
        }
    glGenTextures(1, &softTexture_);
    glBindTexture(GL_TEXTURE_2D, softTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 32, 32, 0, GL_RED, GL_UNSIGNED_BYTE, soft.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    glGenVertexArrays(1, &billboardVao_);
    glGenBuffers(1, &billboardVbo_);
    glBindVertexArray(billboardVao_);
    glBindBuffer(GL_ARRAY_BUFFER, billboardVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kBillboardFloats * 4, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, kBillboardFloats * 4, reinterpret_cast<void*>(12));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, kBillboardFloats * 4, reinterpret_cast<void*>(28));
    glBindVertexArray(0);

    glGenBuffers(1, &quadIbo_);
    ensureQuadIndices(16384);
}

void Renderer::ensureQuadIndices(size_t quads) {
    if (quads <= quadIboCapacity_) return;
    size_t capacity = std::max(quads, quadIboCapacity_ * 2);
    std::vector<uint32_t> idx(capacity * 6);
    for (size_t q = 0; q < capacity; ++q) {
        uint32_t b = static_cast<uint32_t>(q * 4);
        idx[q * 6 + 0] = b;
        idx[q * 6 + 1] = b + 1;
        idx[q * 6 + 2] = b + 2;
        idx[q * 6 + 3] = b;
        idx[q * 6 + 4] = b + 2;
        idx[q * 6 + 5] = b + 3;
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadIbo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(idx.size() * sizeof(uint32_t)), idx.data(), GL_STATIC_DRAW);
    quadIboCapacity_ = capacity;
}

void Renderer::uploadBlockTextures() {
    std::vector<uint8_t> layers = generateBlockTextures(theme_);
    if (blockTextures_ == 0 || !glIsTexture(blockTextures_)) glGenTextures(1, &blockTextures_);
    glBindTexture(GL_TEXTURE_2D_ARRAY, blockTextures_);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, kTexSize, kTexSize, T_COUNT, 0, GL_RGBA, GL_UNSIGNED_BYTE, layers.data());
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 4);
    themeDirty_ = false;
}

void Renderer::ensureCharacter(CharacterAsset& asset) {
    if (asset.contextId == contextId_) return;
    asset.meshes.clear();
    for (const auto& mesh : asset.model.meshes()) {
        CharacterAsset::MeshGpu gpu;
        glGenVertexArrays(1, &gpu.vao);
        glGenBuffers(1, &gpu.vbo);
        glGenBuffers(1, &gpu.ibo);
        glBindVertexArray(gpu.vao);
        glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(float)), mesh.vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(uint16_t)), mesh.indices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, reinterpret_cast<void*>(12));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, reinterpret_cast<void*>(24));
        glBindVertexArray(0);
        gpu.count = static_cast<GLsizei>(mesh.indices.size());
        asset.meshes.push_back(gpu);
    }
    asset.texture = 0;
    if (!asset.image.rgba.empty()) {
        glGenTextures(1, &asset.texture);
        glBindTexture(GL_TEXTURE_2D, asset.texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, asset.image.width, asset.image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, asset.image.rgba.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        // Pixel art: de perto cada pixel da pele fica nítido (estilo Minecraft).
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, asset.pixelArt ? GL_NEAREST : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, asset.pixelArt ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, asset.pixelArt ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    }
    asset.contextId = contextId_;
}

void Renderer::releaseCharacter(CharacterAsset& asset) {
    if (asset.contextId == contextId_) {
        for (auto& m : asset.meshes) {
            glDeleteVertexArrays(1, &m.vao);
            glDeleteBuffers(1, &m.vbo);
            glDeleteBuffers(1, &m.ibo);
        }
        if (asset.texture) glDeleteTextures(1, &asset.texture);
    }
    asset.meshes.clear();
    asset.texture = 0;
    asset.contextId = -1;
}

void Renderer::syncChunks(World& world, const FrameScene& scene) {
    ZZ_TRACE_SCOPE("ZZ upload");
    const double t0 = perf::nowMs();
    world.drainGpuGarbage(garbage_);
    for (ChunkGpu& g : garbage_) {
        if (g.contextId != contextId_) continue;
        glDeleteVertexArrays(3, g.vao);
        glDeleteBuffers(3, g.vbo);
        gpuBytes_ -= std::min<size_t>(gpuBytes_, g.bytes);
        ++stats_.gpuDeletes;
    }
    garbage_.clear();
    uploadQueue_.clear();
    for (Chunk* c : world.chunks()) {
        if (!c->meshed) continue;
        ChunkGpu& gpu = c->gpu;
        if (gpu.contextId == contextId_ && gpu.version == c->version) continue;
        if (gpu.contextId >= 0 && gpu.contextId != contextId_) {
            // Contexto perdido: a malha da CPU já foi liberada, então remalha.
            gpu = ChunkGpu{};
            c->dirty = true;
            continue;
        }
        uploadQueue_.push_back(c);
    }
    // Mais perto primeiro: se o orçamento acabar, o buraco fica no anel mais distante (atrás da névoa).
    const Vec3 f = scene.focus;
    auto d2 = [&](const Chunk* ch) {
        const float dx = ch->cx * 16.0f + 8.0f - f.x, dz = ch->cz * 16.0f + 8.0f - f.z;
        return dx * dx + dz * dz;
    };
    if (uploadQueue_.size() > 1) std::sort(uploadQueue_.begin(), uploadQueue_.end(), [&](const Chunk* p, const Chunk* q) { return d2(p) < d2(q); });
    const UploadBudget& ub = scene.upload;
    size_t bytes = 0;
    int n = 0;
    for (Chunk* c : uploadQueue_) {
        const size_t cb = (c->mesh.opaque.size() + c->mesh.cutout.size() + c->mesh.water.size()) * sizeof(VoxelVertex);
        if (!ub.unlimited && n >= 1 && (n >= ub.maxChunks || bytes + cb > ub.maxBytes)) break;  // piso: sempre sobe o mais próximo
        uploadChunk(*c, cb);
        bytes += cb;
        ++n;
    }
    stats_.uploads = n;
    stats_.uploadBytes = bytes;
    stats_.uploadsPending = static_cast<int>(uploadQueue_.size()) - n;
    stats_.gpuBytes = gpuBytes_;
    stats_.uploadMs = static_cast<float>(perf::nowMs() - t0);
}

void Renderer::uploadChunk(Chunk& chunk, size_t bytes) {
    Chunk* c = &chunk;
    ChunkGpu& gpu = c->gpu;
    if (gpu.contextId != contextId_) {
        glGenVertexArrays(3, gpu.vao);
        glGenBuffers(3, gpu.vbo);
        gpu.contextId = contextId_;
    }
    const std::vector<VoxelVertex>* lists[3] = {&c->mesh.opaque, &c->mesh.cutout, &c->mesh.water};
    for (int pass = 0; pass < 3; ++pass) {
        const auto& verts = *lists[pass];
        gpu.quads[pass] = static_cast<int>(verts.size() / 4);
        ensureQuadIndices(static_cast<size_t>(gpu.quads[pass]));
        glBindVertexArray(gpu.vao[pass]);
        glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo[pass]);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(VoxelVertex)), verts.empty() ? nullptr : verts.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(VoxelVertex), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(VoxelVertex), reinterpret_cast<void*>(6));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(VoxelVertex), reinterpret_cast<void*>(8));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadIbo_);
        glBindVertexArray(0);
    }
    gpu.version = c->version;
    gpuBytes_ = gpuBytes_ - std::min<size_t>(gpuBytes_, gpu.bytes) + bytes;
    gpu.bytes = static_cast<uint32_t>(bytes);
    // A malha já está na GPU: libera a cópia da CPU (mundo grande = muitos chunks).
    c->mesh = ChunkMesh();
}

void Renderer::setLightUniforms(const FrameScene& scene, const ShaderProgram& p) {
    int count = static_cast<int>(std::min<size_t>(8, scene.lights.size()));
    float packed[32] = {0};
    float colors[24] = {0};
    for (int i = 0; i < count; ++i) {
        const PointLight& l = scene.lights[static_cast<size_t>(i)];
        packed[i * 4] = l.position.x;
        packed[i * 4 + 1] = l.position.y;
        packed[i * 4 + 2] = l.position.z;
        packed[i * 4 + 3] = l.radius;
        colors[i * 3] = l.color.x;
        colors[i * 3 + 1] = l.color.y;
        colors[i * 3 + 2] = l.color.z;
    }
    glUniform1i(p.uniform("u_lightCount"), count);
    glUniform4fv(p.uniform("u_lights"), 8, packed);
    glUniform3fv(p.uniform("u_lightColors"), 8, colors);
    glUniform4f(p.uniform("u_muzzle"), scene.muzzleFlash.x, scene.muzzleFlash.y, scene.muzzleFlash.z, scene.muzzleFlash.w);
}

void Renderer::setVoxelUniforms(const FrameScene& scene, const ShaderProgram& p, const Mat4& viewProj) {
    const SkyPreset& sky = scene.sky;
    glUseProgram(p.id);
    glUniformMatrix4fv(p.uniform("u_viewProj"), 1, GL_FALSE, viewProj.m);
    glUniform1f(p.uniform("u_time"), scene.time);
    const float exposure = kExposure;
    Vec3 day = (sky.ambient + sky.sunColor * sky.sunIntensity) * exposure;
    glUniform3f(p.uniform("u_dayLight"), day.x, day.y, day.z);
    glUniform3f(p.uniform("u_fogColor"), sky.fog.x, sky.fog.y, sky.fog.z);
    glUniform2f(p.uniform("u_fogRange"), sky.fogStart, sky.fogEnd);
    glUniform3f(p.uniform("u_camPos"), scene.cameraPos.x, scene.cameraPos.y, scene.cameraPos.z);
    setLightUniforms(scene, p);
    glUniform1i(p.uniform("u_tex"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, blockTextures_);
}

void Renderer::drawChunks(const FrameScene& scene, int pass, const Vec4* planes, float maxDist, const Vec3& center, int* drawCounter) {
    ZZ_TRACE_SCOPE("ZZ drawChunks");
    struct Item { const Chunk* chunk; float dist; };
    static std::vector<Item> items;
    items.clear();
    for (const Chunk* c : scene.world->chunks()) {
        const ChunkGpu& gpu = c->gpu;
        if (gpu.contextId != contextId_ || gpu.quads[pass] == 0) continue;
        const AABB& b = c->bounds;
        Vec3 mid = (b.min + b.max) * 0.5f;
        float dist = distXZ(mid, center);
        if (dist > maxDist + 12) continue;
        bool visible = true;
        for (int i = 0; i < 6; ++i) {
            const Vec4& pl = planes[i];
            Vec3 p(pl.x >= 0 ? b.max.x : b.min.x, pl.y >= 0 ? b.max.y : b.min.y, pl.z >= 0 ? b.max.z : b.min.z);
            if (pl.x * p.x + pl.y * p.y + pl.z * p.z + pl.w < 0) { visible = false; break; }
        }
        if (visible) items.push_back({c, dist});
    }
    if (pass == 2) std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.dist > b.dist; });
    else std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.dist < b.dist; });
    GLint program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    const GLint origin = glGetUniformLocation(program, "u_chunkOrigin");
    for (const Item& it : items) {
        const ChunkGpu& gpu = it.chunk->gpu;
        glUniform2f(origin, it.chunk->cx * 16.0f, it.chunk->cz * 16.0f);
        glBindVertexArray(gpu.vao[pass]);
        glDrawElements(GL_TRIANGLES, gpu.quads[pass] * 6, GL_UNSIGNED_INT, nullptr);
    }
    if (drawCounter) *drawCounter += static_cast<int>(items.size());
    glBindVertexArray(0);
}

void Renderer::drawSky(const FrameScene& scene, const Mat4& viewProj) {
    const SkyPreset& sky = scene.sky;
    Mat4 inv = viewProj.inverse();
    glUseProgram(sky_.id);
    glUniformMatrix4fv(sky_.uniform("u_invViewProj"), 1, GL_FALSE, inv.m);
    glUniform3f(sky_.uniform("u_camPos"), scene.cameraPos.x, scene.cameraPos.y, scene.cameraPos.z);
    glUniform3f(sky_.uniform("u_zenith"), sky.zenith.x, sky.zenith.y, sky.zenith.z);
    glUniform3f(sky_.uniform("u_horizon"), sky.horizon.x, sky.horizon.y, sky.horizon.z);
    glUniform3f(sky_.uniform("u_fog"), sky.fog.x, sky.fog.y, sky.fog.z);
    glUniform3f(sky_.uniform("u_sunDir"), sky.sunDirection.x, sky.sunDirection.y, sky.sunDirection.z);
    glUniform3f(sky_.uniform("u_sunColor"), sky.sunColor.x, sky.sunColor.y, sky.sunColor.z);
    glUniform1f(sky_.uniform("u_night"), sky.night);
    glUniform1f(sky_.uniform("u_time"), scene.time);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glBindVertexArray(skyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::drawClouds(const FrameScene& scene, const Mat4& viewProj) {
    glUseProgram(cloud_.id);
    glUniformMatrix4fv(cloud_.uniform("u_viewProj"), 1, GL_FALSE, viewProj.m);
    float height = std::max(70.0f, scene.cameraPos.y + 34.0f);
    glUniform3f(cloud_.uniform("u_center"), scene.cameraPos.x, height, scene.cameraPos.z);
    glUniform1f(cloud_.uniform("u_size"), 320.0f);
    glUniform1f(cloud_.uniform("u_time"), scene.time);
    glUniform1f(cloud_.uniform("u_fade"), 300.0f);
    Vec3 c = lerp(Vec3(1, 1, 1), scene.sky.horizon, 0.25f) * (scene.sky.night > 0.5f ? 0.35f : 1.0f);
    glUniform3f(cloud_.uniform("u_cloudColor"), c.x, c.y, c.z);
    glUniform3f(cloud_.uniform("u_camPos"), scene.cameraPos.x, scene.cameraPos.y, scene.cameraPos.z);
    glUniform1i(cloud_.uniform("u_tex"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, cloudTexture_);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glBindVertexArray(cloudVao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

void Renderer::setModelUniforms(const FrameScene& scene, const ShaderProgram& p, const Mat4& viewProj) {
    const SkyPreset& sky = scene.sky;
    glUseProgram(p.id);
    glUniformMatrix4fv(p.uniform("u_viewProj"), 1, GL_FALSE, viewProj.m);
    Vec3 sd = normalize(sky.sunDirection);
    glUniform3f(p.uniform("u_sunDir"), sd.x, sd.y, sd.z);
    const float exposure = kExposure;
    Vec3 sun = sky.sunColor * (sky.sunIntensity * exposure);
    glUniform3f(p.uniform("u_sunColor"), sun.x, sun.y, sun.z);
    float extra = 0.06f;
    glUniform3f(p.uniform("u_ambient"), sky.ambient.x * exposure + extra, sky.ambient.y * exposure + extra, sky.ambient.z * exposure + extra);
    glUniform3f(p.uniform("u_fogColor"), sky.fog.x, sky.fog.y, sky.fog.z);
    glUniform2f(p.uniform("u_fogRange"), sky.fogStart, sky.fogEnd);
    glUniform3f(p.uniform("u_camPos"), scene.cameraPos.x, scene.cameraPos.y, scene.cameraPos.z);
    setLightUniforms(scene, p);
    glUniform1i(p.uniform("u_tex"), 0);
    glActiveTexture(GL_TEXTURE0);
}

void Renderer::drawCharacters(const FrameScene& scene, const ShaderProgram& p) {
    GLint uModel = p.uniform("u_model");
    GLint uUseTex = p.uniform("u_useTex");
    GLint uColor = p.uniform("u_color");
    GLint uTint = p.uniform("u_tint");
    GLint uFlash = p.uniform("u_flash");
    GLint uFlashColor = p.uniform("u_flashColor");
    GLint uEmissive = p.uniform("u_emissive");
    glUniform1f(uEmissive, 0.0f);
    for (size_t c = 0; c < scene.characterCount && c < scene.characters.size(); ++c) {
        const CharacterDraw& draw = scene.characters[c];
        if (!draw.asset) continue;
        ++stats_.characterDraws;
        ensureCharacter(*draw.asset);
        {
            glBindTexture(GL_TEXTURE_2D, draw.asset->texture);
            glUniform1f(uUseTex, draw.asset->texture ? 1.0f : 0.0f);
            glUniform4f(uColor, 1, 1, 1, draw.alpha);
            glUniform3f(uTint, draw.tint.x, draw.tint.y, draw.tint.z);
            glUniform1f(uFlash, draw.flash);
            glUniform3f(uFlashColor, draw.flashColor.x, draw.flashColor.y, draw.flashColor.z);
            // Personagens e armas voxel (pixel art) usam a luz de estúdio da página de teste.
            glUniform1f(p.uniform("u_studio"), draw.asset->pixelArt ? 1.0f : 0.0f);
        }
        const bool restoreCull = draw.asset->doubleSided && glIsEnabled(GL_CULL_FACE);
        if (draw.asset->doubleSided) glDisable(GL_CULL_FACE);
        const auto& nodes = draw.asset->model.nodes();
        for (size_t i = 0; i < nodes.size() && i < draw.nodes.size(); ++i) {
            int mesh = nodes[i].mesh;
            if (mesh < 0 || mesh >= static_cast<int>(draw.asset->meshes.size())) continue;
            Mat4 m = draw.world * draw.nodes[i];
            glUniformMatrix4fv(uModel, 1, GL_FALSE, m.m);
            const auto& gpu = draw.asset->meshes[static_cast<size_t>(mesh)];
            glBindVertexArray(gpu.vao);
            glDrawElements(GL_TRIANGLES, gpu.count, GL_UNSIGNED_SHORT, nullptr);
            ++stats_.partDraws;
        }
        if (restoreCull) glEnable(GL_CULL_FACE);
    }
    glUniform1f(p.uniform("u_studio"), 0.0f);
    glBindVertexArray(0);
}

void Renderer::drawBoxes(const FrameScene& scene, const ShaderProgram& p) {
    if (scene.boxes.empty()) return;
    GLint uModel = p.uniform("u_model");
    GLint uColor = p.uniform("u_color");
    GLint uEmissive = p.uniform("u_emissive");
    glUniform1f(p.uniform("u_useTex"), 0.0f);
    glUniform3f(p.uniform("u_tint"), 1, 1, 1);
    glUniform1f(p.uniform("u_flash"), 0.0f);
    glUniform1f(p.uniform("u_studio"), 0.0f);
    glBindVertexArray(boxVao_);
    for (const BoxDraw& box : scene.boxes) {
        glUniformMatrix4fv(uModel, 1, GL_FALSE, box.world.m);
        glUniform4f(uColor, box.color.x, box.color.y, box.color.z, box.color.w);
        glUniform1f(uEmissive, box.emissive);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, nullptr);
        ++stats_.boxDraws;
    }
    glBindVertexArray(0);
}

void Renderer::drawBillboards(const FrameScene& scene, const Mat4& viewProj) {
    Vec3 right(scene.view.m[0], scene.view.m[4], scene.view.m[8]);
    Vec3 up(scene.view.m[1], scene.view.m[5], scene.view.m[9]);
    glUseProgram(billboard_.id);
    glUniformMatrix4fv(billboard_.uniform("u_viewProj"), 1, GL_FALSE, viewProj.m);
    glUniform1f(billboard_.uniform("u_time"), scene.time);
    glUniform3f(billboard_.uniform("u_fogColor"), scene.sky.fog.x, scene.sky.fog.y, scene.sky.fog.z);
    glUniform2f(billboard_.uniform("u_fogRange"), scene.sky.fogStart, scene.sky.fogEnd);
    glUniform3f(billboard_.uniform("u_camPos"), scene.cameraPos.x, scene.cameraPos.y, scene.cameraPos.z);
    glUniform1i(billboard_.uniform("u_tex"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, softTexture_);
    GLint uMode = billboard_.uniform("u_mode");
    glBindVertexArray(billboardVao_);
    glBindBuffer(GL_ARRAY_BUFFER, billboardVbo_);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    auto flush = [&](int mode) {
        if (billboardData_.empty()) return;
        size_t bytes = billboardData_.size() * sizeof(float);
        if (bytes > billboardCapacity_) {
            billboardCapacity_ = std::max(bytes, billboardCapacity_ * 2);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(billboardCapacity_), nullptr, GL_DYNAMIC_DRAW);
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(bytes), billboardData_.data());
        glUniform1i(uMode, mode);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(billboardData_.size() / kBillboardFloats));
        ++stats_.billboardFlushes;
        billboardData_.clear();
    };
    auto groundQuad = [&](const Vec3& pos, float r, const Vec4& color) {
        Vec3 p = pos + Vec3(0, 0.04f, 0);
        pushQuad(billboardData_, p + Vec3(-r, 0, r), p + Vec3(r, 0, r), p + Vec3(r, 0, -r), p + Vec3(-r, 0, -r), color);
    };

    // 1) sombras projetadas e marcas no chão
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -2.0f);
    for (const ShadowBlob& s : scene.shadows) groundQuad(s.position, s.radius, Vec4(0, 0, 0, s.alpha));
    for (const Decal& d : scene.decals)
        if (d.mode == 0) groundQuad(d.position, d.radius, d.color);
    flush(0);
    for (const Decal& d : scene.decals)
        if (d.mode == 1) groundQuad(d.position, d.radius, d.color);
    flush(3);
    glDisable(GL_POLYGON_OFFSET_FILL);

    // 2) portal aberto
    if (scene.portalVisible && scene.portalOpen > 0.01f) {
        Vec3 n = forwardFromYaw(scene.portalYaw);
        Vec3 axis(-n.z, 0, n.x);
        Vec3 b = scene.portalCenter;
        float hw = 1.5f;
        Vec3 a0 = b - axis * hw, a1 = b + axis * hw;
        pushQuad(billboardData_, a0, a1, a1 + Vec3(0, 4, 0), a0 + Vec3(0, 4, 0), Vec4(1, 1, 1, scene.portalOpen));
        flush(2);
    }

    // 3) partículas com transparência comum
    for (const Particle& p : scene.particles) {
        if (p.additive) continue;
        Vec3 r = right * (p.size * 0.5f), u = up * (p.size * 0.5f);
        pushQuad(billboardData_, p.position - r - u, p.position + r - u, p.position + r + u, p.position - r + u, p.color);
    }
    flush(0);

    // 4) aditivos: brilhos, fogo, faíscas e traçantes
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (const Particle& p : scene.particles) {
        if (!p.additive) continue;
        Vec3 r = right * (p.size * 0.5f), u = up * (p.size * 0.5f);
        pushQuad(billboardData_, p.position - r - u, p.position + r - u, p.position + r + u, p.position - r + u, p.color);
    }
    flush(0);
    for (const Beam& beam : scene.beams) {
        Vec3 dir = beam.to - beam.from;
        Vec3 side = normalize(cross(dir, scene.cameraPos - beam.from)) * (beam.width * 0.5f);
        pushQuad(billboardData_, beam.from - side, beam.to - side, beam.to + side, beam.from + side, beam.color);
    }
    flush(1);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glBindVertexArray(0);
}

void Renderer::render(FrameScene& scene) {
    stats_ = RenderStats();
    stats_.gpuBytes = gpuBytes_;
    if (!voxel_.id) return;
    ZZ_TRACE_SCOPE("ZZ render");
    if (themeDirty_) uploadBlockTextures();
    if (scene.world) syncChunks(*scene.world, scene);
    Mat4 viewProj = scene.proj * scene.view;
    extractPlanes(viewProj, frustum_);


    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);
    glClearColor(scene.sky.fog.x, scene.sky.fog.y, scene.sky.fog.z, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    drawSky(scene, viewProj);
    const ShaderProgram& vp = voxel_;
    if (scene.world) {
        setVoxelUniforms(scene, vp, viewProj);
        glUniform1f(vp.uniform("u_alphaTest"), -1.0f);
        glUniform1f(vp.uniform("u_alpha"), 1.0f);
        drawChunks(scene, 0, frustum_, scene.drawDistance, scene.cameraPos, &stats_.chunkDraws[0]);
        glDisable(GL_CULL_FACE);
        glUniform1f(vp.uniform("u_alphaTest"), 0.5f);
        drawChunks(scene, 1, frustum_, scene.drawDistance, scene.cameraPos, &stats_.chunkDraws[1]);
        glEnable(GL_CULL_FACE);
    }
    const ShaderProgram& mp = model_;
    setModelUniforms(scene, mp, viewProj);
    drawCharacters(scene, mp);
    drawBoxes(scene, mp);
    if (scene.world) {
        setVoxelUniforms(scene, vp, viewProj);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glUniform1f(vp.uniform("u_alphaTest"), -1.0f);
        glUniform1f(vp.uniform("u_alpha"), 0.74f);
        drawChunks(scene, 2, frustum_, scene.drawDistance, scene.cameraPos, &stats_.chunkDraws[2]);
        glEnable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    drawClouds(scene, viewProj);
    drawBillboards(scene, viewProj);
}

}  // namespace zz
