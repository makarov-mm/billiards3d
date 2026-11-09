#pragma once

#include "Gl.h"
#include "MathTypes.h"
#include "Physics.h"
#include "TableModel.h"
#include "Themes.h"

#include <array>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace render {

struct BallDraw {
    math::Mat4 model = math::Mat4::Identity(); // with the radius as its scale
    int number = 0;
    bool russian = false;
    float opacity = 1;   // below 1, seen through: where the struck ball will meet another
    float highlight = 0; // a glow at the rim
    float ground = 0;    // the height of the cloth or the pocket under it
};

// A rectangle of the screen behind which the scene is blurred, in pixels from the top left.
struct FrostedPanel {
    float x = 0, y = 0, width = 0, height = 0, radius = 0, opacity = 1;
};

struct Scene {
    math::Mat4 view = math::Mat4::Identity();
    math::Mat4 projection = math::Mat4::Identity();
    math::Vec3 eye;
    float time = 0;
    std::vector<BallDraw> balls;
    std::optional<math::Mat4> cue;
    std::vector<math::Vec4> guides; // segments on the cloth: from xz, to xz
    float guideStrength = 0;
    bool showKitchen = false;
    math::Vec4 pocketMark; // a ring round a pocket: xz, radius, strength
    std::vector<FrostedPanel> panels;
};

// A linked shader program, its sources taken from the executable's resources.
class Program {
public:
    struct Stage {
        gl::GLenum type;
        std::initializer_list<const wchar_t*> sources;
    };

    Program() = default;
    Program(std::string name, std::initializer_list<Stage> stages);
    // The GLSL version the shaders are compiled for, from the context's OpenGL version (46 for 4.6).
    static void SetVersion(int glVersion);

    gl::GLuint Id() const { return m_program.Id(); }
    void Use() const { gl::glUseProgram(m_program.Id()); }
    gl::GLint Location(const char* name) const;

    void Set(const char* name, int value) const;
    void Set(const char* name, float value) const;
    void Set(const char* name, math::Vec2 value) const;
    void Set(const char* name, math::Vec3 value) const;
    void Set(const char* name, math::Vec4 value) const;
    void Set(const char* name, const math::Mat4& value) const;
    void SetArray(const char* name, std::span<const math::Vec4> values) const;

private:
    std::string m_name;
    gl::ProgramObject m_program;
    mutable std::unordered_map<std::string, gl::GLint> m_locations;
};

// Draws the scene into the window: shadows, the lit scene in high dynamic range with multisampling, then bloom
// and tone mapping. The interface is drawn on top afterwards.
class Renderer {
public:
    explicit Renderer(int glVersion);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void SetTheme(const Theme& theme);
    // The table to draw; to be set before the first frame.
    void SetTable(const billiards::TableSpec& table);
    void Resize(int width, int height);
    void Render(const Scene& scene);

private:
    struct GpuMesh {
        gl::VertexArray vertexArray;
        gl::Buffer vertices;
        gl::Buffer indices;
        gl::GLsizei count = 0;
        void Draw(int instances = 1) const;
    };

    static GpuMesh Upload(const model::Mesh& mesh);
    void CreateEnvironmentTextures();
    void BakeEnvironment();
    void UpdateLightView();
    void CreateScreenTargets();
    void UpdateFrameData(const Scene& scene);
    void UploadBalls(const Scene& scene);
    void SetPass(math::Vec3 eye, bool shadows);
    void BindLighting() const;
    void DrawShadowMap(const Scene& scene);
    void DrawMainPass(const Scene& scene);
    void DrawPockets(const Program& program) const;
    void SetTableMaterial(const Scene& scene) const;
    void PostProcess(const Scene& scene);

    int m_width = 0, m_height = 0;
    int m_samples = 4;
    Theme m_theme;
    billiards::TableSpec m_table;
    math::Mat4 m_lightViewProjection;
    float m_shadowRadius = 3, m_shadowRange = 10;
    float m_floorHeight = -0.78f;

    GpuMesh m_ball, m_simpleBall, m_bed, m_cushions, m_rails, m_frame, m_cornerPocket, m_middlePocket, m_floor, m_cue;
    gl::VertexArray m_emptyVertexArray;

    Program m_ballProgram, m_tableProgram, m_shadowProgram, m_backgroundProgram;
    Program m_resolveProgram, m_bloomDownProgram, m_bloomUpProgram, m_compositeProgram;
    Program m_environmentProgram, m_irradianceProgram, m_prefilterProgram, m_brdfProgram;

    gl::Buffer m_frameData, m_passData;
    gl::Buffer m_ballBuffer;
    int m_opaqueBalls = 0, m_ballCount = 0;

    gl::Texture m_shadowMap;
    gl::Framebuffer m_shadowFramebuffer;
    gl::Sampler m_shadowCompareSampler, m_shadowDepthSampler;
    gl::Texture m_environment, m_irradiance, m_prefiltered, m_brdf, m_numbers;
    int m_prefilteredLevels = 6;

    gl::Texture m_sceneColor, m_sceneDepth, m_resolved;
    gl::Framebuffer m_sceneFramebuffer, m_resolvedFramebuffer;
    static constexpr int BloomLevels = 6;
    std::array<gl::Texture, BloomLevels> m_bloom;
    std::array<gl::Framebuffer, BloomLevels> m_bloomFramebuffers;
    std::array<std::array<int, 2>, BloomLevels> m_bloomSizes{};
};

} // namespace render
