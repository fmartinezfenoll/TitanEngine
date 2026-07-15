#pragma once

// A single unit quad (two triangles, centered on the origin in the XY plane,
// spanning [-0.5, 0.5]) shared by every camera-facing billboard and particle
// draw -- allocated once, like Skybox's shared cube, rather than one VBO per
// component. Vertex layout: location 0 = vec2 corner position, location 1 =
// vec2 UV. Camera-facing orientation and world placement are done in the
// vertex shader, so this geometry never changes.
class BillboardGeometry
{
public:
    static void Init();
    static void Shutdown();

    // Binds the shared quad VAO. Caller issues its own draw (glDrawArrays or
    // glDrawArraysInstanced against these 6 vertices) and unbinds afterward.
    static void Bind();
    static void Unbind();

    static unsigned int GetVAO() { return quadVAO; }
    static constexpr int kVertexCount = 6;

private:
    static inline unsigned int quadVAO = 0;
    static inline unsigned int quadVBO = 0;
};
