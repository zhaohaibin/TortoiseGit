// TortoiseGit - a Windows shell extension for easy version control

// Copyright (C) 2009-2013, 2015-2016 - TortoiseGit

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
#include "stdafx.h"
#include "SVNRebaseCommand.h"

#include "SysProgressDlg.h"
#include "ProgressDlg.h"
#include "MessageBox.h"
#include "Git.h"
#include "RebaseDlg.h"
#include "AppUtils.h"

bool SVNRebaseCommand::Execute()
{
	bool isStash = false;

	if(!g_Git.CheckCleanWorkTree())
	{
		if (CMessageBox::Show(hwndExplorer, g_Git.m_CurrentDir + L"\r\n" + CString(MAKEINTRESOURCE(IDS_ERROR_NOCLEAN_STASH)), L"TortoiseGit", 1, IDI_QUESTION, CString(MAKEINTRESOURCE(IDS_STASHBUTTON)), CString(MAKEINTRESOURCE(IDS_ABORTBUTTON))) == 1)
		{
			CSysProgressDlg sysProgressDlg;
			sysProgressDlg.SetTitle(CString(MAKEINTRESOURCE(IDS_APPNAME)));
			sysProgressDlg.SetLine(1, CString(MAKEINTRESOURCE(IDS_PROC_STASHRUNNING)));
			sysProgressDlg.SetLine(2, CString(MAKEINTRESOURCE(IDS_PROGRESSWAIT)));
			sysProgressDlg.SetShowProgressBar(false);
			sysProgressDlg.SetCancelMsg(IDS_PROGRS_INFOFAILED);
			sysProgressDlg.ShowModeless((HWND)nullptr, true);

			CString out;
			if (g_Git.Run(L"git.exe stash", &out, CP_UTF8))
			{
				sysProgressDlg.Stop();
				MessageBox(hwndExplorer, out, L"TortoiseGit", MB_OK | MB_ICONERROR);
				return false;
			}
			sysProgressDlg.Stop();
			isStash = true;
		}
		else
		{
			return false;
		}
	}

	CRebaseDlg dlg;

//	dlg.m_PreCmd=L"git.exe svn fetch";

	CString out, err;
	if (!g_Git.Run(L"git.exe config svn-remote.svn.fetch", &out, &err, CP_UTF8))
	{
		int start = out.Find(L':');
		if( start >=0 )
			out=out.Mid(start);

		if (CStringUtils::StartsWith(out, L":refs"))
			out=out.Mid(6);

		start = 0;
		out = out.Tokenize(L"\n", start);
	}
	else
	{
		MessageBox(hwndExplorer, out + L'\n' + err, L"TortoiseGit", MB_OK | MB_ICONERROR);
		return false;
	}

	dlg.m_Upstream=out;

	CGitHash UpStreamOldHash,HeadHash,UpStreamNewHash;
	if (g_Git.GetHash(UpStreamOldHash, out))
	{
		MessageBox(hwndExplorer, g_Git.GetGitLastErr(L"Could not get hash of SVN branch."), L"TortoiseGit", MB_ICONERROR);
		return false;
	}
	if (g_Git.GetHash(HeadHash, L"HEAD"))
	{
		MessageBox(hwndExplorer, g_Git.GetGitLastErr(L"Could not get HEAD hash."), L"TortoiseGit", MB_ICONERROR);
		return false;
	}
	CProgressDlg progress;
	progress.m_GitCmd = L"git.exe svn fetch";
	progress.m_AutoClose = AUTOCLOSE_IF_NO_ERRORS;

	if(progress.DoModal()!=IDOK)
		return false;

	if(progress.m_GitStatus)
		return false;

	if (g_Git.GetHash(UpStreamNewHash, out))
	{
		MessageBox(hwndExplorer, g_Git.GetGitLastErr(L"Could not get upstream hash after fetching."), L"TortoiseGit", MB_ICONERROR);
		return false;
	}

	//everything updated
	if(UpStreamNewHash==HeadHash)
	{
		MessageBox(hwndExplorer, g_Git.m_CurrentDir + L"\r\n" + CString(MAKEINTRESOURCE(IDS_PROC_EVERYTHINGUPDATED)), L"TortoiseGit", MB_OK | MB_ICONQUESTION);
		if(isStash)
			askIfUserWantsToStashPop();

		return true;
	}

	//fast forward;
	if (g_Git.IsFastForward(L"HEAD", out))
	{
		CProgressDlg progressReset;
		CString cmd;
		cmd.Format(L"git.exe reset --hard %s --", (LPCTSTR)out);
		progressReset.m_GitCmd = cmd;
		progressReset.m_AutoClose = AUTOCLOSE_IF_NO_ERRORS;

		if (progressReset.DoModal() != IDOK)
			return false;
		else
		{
			MessageBox(hwndExplorer, g_Git.m_CurrentDir + L"\r\n" + CString(MAKEINTRESOURCE(IDS_PROC_FASTFORWARD)) + L":\n" + progressReset.m_LogText, L"TortoiseGit", MB_OK | MB_ICONQUESTION);
			if(isStash)
				askIfUserWantsToStashPop();

			return true;
		}
	}

	dlg.m_PostButtonTexts.Add(CString(MAKEINTRESOURCE(IDS_MENULOG)));
	//need rebase
	INT_PTR response = dlg.DoModal();
	if (response == IDOK || response == IDC_REBASE_POST_BUTTON)
	{
		if(isStash)
			askIfUserWantsToStashPop();
		if (response == IDC_REBASE_POST_BUTTON)
		{
			CString cmd = L"/command:log";
			cmd += L" /path:\"" + g_Git.m_CurrentDir + L'"';
			CAppUtils::RunTortoiseGitProc(cmd);
		}
		return true;
	}
	return false;
}

void SVNRebaseCommand::askIfUserWantsToStashPop()
{
	if (MessageBox(hwndExplorer, g_Git.m_CurrentDir + L"\r\n" + CString(MAKEINTRESOURCE(IDS_DCOMMIT_STASH_POP)), L"TortoiseGit", MB_YESNO | MB_ICONINFORMATION) == IDYES)
		CAppUtils::StashPop();
}
