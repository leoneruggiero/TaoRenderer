#ifndef SCENE_H
#define SCENE_H

#include "Mesh.h"
#include "GeometryHelper.h"
#include <vector>

class SceneMeshCollection /* TODO : public std::vector<std::shared_ptr<MeshRenderer>>::iterator*/
{
private:
	std::vector<std::shared_ptr<Renderer>> _meshRendererCollection;
	Utils::BoundingBox<glm::vec3> _sceneAABBox;

public:
	SceneMeshCollection() : _sceneAABBox(std::vector<glm::vec3>()) // TODO: Why bbox dowsn't have a default constructor???
	{

	}

	
	void AddToCollection(std::shared_ptr<Renderer> meshRenderer)
	{
		_meshRendererCollection.push_back(meshRenderer);
		_sceneAABBox.Update(meshRenderer.get()->GetTransformedPoints());
	}

	void UpdateBBox()
	{
		_sceneAABBox = Utils::BoundingBox<glm::vec3>(std::vector<glm::vec3>());
		for (auto& m : _meshRendererCollection)
			_sceneAABBox.Update(m.get()->GetTransformedPoints());
	}

	const std::shared_ptr<Renderer>& at(const size_t Pos) const { return _meshRendererCollection.at(Pos); }

	size_t size() const { return _meshRendererCollection.size(); }

	// TODO return reference instead of vec3?
	float GetSceneBBSize() const { return _sceneAABBox.Size(); } 
	glm::vec3 GetSceneBBExtents() const { return _sceneAABBox.Diagonal(); }
	glm::vec3 GetSceneBBCenter() const { return _sceneAABBox.Center(); }
	std::vector<glm::vec3> GetSceneBBPoints() const { return _sceneAABBox.GetPoints(); }
	std::vector<glm::vec3> GetSceneBBLines() const { return _sceneAABBox.GetLines(); }


};


#endif