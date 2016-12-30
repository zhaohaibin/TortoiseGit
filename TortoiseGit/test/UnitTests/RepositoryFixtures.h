﻿// TortoiseGit - a Windows shell extension for easy version control

// Copyright (C) 2015-2016 - TortoiseGit

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

#pragma once
#include "Git.h"
#include "StringUtils.h"
#include "PathUtils.h"
#include "DirFileEnum.h"

enum config
{
	LIBGIT2_ALL,
	LIBGIT2,
	LIBGIT,
	GIT_CLI,
};

static bool GetResourcesDir(CString& resourcesDir)
{
	resourcesDir = CPathUtils::GetAppDirectory() + L"\\resources";
	if (!PathIsDirectory(resourcesDir))
		resourcesDir = CPathUtils::GetAppDirectory() + L"\\..\\..\\..\\test\\UnitTests\\resources";
	return PathIsDirectory(resourcesDir) != FALSE;
}

static void CopyRecursively(const CString& source, const CString& dest)
{
	CDirFileEnum finder(source);
	bool isDir;
	CString filepath;
	while (finder.NextFile(filepath, &isDir))
	{
		CString relpath = filepath.Mid(source.GetLength());
		if (isDir)
			EXPECT_TRUE(CreateDirectory(dest + relpath, nullptr));
		else
			EXPECT_TRUE(CopyFile(filepath, dest + relpath, false));
	}
}

class CBasicGitFixture : public ::testing::TestWithParam<config>
{
protected:
	virtual void SetUp()
	{
		switch (GetParam())
		{
		case LIBGIT2_ALL:
			m_Git.m_IsUseLibGit2 = true;
			m_Git.m_IsUseLibGit2_mask = 0xffffffff;
			m_Git.m_IsUseGitDLL = false;
			break;
		case LIBGIT2:
			m_Git.m_IsUseLibGit2 = true;
			m_Git.m_IsUseLibGit2_mask = DEFAULT_USE_LIBGIT2_MASK;
			m_Git.m_IsUseGitDLL = false;
			break;
		case LIBGIT:
			m_Git.m_IsUseLibGit2 = false;
			m_Git.m_IsUseLibGit2_mask = 0;
			m_Git.m_IsUseGitDLL = true;
			break;
		case GIT_CLI:
			m_Git.m_IsUseLibGit2 = false;
			m_Git.m_IsUseLibGit2_mask = 0;
			m_Git.m_IsUseGitDLL = false;
		}
		m_Git.m_CurrentDir = m_Dir.GetTempDir();
		// some methods rely on g_Git, so set values there as well
		g_Git.m_IsGitDllInited = false;
		g_Git.m_CurrentDir = m_Git.m_CurrentDir;
		g_Git.m_IsUseGitDLL = m_Git.m_IsUseGitDLL;
		g_Git.m_IsUseLibGit2 = m_Git.m_IsUseLibGit2;
		g_Git.m_IsUseLibGit2_mask = m_Git.m_IsUseLibGit2_mask;
		// libgit relies on CWD being set to working tree
		SetCurrentDirectory(m_Git.m_CurrentDir);
	}

	virtual void TearDown()
	{
		SetCurrentDirectory(CPathUtils::GetAppDirectory());
	}

public:
	CGit m_Git;
	CAutoTempDir m_Dir;
};

class CBasicGitWithTestRepoFixture : public CBasicGitFixture
{
protected:
	CBasicGitWithTestRepoFixture(const CString& arepo = L"git-repo1")
	{
		prefix = L"\\.git";
		repo = arepo;
	}

	virtual void SetUp()
	{
		CBasicGitFixture::SetUp();
		CString resourcesDir;
		ASSERT_TRUE(GetResourcesDir(resourcesDir));
		if (!prefix.IsEmpty())
			EXPECT_TRUE(CreateDirectory(m_Dir.GetTempDir() + prefix, nullptr));
		CString repoDir = resourcesDir + L"\\" + repo;
		CopyRecursively(repoDir, m_Dir.GetTempDir() + prefix);
		CString configFile = m_Dir.GetTempDir() + prefix + L"\\config";
		CString text;
		ASSERT_TRUE(CStringUtils::ReadStringFromTextFile(configFile, text));
		text += L"\n[core]\n  autocrlf = false\n[user]\n  name = User\n  email = user@example.com\n";
		EXPECT_TRUE(CStringUtils::WriteStringToTextFile(configFile, text));
	}
	CString prefix;
private:
	CString repo;
};

class CBasicGitWithTestRepoBareFixture : public CBasicGitWithTestRepoFixture
{
public:
	CBasicGitWithTestRepoBareFixture() : CBasicGitWithTestRepoFixture() {};
	CBasicGitWithTestRepoBareFixture(const CString& arepo) : CBasicGitWithTestRepoFixture(arepo) {};

protected:
	virtual void SetUp()
	{
		prefix.Empty();
		CBasicGitWithTestRepoFixture::SetUp();

		DeleteFile(m_Dir.GetTempDir() + L"\\index");
		CString configFile = m_Dir.GetTempDir() + L"\\config";
		CString text;
		ASSERT_TRUE(CStringUtils::ReadStringFromTextFile(configFile, text));
		EXPECT_EQ(1, text.Replace(L"bare = false", L"bare = true"));
		EXPECT_TRUE(CStringUtils::WriteStringToTextFile(configFile, text));
	}
};

class CBasicGitWithSubmoduleRepositoryFixture : public CBasicGitWithTestRepoFixture
{
public:
	CBasicGitWithSubmoduleRepositoryFixture() : CBasicGitWithTestRepoFixture(L"git-submodules-repo") {};
};

class CBasicGitWithSubmodulRepoeBareFixture : public CBasicGitWithTestRepoBareFixture
{
public:
	CBasicGitWithSubmodulRepoeBareFixture() : CBasicGitWithTestRepoBareFixture(L"git-submodules-repo") {};
};

class CBasicGitWithEmptyRepositoryFixture : public CBasicGitFixture
{
protected:
	virtual void SetUp()
	{
		CBasicGitFixture::SetUp();
		CString output;
		EXPECT_EQ(0, m_Git.Run(L"git.exe init", &output, CP_UTF8));
		EXPECT_FALSE(output.IsEmpty());
		output.Empty();
		EXPECT_EQ(0, m_Git.Run(L"git.exe config core.autocrlf false", &output, CP_UTF8));
		EXPECT_TRUE(output.IsEmpty());
		output.Empty();
		EXPECT_EQ(0, m_Git.Run(L"git.exe config user.name User", &output, CP_UTF8));
		EXPECT_TRUE(output.IsEmpty());
		output.Empty();
		EXPECT_EQ(0, m_Git.Run(L"git.exe config user.email user@example.com", &output, CP_UTF8));
		EXPECT_TRUE(output.IsEmpty());
	}
};

class CBasicGitWithEmptyBareRepositoryFixture : public CBasicGitFixture
{
protected:
	virtual void SetUp()
	{
		CBasicGitFixture::SetUp();
		CString output;
		EXPECT_EQ(0, m_Git.Run(L"git.exe init --bare", &output, CP_UTF8));
		EXPECT_FALSE(output.IsEmpty());
	}
};
