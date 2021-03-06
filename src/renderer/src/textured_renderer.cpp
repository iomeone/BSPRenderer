#include <appfw/binary_file.h>
#include <appfw/services.h>
#include <renderer/textured_renderer.h>

static_assert(sizeof(TexturedRenderer::Vertex) == sizeof(float) * 10, "Size of Vertex is invalid");

static TexturedRenderer::Shader s_Shader;

//-----------------------------------------------------------------------------

class PointShader : public BaseShader {
public:
    PointShader() 
        : BaseShader("PointShader")
        , m_ViewMat(this, "uViewMatrix")
        , m_ProjMat(this, "uProjMatrix") {

    }

    virtual void create() override {
        createProgram();
        createVertexShader("shaders/points.vert");
        createFragmentShader("shaders/points.frag");
        linkProgram();
    }

    void loadMatrices(const BaseRenderer::FrameVars &vars) {
        m_ProjMat.set(vars.projMat);
        m_ViewMat.set(vars.viewMat);
    }

private:
    ShaderUniform<glm::mat4> m_ViewMat, m_ProjMat;
};

/**
 * Reads a binary file with vertices and displays them as points or lines.
 * Used for debugging so file name is hardcoded.
 */
class PointRenderer {
public:
    GLuint m_nVao = 0, m_nVbo = 0;
    size_t m_Count = 0;

    /**
     * Enable point renderer.
     */
    static constexpr bool ENABLE = false;

    /**
     * Draw lines onstead of points.
     */
    static constexpr bool DRAW_LINES = false;

    static PointShader m_sPointShader;

    PointRenderer() {
        appfw::BinaryReader file("test.dat");
        m_Count = file.getFileSize() / sizeof(glm::vec3);
        std::vector<glm::vec3> points(m_Count);
        file.readArray(appfw::span(points));

        glGenVertexArrays(1, &m_nVao);
        glGenBuffers(1, &m_nVbo);

        glBindVertexArray(m_nVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_nVbo);

        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * m_Count, points.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void draw(const BaseRenderer::FrameVars &vars) {
        m_sPointShader.enable();
        m_sPointShader.loadMatrices(vars);
        glPointSize(4);
        glBindVertexArray(m_nVao);
        glDrawArrays(DRAW_LINES ? GL_LINES : GL_POINTS, 0, (GLsizei)m_Count);
        glBindVertexArray(0);
        glPointSize(1);
        m_sPointShader.disable();
    }
};

static PointRenderer *s_pPoints = nullptr;

//-----------------------------------------------------------------------------
// Shader
//-----------------------------------------------------------------------------
TexturedRenderer::Shader::Shader()
    : BaseShader("TexturedRendererShader")
    , m_ViewMat(this, "uViewMatrix")
    , m_ProjMat(this, "uProjMatrix")
    , m_Color(this, "uColor")
    , m_FullBright(this, "uFullBright")
    , m_Texture(this, "uTexture")
    , m_Lightmap(this, "uLightmap") {}

void TexturedRenderer::Shader::create() {
    createProgram();
    createVertexShader("shaders/textured/world.vert");
    createFragmentShader("shaders/textured/world.frag");
    linkProgram();
}

void TexturedRenderer::Shader::loadMatrices(const BaseRenderer::FrameVars &vars) {
    m_ProjMat.set(vars.projMat);
    m_ViewMat.set(vars.viewMat);
    m_FullBright.set(r_fullbright.getValue());
    m_Texture.set(0);
    m_Lightmap.set(1);
}

void TexturedRenderer::Shader::setColor(const glm::vec3 &c) { m_Color.set(c); }

//-----------------------------------------------------------------------------
// Surface
//-----------------------------------------------------------------------------
TexturedRenderer::Surface::Surface(const LevelSurface &baseSurface) noexcept {
    // Prepare buffer data
    m_nMatIdx = baseSurface.nMatIndex;
    m_iVertexCount = baseSurface.vertices.size();
    m_nLightmapTex = baseSurface.nLightmapTex;

    const bsp::BSPTextureInfo &texInfo = *baseSurface.pTexInfo;
    const Material &material = MaterialManager::get().getMaterial(m_nMatIdx);

    std::vector<Vertex> data;
    data.reserve(m_iVertexCount);

    glm::vec3 normal = baseSurface.pPlane->vNormal;
    if (baseSurface.iFlags & SURF_PLANEBACK) {
        normal = -normal;
    }

    size_t i = 0;
    for (glm::vec3 pos : baseSurface.vertices) {
        Vertex v;

        // Position
        v.position = pos;

        // Normal
        v.normal = normal;

        // Texture
        v.texture.s = glm::dot(pos, texInfo.vS) + texInfo.fSShift;
        v.texture.s /= material.getWide();

        v.texture.t = glm::dot(pos, texInfo.vT) + texInfo.fTShift;
        v.texture.t /= material.getTall();

        // Lightmap texture
        if (m_nLightmapTex != 0) {
            v.lmTexture = baseSurface.lmTexCoords[i];
        }

        data.push_back(v);

        i++;
    }

    auto fnGetRandColor = []() { return (rand() % 256) / 255.f; };
    m_Color = {fnGetRandColor(), fnGetRandColor(), fnGetRandColor()};

    glGenVertexArrays(1, &m_nVao);
    glGenBuffers(1, &m_nVbo);

    glBindVertexArray(m_nVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_nVbo);

    
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * m_iVertexCount, data.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, position)));
    glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, normal)));
    glVertexAttribPointer(2, 2, GL_FLOAT, false, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, texture)));
    glVertexAttribPointer(3, 2, GL_FLOAT, false, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, lmTexture)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

