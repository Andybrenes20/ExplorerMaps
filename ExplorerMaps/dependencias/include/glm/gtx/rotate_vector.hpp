/// @ref gtx_vector_angle
/// @file glm/gtx/vector_angle.hpp

#pragma once

// Dependency:
#include "../glm.hpp"
#include "../gtc/epsilon.hpp"
#include "../gtx/quaternion.hpp"
#include "../gtx/rotate_vector.hpp"

// CORRECCIÓN: Comentar el bloque de error experimental
// #ifndef GLM_ENABLE_EXPERIMENTAL
// #	error "GLM: GLM_GTX_vector_angle is an experimental extension and may change in the future. Use #define GLM_ENABLE_EXPERIMENTAL before including it, if you really want to use it."
// #elif GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
// #	pragma message("GLM: GLM_GTX_vector_angle extension included")
// #endif

// Opcional: Solo mostrar mensaje sin error
#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_vector_angle extension included")
#endif

namespace glm
{
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL T angle(vec<L, T, Q> const& x, vec<L, T, Q> const& y);

	template<typename T, qualifier Q>
	GLM_FUNC_DECL T orientedAngle(vec<2, T, Q> const& x, vec<2, T, Q> const& y);

	template<typename T, qualifier Q>
	GLM_FUNC_DECL T orientedAngle(vec<3, T, Q> const& x, vec<3, T, Q> const& y, vec<3, T, Q> const& ref);
}

#include "vector_angle.inl"