//TerrainHeightMap.cpp
//Simple unoptimized terrain divided into 
//several submeshes
#include "TerrainHeightMap.h"
#include "Mesh.h"
#include "VertexDeclarations.h"
#include "IndexBuffer.h"
#include "VertexBuffer.h"
#include "Texture2D.h"
#include "Matrix4x4.h"
#include "GraphicsDevice.h"

#include <algorithm>

TerrainHeightMap::TerrainHeightMap()
{
	Clear();
}
TerrainHeightMap::~TerrainHeightMap()
{
	Clear();
}
void TerrainHeightMap::Clear()
{
	_terrainWidth = 0;
	_terrainHeight = 0;
	_numberOfPatches = 0;
	_patchWidth = 0;
	_patchHeight = 0;
	_totalTerrainVertices = 0;
	_totalTerrainIndices = 0;
	for(unsigned int i = 0; i < _terrainMeshes.size(); i++)
	{
		delete _terrainMeshes[i];
	}
	_terrainMeshes.clear();
}
void TerrainHeightMap::CreateHeightMap(GraphicsDevice* graphicsDevice,Texture2D* heightMap, int patchSize)
{
	Clear();

	_terrainWidth = heightMap->GetWidth();
	_terrainHeight = heightMap->GetHeight();
	int widthPatches = (_terrainWidth / patchSize);
	int heightPatches = (_terrainHeight / patchSize);
	_numberOfPatches = widthPatches * heightPatches;
	_patchWidth = patchSize + 1;
	_patchHeight = patchSize +1;

	_totalTerrainVertices = (_terrainWidth + widthPatches) * (_terrainHeight + heightPatches);
	_totalTerrainIndices = (_terrainWidth) * (_terrainHeight) * 6;

	int patchVertices = _patchWidth * _patchHeight;
	int patchIndices = (_patchWidth - 1) * (_patchHeight - 1) * 6;

	//Get all the height data
	std::vector<GLubyte> imageData = heightMap->GetImageDataRedColor();
	std::vector<std::vector<float>> heightData;
	heightData.resize(_terrainWidth);
	for(int i = 0; i < _terrainWidth; i++)
	{
		heightData[i].resize(_terrainHeight);
	}
	for(int x = 0; x < _terrainWidth; x++)
	{
		for(int y = 0; y < _terrainHeight; y++)
		{
			heightData[x][y] = ((float)imageData[x + y * _terrainWidth]) / 5.0f;
		}
	}
	//Clear the image data
	imageData.clear();

	//AABB test
	Vector3 patchOrigin;

	for(int divisionX = 0; divisionX < widthPatches; divisionX++)
	{
		for(int divisionY = 0; divisionY < heightPatches; divisionY++)
		{
			int patchX = divisionX * (_patchWidth -1);
			int patchY = divisionY * (_patchHeight -1);
			
			//temp
			float highValue = 0.0f;

			VertexPosNormTexContainer* vertexContainer = new VertexPosNormTexContainer(patchVertices);
			for (int x = 0; x < _patchWidth; x++)
			{
			   for (int y = 0; y < _patchHeight; y++)
			   {
				   int heightX = std::min(patchX + x, (_terrainWidth - 1));
				   int heightY = std::min(patchY + y, (_terrainHeight - 1));

				   VertexPosNormTex& currentVertex = vertexContainer->GetVertex(x + y * _patchWidth);
				   currentVertex._position = Vector3((float)heightX, heightData[heightX][heightY], (float)heightY);
				   currentVertex._normal = CalculateNormal(heightX, heightY, heightData);
				   //currentVertex._texCoord = Vector2(((float)heightX / 30.0f), ((float)heightY / 30.0f));
				   currentVertex._texCoord = Vector2(heightX / 1024.0f, heightY / 1024.0f);

				   //TEMP
				   if(x == 0 && y == 0)
				   {
					   patchOrigin = currentVertex._position;
					   patchOrigin._y = 0;
				   }
				   if(highValue < currentVertex._position._y)
				   {
					   highValue = currentVertex._position._y;
				   }
			   }
			}
	
			GLuint* indices = new GLuint[patchIndices];
			int counter = 0;
			for (int y = 0; y < (_patchHeight - 1); y++)
			{
				for (int x = 0; x < (_patchWidth - 1); x++)
				{
					GLuint lowerLeft = (x + y * _patchWidth);
					GLuint lowerRight = ((x + 1) + y * _patchWidth);
					GLuint topLeft = (x + (y + 1) * _patchWidth);
					GLuint topRight = ((x + 1) + (y + 1) * _patchWidth);

					indices[counter++] = topLeft;
					indices[counter++] = lowerRight;
					indices[counter++] = lowerLeft;

					indices[counter++] = topLeft;
					indices[counter++] = topRight;
					indices[counter++] = lowerRight;
				}
			}
			
			int vertexSize = vertexContainer->GetVertexSize();
			VertexBuffer* vertexBuffer = new VertexBuffer(graphicsDevice, patchVertices, (VertexContainer*)vertexContainer, vertexSize);
			vertexBuffer->AddVertexAttributeInformation(0,3,GraphicsDataType::Float(), false, vertexSize, 0);
			vertexBuffer->AddVertexAttributeInformation(1,3,GraphicsDataType::Float(), false, vertexSize, sizeof(Vector3));
			vertexBuffer->AddVertexAttributeInformation(2,2,GraphicsDataType::Float(), false, vertexSize, sizeof(Vector3) * 2);
			IndexBuffer* indexBuffer = new IndexBuffer(graphicsDevice, patchIndices, indices);

			Mesh* terrainMesh = new Mesh(vertexBuffer, indexBuffer);
			//Test with bounding boxes
			int halfPatchX = (int)(patchX + ((patchSize) / 2.0f));
			int halfPatchY = (int)(patchY + ((patchSize) / 2.0f));
			Vector3 boxSize = Vector3(patchSize * 0.5f, highValue * 0.5f, patchSize * 0.5f);
			Vector3 boxPosition = patchOrigin + boxSize;
			terrainMesh->SetBoundingBox(boxPosition, boxSize);
			_terrainMeshes.push_back(terrainMesh);
		}
	}
	//We're done with the height data, so clear it
	for(int i = 0; i < _terrainWidth; i++)
	{heightData[i].clear();}
	heightData.clear();
}

