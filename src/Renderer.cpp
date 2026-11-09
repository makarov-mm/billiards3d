#include "Renderer.h"

#include "Resources.h"
#include "Text.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <utility>

namespace render {

using namespace gl;
using namespace math;

namespace {

int g_glslVersion = 460;

constexpr int ShadowSize = 2048;
constexpr int EnvironmentSize = 512;
constexpr int IrradianceSize = 32;
constexpr int PrefilteredSize = 256;
constexpr int BrdfSize = 128;
constexpr int NumberCell = 128;

// std140 layouts of the uniform blocks in common.glsl.
struct FrameBlock {
    Mat4 viewProjection;
    Mat4 inverseViewProjection;
    Mat4 lightViewProjection;
    Vec4 cameraPosition;
    Vec4 lightDirection;
    Vec4 lightColor;
    Vec4 shadowParams;
    Vec4 viewport;
    Vec4 environment;
    Vec4 accent;
    Vec4 fog;
};

struct PassBlock {
    Vec4 eye;
};

// A ball in the std430 buffer of ball.vert.
struct BallInstance {
    Mat4 model;
    Vec4 params; // number, Russian set, opacity, highlight
    Vec4 extra;  // height of the ground
};
static_assert(sizeof(BallInstance) == 96);

Vec4 V4(Vec3 v, float w) { return {v.x, v.y, v.z, w}; }

int LevelsFor(int size)
{
    int levels = 1;
    while ((size >> levels) > 0)
        ++levels;
    return levels;
}

void Dispatch(int width, int height, int layers)
{
    glDispatchCompute(static_cast<GLuint>((width + 7) / 8), static_cast<GLuint>((height + 7) / 8), static_cast<GLuint>(layers));
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void SetFilter(GLuint texture, GLenum minFilter, GLenum magFilter, GLenum wrap)
{
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(minFilter));
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(magFilter));
    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, static_cast<GLint>(wrap));
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, static_cast<GLint>(wrap));
    glTextureParameteri(texture, GL_TEXTURE_WRAP_R, static_cast<GLint>(wrap));
}

void CheckFramebuffer(const Framebuffer& framebuffer, const char* name)
{
    if (glCheckNamedFramebufferStatus(framebuffer.Id(), GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error(std::format("The {} framebuffer is not complete.", name));
}

} // namespace

// Program ------------------------------------------------------------------------------------------

void Program::SetVersion(int glVersion)
{
    g_glslVersion = glVersion >= 46 ? 460 : 450;
}

Program::Program(std::string name, std::initializer_list<Stage> stages) : m_name(std::move(name))
{
    const GLuint program = glCreateProgram();
    m_program = ProgramObject(program);
    std::vector<GLuint> shaders;
    for (const Stage& stage : stages) {
        std::string source = std::format("#version {} core\n", g_glslVersion);
        int index = 0;
        for (const wchar_t* resource : stage.sources) {
            source += std::format("#line 1 {}\n", index++);
            source += resources::Text(resource);
            source += '\n';
        }
        const GLuint shader = glCreateShader(stage.type);
        const char* text = source.c_str();
        glShaderSource(shader, 1, &text, nullptr);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            std::string log(4096, '\0');
            GLsizei length = 0;
            glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), &length, log.data());
            log.resize(static_cast<std::size_t>(length));
            glDeleteShader(shader);
            for (GLuint attached : shaders)
                glDeleteShader(attached);
            throw std::runtime_error(std::format("The {} shader does not compile:\n{}", m_name, log));
        }
        glAttachShader(program, shader);
        shaders.push_back(shader);
    }
    glLinkProgram(program);
    for (GLuint shader : shaders)
        glDeleteShader(shader);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        std::string log(4096, '\0');
        GLsizei length = 0;
        glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), &length, log.data());
        log.resize(static_cast<std::size_t>(length));
        throw std::runtime_error(std::format("The {} shaders do not link:\n{}", m_name, log));
    }
}

GLint Program::Location(const char* name) const
{
    const auto found = m_locations.find(name);
    if (found != m_locations.end())
        return found->second;
    const GLint location = glGetUniformLocation(m_program.Id(), name);
    m_locations.emplace(name, location);
    return location;
}

