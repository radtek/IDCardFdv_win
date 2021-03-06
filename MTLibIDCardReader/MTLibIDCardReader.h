#pragma once

int __stdcall LoadIDCardReader();
int __stdcall OpenIDCardReader();
int __stdcall Authenticate_Idcard();
int __stdcall Authenticate_Content(int Active = 1);
int __stdcall IDCardReader_GetPeopleIDCode(char* pData, unsigned int iDataLen);
int __stdcall IDCardReader_GetStartDate(char* pData, unsigned int iDataLen);
int __stdcall IDCardReader_GetPhotoBMP(char* pData, unsigned int iDataLen);
int __stdcall CloseIDCardReader();
int __stdcall UnloadIDCardReader();