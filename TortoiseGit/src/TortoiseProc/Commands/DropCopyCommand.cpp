// TortoiseGit - a Windows shell extension for easy version control

// Copyright (C) 2008-2013, 2015-2016 - TortoiseGit
// Copyright (C) 2007-2008 - TortoiseSVN

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
#include "DropCopyCommand.h"
#include "SysProgressDlg.h"
#include "MessageBox.h"
#include "RenameDlg.h"
#include "Git.h"
#include "ShellUpdater.h"

bool DropCopyCommand::Execute()
{
	CString sDroppath = parser.GetVal(L"droptarget");
	if (CTGitPath(sDroppath).IsAdminDir())
	{
		MessageBox(hwndExplorer, L"Can't drop to .git repository directory\n", L"TortoiseGit", MB_OK | MB_ICONERROR);
		return FALSE;
	}
	unsigned long count = 0;

	CString sNewName;
	pathList.RemoveAdminPaths();
	if (parser.HasKey(L"rename") && pathList.GetCount() == 1)
	{
		// ask for a new name of the source item
		do
		{
			CRenameDlg renDlg;
			renDlg.m_windowtitle.LoadString(IDS_PROC_COPYRENAME);
			renDlg.m_name = pathList[0].GetFileOrDirectoryName();
			if (renDlg.DoModal() != IDOK)
				return FALSE;
			sNewName = renDlg.m_name;
		} while (sNewName.IsEmpty() || PathFileExists(sDroppath + L'\\' + sNewName));
	}
	CSysProgressDlg progress;
	progress.SetTitle(IDS_PROC_COPYING);
	progress.SetAnimation(IDR_MOVEANI);
	progress.SetTime(true);
	progress.ShowModeless(CWnd::FromHandle(hwndExplorer));
	for (int nPath = 0; nPath < pathList.GetCount(); ++nPath)
	{
		const CTGitPath& sourcePath = orgPathList[nPath];

		CTGitPath fullDropPath(sDroppath);

		if (sNewName.IsEmpty())
			fullDropPath.AppendPathString(sourcePath.GetFileOrDirectoryName());
		else
			fullDropPath.AppendPathString(sNewName);

		// Check for a drop-on-to-ourselves
		if (sourcePath.IsEquivalentTo(fullDropPath))
		{
			// Offer a rename
			progress.Stop();
			CRenameDlg dlg;
			dlg.m_windowtitle.Format(IDS_PROC_NEWNAMECOPY, (LPCTSTR)sourcePath.GetUIFileOrDirectoryName());
			if (dlg.DoModal() != IDOK)
				return FALSE;
			// rebuild the progress dialog
			progress.EnsureValid();
			progress.SetTitle(IDS_PROC_COPYING);
			progress.SetAnimation(IDR_MOVEANI);
			progress.SetTime(true);
			progress.SetProgress(count, pathList.GetCount());
			progress.ShowModeless(CWnd::FromHandle(hwndExplorer));
			// Rebuild the destination path, with the new name
			fullDropPath.SetFromUnknown(sDroppath);
			fullDropPath.AppendPathString(dlg.m_name);
		}

		if( CopyFile( sourcePath.GetWinPath(), fullDropPath.GetWinPath(), true))
		{
			CString ProjectTopDir;
			if(fullDropPath.HasAdminDir(&ProjectTopDir))
			{
				g_Git.SetCurrentDir(ProjectTopDir);
				SetCurrentDirectory(ProjectTopDir);
				CString cmd;
				cmd = L"git.exe add -- \"";

				CString path;
				path=fullDropPath.GetGitPathString().Mid(ProjectTopDir.GetLength());
				if (!path.IsEmpty() && (path[0] == L'\\' || path[0] == L'/'))
					path = path.Mid(1);
				cmd += path;
				cmd += L'"';

				CString output;
				if (g_Git.Run(cmd, &output, CP_UTF8))
					MessageBox(hwndExplorer, output, L"TortoiseGit", MB_OK | MB_ICONERROR);
				else
					CShellUpdater::Instance().AddPathForUpdate(fullDropPath);
			}

		}else
		{
			CString str;
			str += L"Copy file fail:";
			str+=sourcePath.GetWinPath();

			MessageBox(hwndExplorer, str, L"TortoiseGit", MB_OK | MB_ICONERROR);
		}

		++count;
		if (progress.IsValid())
		{
			progress.FormatPathLine(1, IDS_PROC_COPYINGPROG, sourcePath.GetWinPath());
			progress.FormatPathLine(2, IDS_PROC_CPYMVPROG2, fullDropPath.GetWinPath());
			progress.SetProgress(count, pathList.GetCount());
		}
		if ((progress.IsValid())&&(progress.HasUserCancelled()))
		{
			CMessageBox::Show(hwndExplorer, IDS_USERCANCELLED, IDS_APPNAME, MB_ICONINFORMATION);
			return false;
		}
	}

	return true;
}