void Program::Set(const char* name, int value) const { glProgramUniform1i(Id(), Location(name), value); }
void Program::Set(const char* name, float value) const { glProgramUniform1f(Id(), Location(name), value); }
void Program::Set(const char* name, Vec2 value) const { glProgramUniform2f(Id(), Location(name), value.x, value.y); }
void Program::Set(const char* name, Vec3 value) const { glProgramUniform3f(Id(), Location(name), value.x, value.y, value.z); }
void Program::Set(const char* name, Vec4 value) const { glProgramUniform4f(Id(), Location(name), value.x, value.y, value.z, value.w); }
void Program::Set(const char* name, const Mat4& value) const { glProgramUniformMatrix4fv(Id(), Location(name), 1, GL_FALSE, value.Data()); }

void Program::SetArray(const char* name, std::span<const Vec4> values) const
{
    if (!values.empty())
        glProgramUniform4fv(Id(), Location(name), static_cast<GLsizei>(values.size()), &values.front().x);
}

// Meshes -------------------------------------------------------------------------------------------

void Renderer::GpuMesh::Draw(int instances) const
{
    if (count == 0 || instances <= 0)
        return;
    glBindVertexArray(vertexArray.Id());
    if (instances == 1)
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
    else
        glDrawElementsInstanced(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr, instances);
}

Renderer::GpuMesh Renderer::Upload(const model::Mesh& mesh)
{
    GpuMesh gpu;
    if (mesh.indices.empty())
        return gpu;
    gpu.vertices = CreateBuffer(mesh.vertices.data(), mesh.vertices.size() * sizeof(model::Vertex));
    gpu.indices = CreateBuffer(mesh.indices.data(), mesh.indices.size() * sizeof(std::uint32_t));
    gpu.count = static_cast<GLsizei>(mesh.indices.size());
    GLuint array = 0;
    glCreateVertexArrays(1, &array);
    gpu.vertexArray = VertexArray(array);
    glVertexArrayVertexBuffer(array, 0, gpu.vertices.Id(), 0, sizeof(model::Vertex));
    glVertexArrayElementBuffer(array, gpu.indices.Id());
    const GLuint offsets[3] = {offsetof(model::Vertex, position), offsetof(model::Vertex, normal), offsetof(model::Vertex, uv)};
    const GLint sizes[3] = {3, 3, 2};
    for (GLuint attribute = 0; attribute < 3; ++attribute) {
        glEnableVertexArrayAttrib(array, attribute);
        glVertexArrayAttribFormat(array, attribute, sizes[attribute], GL_FLOAT, GL_FALSE, offsets[attribute]);
        glVertexArrayAttribBinding(array, attribute, 0);
    }
    return gpu;
}

// Setup --------------------------------------------------------------------------------------------

