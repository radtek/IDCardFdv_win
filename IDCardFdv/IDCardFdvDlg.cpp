
// IDCardFdvDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "IDCardFdv.h"
#include "IDCardFdvDlg.h"
#include "afxdialogex.h"

#include "IdCardReader.h"
#include "LocalMac.h"
#include "utility_funcs.h"
#include "base64.h"

#include <fstream>

using namespace std;
using namespace cv;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


#define OPENCV_CAPTURE 1

#define CLEAR_INFOIMG_TIMER 1


UINT CameraShowThread(LPVOID lpParam);
UINT FdvThread(LPVOID lpParam);
std::string callverify(std::string url, std::string appId, std::string apiKey, std::string secretKey,
	std::string macId, std::string registeredNo,
	std::string idcardId, std::string idcardIssuedate,
	std::vector<uchar> idcardPhoto, std::vector<uchar> verifyPhotos[], int verifyPhotoNum);



#if OPENCV_CAPTURE
#else
static VOID __stdcall FaceImageCB(HWND hWnd, BSTR imgBase64, ULONG_PTR userdata)
{
	CIDCardFdvDlg* handle = (CIDCardFdvDlg*)userdata;

	_bstr_t bstr_t(imgBase64);
	handle->m_sCaptureBase64 = bstr_t;
	handle->m_bFaceGot = true;
	ZZReleaseString(imgBase64);
}

static VOID __stdcall FaceResultCB(HWND hWnd, LONG result, BSTR feature, ULONG_PTR userdata)
{
	CIDCardFdvDlg* handle = (CIDCardFdvDlg*)userdata;
	if (0 == result) {
		handle->m_bIsAliveSample = true;
		handle->ProcessCapture();
	}
	else if (-3 == result) {
		handle->m_bIsAliveSample = false;
		handle->ProcessCapture();
	}
	else {
		handle->m_bFaceGot = false;
	}
	ZZReleaseString(feature);
}
#endif


// CIDCardFdvDlg �Ի���



CIDCardFdvDlg::CIDCardFdvDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_IDCARDFDV_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	camdevid = 0;
	m_bCameraRun = false;
	m_thCamera = NULL;
	ResetEvent(m_eCameraEnd);
	m_iplImgDisplay = NULL;
	m_bFlip = true;

	m_bCmdCapture = false;

	m_bFaceGot = false;
	m_bIsAliveSample = false;
	m_bFdvRun = false;

	memset(m_IdCardId, 0, sizeof(m_IdCardId));
	memset(m_IdCardIssuedate, 0, sizeof(m_IdCardIssuedate));
	memset(m_IdCardPhoto, 0, sizeof(m_IdCardPhoto));
	m_iplImgPhoto = NULL;

	m_pInfoDlg = NULL;

	m_hBIconCamera = NULL;
	m_iplImgCameraImg = NULL;
	m_iplImgResultIconRight = NULL;
	m_iplImgResultIconWrong = NULL;
	m_dThreshold = 77.0;
}

void CIDCardFdvDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CIDCardFdvDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_CLOSE()
	ON_WM_CTLCOLOR()
	ON_WM_LBUTTONDOWN()
	ON_WM_TIMER()
	ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()


string ExtractFilePath(const string& szFile)
{
	if (szFile == "")
		return "";

	size_t idx = szFile.find_last_of("\\:");

	if (-1 == idx)
		return "";
	return string(szFile.begin(), szFile.begin() + idx + 1);
}

// CIDCardFdvDlg ��Ϣ��������

