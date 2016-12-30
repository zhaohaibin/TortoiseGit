/************************************************************************************* 
  This file is a part of CrashRpt library.

  Copyright (c) 2003, Michael Carruth
  All rights reserved.

  Adjusted by Sven Strickroth <email@cs-ware.de>, 2011, 2015
   * make it work with no attachments
   * added flag to show mail compose dialog
   * make it work with 32-64bit inconsistencies (http://msdn.microsoft.com/en-us/library/dd941355.aspx)
   * auto extract filenames of attachments
   * make work with multiple recipients (to|cc)

  Redistribution and use in source and binary forms, with or without modification, 
  are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright notice, this 
     list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above copyright notice, 
     this list of conditions and the following disclaimer in the documentation 
     and/or other materials provided with the distribution.

   * Neither the name of the author nor the names of its contributors 
     may be used to endorse or promote products derived from this software without 
     specific prior written permission.


  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY 
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
  SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED 
  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT 
  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************************/

///////////////////////////////////////////////////////////////////////////////
//
//  Module: MailMsg.cpp
//
//    Desc: See MailMsg.h
//
// Copyright (c) 2003 Michael Carruth
//
///////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MailMsg.h"
#include "UnicodeUtils.h"
#include "StringUtils.h"

CMailMsg::CMailMsg()
{
	m_hMapi					= nullptr;
	m_lpMapiSendMail		= nullptr;
	m_bReady				= FALSE;
	m_bShowComposeDialog	= FALSE;
}

CMailMsg::~CMailMsg()
{
	if (m_bReady)
		MAPIFinalize();
}


void CMailMsg::SetFrom(const CString& sAddress, const CString& sName)
{  
	m_from.email = CUnicodeUtils::GetUTF8(L"SMTP:" + sAddress);
	m_from.name = CUnicodeUtils::GetUTF8(sName);
}

static void addAdresses(std::vector<MailAddress>& recipients, const CString& sAddresses)
{
	int start = 0;
	while (start >= 0)
	{
		CString address = sAddresses.Tokenize(L";", start);
		CString name;
		CStringUtils::ParseEmailAddress(address, address, &name);
		if (address.IsEmpty())
			continue;
		recipients.emplace_back(L"SMTP:" + address, name);
	}
}

void CMailMsg::SetTo(const CString& sAddresses)
{
	addAdresses(m_to, sAddresses);
}

void CMailMsg::SetSubject(const CString& sSubject)
{
	m_sSubject = CUnicodeUtils::GetUTF8(sSubject);
}

void CMailMsg::SetMessage(const CString& sMessage)
{
	m_sMessage = CUnicodeUtils::GetUTF8(sMessage);
};

void CMailMsg::SetShowComposeDialog(BOOL showComposeDialog)
{
	m_bShowComposeDialog = showComposeDialog;
};

void CMailMsg::SetCC(const CString& sAddresses)
{
	addAdresses(m_cc, sAddresses);
}

void CMailMsg::AddAttachment(const CString& sAttachment, CString sTitle)
{
	if (sTitle.IsEmpty())
	{
		int position = sAttachment.ReverseFind(L'\\');
		if(position >=0)
		{
			sTitle = sAttachment.Mid(position+1);
		}
		else
		{
			sTitle = sAttachment;
		}
	}
	m_attachments[(LPCSTR)CUnicodeUtils::GetUTF8(sAttachment)] = CUnicodeUtils::GetUTF8(sTitle);
}

