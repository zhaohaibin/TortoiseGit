// TortoiseGit - a Windows shell extension for easy version control

// Copyright (C) 2008, 2014 - TortoiseSVN
// Copyright (C) 2008-2016 - TortoiseGit

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
#include "GitStatusListCtrl.h"
#include "TGitPath.h"
#include "ColumnManager.h"

// sorter utility class
CSorter::CSorter ( ColumnManager* columnManager
									, int sortedColumn
									, bool ascending)
									: columnManager (columnManager)
									, sortedColumn (sortedColumn)
									, ascending (ascending)
{
}

bool CSorter::operator() (const CTGitPath* entry1 , const CTGitPath* entry2) const
{
#define SGN(x) ((x)==0?0:((x)>0?1:-1))

	int result = 0;
	switch (sortedColumn)
	{
	case 7: // File size
		{
			if (result == 0)
			{
				__int64 fileSize1 = entry1->IsDirectory() ? 0 : entry1->GetFileSize();
				__int64 fileSize2 = entry2->IsDirectory() ? 0 : entry2->GetFileSize();

				result = int(fileSize1 - fileSize2);
			}
			break;
		}
	case 6: //Last Modification Date
		{
			if (result == 0)
			{
				__int64 writetime1 = entry1->GetLastWriteTime();
				__int64 writetime2 = entry2->GetLastWriteTime();

				FILETIME* filetime1 = (FILETIME*)(__int64*)&writetime1;
				FILETIME* filetime2 = (FILETIME*)(__int64*)&writetime2;

				result = CompareFileTime(filetime1, filetime2);
			}
			break;
		}
	case 5: //Del Number
		{
			if (result == 0)
			{
//				result = entry1->lock_comment.CompareNoCase(entry2->lock_comment);
				result = A2L(entry1->m_StatDel)-A2L(entry2->m_StatDel);
			}
			break;
		}
	case 4: //Add Number
		{
			if (result == 0)
			{
				//result = entry1->lock_owner.CompareNoCase(entry2->lock_owner);
				result = A2L(entry1->m_StatAdd)-A2L(entry2->m_StatAdd);
			}
			break;
		}

	case 3: // Status
		{
			if (result == 0)
			{
				result = entry1->GetActionName(entry1->m_Action).CompareNoCase(entry2->GetActionName(entry2->m_Action));
			}
			break;
		}
	case 2: //Ext file
		{
			if (result == 0)
			{
				result = entry1->GetFileExtension().CompareNoCase(entry2->GetFileExtension());
			}
			break;
		}
	case 1: // File name
		{
			if (result == 0)
			{
				result = entry1->GetFileOrDirectoryName().CompareNoCase(entry2->GetFileOrDirectoryName());
			}
			break;
		}
	case 0: // Full path column
		{
			if (result == 0)
			{
				result = CTGitPath::Compare(entry1->GetGitPathString(), entry2->GetGitPathString());
			}
			break;
		}
	} // switch (m_nSortedColumn)
	// sort by path name as second priority
	if (sortedColumn > 0 && result == 0)
		result = CTGitPath::Compare(entry1->GetGitPathString(), entry2->GetGitPathString());
	if (!ascending)
		result = -result;

	return result < 0;
}