BOOL CIDCardFdvDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// ���ô˶Ի����ͼ�ꡣ  ��Ӧ�ó��������ڲ��ǶԻ���ʱ����ܽ��Զ�
	//  ִ�д˲���
	SetIcon(m_hIcon, TRUE);			// ���ô�ͼ��
	SetIcon(m_hIcon, FALSE);		// ����Сͼ��

	ShowWindow(SW_MAXIMIZE);

	// TODO: �ڴ����Ӷ���ĳ�ʼ������

	//��õ�����ʾ�������ؿ��Ⱥ����ظ߶�
	//int ax = GetDC()->GetDeviceCaps(HORZRES);
	//int ay = GetDC()->GetDeviceCaps(VERTRES);

	char szPath[1024] = { 0 };
	GetModuleFileName(NULL, szPath, MAX_PATH);
	m_strModulePath = ExtractFilePath(szPath);

	
	// �����ؼ���Сλ��
	CRect rect;
	GetClientRect(&rect);		// ��ȡ�����Ի����С
	int rw = rect.right - rect.left;
	//int rh = rect.bottom - rect.top;
	int rh = rw * 720 / 1280;
	//rw = (int)(rw * 0.6);
	int offsetX = rect.left;
	int offsetY = rect.top;
	GetDlgItem(IDC_PREVIEW_IMG)->MoveWindow(offsetX, offsetY, rw, rh, false);

	// ������Ϣ����
	ClientToScreen(&rect);
	float IPRATE = 0.15f;
	rw = (int)((rect.right - rect.left) * IPRATE); // ��Ϣ���ڿ�
	rh = rect.bottom - rect.top;					// ��Ϣ���ڸ�
	offsetX = rect.left + (int)((rect.right - rect.left) * (1 - IPRATE));
	offsetY = rect.top;
	if (NULL == m_pInfoDlg) {
		m_pInfoDlg = new CInfoDlg(offsetX, offsetY, rw, rh);
		m_pInfoDlg->Create(IDD_INFO_DIALOG, this);
	}
	m_pInfoDlg->ShowWindow(SW_SHOW);

	/*
	m_hBIconCamera = (HBITMAP)LoadImage(AfxGetInstanceHandle(),
	MAKEINTRESOURCE(IDB_ICON_CAMERA),
	IMAGE_BITMAP, 0, 0,
	LR_LOADMAP3DCOLORS);
	CStatic* m_cs = (CStatic*)GetDlgItem(IDC_CAMERA_IMAGE);
	m_cs->SetBitmap(m_hBIconCamera);
	*/

	//std::string fn = m_strModulePath;
	//fn += "camera.png";
	//m_iplImgCameraImg = cvLoadImage(fn.c_str(), -1);
	std::string fn = m_strModulePath + "right.png";
	m_iplImgResultIconRight = cvLoadImage(fn.c_str(), -1);
	fn = m_strModulePath + "wrong.png";
	m_iplImgResultIconWrong = cvLoadImage(fn.c_str(), -1);

	// Mac ID
	char mac[64];
	GetLocalMAC(mac, sizeof(mac));
	m_macId = mac;

	// config
	m_cfgAppId = "10022245";
	m_cfgApiKey = "MGRhNjEyYWExOTdhYzYxNTkx";
	m_cfgSecretKey = "NzQyNTg0YmZmNDg3OWFjMTU1MDQ2YzIw";
	m_cfgUrl = "http://192.168.1.201:8004/idcardfdv";
	m_cfgRegisteredNo = "0";
	std::ifstream confFile(m_strModulePath + "config.txt");
	std::string line;
	while (std::getline(confFile, line))
	{
		std::istringstream is_line(line);

		std::string key;
		if (std::getline(is_line, key, '='))
		{
			std::string value;
			if (std::getline(is_line, value)) {
				if (key == "appId")
					m_cfgAppId = value;
				if (key == "apiKey")
					m_cfgApiKey = value;
				if (key == "secretKey")
					m_cfgSecretKey = value;
				if (key == "url")
					m_cfgUrl = value;
				if (key == "registeredNo")
					m_cfgRegisteredNo = value;
			}
		}
	}
	confFile.close();

	
	m_pInfoDlg->setResultText("");
	m_pInfoDlg->setThresholdText("");
	


#if OPENCV_CAPTURE
	ResetEvent(m_eCameraEnd);
	startCameraThread();
