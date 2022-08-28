#ifndef GEOMETRYHELPER_H
#define GEOMETRYHELPER_H

#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <limits>
#include <algorithm>

namespace Utils
{
	/// <summary>
	/// Returns a list of 3D points representing the frustum's corners in world space.
	/// </summary>
	/// <param name="viewProjMatrix"></param>
	/// <returns>The list of 3D points.</returns>
	std::vector<glm::vec3> GetFrustumCorners(glm::mat4 viewProjMatrix)
	{
		glm::mat4 viewProjMatrixInv = glm::inverse(viewProjMatrix);

		// CCW from low-left corner
		std::vector<glm::vec3> cornerPts = std::vector<glm::vec3>
		{
			// Near
			glm::vec3(-1, -1, -1),
			glm::vec3(1, -1, -1),
			glm::vec3(1,  1, -1),
			glm::vec3(-1,  1, -1),

			// Far
			glm::vec3(-1, -1,  1),
			glm::vec3(1, -1,  1),
			glm::vec3(1,  1,  1),
			glm::vec3(-1,  1,  1)
		};

		for (auto& v : cornerPts)
		{
			glm::vec4 v4 = viewProjMatrixInv * glm::vec4(v.x, v.y, v.z, 1.0);
			v = glm::vec3(v4.x / v4.w, v4.y / v4.w, v4.z / v4.w);
		}

		return cornerPts;
	}
	/// <summary>
	/// Returns a list of 3D points representing the frustum's corners in world space.
	/// </summary>
	/// <param name="viewMatrix"></param>
	/// <param name="projectionMatrix"></param>
	/// <returns>The list of 3D points.</returns>
	std::vector<glm::vec3> GetFrustumCorners(glm::mat4 viewMatrix, glm::mat4 projectionMatrix)
	{
		return GetFrustumCorners(projectionMatrix * viewMatrix);
	}

	/// <summary>
	/// Returns a list of 3D lines representing the frustum's edges in world space.
	/// </summary>
	/// <param name="viewProjMatrix"></param>
	/// <returns> The list of 3D lines in the form start = list[i], end = list[i+1] for each i%2==0 with i less than listLen. </returns>
	std::vector<glm::vec3> GetFrustumEdges(glm::mat4 viewProjMatrix)
	{
		std::vector<glm::vec3> cornerPts = GetFrustumCorners(viewProjMatrix);

		std::vector<glm::vec3> edgesPts = std::vector<glm::vec3>
		{
			cornerPts[0], cornerPts[1],
			cornerPts[1], cornerPts[2],
			cornerPts[2], cornerPts[3],
			cornerPts[3], cornerPts[0],

			cornerPts[4], cornerPts[5],
			cornerPts[5], cornerPts[6],
			cornerPts[6], cornerPts[7],
			cornerPts[7], cornerPts[4],

			cornerPts[0], cornerPts[4],
			cornerPts[1], cornerPts[5],
			cornerPts[2], cornerPts[6],
			cornerPts[3], cornerPts[7],
		};

		return edgesPts;
	}

	/// <summary>
	/// Returns a list of 3D lines representing the frustum's edges in world space.
	/// </summary>
	/// <param name="viewMatrix"></param>
	/// <param name="projectionMatrix"></param>
	/// <returns> The list of 3D lines in the form start = list[i], end = list[i+1] for each i%2==0 with i less than listLen. </returns>
	std::vector<glm::vec3> GetFrustumEdges(glm::mat4 viewMatrix, glm::mat4 projectionMatrix)
	{
		return GetFrustumEdges(projectionMatrix * viewMatrix);
	}

	
	// TODO: Arbitrary regions.
	// TODO: Region definition.
	enum class DomainType2D
	{
		Square,
		Disk
	};

	/// <summary>
	/// 2D domain description.
	/// </summary>
	class Domain2D
	{
		public:
			DomainType2D type;
			float size;

		/// <summary>
		/// Standard constructor.
		/// </summary>
		/// <param name="domainType"> The domain type.</param>
		/// <param name="domainSize"> The domain size. It's meaning depends on the type paramenter. </param>
		Domain2D(DomainType2D domainType, float domainSize) : type(domainType), size(domainSize) {}

