// utility_funcs.cpp : ʵ���ļ�
//
#include "stdafx.h"
#include "utility_funcs.h"
#include <algorithm>

//�ļ�ͷ�ṹ��
#pragma pack(push)
#pragma pack(2)  //���ֽڶ��룬����bmp_fileheader��ռ16Byte
//bmp�ļ�ͷ
struct bmp_fileheader
{
	unsigned short   bfType; //�ļ���ʶ
	unsigned long	 bfSize; //�ļ���С
	unsigned long	 bfReserved1; //����
	unsigned long    bfOffBits;//����ƫ��
};

//bmp��Ϣͷ
struct bmp_infoheader
{
	unsigned long    biSize;//��Ϣͷ����
	unsigned long    biWidth;//����
	unsigned long    biHeight;//�߶�
	unsigned short   biPlanes;//������=1
	unsigned short   biBitCount;//����λ��
	unsigned long    biCompression;//ѹ��˵��
	unsigned long    biSizeImage;//λͼ���ݵĴ�С
	unsigned long    biXPelsPerMeter;//ˮƽ�ֱ���
	unsigned long    biYPelsPerMeter;//��ֱ�ֱ���
	unsigned long    biClrUsed;//ʹ�õ���ɫ��
	unsigned long    biClrImportant;//��Ҫ����ɫ��
};

#pragma pack(pop)

using namespace cv;
IplImage *BMP2Ipl(unsigned char *src, int FileSize)
{
	bmp_fileheader *Fileheader = (bmp_fileheader *)src;
	//������
	assert(FileSize == Fileheader->bfSize);
	//�ж��Ƿ���λͼ
	if (Fileheader->bfType != 0x4d42)
		return NULL;
	//��ʼ������Ϣͷ
	bmp_infoheader *bmpheader = (bmp_infoheader *)(src + sizeof(bmp_fileheader));
	unsigned int width = bmpheader->biWidth;//����
	unsigned int height = bmpheader->biHeight;//�߶�
	unsigned int hSize = bmpheader->biSize;//��Ϣͷ����

	if (bmpheader->biBitCount < 24)
		return NULL;//֧����ɫ��32λ

	if (bmpheader->biCompression != 0)
		return NULL;//��֧��ѹ���㷨

					//���ݴ�С
	//unsigned int dataByteSize = bmpheader->biSizeImage;//ʵ��λͼ���ݴ�С  (�������������ݿ��ܳ���biSizeImage��0�����)
	unsigned int dataByteSize = FileSize - Fileheader->bfOffBits; //ʵ��λͼ���ݴ�С
	unsigned int rowByteSize = dataByteSize / height;//�����ÿ������

	IplImage *img = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, bmpheader->biBitCount / 8);
	//����ʵ����������ɫ��
	//img->origin = IPL_ORIGIN_BL;
	//memcpy(img->imageData, src + Fileheader->bfOffBits, FileSize - Fileheader->bfOffBits);

	//*
	img->origin = IPL_ORIGIN_TL;
	unsigned char *gc = src + FileSize - rowByteSize;
	char *p = img->imageData;
	for (unsigned int i = 0; i<height; ++i)
	{
		memcpy(p, gc, rowByteSize);
		p += rowByteSize;
		gc -= rowByteSize;
	}/**/

	
	return img;

}


int getDeviceIndex(std::string vid, std::string pid)
{
	std::string str_vid = "VID_" + vid;
	std::string str_pid = "PID_" + pid;
	transform(str_vid.begin(), str_vid.end(), str_vid.begin(), ::tolower);
	transform(str_pid.begin(), str_pid.end(), str_pid.begin(), ::tolower);
	int vidfind = -1;
	int pidfind = -1;
	int retIdx = -1;

	ICreateDevEnum *pDevEnum = NULL;
	IEnumMoniker *pEnum = NULL;
	int deviceCounter = 0;
	CoInitialize(NULL);
	HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
		CLSCTX_INPROC_SERVER, IID_ICreateDevEnum,
		reinterpret_cast<void**>(&pDevEnum));


	if (SUCCEEDED(hr))
	{
		// Create an enumerator for the video capture category.
		hr = pDevEnum->CreateClassEnumerator(
			CLSID_VideoInputDeviceCategory,
			&pEnum, 0);

		if (hr == S_OK) {

			//if (!silent)printf("SETUP: Looking For Capture Devices\n");
			IMoniker *pMoniker = NULL;

			while (pEnum->Next(1, &pMoniker, NULL) == S_OK) {

				IPropertyBag *pPropBag;
				hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag,
					(void**)(&pPropBag));

				if (FAILED(hr)) {
					pMoniker->Release();
					continue;  // Skip this one, maybe the next one will work.
				}


				// Find the DevicePath, description or friendly name.
				VARIANT varName;
				VariantInit(&varName);
				//hr = pPropBag->Read(L"Description", &varName, 0);

				//if (FAILED(hr)) hr = pPropBag->Read(L"FriendlyName", &varName, 0);

				hr = pPropBag->Read(L"DevicePath", &varName, 0);

				if (SUCCEEDED(hr)) {

					//hr = pPropBag->Read(L"FriendlyName", &varName, 0);

					_bstr_t bstr_t(varName.bstrVal);
					std::string strVal(bstr_t);
					
					vidfind = (int)strVal.find(str_vid);
					pidfind = (int)strVal.find(str_pid);

					printf("SETUP: %i) %s \n", deviceCounter, strVal.c_str());
				}

				pPropBag->Release();
				pPropBag = NULL;

				pMoniker->Release();
				pMoniker = NULL;

				if (vidfind >= 0 && pidfind >= 0) {
					retIdx = deviceCounter;
					break;
				}

				deviceCounter++;
			}

			pDevEnum->Release();
			pDevEnum = NULL;

			pEnum->Release();
			pEnum = NULL;
		}

		//printf("SETUP: %i Device(s) found\n\n", deviceCounter);
	}

	//return deviceCounter;
	return retIdx;
}

void MatAlphaBlend(cv::Mat &dst, cv::Mat &scr)
{
	if (dst.channels() != 3 || scr.channels() != 4)
		return;
	
	std::vector<cv::Mat>scr_channels;
	std::vector<cv::Mat>dstt_channels;
	split(scr, scr_channels);
	split(dst, dstt_channels);
	CV_Assert(scr_channels.size() == 4 && dstt_channels.size() == 3);

	for (int i = 0; i < 3; i++)
	{
		dstt_channels[i] = dstt_channels[i].mul(255.0 - scr_channels[3], 1.0 / 255.0);
		dstt_channels[i] += scr_channels[i].mul(scr_channels[3], 1.0 / 255.0);
	}
	merge(dstt_channels, dst);
}

// function:�������ȴ�,��������Ϣ
void WaitObjectAndMsg(HANDLE hEvent, DWORD dwMilliseconds) {
	BOOL bWait = TRUE;
	DWORD dwResult = 0;

	while (bWait)
	{
		DWORD dwResult = ::MsgWaitForMultipleObjects(1, &hEvent, FALSE, dwMilliseconds, QS_ALLINPUT);

		if (WAIT_OBJECT_0 == dwResult) {
			break;
		}
		else {
			MSG msg;
			PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
			DispatchMessage(&msg);
		}
	}
}