#else
	ZZInitFaceMgr();
	ZZOpenDevice(GetDlgItem(IDC_PREVIEW_IMG)->m_hWnd, 2);
	ZZOpenHideDevice(GetDlgItem(IDC_PREVIEW_IMG)->m_hWnd, 4);
	ZZOpenVideo(GetDlgItem(IDC_PREVIEW_IMG)->m_hWnd);
	ZZOpenHideVideo(GetDlgItem(IDC_PREVIEW_IMG)->m_hWnd);
	ZZGetFaceFeature(GetDlgItem(IDC_PREVIEW_IMG)->m_hWnd, FaceImageCB, (ULONG_PTR)this, 2, FaceResultCB, (ULONG_PTR)this);
#endif

	// ��ʾ��ʼֵ
	CString thstr;
	thstr.Format("%.2f%%", m_dThreshold);
	m_pInfoDlg->setThresholdText(thstr.GetString());

	return TRUE;  // ���ǽ��������õ��ؼ������򷵻� TRUE
}

HBRUSH CIDCardFdvDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

	// TODO:  �ڴ˸��� DC ���κ�����
	if (nCtlColor == CTLCOLOR_DLG)  //�Ի�����ɫ
	{
		return (HBRUSH)::GetStockObject(WHITE_BRUSH);//�����Լ����õ�ˢ��
	}

	

	// TODO:  ���Ĭ�ϵĲ������軭�ʣ��򷵻���һ������
	return hbr;
}

// �����Ի���������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ  ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�

void CIDCardFdvDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // ���ڻ��Ƶ��豸������

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// ʹͼ���ڹ����������о���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// ����ͼ��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		//CPaintDC dc(this);
		//CRect rect;
		//GetClientRect(&rect);
		//dc.FillSolidRect(rect, RGB(255, 255, 255));	//��ɫ���ͻ���

		CDialogEx::OnPaint();
	}
}

//���û��϶���С������ʱϵͳ���ô˺���ȡ�ù��
//��ʾ��
HCURSOR CIDCardFdvDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CIDCardFdvDlg::showPreview(IplImage* img)	
{
	UINT ID = IDC_PREVIEW_IMG;				// ID ��Picture Control�ؼ���ID��

	CDC* pDC = GetDlgItem(ID)->GetDC();		// �����ʾ�ؼ��� DC
	HDC hDC = pDC->GetSafeHdc();				// ��ȡ HDC(�豸���) �����л�ͼ����

	CRect rect;

	GetDlgItem(ID)->GetClientRect(&rect);
	int rw = rect.right - rect.left;			// ���ͼƬ�ؼ��Ŀ��͸�
	int rh = rect.bottom - rect.top;

	int iw = img->width;						// ��ȡͼƬ�Ŀ��͸�
	int ih = img->height;
	//TRACE("rect:w,h %d, %d: %d, %d", rw, rh, iw, ih);
	//int tx = (int)(rw - iw)/2;					// ʹͼƬ����ʾλ�������ڿؼ�������
	//int ty = (int)(rh - ih)/2;
	//SetRect( rect, tx, ty, tx+iw, ty+ih );
	//SetRect(rect, 0, 0, rw, rh);	// �����ؼ�
	
	int displayW = rw;					// ���ֿ��߱Ⱥ������ؼ���
	int displayH = rw * ih / iw;
	int displayX = 0;
	int displayY = (int)(rh - displayH) / 2;	// ��ֱ����
	SetRect(rect, displayX, displayY, displayX + displayW, displayY + displayH);

	CvSize ImgSize;
	ImgSize.height = displayW;
	ImgSize.width = displayH;
	CvSize ImgOrgSize;
	ImgOrgSize.width = img->width;
	ImgOrgSize.height = img->height;
	if (m_iplImgDisplay == NULL)
		m_iplImgDisplay = cvCreateImage(ImgSize, IPL_DEPTH_8U, 3);
	
	if (m_iplImgTemp == NULL)
		m_iplImgTemp = cvCreateImage(ImgOrgSize, IPL_DEPTH_8U, 3);

	if (m_bFlip)
		cvFlip(img, m_iplImgTemp, 1);
	else
		cvCopy(img, m_iplImgTemp);
	

	cvResize(m_iplImgTemp, m_iplImgDisplay, INTER_CUBIC);

	CvvImage cimg;
	cimg.CopyOf(m_iplImgDisplay, -1);							// ����ͼƬ
	cimg.DrawToHDC(hDC, &rect);				// ��ͼƬ���Ƶ���ʾ�ؼ���ָ��������

	ReleaseDC(pDC);
}