Renderer::Renderer(int glVersion)
{
    Program::SetVersion(glVersion);

    using Stage = Program::Stage;
    m_ballProgram = Program("ball", {Stage{GL_VERTEX_SHADER, {L"COMMON_GLSL", L"BALL_VERT"}},
                                     Stage{GL_FRAGMENT_SHADER, {L"COMMON_GLSL", L"LIGHTING_GLSL", L"BALL_FRAG"}}});
    m_tableProgram = Program("table", {Stage{GL_VERTEX_SHADER, {L"COMMON_GLSL", L"TABLE_VERT"}},
                                       Stage{GL_FRAGMENT_SHADER, {L"COMMON_GLSL", L"LIGHTING_GLSL", L"TABLE_FRAG"}}});
    m_shadowProgram = Program("shadow", {Stage{GL_VERTEX_SHADER, {L"COMMON_GLSL", L"SHADOW_VERT"}}, Stage{GL_FRAGMENT_SHADER, {L"COMMON_GLSL", L"SHADOW_FRAG"}}});
    m_backgroundProgram = Program("background", {Stage{GL_VERTEX_SHADER, {L"FULLSCREEN_VERT"}}, Stage{GL_FRAGMENT_SHADER, {L"COMMON_GLSL", L"BACKGROUND_FRAG"}}});
    m_resolveProgram = Program("resolve", {Stage{GL_VERTEX_SHADER, {L"FULLSCREEN_VERT"}}, Stage{GL_FRAGMENT_SHADER, {L"COMMON_GLSL", L"RESOLVE_FRAG"}}});
    m_bloomDownProgram = Program("bloom", {Stage{GL_VERTEX_SHADER, {L"FULLSCREEN_VERT"}}, Stage{GL_FRAGMENT_SHADER, {L"COMMON_GLSL", L"BLOOM_DOWN_FRAG"}}});
    m_bloomUpProgram = Program("bloom", {Stage{GL_VERTEX_SHADER, {L"FULLSCREEN_VERT"}}, Stage{GL_FRAGMENT_SHADER, {L"COMMON_GLSL", L"BLOOM_UP_FRAG"}}});
    m_compositeProgram = Program("composite", {Stage{GL_VERTEX_SHADER, {L"FULLSCREEN_VERT"}}, Stage{GL_FRAGMENT_SHADER, {L"COMMON_GLSL", L"COMPOSITE_FRAG"}}});
    m_environmentProgram = Program("environment", {Stage{GL_COMPUTE_SHADER, {L"IBL_COMMON_GLSL", L"ENVIRONMENT_COMP"}}});
    m_irradianceProgram = Program("irradiance", {Stage{GL_COMPUTE_SHADER, {L"IBL_COMMON_GLSL", L"IRRADIANCE_COMP"}}});
    m_prefilterProgram = Program("prefilter", {Stage{GL_COMPUTE_SHADER, {L"IBL_COMMON_GLSL", L"PREFILTER_COMP"}}});
    m_brdfProgram = Program("brdf", {Stage{GL_COMPUTE_SHADER, {L"IBL_COMMON_GLSL", L"BRDF_COMP"}}});

    m_ball = Upload(model::BuildBall(32, 64));
    m_simpleBall = Upload(model::BuildBall(10, 20));
    m_cue = Upload(model::BuildCue());
    GLuint emptyArray = 0;
    glCreateVertexArrays(1, &emptyArray);
    m_emptyVertexArray = VertexArray(emptyArray);

    m_frameData = CreateBuffer(nullptr, sizeof(FrameBlock), GL_DYNAMIC_STORAGE_BIT);
    m_passData = CreateBuffer(nullptr, sizeof(PassBlock), GL_DYNAMIC_STORAGE_BIT);
    m_ballBuffer = CreateBuffer(nullptr, sizeof(BallInstance) * billiards::MaxBalls * 2, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_frameData.Id());
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_passData.Id());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_ballBuffer.Id());

    // The shadow map, read both with comparison (for filtering) and without (to find the casters).
    m_shadowMap = CreateTexture(GL_TEXTURE_2D);
    glTextureStorage2D(m_shadowMap.Id(), 1, GL_DEPTH_COMPONENT32F, ShadowSize, ShadowSize);
    m_shadowFramebuffer = CreateFramebuffer();
    glNamedFramebufferTexture(m_shadowFramebuffer.Id(), GL_DEPTH_ATTACHMENT, m_shadowMap.Id(), 0);
    glNamedFramebufferDrawBuffer(m_shadowFramebuffer.Id(), GL_NONE);
    CheckFramebuffer(m_shadowFramebuffer, "shadow");
    GLuint samplers[2] = {};
    glCreateSamplers(2, samplers);
    m_shadowCompareSampler = Sampler(samplers[0]);
    m_shadowDepthSampler = Sampler(samplers[1]);
    for (GLuint sampler : samplers) {
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glSamplerParameteri(samplers[0], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(samplers[0], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(samplers[0], GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glSamplerParameteri(samplers[0], GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glSamplerParameteri(samplers[1], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(samplers[1], GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    CreateEnvironmentTextures();

    // The numbers on the balls.
    const std::vector<std::uint8_t> numbers = text::RenderNumberAtlas(NumberCell);
    m_numbers = CreateTexture(GL_TEXTURE_2D);
    glTextureStorage2D(m_numbers.Id(), LevelsFor(NumberCell * 4), GL_R8, NumberCell * 4, NumberCell * 4);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTextureSubImage2D(m_numbers.Id(), 0, 0, 0, NumberCell * 4, NumberCell * 4, GL_RED, GL_UNSIGNED_BYTE, numbers.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glGenerateTextureMipmap(m_numbers.Id());
    SetFilter(m_numbers.Id(), GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE);
    GLfloat anisotropy = 1;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &anisotropy);
    glTextureParameterf(m_numbers.Id(), GL_TEXTURE_MAX_ANISOTROPY, std::min(anisotropy, 8.0f));

    GLint maxSamples = 4, maxColorSamples = 4, maxDepthSamples = 4;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &maxColorSamples);
    glGetIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES, &maxDepthSamples);
    m_samples = std::max(1, std::min({4, maxSamples, maxColorSamples, maxDepthSamples}));
}

Renderer::~Renderer() = default;

void Renderer::CreateEnvironmentTextures()
{
    auto cube = [](int size, int levels) {
        Texture texture = CreateTexture(GL_TEXTURE_CUBE_MAP);
        glTextureStorage2D(texture.Id(), levels, GL_RGBA16F, size, size);
        SetFilter(texture.Id(), levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE);
        return texture;
    };
    m_environment = cube(EnvironmentSize, LevelsFor(EnvironmentSize));
    m_irradiance = cube(IrradianceSize, 1);
    m_prefiltered = cube(PrefilteredSize, m_prefilteredLevels);

    m_brdf = CreateTexture(GL_TEXTURE_2D);
    glTextureStorage2D(m_brdf.Id(), 1, GL_RG16F, BrdfSize, BrdfSize);
    SetFilter(m_brdf.Id(), GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE);
    glUseProgram(m_brdfProgram.Id());
    glBindImageTexture(0, m_brdf.Id(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
    Dispatch(BrdfSize, BrdfSize, 1);
}

// The room and its lamp, baked into the light probes.
void Renderer::BakeEnvironment()
{
    const Program& environment = m_environmentProgram;
    environment.Set("uCeiling", m_theme.ceiling);
    environment.Set("uHorizon", m_theme.horizon);
    environment.Set("uFloor", m_theme.ground);
    std::array<Vec4, 8> directions{}, shapes{}, colors{};
    for (int i = 0; i < m_theme.panelCount; ++i) {
        const StudioPanel& panel = m_theme.panels[static_cast<std::size_t>(i)];
        directions[static_cast<std::size_t>(i)] = V4(panel.direction, panel.roll);
        shapes[static_cast<std::size_t>(i)] = {panel.halfSize.x, panel.halfSize.y, panel.corner, panel.softness};
        colors[static_cast<std::size_t>(i)] = V4(panel.radiance, panel.falloff);
    }
    environment.SetArray("uPanelDirection", directions);
    environment.SetArray("uPanelShape", shapes);
    environment.SetArray("uPanelColor", colors);
    environment.Set("uPanelCount", m_theme.panelCount);
    environment.Use();
    glBindImageTexture(0, m_environment.Id(), 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    Dispatch(EnvironmentSize, EnvironmentSize, 6);
    glGenerateTextureMipmap(m_environment.Id());

    glBindTextureUnit(0, m_environment.Id());
    m_irradianceProgram.Set("uSourceLod", 4.0f);
    m_irradianceProgram.Use();
    glBindImageTexture(0, m_irradiance.Id(), 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    Dispatch(IrradianceSize, IrradianceSize, 6);

    m_prefilterProgram.Set("uSourceSize", static_cast<float>(EnvironmentSize));
    m_prefilterProgram.Use();
    for (int level = 0; level < m_prefilteredLevels; ++level) {
        const int size = PrefilteredSize >> level;
        m_prefilterProgram.Set("uRoughness", static_cast<float>(level) / static_cast<float>(m_prefilteredLevels - 1));
        glBindImageTexture(0, m_prefiltered.Id(), level, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        Dispatch(size, size, 6);
    }
}

void Renderer::SetTheme(const Theme& theme)
{
    m_theme = theme;
    UpdateLightView();
    BakeEnvironment();
}

void Renderer::SetTable(const billiards::TableSpec& table)
{
    m_table = table;
    const model::TableMeshes meshes = model::BuildTable(table);
    m_bed = Upload(meshes.bed);
    m_cushions = Upload(meshes.cushions);
    m_rails = Upload(meshes.rails);
    m_frame = Upload(meshes.frame);
    m_floorHeight = meshes.floorHeight;
    m_floor = Upload(model::BuildFloor(40, m_floorHeight));
    const float top = static_cast<float>(table.railHeight);
    m_cornerPocket = Upload(model::BuildPocket(static_cast<float>(table.pockets[0].cupRadius), top, static_cast<float>(table.pockets[0].depth)));
    m_middlePocket = Upload(model::BuildPocket(static_cast<float>(table.pockets[1].cupRadius), top, static_cast<float>(table.pockets[1].depth)));
    UpdateLightView();
}

// The key light's view: a box round the table and the floor about it.
void Renderer::UpdateLightView()
{
    m_shadowRadius = static_cast<float>(m_table.length / 2 + m_table.cushionWidth + m_table.railWidth) * 1.25f + 0.4f;
    m_shadowRange = 2 * m_shadowRadius + 6;
    const Vec3 toLight = Normalize(m_theme.keyDirection);
    const Vec3 up = std::abs(toLight.y) > 0.99f ? Vec3{0, 0, 1} : Vec3{0, 1, 0};
    const Vec3 center{0, -0.2f, 0};
    const Mat4 view = Mat4::LookAt(center + toLight * (m_shadowRadius + 3), center, up);
    const Mat4 projection = Mat4::Orthographic(-m_shadowRadius, m_shadowRadius, -m_shadowRadius, m_shadowRadius, 0.5f, 0.5f + m_shadowRange);
    m_lightViewProjection = projection * view;
}

void Renderer::Resize(int width, int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (width == m_width && height == m_height)
        return;
    m_width = width;
    m_height = height;
    CreateScreenTargets();
}

void Renderer::CreateScreenTargets()
{
    m_sceneColor = CreateTexture(GL_TEXTURE_2D_MULTISAMPLE);
    glTextureStorage2DMultisample(m_sceneColor.Id(), m_samples, GL_RGBA16F, m_width, m_height, GL_TRUE);
    m_sceneDepth = CreateTexture(GL_TEXTURE_2D_MULTISAMPLE);
    glTextureStorage2DMultisample(m_sceneDepth.Id(), m_samples, GL_DEPTH_COMPONENT32F, m_width, m_height, GL_TRUE);
    m_sceneFramebuffer = CreateFramebuffer();
    glNamedFramebufferTexture(m_sceneFramebuffer.Id(), GL_COLOR_ATTACHMENT0, m_sceneColor.Id(), 0);
    glNamedFramebufferTexture(m_sceneFramebuffer.Id(), GL_DEPTH_ATTACHMENT, m_sceneDepth.Id(), 0);
    CheckFramebuffer(m_sceneFramebuffer, "scene");

    m_resolved = CreateTexture(GL_TEXTURE_2D);
    glTextureStorage2D(m_resolved.Id(), 1, GL_RGBA16F, m_width, m_height);
    SetFilter(m_resolved.Id(), GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE);
    m_resolvedFramebuffer = CreateFramebuffer();
    glNamedFramebufferTexture(m_resolvedFramebuffer.Id(), GL_COLOR_ATTACHMENT0, m_resolved.Id(), 0);
    CheckFramebuffer(m_resolvedFramebuffer, "resolve");

    for (std::size_t level = 0; level < BloomLevels; ++level) {
        const int width = std::max(1, m_width >> (level + 1)), height = std::max(1, m_height >> (level + 1));
        m_bloomSizes[level] = {width, height};
        m_bloom[level] = CreateTexture(GL_TEXTURE_2D);
        glTextureStorage2D(m_bloom[level].Id(), 1, GL_R11F_G11F_B10F, width, height);
        SetFilter(m_bloom[level].Id(), GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE);
        m_bloomFramebuffers[level] = CreateFramebuffer();
        glNamedFramebufferTexture(m_bloomFramebuffers[level].Id(), GL_COLOR_ATTACHMENT0, m_bloom[level].Id(), 0);
        CheckFramebuffer(m_bloomFramebuffers[level], "bloom");
    }
}

// Frame --------------------------------------------------------------------------------------------

void Renderer::UpdateFrameData(const Scene& scene)
{
    FrameBlock block;
    block.viewProjection = scene.projection * scene.view;
    block.inverseViewProjection = block.viewProjection.Inverse();
    block.lightViewProjection = m_lightViewProjection;
    block.cameraPosition = V4(scene.eye, scene.time);
    block.lightDirection = V4(Normalize(m_theme.keyDirection), 2 * m_shadowRadius / ShadowSize);
    block.lightColor = V4(m_theme.keyRadiance, m_theme.keySoftness);
    // The depth bias, in world units the same whatever the shadow's range; the fog begins well beyond the table.
    const float fogStart = Length(scene.eye) + static_cast<float>(m_table.length) * 0.9f;
    block.shadowParams = {m_shadowRange, 0.004f / m_shadowRange, fogStart, 0};
    block.viewport = {static_cast<float>(m_width), static_cast<float>(m_height), 1.0f / static_cast<float>(m_width), 1.0f / static_cast<float>(m_height)};
    block.environment = {m_theme.diffuseLight, m_theme.reflectedLight, static_cast<float>(m_prefilteredLevels - 1), m_theme.background};
    block.accent = V4(m_theme.accent, 1);
    block.fog = V4(m_theme.fog, 0.08f);
    glNamedBufferSubData(m_frameData.Id(), 0, sizeof(block), &block);
}

// The balls into the instance buffer: the solid ones first, then those seen through.
void Renderer::UploadBalls(const Scene& scene)
{
    std::vector<BallInstance> instances;
    for (const bool seeThrough : {false, true})
        for (const BallDraw& ball : scene.balls)
            if ((ball.opacity < 1) == seeThrough && instances.size() < billiards::MaxBalls * 2)
                instances.push_back({ball.model, {static_cast<float>(ball.number), ball.russian ? 1.0f : 0.0f, ball.opacity, ball.highlight}, {ball.ground, 0, 0, 0}});
    m_ballCount = static_cast<int>(instances.size());
    m_opaqueBalls = static_cast<int>(std::ranges::count_if(scene.balls, [](const BallDraw& ball) { return ball.opacity >= 1; }));
    m_opaqueBalls = std::min(m_opaqueBalls, m_ballCount);
    if (!instances.empty())
        glNamedBufferSubData(m_ballBuffer.Id(), 0, static_cast<GLsizeiptr>(instances.size() * sizeof(BallInstance)), instances.data());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_ballBuffer.Id());
}

void Renderer::SetPass(Vec3 eye, bool shadows)
{
    const PassBlock block{V4(eye, shadows ? 0.0f : 1.0f)};
    glNamedBufferSubData(m_passData.Id(), 0, sizeof(block), &block);
}

void Renderer::BindLighting() const
{
    glBindTextureUnit(0, m_shadowMap.Id());
    glBindSampler(0, m_shadowCompareSampler.Id());
    glBindTextureUnit(1, m_shadowMap.Id());
    glBindSampler(1, m_shadowDepthSampler.Id());
    glBindTextureUnit(2, m_irradiance.Id());
    glBindTextureUnit(3, m_prefiltered.Id());
    glBindTextureUnit(4, m_brdf.Id());
    glBindTextureUnit(6, m_numbers.Id());
}

void Renderer::Render(const Scene& scene)
{
    UpdateFrameData(scene);
    UploadBalls(scene);
    DrawShadowMap(scene);
    DrawMainPass(scene);
    PostProcess(scene);
}

void Renderer::DrawPockets(const Program& program) const
{
    for (const billiards::Pocket& pocket : m_table.pockets) {
        program.Set("uModel", Mat4::Translation({static_cast<float>(pocket.hole.x), 0, static_cast<float>(pocket.hole.z)}));
        (pocket.corner ? m_cornerPocket : m_middlePocket).Draw();
    }
}

void Renderer::DrawShadowMap(const Scene& scene)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFramebuffer.Id());
    glViewport(0, 0, ShadowSize, ShadowSize);
    const float farDepth = 1;
    glClearNamedFramebufferfv(m_shadowFramebuffer.Id(), GL_DEPTH, 0, &farDepth);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.5f, 3.0f);

    const Program& program = m_shadowProgram;
    program.Use();
    program.Set("uInstanced", 0);
    program.Set("uModel", Mat4::Identity());
    m_cushions.Draw();
    m_rails.Draw();
    m_frame.Draw();
    if (scene.cue) {
        program.Set("uModel", *scene.cue);
        m_cue.Draw();
    }
    program.Set("uInstanced", 1);
    program.Set("uFirst", 0);
    m_simpleBall.Draw(m_opaqueBalls);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void Renderer::SetTableMaterial(const Scene& scene) const
{
    const Program& p = m_tableProgram;
    const Theme& t = m_theme;
    p.Set("uCloth", t.cloth);
    p.Set("uClothStyle", Vec4{t.clothRoughness, t.clothNap, t.clothSheen, t.clothWear});
    p.Set("uRailColor", t.rail);
    p.Set("uRailStyle", Vec4{t.railRoughness, t.railClearcoat, t.railPattern, t.railSeed});
    p.Set("uFrameColor", t.frame);
    p.Set("uFrameStyle", Vec4{t.frameRoughness, t.frameClearcoat, t.framePattern, t.frameSeed});
    const bool russian = m_table.kind == billiards::TableKind::Pyramid;
    p.Set("uPocketColor", russian ? Vec3{0.32f, 0.26f, 0.18f} * (t.railPattern > 0.5f ? 0.3f : 1.0f) : t.pocket);
    p.Set("uPocketStyle", Vec4{t.pocketRoughness, russian ? 1.0f : 0.0f, 0, 0});
    p.Set("uFloorColor", t.floor);
    p.Set("uFloorStyle", Vec4{t.floorRoughness, t.floorPattern, 0, 0});
    p.Set("uSightColor", t.sight);
    p.Set("uSightStyle", Vec4{t.sightMetallic, t.sightRoughness, russian ? 1.0f : t.sightShape, t.sightGlow});
    const float length = static_cast<float>(m_table.length), width = static_cast<float>(m_table.width);
    p.Set("uTable", Vec4{length, width, static_cast<float>(m_table.ballRadius), static_cast<float>(m_table.HeadString())});
    p.Set("uMarks", Vec4{scene.showKitchen ? 1.0f : 0.0f, russian ? 1.0f : 0.0f, 0, 0});
    std::array<Vec4, 6> holes{}, cups{};
    for (std::size_t i = 0; i < 6; ++i) {
        const billiards::Pocket& pocket = m_table.pockets[i];
        holes[i] = {static_cast<float>(pocket.hole.x), static_cast<float>(pocket.hole.z), static_cast<float>(pocket.holeRadius), 0};
        cups[i] = {static_cast<float>(pocket.hole.x), static_cast<float>(pocket.hole.z), static_cast<float>(pocket.cupRadius), 0};
    }
    p.SetArray("uHoles", holes);
    p.SetArray("uCups", cups);
    // The sights: three between the pockets along each rail, in the middle of the rail.
    std::vector<Vec4> sights;
    const float middle = static_cast<float>(m_table.cushionWidth + m_table.railWidth * 0.45);
    const float size = russian ? 0.0065f : 0.009f;
    for (const float side : {-1.0f, 1.0f}) {
        for (int k = 1; k < 8; ++k)
            if (k != 4)
                sights.push_back({-length / 2 + length * static_cast<float>(k) / 8, side * (width / 2 + middle), size, 0});
        for (int k = 1; k < 4; ++k)
            sights.push_back({side * (length / 2 + middle), -width / 2 + width * static_cast<float>(k) / 4, size, 0});
    }
    p.SetArray("uSights", sights);
    p.Set("uSightCount", static_cast<int>(sights.size()));
    const std::size_t guides = std::min<std::size_t>(scene.guides.size(), 16);
    if (guides > 0)
        p.SetArray("uGuides", std::span(scene.guides.data(), guides));
    p.Set("uGuideCount", static_cast<int>(guides));
    p.Set("uGuideColor", V4(m_theme.accent, scene.guideStrength));
    p.Set("uPocketMark", scene.pocketMark);
    // The balls lying on the cloth, for the shade under them.
    std::vector<Vec4> balls;
    for (const BallDraw& ball : scene.balls) {
        const float radius = Length(Vec3{ball.model(0, 0), ball.model(1, 0), ball.model(2, 0)});
        if (ball.opacity > 0.5f && ball.ground == 0 && std::abs(ball.model(1, 3) - radius) < 0.25f * radius && balls.size() < 16)
            balls.push_back({ball.model(0, 3), ball.model(2, 3), radius, ball.opacity});
    }
    if (!balls.empty())
        p.SetArray("uBalls", balls);
    p.Set("uBallCount", static_cast<int>(balls.size()));
}

void Renderer::DrawMainPass(const Scene& scene)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFramebuffer.Id());
    glViewport(0, 0, m_width, m_height);
    const float clear[4] = {m_theme.fog.x, m_theme.fog.y, m_theme.fog.z, 1};
    const float farDepth = 1;
    glClearNamedFramebufferfv(m_sceneFramebuffer.Id(), GL_COLOR, 0, clear);
    glClearNamedFramebufferfv(m_sceneFramebuffer.Id(), GL_DEPTH, 0, &farDepth);

    SetPass(scene.eye, true);
    BindLighting();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    const Program& table = m_tableProgram;
    SetTableMaterial(scene);
    table.Use();
    table.Set("uModel", Mat4::Identity());
    table.Set("uKind", 5);
    m_floor.Draw();
    table.Set("uKind", 3);
    m_frame.Draw();
    table.Set("uKind", 1);
    m_cushions.Draw();
    // The cloth, the rails and the nets have holes cut in them, smoothed over the samples of each pixel.
    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDisable(GL_CULL_FACE);
    table.Set("uKind", 4);
    DrawPockets(table);
    glEnable(GL_CULL_FACE);
    table.Set("uModel", Mat4::Identity());
    table.Set("uKind", 2);
    m_rails.Draw();
    table.Set("uKind", 0);
    m_bed.Draw();
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

    m_ballProgram.Use();
    m_ballProgram.Set("uBallSurface", Vec4{m_theme.ballRoughness, m_theme.ballCoatRoughness, 1, 0});
    m_ballProgram.Set("uFirst", 0);
    m_ball.Draw(m_opaqueBalls);

    if (scene.cue) {
        table.Use();
        table.Set("uKind", 6);
        table.Set("uModel", *scene.cue);
        m_cue.Draw();
    }

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    m_backgroundProgram.Use();
    glBindVertexArray(m_emptyVertexArray.Id());
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // The balls seen through, over the rest.
    if (m_ballCount > m_opaqueBalls) {
        glDepthFunc(GL_LESS);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        m_ballProgram.Use();
        m_ballProgram.Set("uFirst", m_opaqueBalls);
        m_ball.Draw(m_ballCount - m_opaqueBalls);
        glDisable(GL_BLEND);
    }

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

void Renderer::PostProcess(const Scene& scene)
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(m_emptyVertexArray.Id());

    glBindFramebuffer(GL_FRAMEBUFFER, m_resolvedFramebuffer.Id());
    glViewport(0, 0, m_width, m_height);
    m_resolveProgram.Set("uSamples", m_samples);
    m_resolveProgram.Use();
    glBindTextureUnit(0, m_sceneColor.Id());
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Bloom: halve down to a small image, then add the levels back up.
    m_bloomDownProgram.Use();
    for (std::size_t level = 0; level < BloomLevels; ++level) {
        const auto [width, height] = m_bloomSizes[level];
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFramebuffers[level].Id());
        glViewport(0, 0, width, height);
        const int sourceWidth = level == 0 ? m_width : m_bloomSizes[level - 1][0];
        const int sourceHeight = level == 0 ? m_height : m_bloomSizes[level - 1][1];
        glBindTextureUnit(0, level == 0 ? m_resolved.Id() : m_bloom[level - 1].Id());
        m_bloomDownProgram.Set("uSourceTexel", Vec2{1.0f / static_cast<float>(sourceWidth), 1.0f / static_cast<float>(sourceHeight)});
        m_bloomDownProgram.Set("uFirst", level == 0 ? 1 : 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    m_bloomUpProgram.Use();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    for (int level = BloomLevels - 2; level >= 0; --level) {
        const auto index = static_cast<std::size_t>(level);
        const auto [width, height] = m_bloomSizes[index];
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFramebuffers[index].Id());
        glViewport(0, 0, width, height);
        glBindTextureUnit(0, m_bloom[index + 1].Id());
        m_bloomUpProgram.Set("uSourceTexel", Vec2{1.0f / static_cast<float>(m_bloomSizes[index + 1][0]), 1.0f / static_cast<float>(m_bloomSizes[index + 1][1])});
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glDisable(GL_BLEND);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_width, m_height);
    const Program& composite = m_compositeProgram;
    composite.Set("uExposure", m_theme.exposure);
    composite.Set("uBloomStrength", m_theme.bloom);
    composite.Set("uVignette", m_theme.vignette);
    std::array<Vec4, 4> panels{}, styles{};
    const std::size_t count = std::min<std::size_t>(scene.panels.size(), 4);
    for (std::size_t i = 0; i < count; ++i) {
        const FrostedPanel& panel = scene.panels[i];
        panels[i] = {panel.x + panel.width / 2, panel.y + panel.height / 2, panel.width / 2, panel.height / 2};
        styles[i] = {panel.radius, panel.opacity, 0, 0};
    }
    composite.SetArray("uPanels", panels);
    composite.SetArray("uPanelStyle", styles);
    composite.Set("uPanelCount", static_cast<int>(count));
    composite.Use();
    glBindTextureUnit(0, m_resolved.Id());
    glBindTextureUnit(1, m_bloom[0].Id());
    glBindTextureUnit(2, m_bloom[2].Id());
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

} // namespace render
