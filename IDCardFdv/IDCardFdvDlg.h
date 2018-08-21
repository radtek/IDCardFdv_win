
// IDCardFdvDlg.h : ͷ�ļ�
//

#pragma once

#include "CvvImage.h"
#include "InfoDlg.h"

// CIDCardFdvDlg �Ի���
class CIDCardFdvDlg : public CDialogEx
{
// ����
public:
	CIDCardFdvDlg(CWnd* pParent = NULL);	// ��׼���캯��

// �Ի�������
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_IDCARDFDV_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��


// ʵ��
protected:
	HICON m_hIcon;

	// ���ɵ���Ϣӳ�亯��
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()


public:
	std::string m_strModulePath;

	std::string m_macId;

	//config data
	std::string m_cfgAppId;
	std::string m_cfgApiKey;
	std::string m_cfgSecretKey;
	std::string m_cfgUrl;
	std::string m_cfgRegisteredNo;

	int camdevid;
	bool m_bCameraRun;
	CWinThread* m_thCamera;
	CEvent m_eCameraEnd;
	bool m_bFlip;
	IplImage* m_iplImgDisplay;
	IplImage* m_iplImgTemp;

	// fdv
	CWinThread* m_thFdv;
	bool m_bFdvRun;

	// capture
	bool m_bCmdCapture;
	IplImage* m_CaptureImage;
	CEvent m_eCaptureEnd;
	std::string m_sCaptureBase64;
	bool m_bFaceGot;
	bool m_bIsAliveSample;

	// idcard
	char m_IdCardId[256];
	char m_IdCardIssuedate[256];
	char m_IdCardPhoto[102400];
	IplImage* m_iplImgPhoto;

	// info panel
	CInfoDlg* m_pInfoDlg;
	HBITMAP m_hBIconCamera;	// test
	IplImage* m_iplImgCameraImg;
	IplImage* m_iplImgResultIconRight;
	IplImage* m_iplImgResultIconWrong;
	double m_dThreshold;
public:
	void showPreview(IplImage* img);
	void startCameraThread();
	void stopCameraThread();

	void drawCameraImage(IplImage* img);

	void ProcessCapture();
	void setClearTimer();

	virtual BOOL DestroyWindow();
	afx_msg void OnClose();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
};