void CIDCardFdvDlg::drawCameraImage(IplImage* img)
{
	UINT ID = 0;

	CDC* pDC = GetDlgItem(ID)->GetDC();		// �����ʾ�ؼ��� DC
	HDC hDC = pDC->GetSafeHdc();				// ��ȡ HDC(�豸���) �����л�ͼ����

	CRect rect;

	GetDlgItem(ID)->GetClientRect(&rect);
	int rw = rect.right - rect.left;			// ���ͼƬ�ؼ��Ŀ��͸�
	int rh = rect.bottom - rect.top;

	SetRect(rect, 0, 0, rw, rh);	// �����ؼ�

	CvvImage cimg;
	cimg.CopyOf(img, -1);							// ����ͼƬ
	cimg.DrawToHDC(hDC, &rect);				// ��ͼƬ���Ƶ���ʾ�ؼ���ָ��������

	ReleaseDC(pDC);
}

void CIDCardFdvDlg::startCameraThread()
{
	ResetEvent(m_eCameraEnd);

	if (m_thCamera == NULL) {
		m_thCamera = AfxBeginThread(CameraShowThread, this);
		if (NULL == m_thCamera)
		{
			TRACE("�����µ��̳߳�����\n");
			return;
		}
	}
}

void CIDCardFdvDlg::stopCameraThread()
{
	m_bCameraRun = false;
	if (m_thCamera) {
		WaitForSingleObject(m_eCameraEnd, INFINITE);
		ResetEvent(m_eCameraEnd);
	}

	m_thCamera = NULL;
}


void CIDCardFdvDlg::ProcessCapture()
{

#if OPENCV_CAPTURE
	if (!m_bCameraRun)
		return;
#else
	if (!m_bFaceGot)
		return;
#endif

	if (m_bFdvRun)
		return;

	// clear
	KillTimer(CLEAR_INFOIMG_TIMER);
	//m_pInfoDlg->clearCameraImage();
	//m_pInfoDlg->clearIdcardImage();
	//m_pInfoDlg->clearResultIcon();
	//m_pInfoDlg->setResultText("");

	m_thFdv = AfxBeginThread(FdvThread, this);

	if (NULL == m_thFdv)
	{
		TRACE("�����µ��̳߳�����\n");
		return;
	}
}

UINT CameraShowThread(LPVOID lpParam)
{
	CIDCardFdvDlg* pDlg = (CIDCardFdvDlg*)lpParam;
	IplImage* cFrame = NULL;
	CvCapture* pCapture = cvCreateCameraCapture(pDlg->camdevid);
	//int frameW = (int)cvGetCaptureProperty(pCapture, CV_CAP_PROP_FRAME_WIDTH);
	cvSetCaptureProperty(pCapture, CV_CAP_PROP_FRAME_WIDTH, 1280);
	cvSetCaptureProperty(pCapture, CV_CAP_PROP_FRAME_HEIGHT, 720);

	Sleep(500);

	if (pCapture == NULL) {
		SetEvent(pDlg->m_eCameraEnd);
		return 0;
	}

	pDlg->m_bCameraRun = true;
	while (pDlg->m_bCameraRun)
	{
		WINDOWPLACEMENT wpl;

		wpl.length = sizeof(WINDOWPLACEMENT);
		if (pDlg->GetWindowPlacement(&wpl) && (wpl.showCmd == SW_SHOWMINIMIZED)) {
			if (pCapture) {
				cvReleaseCapture(&pCapture);
				pCapture = NULL;
			}
			Sleep(5);
			continue;
		}

		if (pCapture == NULL) {
			pCapture = cvCreateCameraCapture(pDlg->camdevid);
			if (pCapture == NULL) {
				Sleep(2000);
				continue;
			}
			Sleep(200);
		}
		cFrame = cvQueryFrame(pCapture);
		if (!cFrame) {
			//pDlg->MessageBox("��ȡͼ��֡����", "������Ϣ��", MB_ICONERROR | MB_OK);
			continue;
		}


		IplImage* newframe = cvCloneImage(cFrame);
		if (pDlg->m_bCmdCapture) {
			pDlg->m_CaptureImage = cvCloneImage(cFrame);
			pDlg->m_bCmdCapture = false;
			SetEvent(pDlg->m_eCaptureEnd);
		}
		//pDlg->drawCameraImage(pDlg->m_iplImgCameraImg);
		pDlg->showPreview(newframe);
		

		Sleep(7);
		//		int c=cvWaitKey(33);   // not work in MFC proj
		cvReleaseImage(&newframe);
		//		if(c==27)break;
	}

	if (pCapture) {
		cvReleaseCapture(&pCapture);
		pCapture = NULL;
	}

	SetEvent(pDlg->m_eCameraEnd);
	return 0;
}