TexturedRenderer::Surface::Surface(Surface &&from) noexcept {
    if (m_nVao != 0) {
        glDeleteVertexArrays(1, &m_nVao);
    }

    if (m_nVbo != 0) {
        glDeleteBuffers(1, &m_nVbo);
    }

    m_nVao = from.m_nVao;
    m_nVbo = from.m_nVbo;
    m_iVertexCount = from.m_iVertexCount;
    m_nMatIdx = from.m_nMatIdx;
    m_nLightmapTex = from.m_nLightmapTex;

    from.m_nVao = 0;
    from.m_nVbo = 0;
    from.m_iVertexCount = 0;
    from.m_nMatIdx = NULL_MATERIAL;
    from.m_nLightmapTex = 0;
}

TexturedRenderer::Surface::~Surface() {
    if (m_nVao != 0) {
        glDeleteVertexArrays(1, &m_nVao);
    }

    if (m_nVbo != 0) {
        glDeleteBuffers(1, &m_nVbo);
    }
}

void TexturedRenderer::Surface::draw() noexcept {
    const Material &mat = MaterialManager::get().getMaterial(m_nMatIdx);
    mat.bindTextures();

    // Bind lightmap texture
    if (m_nLightmapTex != 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_nLightmapTex);
    }

    glBindVertexArray(m_nVao);

    s_Shader.setColor(m_Color);
    glDrawArrays(GL_TRIANGLE_FAN, 0, (GLsizei)m_iVertexCount);

    glBindVertexArray(0);
}

//-----------------------------------------------------------------------------
// TexturedRenderer
//-----------------------------------------------------------------------------
void TexturedRenderer::createSurfaces() {
    AFW_ASSERT(m_Surfaces.empty());
    m_Surfaces.reserve(getLevelVars().baseSurfaces.size());

    for (size_t i = 0; i < getLevelVars().baseSurfaces.size(); i++) {
        const LevelSurface &baseSurface = getLevelVars().baseSurfaces[i];
        m_Surfaces.emplace_back(baseSurface);
    }

    if constexpr (PointRenderer::ENABLE) {
        s_pPoints = new PointRenderer();
    }
}

void TexturedRenderer::destroySurfaces() {
    m_Surfaces.clear();
    m_Surfaces.shrink_to_fit();
}

void TexturedRenderer::drawWorldSurfaces(const std::vector<size_t> &surfaceIdxs) noexcept {
    glEnable(GL_DEPTH_TEST);

    if (r_cull.getValue() != 0) {
        glEnable(GL_CULL_FACE);
        glFrontFace(GL_CW); 
        glCullFace(r_cull.getValue() == 1 ? GL_BACK : GL_FRONT);
    }

    s_Shader.enable();
    s_Shader.loadMatrices(getFrameVars());

    for (size_t idx : surfaceIdxs) {
        Surface &surf = m_Surfaces[idx];
        surf.draw();
        m_DrawStats.worldSurfaces++;
    }

    s_Shader.disable();
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    if constexpr (PointRenderer::ENABLE) {
        s_pPoints->draw(getFrameVars());
    }
}
