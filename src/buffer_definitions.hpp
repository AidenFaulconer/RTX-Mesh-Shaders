/*!
 *  \author    Viktor Zoutman
 *  \date      2019-2020
 *  \copyright GNU General Public License v3.0
 */

#pragma once

#include <mat4x4.hpp>

namespace cb
{

	struct Basic
	{
		float m_time;
		alignas(16) glm::mat4 m_model;
		alignas(16) glm::mat4 m_view;
		alignas(16) glm::mat4 m_proj;
	};

} /* CB */