		/// <summary>
		/// Computes the area of the 2D domain.
		/// </summary>
		float Area()
		{
			switch (type)
			{
				case DomainType2D::Disk:
					return glm::pi<float>() *size * size;
				break;

				case DomainType2D::Square:
					return size * size;
				break;
			}
		}
	};

	unsigned int MirrorAtCenter(unsigned int x, unsigned int n)
	{
		unsigned int mask = (1u << n) - 1;
		return mask & ~x;
	}

	std::vector<glm::uvec2> GetHilbertCurve2D(unsigned int order)
	{
		if (order == 1)
			return std::vector<glm::uvec2>
		{
				glm::uvec2(0, 0),
				glm::uvec2(0, 1),
				glm::uvec2(1, 1),
				glm::uvec2(1, 0)
		};
		else
		{
			std::vector<glm::uvec2> quadT = GetHilbertCurve2D(order - 1);
			std::vector<glm::uvec2> quadB = GetHilbertCurve2D(order - 1);
			
			/* Flip bottom quadrant */
			for (auto &p : quadB)
			{
				unsigned int buff = p.x;
				p.x = p.y;
				p.y = buff;
			}

			/* Translate top quadrant */
			for (auto &p : quadT)
				p.y += 1u << (order - 1);

			std::vector<glm::uvec2> half{};
			half.insert(half.end(), quadB.begin(), quadB.end());
			half.insert(half.end(), quadT.begin(), quadT.end());

			std::vector<glm::uvec2> res{};
			res.insert(res.begin(), half.begin(), half.end());

			/* Flip and translate half */
			for (auto &p : half)
			{
				p.x = MirrorAtCenter(p.x, order - 1);
				p.x += 1u << (order - 1);
			}
			std::reverse(half.begin(), half.end());
			res.insert(res.end(), half.begin(), half.end());

			return res;
		}
	}
	
	std::vector<glm::vec3> GetHilberCurve2D(unsigned int order, float size)
	{
		std::vector<glm::vec3> res{};
		res.reserve(1u << (order + 1));
		for (auto const& u2 : GetHilbertCurve2D(order))
			res.push_back(
				glm::vec3(
					(float)u2.x * size / (1u << (order + 1)),
					(float)u2.y * size / (1u << (order + 1)),
					0.0f
				));

		return res;
	}
	
	/// <summary>
	/// Returns a <see cref="std::vector"/> of uniformly distributed samples between min and max.
	/// </summary>
	/// <param name="samplesCount"> The number of samples.</param>
	/// <param name="min"> The minimum sample value.</param>
	/// <param name="max"> The maximum sample value.</param>
	std::vector<float> GetUniformDistributedSamples1D(int samplesCount, float min, float max)
	{
		std::default_random_engine generator;
		std::uniform_real_distribution<float> distribution(min, max);
		std::vector<float> noiseVec;

		for (unsigned int i = 0; i < samplesCount; i++)
			noiseVec.push_back(distribution(generator));

		return noiseVec;
	}

	/// <summary>
	/// Returns a <see cref="std::vector"/> of uniformly distributed samples on the specified 2D domain.
	/// </summary>
	/// <param name="samplesCount"> The number of samples.</param>
	/// <param name="domain"> The domain description.</param>
	std::vector<glm::vec2> GetUniformDistributedSamples2D(int samplesCount, Domain2D domain)
	{
		int smp1DCount = samplesCount * 2;

		std::vector<float> smp1D = GetUniformDistributedSamples1D(smp1DCount, 0.0f, 1.0f);

		std::vector<glm::vec2> smp2D{};
		smp2D.reserve(smp1DCount);

		for (int i = 0; i < smp1DCount; /**/)
		{
			// SQUARE case
			// ------------------------------------
			if (domain.type == DomainType2D::Square)
			{
				smp2D.push_back(glm::vec2(
					smp1D[i++] * domain.size,
					smp1D[i++] * domain.size
				));
			}
			// DISK case
			// ------------------------------------
			else
			{
				float r = 1.0f * glm::sqrt(smp1D[i++]);
				float theta = smp1D[i++] * 2.0f * glm::pi<float>();

				// radius-angle to cartesian
				smp2D.push_back(glm::vec2(
					glm::cos(theta) * r * domain.size,
					glm::sin(theta) * r * domain.size
				));
			}
		}

		return smp2D;
	}

