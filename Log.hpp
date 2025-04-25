#pragma once

#include <ntifs.h>

//************************************
// Method:    InitializeLogFile
// FullName:  InitializeLogFile
// Access:    public 
// Returns:   NTSTATUS
// Qualifier:
// Parameter: IN CONST PWCHAR FilePath
// ��ʼ��Log�ļ��Ĳ���
// ��Ҫע����ǣ�������ļ�·����������ҪӦ�ò��ṩ��
//************************************
NTSTATUS InitializeLogFile(IN CONST PWCHAR FilePath);

void LogInfo(const char* sFormat, ...);

#define ENABLE_LOG 1

#ifdef ENABLE_LOG

#define _LOGINFO_RAW(...) LogInfo(__VA_ARGS__)

///
/// Log error with message.
///
/// @param eStatus - NTSTATUS for message.
/// @param ... [opt] - additional message: format + parameters.
/// ��鷵��ֵ�Ĵ�����־
///
#define LOGERROR(eStatus, ...) (_LOGINFO_RAW("[ERROR] Line: %s(%d). Errno: %08X. ", __FILE__, __LINE__, eStatus), _LOGINFO_RAW(__VA_ARGS__), (eStatus))

// ������󷵻�ֵ��־
#define IFERR_RET(x, ...) {const NTSTATUS _ns = (x); if(!NT_SUCCESS(_ns)) {return LOGERROR(_ns, __VA_ARGS__);}}

// �����У����Ϣ�Ĵ�����־
// ֱ��Log��Ϣ
#define LOGINFO(...) (_LOGINFO_RAW("[LOGINFO] Line: %s(%d). ", __FILE__, __LINE__), _LOGINFO_RAW(__VA_ARGS__))


///
/// if x (NTSTATUS) is error, logs error.
///
/// @param x - expression returned NTSTATUS.
/// @param ... [opt] - additional message: format + parameters.
///
#define IFERR_LOG(x, ...) {const NTSTATUS _ns = (x); if(!NT_SUCCESS(_ns)) {LOGERROR(_ns, __VA_ARGS__);}}

///
/// if x (NTSTATUS) is error, logs error.
///
/// @param x - expression returned NTSTATUS.
/// @param ... [opt] - additional message: format + parameters.
///
#define IFERR_LOG_RET(x, ...) {const NTSTATUS _ns = (x); if(!NT_SUCCESS(_ns)) {LOGERROR(_ns, __VA_ARGS__);return;}}

///
/// if x (NTSTATUS) is error, return this error.
///
/// @param x - expression returned NTSTATUS.
///
#define IFERR_RET_NOLOG(x) {const NTSTATUS _ns = (x); if(!NT_SUCCESS(_ns)) {return _ns;}}


#else
#define LOGERROR(eStatus) (eStatus)
#define IFERR_RET(x, ...) {const NTSTATUS _ns = (x); if(!NT_SUCCESS(_ns)) {return _ns;}}
#endif