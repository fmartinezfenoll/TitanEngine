#include "Renderer/IRenderer.h"

void IRenderer::SetEntitiesReference(std::vector<TNode*>* entitiesRef)
{
    entities = entitiesRef;
}