	struct WeightedSample
	{
		public:
			glm::vec2 sample;
			float weight;
	};

	float GetWeightContribution(glm::vec2 s1, glm::vec2 s2, float rMin, float rMax)
	{
		float alpha = 8.0f;
		float distance = glm::distance(s1, s2);
		
		if (distance >= 2.0f * rMax) return 0.0f;

		float d =
			(distance > 2.0f * rMin)
			? glm::min(2.0f * rMax, distance)
			: 2.0f * rMin;

		return  pow(1.0f - (d / (2.0 * rMax)), alpha);
	}

	std::pair<std::vector<glm::vec2>, std::vector<glm::vec2>>
		WeightedSampleElimination(const std::vector<glm::vec2>& initialDistribution, int samplesCount, Domain2D domain)
	{
		// Based on http://www.cemyuksel.com/research/sampleelimination/

		// Constants
		// ----------------------
		float
			beta = 0.65f,
			gamma = 1.5f;

		int M = initialDistribution.size();

		std::vector<WeightedSample> weightedSamples{};
		weightedSamples.reserve(M);

		// 1. Assign weights wi to each sample si
		// --------------------------------------
		float
			rMax = sqrt(domain.Area() / (2.0f * sqrt(3.0f) * samplesCount)),
			rMin = rMax * beta * (1.0f - pow(samplesCount / M, gamma));

		// TODO: some kind of space partitioning !!!
		for (int i = 0; i < M; i++)
		{
			float w = 0;
			for (int j = 0; j < M; j++)
			{
				if (i == j) continue;

				w += GetWeightContribution(initialDistribution[i], initialDistribution[j], rMin, rMax);
			}

			weightedSamples.push_back(WeightedSample{ initialDistribution[i], w });
		}

		auto wsCompare = [](WeightedSample& a, WeightedSample& b) {return a.weight < b.weight; };

		// 2. Build a heap for si using weights wi
		// ---------------------------------------
		std::make_heap(weightedSamples.begin(), weightedSamples.end(), wsCompare);

		// 3. while number of samples > desired 
		//    pull from heap, update weight, update heap
		// ---------------------------------------------
		std::vector<glm::vec2> removedSamples{};
		removedSamples.reserve(M - samplesCount);

		while (weightedSamples.size() > samplesCount)
		{
			std::pop_heap(weightedSamples.begin(), weightedSamples.end(), wsCompare);

			WeightedSample removed = weightedSamples[weightedSamples.size() - 1];
			removedSamples.push_back(removed.sample);
			weightedSamples.pop_back();

			for (auto& s : weightedSamples)
				s.weight -= GetWeightContribution(s.sample, removed.sample, rMin, rMax);

			std::make_heap(weightedSamples.begin(), weightedSamples.end(), wsCompare);
		}

		// 5. Progessive Sampling
		// ----------------------
		std::vector<glm::vec2> res{};
		res.reserve(samplesCount);

		for (const auto& s : weightedSamples)
			res.push_back(s.sample);

		return std::make_pair(res, removedSamples);
	}

	bool IsPot(int a) { return (a > 0) && ((a & (a - 1)) == 0); }

	/// <summary>
	/// Computes a poisson disk sample set. 
	/// The set is ordererd (see reference).
	/// Based on http://www.cemyuksel.com/research/sampleelimination . 
	/// </summary>
	/// <param name="samplesCount"> The samples count. Must be a power of two </param>
	/// <param name="domain"> The domain. </param>
	std::vector<glm::vec2> GetPoissonDiskSamples2D(int samplesCount, Domain2D domain)
	{
		if (!IsPot(samplesCount))
			throw "sample count must be POT.";

		int M = samplesCount * 15;

		std::vector<glm::vec2> uDistr = GetUniformDistributedSamples2D(
			M, // More initial samples than target count.
			domain
		);

		std::vector<glm::vec2> samples = WeightedSampleElimination(uDistr, samplesCount, domain).first;

		// Progessive Sampling
		// ----------------------
		std::vector<glm::vec2> res{};
		res.reserve(samplesCount);

		for (int i = 1, targetCount = samplesCount / 2; targetCount >= 1; i++, targetCount/=2 )
		{
			auto iterationRes = WeightedSampleElimination(samples, targetCount, domain);
			std::vector<glm::vec2> removedSamples = iterationRes.second;
			samples = iterationRes.first;

			res.insert(res.begin(), removedSamples.begin(), removedSamples.end());
		}

		if (samples.size() > 1)
			throw "";

		res.push_back(samples[0]);

		return res;
	}


