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

#include <unistd.h>
#include <errno.h>
#include <imagine/base/Screen.hh>
#include <imagine/base/Base.hh>
#include <imagine/time/Time.hh>
#include <imagine/util/algorithm.h>
#include <imagine/logger/logger.h>
#include "internal.hh"
#include "android.hh"
#include "../common/screenPrivate.hh"
#include "../common/SimpleFrameTimer.hh"

namespace Base
{

static JavaInstMethod<jint()> jGetRotation{};
static JavaInstMethod<jobject()> jGetSupportedRefreshRates{};
static JavaInstMethod<jobject(jobject)> jGetMetrics{};
JavaInstMethod<jobject(jobject, jlong)> jPresentation{};
static Screen main;

void initScreens(JNIEnv *env, jobject activity, jclass activityCls)
{
	assert(!main);
	JavaInstMethod<jobject()> jDefaultDpy{env, activityCls, "defaultDpy", "()Landroid/view/Display;"};
	// DisplayMetrics obtained via getResources().getDisplayMetrics() so the scaledDensity field is correct
	JavaInstMethod<jobject()> jDisplayMetrics{env, activityCls, "displayMetrics", "()Landroid/util/DisplayMetrics;"};
	auto mainDpy = jDefaultDpy(env, activity);
	jclass jDisplayCls = env->GetObjectClass(mainDpy);
	jGetRotation.setup(env, jDisplayCls, "getRotation", "()I");
	jGetMetrics.setup(env, Base::jBaseActivityCls, "getDisplayMetrics", "(Landroid/view/Display;)Landroid/util/DisplayMetrics;");
	JavaInstMethod<jint()> jGetDisplayId{env, jDisplayCls, "getDisplayId", "()I"};
	JavaInstMethod<jfloat()> jGetRefreshRate{env, jDisplayCls, "getRefreshRate", "()F"};
	main.init(env, mainDpy, jDisplayMetrics(env, activity), 0, jGetRefreshRate(env, mainDpy));
	Screen::addScreen(&main);
	if(Base::androidSDK() >= 17)
	{
		jPresentation.setup(env, activityCls, "presentation", "(Landroid/view/Display;J)Lcom/imagine/PresentationHelper;");
		logMsg("setting up screen notifications");
		JavaInstMethod<jobject()> jDisplayListenerHelper{env, activityCls, "displayListenerHelper", "()Lcom/imagine/DisplayListenerHelper;"};
		auto displayListenerHelper = jDisplayListenerHelper(env, activity);
		assert(displayListenerHelper);
		auto displayListenerHelperCls = env->GetObjectClass(displayListenerHelper);
		JNINativeMethod method[]
		{
			{
				"displayAdd", "(ILandroid/view/Display;F)V",
				(void*)(void (*)(JNIEnv*, jobject, jint, jobject, jfloat))
				([](JNIEnv* env, jobject thiz, jint id, jobject disp, jfloat refreshRate)
				{
					for(auto s : screen_)
					{
						if(s->id() == id)
						{
							logMsg("screen %d already in device list", id);
							break;
						}
					}
					Screen *s = new Screen();
					s->init(env, disp, nullptr, id, refreshRate);
					Screen::addScreen(s);
					if(Screen::onChange)
						Screen::onChange(*s, {Screen::Change::ADDED});
				})
			},
			{
				"displayChange", "(IF)V",
				(void*)(void (*)(JNIEnv*, jobject, jint, jfloat))
				([](JNIEnv* env, jobject thiz, jint id, jfloat refreshRate)
				{
					for(auto s : screen_)
					{
						if(s->id() == id)
						{
							s->updateRefreshRate(refreshRate);
							break;
						}
					}
				})
			},
			{
				"displayRemove", "(I)V",
				(void*)(void (*)(JNIEnv*, jobject, jint))
				([](JNIEnv* env, jobject thiz, jint id)
				{
					logMsg("screen %d removed", id);
					forEachInContainer(screen_, it)
					{
						Screen *removedScreen = *it;
						if(removedScreen->id() == id)
						{
							it.erase();
							if(Screen::onChange)
								Screen::onChange(*removedScreen, {Screen::Change::REMOVED});
							removedScreen->deinit();
							delete removedScreen;
							break;
						}
					}
				})
			}
		};
		env->RegisterNatives(displayListenerHelperCls, method, std::size(method));

		// get the current presentation screens
		JavaInstMethod<jobject()> jGetPresentationDisplays{env, displayListenerHelperCls, "getPresentationDisplays", "()[Landroid/view/Display;"};
		auto jPDisplay = (jobjectArray)jGetPresentationDisplays(env, displayListenerHelper);
		uint32_t pDisplays = env->GetArrayLength(jPDisplay);
		if(pDisplays)
		{
			logMsg("checking %d presentation display(s)", pDisplays);
			iterateTimes(pDisplays, i)
			{
				auto display = env->GetObjectArrayElement(jPDisplay, i);
				Screen *s = new Screen();
				s->init(env, display, nullptr, jGetDisplayId(env, display), jGetRefreshRate(env, display));
				Screen::addScreen(s);
			}
		}
	}
}

void AndroidScreen::init(JNIEnv *env, jobject aDisplay, jobject metrics, int id, float refreshRate)
{
	assert(aDisplay);
	this->aDisplay = env->NewGlobalRef(aDisplay);
	bool isStraightRotation = true;
	if(id == 0)
	{
		id_ = 0;
		auto orientation = (SurfaceRotation)jGetRotation(env, aDisplay);
		logMsg("starting orientation %d", orientation);
		osRotation = orientation;
		isStraightRotation = surfaceRotationIsStraight(orientation);
	}
	else
	{
		id_ = id;
		logMsg("init display with id: %d", id_);
	}

	updateRefreshRate(refreshRate);
	if(Base::androidSDK() <= 10)
	{
		// corrections for devices known to report wrong refresh rates
		auto buildDevice = Base::androidBuildDevice();
		if(Config::MACHINE_IS_GENERIC_ARMV7 && string_equal(buildDevice.data(), "R800at"))
			refreshRate_ = 61.5;
		else if(Config::MACHINE_IS_GENERIC_ARMV7 && string_equal(buildDevice.data(), "sholes"))
			refreshRate_ = 60;
		else
			reliableRefreshRate = false;
	}

	// DisplayMetrics
	if(!metrics)
	{
		logMsg("getting metrics from display");
		metrics = jGetMetrics(env, Base::jBaseActivity, aDisplay);
		assert(metrics);
	}
	jclass jDisplayMetricsCls = env->GetObjectClass(metrics);
	auto jXDPI = env->GetFieldID(jDisplayMetricsCls, "xdpi", "F");
	auto jYDPI = env->GetFieldID(jDisplayMetricsCls, "ydpi", "F");
	auto jScaledDensity = env->GetFieldID(jDisplayMetricsCls, "scaledDensity", "F");
	auto jWidthPixels = env->GetFieldID(jDisplayMetricsCls, "widthPixels", "I");
	auto jHeightPixels = env->GetFieldID(jDisplayMetricsCls, "heightPixels", "I");
	auto metricsXDPI = env->GetFloatField(metrics, jXDPI);
	auto metricsYDPI = env->GetFloatField(metrics, jYDPI);
	auto widthPixels = env->GetIntField(metrics, jWidthPixels);
	auto heightPixels = env->GetIntField(metrics, jHeightPixels);
	densityDPI_ = 160.*env->GetFloatField(metrics, jScaledDensity);
	assert(densityDPI_);
	logMsg("screen with size %dx%d, DPI size %fx%f, scaled density DPI %f",
		widthPixels, heightPixels, (double)metricsXDPI, (double)metricsYDPI, (double)densityDPI_);
	#ifndef NDEBUG
	{
		auto jDensity = env->GetFieldID(jDisplayMetricsCls, "density", "F");
		auto jDensityDPI = env->GetFieldID(jDisplayMetricsCls, "densityDpi", "I");
		logMsg("display density %f, densityDPI %d, %dx%d pixels, %.2fHz",
			(double)env->GetFloatField(metrics, jDensity), env->GetIntField(metrics, jDensityDPI),
			env->GetIntField(metrics, jWidthPixels), env->GetIntField(metrics, jHeightPixels),
			(double)refreshRate_);
	}
	#endif
	// DPI values are un-rotated from DisplayMetrics
	xDPI = isStraightRotation ? metricsXDPI : metricsYDPI;
	yDPI = isStraightRotation ? metricsYDPI : metricsXDPI;
	width_ = isStraightRotation ? widthPixels : heightPixels;
	height_ = isStraightRotation ? heightPixels : widthPixels;
}

SurfaceRotation AndroidScreen::rotation(JNIEnv *env) const
{
	return (SurfaceRotation)jGetRotation(env, aDisplay);
}

std::pair<float, float> AndroidScreen::dpi() const
{
	return {xDPI, yDPI};
}

float AndroidScreen::densityDPI() const
{
	return densityDPI_;
}

jobject AndroidScreen::displayObject() const
{
	return aDisplay;
}

int AndroidScreen::id() const
{
	return id_;
}

void AndroidScreen::updateRefreshRate(float refreshRate)
{
	if(refreshRate_ && refreshRate != refreshRate_)
	{
		logMsg("refresh rate updated to:%.2f on screen:%d", refreshRate, id());
	}
	if(refreshRate < 20.f || refreshRate > 250.f) // sanity check in case device has a junk value
	{
		logWarn("ignoring unusual refresh rate: %f", (double)refreshRate);
		refreshRate = 60;
		reliableRefreshRate = false;
	}
	refreshRate_ = refreshRate;
	frameTime_ = IG::FloatSeconds(1. / refreshRate);

}

bool AndroidScreen::operator ==(AndroidScreen const &rhs) const
{
	return id_ == rhs.id_;
}

AndroidScreen::operator bool() const
{
	return aDisplay;
}

void Screen::deinit()
{
	unpostFrame();
	jEnvForThread()->DeleteGlobalRef(aDisplay);
	*this = {};
}

int Screen::width()
{
	return width_;
}

int Screen::height()
{
	return height_;
}

double Screen::frameRate() const
{
	return refreshRate_;
}

IG::FloatSeconds Screen::frameTime() const
{
	return IG::FloatSeconds(1. / frameRate());
}

bool Screen::frameRateIsReliable() const
{
	return reliableRefreshRate;
}

void Screen::postFrame()
{
	if(!isActive)
	{
		//logMsg("can't post frame when app isn't running");
		return;
	}
	if(framePosted)
		return;
	//logMsg("posting frame");
	framePosted = true;
	frameTimer->scheduleVSync();
	if(!inFrameHandler)
	{
		prevFrameTimestamp = {};
	}
}

void Screen::unpostFrame()
{
	if(!framePosted)
		return;
	framePosted = false;
	if(!screensArePosted())
		frameTimer->cancel();
}

void Screen::setFrameInterval(int interval)
{
	// TODO
	//logMsg("setting frame interval %d", (int)interval);
	assert(interval >= 1);
}

bool Screen::supportsFrameInterval()
{
	return false;
}

bool Screen::supportsTimestamps()
{
	return Base::androidSDK() >= 16;
}

void Screen::setFrameRate(double rate)
{
	// unsupported
}

std::vector<double> Screen::supportedFrameRates()
{
	std::vector<double> rateVec;
	if(Base::androidSDK() < 21)
	{
		rateVec.reserve(1);
		rateVec.emplace_back(frameRate());
	}
	auto env = jEnvForThread();
	if(unlikely(!jGetSupportedRefreshRates))
	{
		jclass jDisplayCls = env->GetObjectClass(aDisplay);
		jGetSupportedRefreshRates.setup(env, jDisplayCls, "getSupportedRefreshRates", "()[F");
	}
	auto jRates = (jfloatArray)jGetSupportedRefreshRates(env, aDisplay);
	auto rate = env->GetFloatArrayElements(jRates, 0);
	auto rates = env->GetArrayLength(jRates);
	rateVec.reserve(rates);
	logMsg("screen %d supports %d rate(s):", id_, rates);
	iterateTimes(rates, i)
	{
		double r = rate[i];
		logMsg("%f", r);
		rateVec.emplace_back(r);
	}
	env->ReleaseFloatArrayElements(jRates, rate, 0);
	return rateVec;
}

}