BOOL CMailMsg::DetectMailClient(CString& sMailClientName)
{
	CRegKey regKey;
	TCHAR buf[1024] = L"";
	ULONG buf_size = 0;
	LONG lResult;

	lResult = regKey.Open(HKEY_CURRENT_USER, L"SOFTWARE\\Clients\\Mail", KEY_READ);
	if(lResult!=ERROR_SUCCESS)
	{
		lResult = regKey.Open(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Clients\\Mail", KEY_READ);
	}

	if(lResult==ERROR_SUCCESS)
	{
		buf_size = 1023;
#pragma warning(disable:4996)
		LONG result = regKey.QueryValue(buf, L"", &buf_size);
#pragma warning(default:4996)
		if(result==ERROR_SUCCESS)
		{
			sMailClientName = buf;
			return TRUE;
		}
		regKey.Close();
	}
	else
	{
		sMailClientName = "Not Detected";
	}

	return FALSE;
}

BOOL CMailMsg::MAPIInitialize()
{
	// Determine if there is default email program

	CString sMailClientName;
	if(!DetectMailClient(sMailClientName))
	{
		m_sErrorMsg = L"Error detecting E-mail client";
		return FALSE;
	}
	else
	{
		m_sErrorMsg = L"Detected E-mail client " + sMailClientName;
	}

	// Load MAPI.dll

	m_hMapi = AtlLoadSystemLibraryUsingFullPath(L"mapi32.dll");
	if (!m_hMapi)
	{
		m_sErrorMsg = L"Error loading mapi32.dll";
		return FALSE;
	}

	m_lpMapiSendMail = (LPMAPISENDMAIL)::GetProcAddress(m_hMapi, "MAPISendMail");

	m_bReady = !!m_lpMapiSendMail;

	if(!m_bReady)
	{
		m_sErrorMsg = L"Not found required function entries in mapi32.dll";
	}

	return m_bReady;
}

void CMailMsg::MAPIFinalize()
{
	::FreeLibrary(m_hMapi);
}

CString CMailMsg::GetEmailClientName()
{
	return m_sEmailClientName;
}

BOOL CMailMsg::Send()
{
	if (!m_lpMapiSendMail)
		return FALSE;

	TStrStrMap::iterator	p;
	int						nIndex = 0;
	MapiRecipDesc*			pRecipients = nullptr;
	int						nAttachments = 0;
	MapiFileDesc*			pAttachments = nullptr;
	ULONG					status = 0;
	MapiMessage				message;

	if(!m_bReady && !MAPIInitialize())
		return FALSE;

	pRecipients = new MapiRecipDesc[1 + m_to.size() + m_cc.size()];
	if(!pRecipients)
	{
		m_sErrorMsg = L"Error allocating memory";
		return FALSE;
	}

	nAttachments = (int)m_attachments.size();
	if (nAttachments)
	{
		pAttachments = new MapiFileDesc[nAttachments];
		if(!pAttachments)
		{
			m_sErrorMsg = L"Error allocating memory";
			delete[] pRecipients;
			return FALSE;
		}
	}

	// set from
	pRecipients[0].ulReserved = 0;
	pRecipients[0].ulRecipClass = MAPI_ORIG;
	pRecipients[0].lpszAddress = (LPSTR)m_from.email.c_str();
	pRecipients[0].lpszName = (LPSTR)m_from.name.c_str();
	pRecipients[0].ulEIDSize = 0;
	pRecipients[0].lpEntryID = nullptr;

	// add to recipients
	for (size_t i = 0; i < m_to.size(); ++i)
	{
		++nIndex;
		pRecipients[nIndex].ulReserved = 0;
		pRecipients[nIndex].ulRecipClass = MAPI_TO;
		pRecipients[nIndex].lpszAddress = (LPSTR)m_to.at(i).email.c_str();
		pRecipients[nIndex].lpszName = (LPSTR)m_to.at(i).name.c_str();
		pRecipients[nIndex].ulEIDSize = 0;
		pRecipients[nIndex].lpEntryID = nullptr;
	}

	// add cc receipients
	for (size_t i = 0; i < m_cc.size(); ++i)
	{
		++nIndex;
		pRecipients[nIndex].ulReserved = 0;
		pRecipients[nIndex].ulRecipClass = MAPI_CC;
		pRecipients[nIndex].lpszAddress = (LPSTR)m_cc.at(i).email.c_str();
		pRecipients[nIndex].lpszName = (LPSTR)m_cc.at(i).name.c_str();
		pRecipients[nIndex].ulEIDSize = 0;
		pRecipients[nIndex].lpEntryID = nullptr;
	}

	// add attachments
	for (p = m_attachments.begin(), nIndex = 0; p != m_attachments.end(); ++p, ++nIndex)
	{
		pAttachments[nIndex].ulReserved		= 0;
		pAttachments[nIndex].flFlags		= 0;
		pAttachments[nIndex].nPosition		= 0xFFFFFFFF;
		pAttachments[nIndex].lpszPathName	= (LPSTR)p->first.c_str();
		pAttachments[nIndex].lpszFileName	= (LPSTR)p->second.c_str();
		pAttachments[nIndex].lpFileType		= nullptr;
	}

	message.ulReserved						= 0;
	message.lpszSubject						= (LPSTR)m_sSubject.c_str();
	message.lpszNoteText					= (LPSTR)m_sMessage.c_str();
	message.lpszMessageType					= nullptr;
	message.lpszDateReceived				= nullptr;
	message.lpszConversationID				= nullptr;
	message.flFlags							= 0;
	message.lpOriginator					= pRecipients;
	message.nRecipCount						= (ULONG)(m_to.size() + m_cc.size());
	message.lpRecips						= &pRecipients[1];
	message.nFileCount						= nAttachments;
	message.lpFiles							= nAttachments ? pAttachments : nullptr;

	status = m_lpMapiSendMail(NULL, 0, &message, (m_bShowComposeDialog ? MAPI_DIALOG : 0) | MAPI_LOGON_UI, 0);

	if(status!=SUCCESS_SUCCESS)
	{
		m_sErrorMsg.Format(L"MAPISendMail has failed with code %lu.", status);
	}

	delete[] pRecipients;

	delete[] pAttachments;

	return (SUCCESS_SUCCESS == status);
}

