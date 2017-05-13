/****************************************************************************
 *
 *   Copyright (C) 2012 - 2017 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file Blockparam.cpp
 *
 * Controller library code
 */

#include "BlockParam.hpp"

#include <containers/List.hpp>

namespace control
{

BlockParamBase::BlockParamBase(Block *parent, const char *name, bool parent_prefix)
{
	char fullname[blockNameLengthMax];

	if (parent == nullptr) {
		strncpy(fullname, name, blockNameLengthMax);

	} else {
		char parentName[blockNameLengthMax];
		parent->getName(parentName, blockNameLengthMax);

		if (!strcmp(name, "")) {
			strncpy(fullname, parentName, blockNameLengthMax);
			// ensure string is terminated
			fullname[sizeof(fullname) - 1] = '\0';

		} else if (parent_prefix) {
			snprintf(fullname, blockNameLengthMax, "%s_%s", parentName, name);

		} else {
			strncpy(fullname, name, blockNameLengthMax);
			// ensure string is terminated
			fullname[sizeof(fullname) - 1] = '\0';
		}

		parent->getParams().add(this);
	}

	_handle = param_find(fullname);

	if (_handle == PARAM_INVALID) {
		printf("error finding param: %s\n", fullname);
	}
};

template <class T>
BlockParam<T>::BlockParam(Block *block, const char *name, bool parent_prefix) :
	BlockParamBase(block, name, parent_prefix),
	_val()
{
	update();
}

template <class T>
void BlockParam<T>::update()
{
	if (_handle != PARAM_INVALID) {
		param_get(_handle, &_val);
	}
}

template <class T>
void BlockParam<T>::commit()
{
	if (_handle != PARAM_INVALID) {
		param_set(_handle, &_val);
	}
}

template <class T>
void BlockParam<T>::commit_no_notification()
{
	if (_handle != PARAM_INVALID) {
		param_set_no_notification(_handle, &_val);
	}
}

template class __EXPORT BlockParam<float>;
template class __EXPORT BlockParam<int>;


template <class T>
BlockParamExt<T>::BlockParamExt(Block *block, const char *name, bool parent_prefix, T &extern_val) :
	BlockParamBase(block, name, parent_prefix),
	_extern_val(extern_val)
{
	update();
}

template <class T>
void BlockParamExt<T>::update()
{
	if (_handle != PARAM_INVALID) {
		param_get(_handle, &_extern_val);
	}
}

template <class T>
void BlockParamExt<T>::commit()
{
	if (_handle != PARAM_INVALID) {
		param_set(_handle, &_extern_val);
	}
}

template <class T>
void BlockParamExt<T>::commit_no_notification()
{
	if (_handle != PARAM_INVALID) {
		param_set_no_notification(_handle, &_extern_val);
	}
}

template class __EXPORT BlockParamExt<float>;
template class __EXPORT BlockParamExt<int32_t>;

} // namespace control
