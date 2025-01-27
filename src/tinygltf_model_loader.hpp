/*!
 *  \author    Viktor Zoutman
 *  \date      2019-2020
 *  \copyright GNU General Public License v3.0
 */

#pragma once

#include "resource_loader.hpp"

#include "resource_structs.hpp"

class TinyGLTFModelLoader : public ResourceLoader<ModelData>
{
public:
	TinyGLTFModelLoader();
	~TinyGLTFModelLoader() final = default;

	AnonResource LoadFromDisc(std::string const & path) final;
};