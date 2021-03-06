/*
 * This file is part of Compare Plugin for Notepad++
 * Copyright (C) 2016 Pavel Nedev (pg.nedev@gmail.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#pragma once

#include <map>
#include <windows.h>


/**
 *  \struct
 *  \brief
 */
struct ScopedIncrementer
{
	ScopedIncrementer(volatile unsigned& useCount) : _useCount(useCount)
	{
		++_useCount;
	}

	~ScopedIncrementer()
	{
		--_useCount;
	}

private:
	volatile unsigned&	_useCount;
};


/**
 *  \class
 *  \brief
 */
class DelayedWork
{
public:
	bool isPending()
	{
		return (_timerId != 0);
	}

	bool post(UINT delay_ms);
	void cancel();

protected:
	DelayedWork() : _timerId(0) {}
	virtual ~DelayedWork()
	{
		cancel();
	}

	virtual void operator()() = 0;

	UINT_PTR _timerId;

private:
	static VOID CALLBACK timerCB(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

	static std::map<UINT_PTR, DelayedWork*> workMap;
};