// capture thread
#include <cpprest/json.h>
UINT FdvThread(LPVOID lpParam)
{
	CIDCardFdvDlg* pDlg = (CIDCardFdvDlg*)lpParam;
	pDlg->m_bFdvRun = true;

	char szPath[1024] = { 0 };
	GetModuleFileName(NULL, szPath, MAX_PATH);
	std::string strModulePath = ExtractFilePath(szPath);

	// init
	CIdCardReader* pIdCardReader = new CIdCardReader();
	int error = pIdCardReader->Init(strModulePath);
	if (error)
	{
		delete pIdCardReader;
		pIdCardReader = NULL;
		switch (error)
		{
		case -1:
			//m_strProcText.SetWindowText("����֤��������װ��ʧ��!");
			break;
		case -2:
			//m_strProcText.SetWindowText("����֤��������֧�����蹦��!");
			break;
		default:
			break;
		}

		pDlg->m_bFdvRun = false;
		return 0;
	}

#if OPENCV_CAPTURE
	pDlg->m_bCmdCapture = true;
	ResetEvent(pDlg->m_eCaptureEnd);
	WaitForSingleObject(pDlg->m_eCaptureEnd, INFINITE);
	pDlg->m_bIsAliveSample = true;
#else
	// ��ȡ����ͷ����
	std::string facedatastr;
	Base64::Decode(pDlg->m_sCaptureBase64, &facedatastr);
	std::vector<uchar> facebuff(facedatastr.begin(), facedatastr.end());
	cv::Mat facemat = cv::imdecode(facebuff, CV_LOAD_IMAGE_UNCHANGED);
	IplImage imgTmp = facemat;
	pDlg->m_CaptureImage = cvCloneImage(&imgTmp);
#endif

	// read idcard
	int usbport = -1;
	for (int port = 1001; port <= 1016; ++port)
	{
		if (pIdCardReader->InitComm(port) == 1) {
			usbport = port;
			break;
		}
	}
	if (usbport == -1) {
		// pDlg->m_strProcText.SetWindowText("����֤����������ʧ��!");
		pIdCardReader->CloseComm();
		pDlg->m_bFdvRun = false;
		return 0;
	}

	int li_ret = pIdCardReader->Authenticate();
	li_ret = 1;
	if (li_ret <= 0) {
		//pDlg->m_strProcText.SetWindowText("����֤��֤ʧ��!");
		pIdCardReader->CloseComm();
		pDlg->m_bFdvRun = false;
		return 0;
	}
	else
	{
		if (1 != pIdCardReader->Read_Content(1)) {
			// pDlg->m_strProcText.SetWindowText("����֤��Ϣ��ȡʧ��!");
			pIdCardReader->CloseComm();
			pDlg->m_bFdvRun = false;
			return 0;
		}

		// ��ȡ����֤��
		long len = pIdCardReader->GetPeopleIDCode(pDlg->m_IdCardId, sizeof(pDlg->m_IdCardId));
		// ��ȡ����֤��Ч��
		len = pIdCardReader->GetStartDate(pDlg->m_IdCardIssuedate, sizeof(pDlg->m_IdCardIssuedate));

		// ��ȡ��Ƭ
		long lenbmp = pIdCardReader->GetPhotoBMP(pDlg->m_IdCardPhoto, sizeof(pDlg->m_IdCardPhoto));
		pIdCardReader->CloseComm();

		pDlg->m_iplImgPhoto = BMP2Ipl((unsigned char*)pDlg->m_IdCardPhoto, lenbmp);
		pDlg->m_pInfoDlg->clearResultIcon();  // ��֤���ͼ�����
		pDlg->m_pInfoDlg->drawCameraImage(pDlg->m_CaptureImage);
		pDlg->m_pInfoDlg->drawIdcardImage(pDlg->m_iplImgPhoto);
		if (false == pDlg->m_bIsAliveSample) {
			// �ǻ��徯��
			pDlg->m_pInfoDlg->setResultTextSize(60);
			pDlg->m_pInfoDlg->setResultText("�ǻ��壡");
			pDlg->setClearTimer();
			pDlg->m_bFdvRun = false;
			return 0;
		}
		pDlg->m_pInfoDlg->setResultTextSize(60);
		pDlg->m_pInfoDlg->setResultText("--%");

		std::vector<uchar> idcardPhoto(pDlg->m_IdCardPhoto, pDlg->m_IdCardPhoto + lenbmp);
		std::vector<uchar> verifyPhotos[1];
#if OPENCV_CAPTURE
		vector<int> param = vector<int>(2);
		//param.push_back(CV_IMWRITE_JPEG_QUALITY);
		//param.push_back(9); //image quality
		//					 //param[0] = CV_IMWRITE_PNG_COMPRESSION;
		//					 //param[1] = 3; //default(3)  0-9.

		Mat frame(pDlg->m_CaptureImage, false);
		cv::imencode(".jpg", frame, verifyPhotos[0], param);
#else
		verifyPhotos[0] = facebuff;
#endif
		std::string retstr = callverify(pDlg->m_cfgUrl,
			pDlg->m_cfgAppId, pDlg->m_cfgApiKey, pDlg->m_cfgSecretKey,
			pDlg->m_macId, pDlg->m_cfgRegisteredNo,
			pDlg->m_IdCardId, pDlg->m_IdCardIssuedate, idcardPhoto, verifyPhotos, 1);

		
		// ���ؽ������
		double retsim = atof(retstr.c_str());
		if (retsim >= pDlg->m_dThreshold ) {
			pDlg->m_pInfoDlg->setResultTextSize(60);
			pDlg->m_pInfoDlg->setResultText(retstr + "%");
			pDlg->m_pInfoDlg->drawResultIcon(pDlg->m_iplImgResultIconRight);
		}
		else if (retsim > 0.0) {
			pDlg->m_pInfoDlg->setResultTextSize(60);
			pDlg->m_pInfoDlg->setResultText(retstr + "%");
			pDlg->m_pInfoDlg->drawResultIcon(pDlg->m_iplImgResultIconWrong);
		}
		else {
			// erro msg
			pDlg->m_pInfoDlg->setResultTextSize(20);
			pDlg->m_pInfoDlg->setResultText(retstr);
			pDlg->m_pInfoDlg->drawResultIcon(pDlg->m_iplImgResultIconWrong);
		}

		pDlg->setClearTimer();
	}

	pDlg->m_bFdvRun = false;
	return 0;
}