	template<typename T>
	class Functor_MinMax
	{
	private:
		T
			_min, _max;

	public:
		Functor_MinMax(const T& min, const T& max) : _min { min }, _max{ max } {};

		void operator()(const T& p)
		{
			// Find Minimum 
			// ---------------------------
			_min.x = glm::min(_min.x, p.x);
			_min.y = glm::min(_min.y, p.y);
			_min.z = glm::min(_min.z, p.z);

			// Find Maximum 
			// ---------------------------
			_max.x = glm::max(_max.x, p.x);
			_max.y = glm::max(_max.y, p.y);
			_max.z = glm::max(_max.z, p.z);
		}

		T Min() { return _min; }
		T Max() { return _max; }
	};

	template<typename T>
	class BoundingBox
	{
	private:
		T _min, _max, _center, _diagonal;
		typename T::value_type _size;

	public:
		BoundingBox(const std::vector<T> &points)
		{

			typename T::value_type minVal = std::numeric_limits<T::value_type>::min();
			typename T::value_type maxVal = std::numeric_limits<T::value_type>::max();

			_min = T{maxVal};
			_max = T{minVal};

			Update(points);
		}

		
		static void GetMinMax(const std::vector<T>& points, T& min, T& max)
		{
			Functor_MinMax<T> f{ T{min}, T{max}};

			f = std::for_each(points.begin(), points.end(), f);

			/* Error conditions ?*/

			min = f.Min();
			max = f.Max();
		}

		static std::pair<T, T> GetMinMax(const std::vector<T>& points)
		{
			typename T::value_type minVal = std::numeric_limits<T::value_type>::min();
			typename T::value_type maxVal = std::numeric_limits<T::value_type>::max();

			T
				min{ maxVal }, max{ minVal };

			GetMinMax(points, min, max);

			return std::make_pair(min, max);
		}

		void Update(const std::vector<T> &points)
		{
			
			T
				nMin{ _min }, nMax{ _max };

			GetMinMax(points, nMin, nMax);

			_min = nMin;
			_max = nMax;

			_diagonal = _max - _min;
			_center = _min + _diagonal * 0.5f;
			_size = glm::length(_diagonal);
		}

		std::vector<T> GetLines() const;

		std::vector<T> GetPoints() const;
		
		T Center() const { return _center; };
		T Diagonal() const { return _diagonal; };
		typename T::value_type Size() const { return _size; };
	};


	// Default Implementation ("thow exception....") =====================================
	template<typename T>
	std::vector<T> BoundingBox<T>::GetLines() const
	{
		throw "Not the best way to handle this";
	}

	template<typename T>
	std::vector<T> BoundingBox<T>::GetPoints() const
	{
		throw "Not the best way to handle this";
	}

