#pragma once

/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#include <imagine/config/defs.hh>
#include <imagine/audio/defs.hh>
#include <imagine/time/Time.hh>
#include <imagine/audio/Format.hh>
#include <imagine/util/DelegateFunc.hh>
#include <imagine/base/Error.hh>
#include <memory>

namespace IG::Audio
{
using OnSamplesNeededDelegate = DelegateFunc<bool(void *buff, unsigned bytes)>;

class OutputStreamConfig
{
public:
	constexpr OutputStreamConfig() {}
	constexpr OutputStreamConfig(Format format, OnSamplesNeededDelegate onSamplesNeeded = nullptr):
		format_{format}, onSamplesNeeded_{onSamplesNeeded}
		{}

	Format format() const;

	void setOnSamplesNeeded(OnSamplesNeededDelegate del)
	{
		onSamplesNeeded_ = del;
	}

	OnSamplesNeededDelegate onSamplesNeeded() const
	{
		return onSamplesNeeded_;
	}

	void setWantedLatencyHint(IG::Microseconds uSecs)
	{
		wantedLatency = uSecs;
	}

	IG::Microseconds wantedLatencyHint() const
	{
		return wantedLatency;
	}

	void setStartPlaying(bool on)
	{
		startPlaying_ = on;
	}

	bool startPlaying()
	{
		return startPlaying_;
	}

protected:
	Format format_{};
	OnSamplesNeededDelegate onSamplesNeeded_{};
	IG::Microseconds wantedLatency{20000};
	bool startPlaying_ = true;
};

class OutputStream
{
public:
	virtual ~OutputStream();
	virtual IG::ErrorCode open(OutputStreamConfig config) = 0;
	virtual void play() = 0;
	virtual void pause() = 0;
	virtual void close() = 0;
	virtual void flush() = 0;
	virtual bool isOpen() = 0;
	virtual bool isPlaying() = 0;
};

std::unique_ptr<OutputStream> makeOutputStream(Api api = Api::DEFAULT);

}