void CIDCardFdvDlg::setClearTimer()
{
	KillTimer(CLEAR_INFOIMG_TIMER);
	SetTimer(CLEAR_INFOIMG_TIMER, 1000 * 10, NULL);
}

//==============================================
#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <cpprest/json.h>
#include <openssl/sha.h>
using namespace web;
using namespace web::http;
using namespace web::http::client;
std::string callverifySub(utility::string_t& url, web::json::value& postParameters);

// ---- sha256ժҪ��ϣ ---- //    
void sha256(const std::string &srcStr, std::string &encodedStr, std::string &encodedHexStr)
{
	// ����sha256��ϣ    
	unsigned char mdStr[33] = { 0 };
	SHA256((const unsigned char *)srcStr.c_str(), srcStr.length(), mdStr);

	// ��ϣ����ַ���    
	encodedStr = std::string((const char *)mdStr);
	// ��ϣ���ʮ�����ƴ� 32�ֽ�    
	char buf[65] = { 0 };
	char tmp[3] = { 0 };
	for (int i = 0; i < 32; i++)
	{
		sprintf_s(tmp, "%02x", mdStr[i]);
		strcat_s(buf, tmp);
	}
	buf[32] = '\0'; // ���涼��0����32�ֽڽض�    
	encodedHexStr = std::string(buf);
}