const std::vector<Mesh*>& TerrainHeightMap::GetTerrainMeshes()
{
	return _terrainMeshes;
}
int TerrainHeightMap::GetNumberOfPatches()const
{
	return _numberOfPatches;
}

//Calculate all the normals for the entire terrain, so that we don't get
//the visible edges
Vector3 TerrainHeightMap::CalculateNormal(int x, int y, std::vector<std::vector<float>>& heightData)
{
	Vector3 normal = Vector3::Zero();
	bool top = y > 0 ? true : false;
	bool down = (y < ( _terrainHeight-1)) ? true : false;;
	bool left = x > 0 ? true : false;
	bool right = (x <( _terrainWidth-1)) ? true : false;

	Vector3 center((float)x,heightData[x][y],(float)y);
	Vector3 point1 = Vector3::Zero();
	Vector3 point2 = Vector3::Zero();

	if(top && left)
	{
		point1.SetValues((float)x - 1, heightData[x - 1][y], (float)y);
		point2.SetValues((float)x - 1, heightData[x - 1][y - 1], (float)y - 1);
		normal += GetNormalValue(point1, point2, center);
	}
	if(top && right)
	{
		point1.SetValues((float)x, heightData[x][y -1], (float)y - 1.0f);
		point2.SetValues((float)x + 1, heightData[x + 1][y - 1], (float)y-1.0f);
		normal += GetNormalValue(point1, point2, center);
	}
	if(down && right)
	{
		point1.SetValues((float)x + 1, heightData[x + 1][y], (float)y);
		point2.SetValues((float)x + 1, heightData[x + 1][y + 1], (float)y + 1);
		normal += GetNormalValue(point1, point2, center);
	}
	if(down && left)
	{
		point1.SetValues((float)x, heightData[x][y+1], (float)y+1);
		point2.SetValues((float)x - 1, heightData[x - 1][y + 1], (float)y + 1);
		normal += GetNormalValue(point1, point2, center);
	}
	normal.Normalize();
	return normal;
}
Vector3 TerrainHeightMap::GetNormalValue(const Vector3& point1, const Vector3& point2, const Vector3& center)
{
	Vector3 v1 = point1 - center;
	Vector3 v2 = point2 - point1;
	return Vector3::Cross(v1, v2);
}
