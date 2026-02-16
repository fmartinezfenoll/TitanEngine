#include "Renderer/IRenderer.h"

void IRenderer::SetEntitiesReference(std::vector<TNodo*>* entitiesRef)
{
    entities = entitiesRef;
}