	// Specialized Implementation for glm::vec3 ==========================================
	template<>
	std::vector<glm::vec3> BoundingBox<glm::vec3>::GetLines() const
	{
		return
			std::vector<glm::vec3>
		{
				// Bottom =======================================================================
				_min,
				_min + glm::vec3(1, 0, 0) * _diagonal.x,
				_min + glm::vec3(1, 0, 0) * _diagonal.x,
				_min + glm::vec3(1, 0, 0) * _diagonal.x + glm::vec3(0, 1, 0) * _diagonal.y,
				_min + glm::vec3(1, 0, 0) * _diagonal.x + glm::vec3(0, 1, 0) * _diagonal.y,
				_min + glm::vec3(0, 1, 0) * _diagonal.y,
				_min + glm::vec3(0, 1, 0) * _diagonal.y,
				_min,

				// Top =======================================================================
				_min + glm::vec3(0, 0, 1) * _diagonal.z,
				_min + glm::vec3(1, 0, 0) * _diagonal.x + glm::vec3(0, 0, 1) * _diagonal.z,
				_min + glm::vec3(1, 0, 0) * _diagonal.x + glm::vec3(0, 0, 1) * _diagonal.z,
				_min + glm::vec3(1, 0, 0) * _diagonal.x + glm::vec3(0, 1, 0) * _diagonal.y + glm::vec3(0, 0, 1) * _diagonal.z,
				_min + glm::vec3(1, 0, 0) * _diagonal.x + glm::vec3(0, 1, 0) * _diagonal.y + glm::vec3(0, 0, 1) * _diagonal.z,
				_min + glm::vec3(0, 1, 0) * _diagonal.y + glm::vec3(0, 0, 1) * _diagonal.z,
				_min + glm::vec3(0, 1, 0) * _diagonal.y + glm::vec3(0, 0, 1) * _diagonal.z,
				_min + glm::vec3(0, 0, 1) * _diagonal.z,

				// Sides =======================================================================
				_min,
				_min + glm::vec3(0, 0, 1) * _diagonal.z,
				_min + glm::vec3(1, 0, 0) * _diagonal.x,
				_min + glm::vec3(1, 0, 0) * _diagonal.x + glm::vec3(0, 0, 1) * _diagonal.z,
				_min + glm::vec3(1, 0, 0) * _diagonal.x + glm::vec3(0, 1, 0) * _diagonal.y,
				_min + glm::vec3(1, 0, 0) * _diagonal.x + glm::vec3(0, 1, 0) * _diagonal.y + glm::vec3(0, 0, 1) * _diagonal.z,
				_min + glm::vec3(0, 1, 0) * _diagonal.y,
				_min + glm::vec3(0, 1, 0) * _diagonal.y + glm::vec3(0, 0, 1) * _diagonal.z,
		};
	}

	template<>
	std::vector<glm::vec3> BoundingBox<glm::vec3>::GetPoints() const
	{
		return
			std::vector<glm::vec3>
		{
				// Bottom =======================================================================
				_min,
				_min + glm::vec3(1, 0, 0) * _diagonal.x,
				_min + glm::vec3(1, 0, 0) * _diagonal.x + glm::vec3(0, 1, 0) * _diagonal.y,
				_min + glm::vec3(0, 1, 0) * _diagonal.y,

				// Top =======================================================================
				_min + glm::vec3(0, 0, 1) * _diagonal.z,
				_min + glm::vec3(1, 0, 0) * _diagonal.x + glm::vec3(0, 0, 1) * _diagonal.z,
				_min + glm::vec3(1, 0, 0) * _diagonal.x + glm::vec3(0, 1, 0) * _diagonal.y + glm::vec3(0, 0, 1) * _diagonal.z,
				_min + glm::vec3(0, 1, 0) * _diagonal.y + glm::vec3(0, 0, 1) * _diagonal.z,
		};
	}

	const float fullScreenQuad_verts[]
	{
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f
	};


	void GetPointShadowMatrices(glm::vec3 position, float radius, float& near, float& far, std::vector<glm::mat4>& shadowTransforms)
	{
		
		near = 1E-1;
		far = radius;

		glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, near, far);

