#include "Scene/TNode.h"
#include "Scene/MeshComponent.h"
#include "Scene/MaterialComponent.h"
#include "Core/EngineSettings.h"

void Frustum::updateFromCamera(const glm::mat4& vp) {
    // Gribb/Hartmann plane extraction from the combined view-projection matrix.
    // Row-major indexing via operator[] on a column-major glm::mat4 means
    // vp[col][row]; we build each plane from rows of the matrix, so we index
    // (row, col) as vp[col][row].
    auto row = [&vp](int r) {
        return glm::vec4(vp[0][r], vp[1][r], vp[2][r], vp[3][r]);
    };

    glm::vec4 r0 = row(0);
    glm::vec4 r1 = row(1);
    glm::vec4 r2 = row(2);
    glm::vec4 r3 = row(3);

    auto toPlane = [](const glm::vec4& p) {
        Plane plane;
        glm::vec3 n(p.x, p.y, p.z);
        float len = glm::length(n);
        if (len > 1e-8f) {
            plane.normal = n / len;
            plane.distance = -p.w / len;
        }
        return plane;
    };

    leftFace   = toPlane(r3 + r0);
    rightFace  = toPlane(r3 - r0);
    bottomFace = toPlane(r3 + r1);
    topFace    = toPlane(r3 - r1);
    nearFace   = toPlane(r3 + r2);
    farFace    = toPlane(r3 - r2);
}

void TNode::draw(const Frustum& frustum, const glm::mat4& view, const glm::mat4& projection,
                 const std::vector<LightUniformData>& lights, const ShadowRenderData& shadowData,
                 const glm::mat4& parentMatrix) {
    glm::mat4 modelMatrix = parentMatrix * transform.getModelMatrix();

    bool passesCulling = !EngineSettings::IsFrustumCullingEnabled()
        || !boundingBox || boundingBox->isOnFrustum(frustum, modelMatrix);

    if (passesCulling) {
        if (auto* mesh = GetComponent<MeshComponent>()) {
            mesh->Draw(modelMatrix, GetComponent<MaterialComponent>(), view, projection, lights, shadowData);
        }

        for (TNode* child : children) {
            if (child) {
                child->draw(frustum, view, projection, lights, shadowData, modelMatrix);
            }
        }
    }
}

void TNode::drawDepthOnly(OpenGLShader* depthShader, const glm::mat4& parentMatrix) {
    glm::mat4 modelMatrix = parentMatrix * transform.getModelMatrix();

    if (auto* mesh = GetComponent<MeshComponent>()) {
        mesh->DrawDepthOnly(modelMatrix, depthShader);
    }

    for (TNode* child : children) {
        if (child) {
            child->drawDepthOnly(depthShader, modelMatrix);
        }
    }
}