std::string callverify(std::string url, std::string appId, std::string apiKey, std::string secretKey,
				std::string macId, std::string registeredNo,
				std::string idcardId, std::string idcardIssuedate,
				std::vector<uchar> idcardPhoto, std::vector<uchar> verifyPhotos[], int verifyPhotoNum)
{
	json::value verify_json = json::value::object();

	std::string shastr="";
	// uuid
	CString struuid(L"error");
	RPC_CSTR guidStr;
	GUID guid;
	if (UuidCreateSequential(&guid) == RPC_S_OK)
	{
		if (UuidToString(&guid, &guidStr) == RPC_S_OK)
		{
			struuid = (LPTSTR)guidStr;
			RpcStringFree(&guidStr);
		}
	}

	verify_json[U("appId")] = json::value::string(utility::conversions::to_string_t(appId));
	verify_json[U("apiKey")] = json::value::string(utility::conversions::to_string_t(apiKey));
	verify_json[U("secretKey")] = json::value::string(utility::conversions::to_string_t(secretKey));
	verify_json[U("uuid")] = json::value::string(utility::conversions::to_string_t(std::string(struuid)));
	shastr += appId;
	shastr += apiKey;
	shastr += secretKey;
	shastr += struuid;

	//std::string macId = "00:09:4c:53:78:2c"; // test
	verify_json[U("MacId")] = json::value::string(utility::conversions::to_string_t(macId));
	shastr += macId;

	//std::string registerno = "9081"; // test
	verify_json[U("RegisteredNo")] = json::value::string(utility::conversions::to_string_t(registeredNo));
	shastr += registeredNo;

	verify_json[U("idcard_id")] = json::value::string(utility::conversions::to_string_t(idcardId));
	verify_json[U("idcard_issuedate")] = json::value::string(utility::conversions::to_string_t(idcardIssuedate));
	shastr += idcardId;
	shastr += idcardIssuedate;

	utility::string_t b64str = U("data:image/bmp;base64,") + utility::conversions::to_base64(idcardPhoto);
	verify_json[U("idcard_photo")] = json::value::string(b64str);
	shastr += utility::conversions::to_utf8string(b64str);

	verify_json[U("verify_photos")] = json::value::array();
	for (int i = 0; i < verifyPhotoNum; i++) {
		utility::string_t b64str = U("data:image/jpeg;base64,") + utility::conversions::to_base64(verifyPhotos[i]);
		verify_json[U("verify_photos")][i] = json::value::string(b64str);
		shastr += utility::conversions::to_utf8string(b64str);
	}

	std::string shaEncoded;
	std::string shaEncodedHex;
	sha256(shastr, shaEncoded, shaEncodedHex);
	verify_json[U("checksum")] = json::value::string(utility::conversions::to_string_t(shaEncodedHex));

	return callverifySub(utility::conversions::to_string_t(url), verify_json);
}
std::string callverifySub(utility::string_t& url, web::json::value& postParameters)
{
	std::string ret;

	http::uri uri = http::uri(url);
	http_client client(uri);
	web::http::http_request postRequest;
	postRequest.set_method(methods::POST);
	postRequest.set_body(postParameters);

	try {
		Concurrency::task<web::http::http_response> getTask = client.request(postRequest);
		http_response resp = getTask.get();
		const json::value& jval = resp.extract_json().get();
		const web::json::object& jobj = jval.as_object();
		if (jval.has_field(U("Err_no"))) {
			int err_no = jobj.at(L"Err_no").as_integer();
			if (err_no != 0) {
				utility::string_t err_msg = jobj.at(L"Err_msg").as_string();
				ret = utility::conversions::to_utf8string(err_msg);
				//AfxMessageBox(err_msg.c_str());
				//printf("%ls\n", err_msg.c_str());
			}
			else {
				double sim = jobj.at(L"Similarity").as_double() * 100;
				CString csTemp;
				csTemp.Format("%.2f", sim);
				ret = csTemp.GetString();
			}
		}
	}
	catch (...) {
		ret = "network error!";
		//AfxMessageBox(_T("network error!"));
		//printf("network error!\n");
	}

	return ret;
}