		shadowTransforms.clear();
		shadowTransforms.push_back(shadowProj * glm::lookAt(position, position + glm::vec3( 1.0,  0.0,  0.0), glm::vec3(0.0, -1.0,  0.0)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(position, position + glm::vec3(-1.0,  0.0,  0.0), glm::vec3(0.0, -1.0,  0.0)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(position, position + glm::vec3( 0.0,  1.0,  0.0), glm::vec3(0.0,  0.0,  1.0)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(position, position + glm::vec3( 0.0, -1.0,  0.0), glm::vec3(0.0,  0.0, -1.0)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(position, position + glm::vec3( 0.0,  0.0,  1.0), glm::vec3(0.0, -1.0,  0.0)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(position, position + glm::vec3( 0.0,  0.0, -1.0), glm::vec3(0.0, -1.0,  0.0)));
	}

	void GetDirectionalShadowMatrices(glm::vec3 position, glm::vec3 direction, std::vector<glm::vec3> bboxPoints, glm::mat4 &view, glm::mat4 &proj)
	{
		glm::vec3 center = (position + direction);
		glm::vec3 worldZ = glm::vec3(0.0f, 0.0f, 1.0f);
		
		/*
		* NOTE: the lookAt matrix maps eye to the origin and center to the negative Z axis
		* (the opposite of what you think)...
		*/
		view = glm::lookAt(position, center, worldZ);

		// this containter stores the bbox points in camera space
		std::vector<glm::vec3> projPoints = std::vector<glm::vec3>();

		for (int i = 0; i < bboxPoints.size(); i++)
		{
			glm::vec3 projPoint =view * glm::vec4(bboxPoints[i], 1.0f);

			projPoints.push_back(projPoint);
		}

		std::pair<glm::vec3, glm::vec3> boxExt = BoundingBox<glm::vec3>::GetMinMax(projPoints);

		proj = glm::ortho(
			boxExt.first.x,
			boxExt.second.x,
			boxExt.first.y,
			boxExt.second.y,

			-1.0f * boxExt.second.z,
			-1.0f * boxExt.first.z
		);
	}

	void GetTightNearFar(std::vector<glm::vec3> bboxPoints, glm::mat4 view, float tol, float& near, float& far)
	{
		
		// this containter stores the bbox points in camera space
		std::vector<glm::vec3> projPoints = std::vector<glm::vec3>();

		for (int i = 0; i < bboxPoints.size(); i++)
		{
			glm::vec3 projPoint = view * glm::vec4(bboxPoints[i], 1.0f);
			projPoint.z *= -1.0f; // z positive towards far
			projPoints.push_back(projPoint);
		}

		glm::vec3 min, max;

		std::pair<glm::vec3, glm::vec3> boxExt = BoundingBox<glm::vec3>::GetMinMax(projPoints);

		near = glm::max(0.001f, boxExt.first.z - tol);
		far = glm::max(0.001f, boxExt.second.z + tol);
	}

	void GetTightNearFar(std::vector<glm::vec3> bboxPoints, glm::mat4 view, float& near, float& far)
	{
		GetTightNearFar(bboxPoints, view, 0.0f, near, far);
	}

	// https://stackoverflow.com/questions/17294629/merging-flattening-sub-vectors-into-a-single-vector-c-converting-2d-to-1d
	template <typename T>
	std::vector<T> flatten(const std::vector<std::vector<T>>& v) {
		std::size_t total_size = 0;
		for (const auto& sub : v)
			total_size += sub.size(); // I wish there was a transform_accumulate
		std::vector<T> result;
		result.reserve(total_size);
		for (const auto& sub : v)
			result.insert(result.end(), sub.begin(), sub.end());
		return result;

	}
	// TODO: nice to have a generic implementation
	// ask stackoverflow?
	std::vector<float> flatten3(const std::vector<glm::vec3>& v) {
		std::size_t total_size = v.size() * 3;
		std::vector<float> result;

		result.reserve(total_size);

		for (const auto& sub : v)
		{
			result.push_back(sub.x);
			result.push_back(sub.y);
			result.push_back(sub.z);
		}

		return result;
	}

	// TODO: nice to have a generic implementation
	// ask stackoverflow?
	std::vector<float> flatten2(const std::vector<glm::vec2>& v) {
		std::size_t total_size = v.size() * 2;
		std::vector<float> result;

		result.reserve(total_size);

		for (const auto& sub : v)
		{
			result.push_back(sub.x);
			result.push_back(sub.y);
		}

		return result;
	}



	float Lerp(float a, float b, float t)
	{
		return a + (b - a) * t;
	}
}

std::vector<std::vector<int>> ComputePascalOddRows(int maxLevel)
{
	std::vector<std::vector<int>> triangle;
	std::vector<int> firstRow;

	firstRow.push_back(1);
	for (int j = 1; j < maxLevel; j++)
	{
		firstRow.push_back(0);
	}

	triangle.push_back(firstRow);

	for (int l = 1; l < maxLevel; l++)
	{
		std::vector<int> row;
		for (int j = 0; j <= l; j++)
		{
			int
				k, km1, kp1;
			k = km1 = kp1 = 0;

			if (j < l)
				k = triangle.at(l - 1).at(j);
			if (j < l - 1)
				kp1 = triangle.at(l - 1).at(j + 1);
			if (j > 0)
				km1 = triangle.at(l - 1).at(j - 1);
			else if (l > 1)
				km1 = triangle.at(l - 1).at(j + 1);

			int item = k + kp1;
			item += k + km1;

			row.push_back(item);
		}
		for (int j = l + 1; j < maxLevel; j++)
		{
			row.push_back(0);
		}

		triangle.push_back(row);
	}

	return triangle;
}





#endif