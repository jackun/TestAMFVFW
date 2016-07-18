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

#include "stdafx.h"
#include "DeviceDX11.h"
#include "Log.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#define Log(...) Dbg(__VA_ARGS__)

static inline UINT32 GetWinVer()
{
	OSVERSIONINFO ovi;
	ovi.dwOSVersionInfoSize = sizeof(ovi);
	GetVersionEx(&ovi);

	return (ovi.dwMajorVersion << 8) | (ovi.dwMinorVersion);
}

static const IID dxgiFactory2 =
{ 0x50c83a1c, 0xe072, 0x4c48, { 0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0 } };

void DeviceDX11::EnumerateDevices(bool onlyWithOutputs, std::vector<std::wstring> *adapters)
{
	HRESULT hr;
	ComPtr<IDXGIFactory1> pFactory;

	mAdapters.clear();

	IID factoryIID = (GetWinVer() >= 0x602) ? dxgiFactory2 :
		__uuidof(IDXGIFactory1);

	hr = CreateDXGIFactory1(factoryIID, (void**)pFactory.Assign());
	if (FAILED(hr))
	{
		Log(TEXT("CreateDXGIFactory failed. Error: %08X\n"), hr);
		return;
	}

	UINT count = 0;
	Log(TEXT("List of AMD DX11 devices:\n"));
	while (true)
	{
		ComPtr<IDXGIAdapter1> pAdapter;
		if (pFactory->EnumAdapters1(count, pAdapter.Assign()) == DXGI_ERROR_NOT_FOUND)
		{
			break;
		}

		DXGI_ADAPTER_DESC desc;
		pAdapter->GetDesc(&desc);

		if (desc.VendorId != 0x1002)
		{
			count++;
			continue;
		}

		ComPtr<IDXGIOutput> pOutput;
		if (onlyWithOutputs && pAdapter->EnumOutputs(0, pOutput.Assign()) == DXGI_ERROR_NOT_FOUND)
		{
			count++;
			continue;
		}

		Log(TEXT("    %d: Device ID: %X [%s]\n"), mAdapters.size(), desc.DeviceId, desc.Description);
		if (adapters)
			adapters->push_back(std::wstring(desc.Description));
		mAdapters.push_back(count);
		count++;
	}
}

bool DeviceDX11::Create(UINT32 adapter, bool onlyWithOutputs)
{
	HRESULT hr;
	bool ret = false;

	ComPtr<IDXGIFactory1> pFactory;
	ComPtr<IDXGIAdapter1> pAdapter;
	ComPtr<IDXGIOutput> pOutput;
	ComPtr<ID3D11Device> pD3D11Device;
	ComPtr<ID3D11DeviceContext> pD3D11Context;

	EnumerateDevices(onlyWithOutputs);

	if (mAdapters.size() <= adapter)
	{
		Log(TEXT("Adapter index is out of range.\n"));
		return false;
	}
	/*auto id = std::find(mAdapters.begin(), mAdapters.end(), adapter);
	if (id == mAdapters.end())
	{
		Log(TEXT("Invalid adapter index.\n");
		return false;
	}*/

	adapter = mAdapters[adapter];

	IID factoryIID = (GetWinVer() >= 0x602) ? dxgiFactory2 :
		__uuidof(IDXGIFactory1);

	hr = CreateDXGIFactory1(factoryIID, (void**)pFactory.Assign());
	if (FAILED(hr))
	{
		Log(TEXT("CreateDXGIFactory failed. Error: %08X\n"), hr);
		return false;
	}

	if (pFactory->EnumAdapters1(adapter, pAdapter.Assign()) == DXGI_ERROR_NOT_FOUND)
	{
		Log(TEXT("Adapter %d not found.\n"), adapter);
		goto finish;
	}

	//if (SUCCEEDED(pAdapter->EnumOutputs(0, pOutput.Assign())))
	//{
	//    DXGI_OUTPUT_DESC outputDesc;
	//    pOutput->GetDesc(&outputDesc);
	//}

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};
	D3D_FEATURE_LEVEL featureLevel;

	D3D_DRIVER_TYPE eDriverType = pAdapter != NULL ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
	hr = D3D11CreateDevice(pAdapter, eDriverType, NULL, createDeviceFlags, featureLevels, _countof(featureLevels),
		D3D11_SDK_VERSION, pD3D11Device.Assign(), &featureLevel, pD3D11Context.Assign());

	if (FAILED(hr))
	{
		Log(TEXT("Failed to create HW DX11.1 device.\n"));
		hr = D3D11CreateDevice(pAdapter, eDriverType, NULL, createDeviceFlags, featureLevels + 1, _countof(featureLevels) - 1,
			D3D11_SDK_VERSION, pD3D11Device.Assign(), &featureLevel, pD3D11Context.Assign());
	}

	if (FAILED(hr))
	{
		Log(TEXT("Failed to created HW DX11 device\n"));
		goto finish;
	}

	DXGI_ADAPTER_DESC desc;
	pAdapter->GetDesc(&desc);
	Log(TEXT("Using adapter %d with Device ID: %X [%s]\n"), adapter, desc.DeviceId, desc.Description);

	mDevice = pD3D11Device.Detach();

	ret = true;

finish:
	return ret;
}