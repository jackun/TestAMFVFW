/******************************************************************************
Copyright (C) 2014 by jackun <jack.un@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#include <vector>
#include <d3d11.h>
#include "ComPtr.h"

class DeviceDX11
{
public:
	DeviceDX11() : mDevice(NULL){}
	~DeviceDX11()
	{
		Free();
	}

	void Free()
	{
		mDevice.Clear();
	}

	bool Create(unsigned int adapter, bool onlyWithOutputs = false);
	void EnumerateDevices(bool onlyWithOutputs, std::vector<std::wstring> *adapters = nullptr);
	ID3D11Device *GetDevice()
	{
		return mDevice;
	}

	bool Valid()
	{
		return mDevice != nullptr;
	}

private:
	ComPtr<ID3D11Device> mDevice;
	std::vector<unsigned int> mAdapters;
};