//==============================================


BOOL CIDCardFdvDlg::DestroyWindow()
{
	// TODO: Add your specialized code here and/or call the base class
#if OPENCV_CAPTURE
	stopCameraThread();
	Sleep(20);
#else
	m_pInfoDlg->ShowWindow(SW_HIDE);
	ZZStopGetFace(GetDlgItem(IDC_PREVIEW_IMG)->m_hWnd);
	ZZCloseHideVideo(GetDlgItem(IDC_PREVIEW_IMG)->m_hWnd);
	ZZCloseVideo(GetDlgItem(IDC_PREVIEW_IMG)->m_hWnd);
	ZZCloseHideDevice(GetDlgItem(IDC_PREVIEW_IMG)->m_hWnd);
	ZZCloseDevice(GetDlgItem(IDC_PREVIEW_IMG)->m_hWnd);
	ZZDeinitFaceMgr();
#endif
	if (m_iplImgDisplay != NULL) {
		cvReleaseImage(&m_iplImgDisplay);
		m_iplImgDisplay = NULL;
	}

	if (m_iplImgTemp != NULL) {
		cvReleaseImage(&m_iplImgTemp);
		m_iplImgTemp = NULL;
	}
	
	if (m_hBIconCamera != NULL) {
		DeleteObject(m_hBIconCamera);
		m_hBIconCamera = NULL;
	}

	if (m_iplImgCameraImg != NULL) {
		cvReleaseImage(&m_iplImgCameraImg);
		m_iplImgCameraImg = NULL;
	}

	if (m_iplImgResultIconRight != NULL) {
		cvReleaseImage(&m_iplImgResultIconRight);
		m_iplImgResultIconRight = NULL;
	}

	if (m_iplImgResultIconWrong != NULL) {
		cvReleaseImage(&m_iplImgResultIconWrong);
		m_iplImgResultIconWrong = NULL;
	}
	
	if (m_CaptureImage != NULL) {
		cvReleaseImage(&m_CaptureImage);
		m_CaptureImage = NULL;
	}

	if (m_iplImgPhoto != NULL) {
		cvReleaseImage(&m_iplImgPhoto);
		m_iplImgPhoto = NULL;
	}

	if (m_pInfoDlg) {
		delete m_pInfoDlg;
		m_pInfoDlg = NULL;
	}

	PostQuitMessage(0);
	return CDialogEx::DestroyWindow();
}

void CIDCardFdvDlg::OnClose()
{
	// TODO: �ڴ�������Ϣ������������/�����Ĭ��ֵ

	CDialogEx::OnClose();
}




void CIDCardFdvDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
	// TODO: �ڴ�������Ϣ������������/�����Ĭ��ֵ
	// test code
	ProcessCapture();
	CDialogEx::OnLButtonDown(nFlags, point);
}


void CIDCardFdvDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: �ڴ�������Ϣ������������/�����Ĭ��ֵ
	switch (nIDEvent)
	{
	case CLEAR_INFOIMG_TIMER:
		m_pInfoDlg->clearCameraImage();
		m_pInfoDlg->clearIdcardImage();
		m_pInfoDlg->clearResultIcon();
		m_pInfoDlg->setResultText("");
		KillTimer(CLEAR_INFOIMG_TIMER);
		break;
	}
	CDialogEx::OnTimer(nIDEvent);
}


void CIDCardFdvDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	// TODO: �ڴ�������Ϣ������������/�����Ĭ��ֵ
	if ((nID & 0xFFF0) == SC_MOVE)
		return;

	CDialogEx::OnSysCommand(nID, lParam);
}