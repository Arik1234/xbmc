#pragma once
/*
 *      Copyright (C) 2012-2013 Team XBMC
 *      http://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <math.h>
#include <pthread.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <queue>

#include <android/native_activity.h>

#include <androidjni/Activity.h>
#include <androidjni/AudioManager.h>
#include <androidjni/BroadcastReceiver.h>
#include <androidjni/SurfaceHolder.h>
#include <androidjni/Image.h>
#include <androidjni/View.h>

#include "threads/Event.h"
#include "interfaces/IAnnouncer.h"

#include "IActivityHandler.h"
#include "IInputHandler.h"
#include "JNIMainActivity.h"
#include "JNIXBMCAudioManagerOnAudioFocusChangeListener.h"
#include "JNIXBMCMainView.h"
#include "JNIXBMCMediaSession.h"
#include "JNIXBMCInputDeviceListener.h"
#include "JNIXBMCBroadcastReceiver.h"
#include "platform/xbmc.h"
#include "utils/Geometry.h"

// forward declares
class CJNIWakeLock;
class CAESinkAUDIOTRACK;
class CVariant;
class IInputDeviceCallbacks;
class IInputDeviceEventHandler;
class CVideoSyncAndroid;
typedef struct _JNIEnv JNIEnv;

struct androidIcon
{
  unsigned int width;
  unsigned int height;
  void *pixels;
};

struct androidPackage
{
  std::string packageName;
  std::string packageLabel;
  int icon;
};

class CActivityResultEvent : public CEvent
{
public:
  explicit CActivityResultEvent(int requestcode)
    : m_requestcode(requestcode), m_resultcode(0)
  {}
  int GetRequestCode() const { return m_requestcode; }
  int GetResultCode() const { return m_resultcode; }
  void SetResultCode(int resultcode) { m_resultcode = resultcode; }
  CJNIIntent GetResultData() const { return m_resultdata; }
  void SetResultData(const CJNIIntent &resultdata) { m_resultdata = resultdata; }

protected:
  int m_requestcode;
  CJNIIntent m_resultdata;
  int m_resultcode;
};

class CCaptureEvent : public CEvent
{
public:
  CCaptureEvent() {}
  jni::CJNIImage GetImage() const { return m_image; }
  void SetImage(const jni::CJNIImage &image) { m_image = image; }

protected:
  jni::CJNIImage m_image;
};

class CXBMCApp
    : public IActivityHandler
    , public CJNIMainActivity
    , public CJNIBroadcastReceiver
    , public ANNOUNCEMENT::IAnnouncer
    , public CJNISurfaceHolderCallback
    , public jni::CJNIXBMCInputDeviceListener
{
public:
  explicit CXBMCApp(ANativeActivity *nativeActivity);
  virtual ~CXBMCApp();
  static CXBMCApp* get() { return m_xbmcappinstance; }
  bool isExiting() { return m_exiting; }

  // IAnnouncer IF
  virtual void Announce(ANNOUNCEMENT::AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data) override;

  virtual void onReceive(CJNIIntent intent) override;
  virtual void onNewIntent(CJNIIntent intent) override;
  virtual void onActivityResult(int requestCode, int resultCode, CJNIIntent resultData) override;
  virtual void onVolumeChanged(int volume) override;
  virtual void onAudioFocusChange(int focusChange);
  virtual void doFrame(int64_t frameTimeNanos) override;
  virtual void onVisibleBehindCanceled() override;
  virtual void onCaptureAvailable(jni::CJNIImage image) override;
  virtual void onScreenshotAvailable(jni::CJNIImage image) override;
  
  virtual void onMultiWindowModeChanged(bool isInMultiWindowMode) override;
  virtual void onPictureInPictureModeChanged(bool isInPictureInPictureMode) override;

  // implementation of CJNIInputManagerInputDeviceListener
  virtual void onInputDeviceAdded(int deviceId) override;
  virtual void onInputDeviceChanged(int deviceId) override;
  virtual void onInputDeviceRemoved(int deviceId) override;

  // CJNISurfaceHolderCallback interface
  virtual void surfaceChanged(CJNISurfaceHolder holder, int format, int width, int height) override;
  virtual void surfaceCreated(CJNISurfaceHolder holder) override;
  virtual void surfaceDestroyed(CJNISurfaceHolder holder) override;

  bool isValid() { return m_activity != NULL; }

  void onStart() override;
  void onResume() override;
  void onPause() override;
  void onStop() override;
  void onDestroy() override;

  void onSaveState(void **data, size_t *size) override;
  void onConfigurationChanged() override;
  void onLowMemory() override;

  void onCreateWindow(ANativeWindow* window) override;
  void onResizeWindow() override;
  void onDestroyWindow() override;
  void onGainFocus() override;
  void onLostFocus() override;

  void Initialize();
  void Deinitialize();

  static ANativeWindow* GetNativeWindow(int timeout);
  static int SetBuffersGeometry(int width, int height, int format);
  static int android_printf(const char *format, ...);
  
  void BringToFront();

  static int GetBatteryLevel();
  bool EnableWakeLock(bool on);
  static bool HasFocus() { return m_hasFocus; }
  static bool IsResumed() { return m_isResumed; }
  static bool IsHeadsetPlugged();
  static bool IsHDMIPlugged();

  bool StartActivity(const std::string &package, const std::string &intent = std::string(), const std::string &dataType = std::string(), const std::string &dataURI = std::string());
  std::vector <androidPackage> GetApplications();

  /*!
   * \brief If external storage is available, it returns the path for the external storage (for the specified type)
   * \param path will contain the path of the external storage (for the specified type)
   * \param type optional type. Possible values are "", "files", "music", "videos", "pictures", "photos, "downloads"
   * \return true if external storage is available and a valid path has been stored in the path parameter
   */
  static bool GetExternalStorage(std::string &path, const std::string &type = "");
  static bool GetStorageUsage(const std::string &path, std::string &usage);
  int GetMaxSystemVolume();
  float GetSystemVolume();
  void SetSystemVolume(float percent);
  void InitDirectories();

  void SetRefreshRate(float rate);
  void SetDisplayMode(int mode);
  int GetDPI();

  static CRect GetSurfaceRect();
  static CRect MapRenderToDroid(const CRect& srcRect);
  static CPoint MapDroidToGui(const CPoint& src);

  int WaitForActivityResult(const CJNIIntent &intent, int requestCode, CJNIIntent& result);
  bool WaitForCapture(jni::CJNIImage& image);
  bool GetCapture(jni::CJNIImage& img);
  void TakeScreenshot();
  void StopCapture();

  // Playback callbacks
  void OnPlayBackStarted();
  void OnPlayBackPaused();
  void OnPlayBackStopped();

  // Info callback
  void UpdateSessionMetadata();
  void UpdateSessionState();

  // input device methods
  void RegisterInputDeviceCallbacks(IInputDeviceCallbacks* handler);
  void UnregisterInputDeviceCallbacks();
  const CJNIViewInputDevice GetInputDevice(int deviceId);
  std::vector<int> GetInputDeviceIds();

  void RegisterInputDeviceEventHandler(IInputDeviceEventHandler* handler);
  void UnregisterInputDeviceEventHandler();
  bool onInputDeviceEvent(const AInputEvent* event);

  static void InitFrameCallback(CVideoSyncAndroid *syncImpl);
  static void DeinitFrameCallback();

  // Application slow ping
  void ProcessSlow();

  //PIP
  void RequestPictureInPictureMode();

  static bool WaitVSync(unsigned int milliSeconds);

  bool getVideosurfaceInUse();
  void setVideosurfaceInUse(bool videosurfaceInUse);

  static void CalculateGUIRatios();
  void onLayoutChange(int left, int top, int width, int height);

protected:
  // limit who can access Volume
  friend class CAESinkAUDIOTRACK;

  int GetMaxSystemVolume(JNIEnv *env);
  bool AcquireAudioFocus();
  bool ReleaseAudioFocus();
  void RequestVisibleBehind(bool requested);

private:
  static CXBMCApp* m_xbmcappinstance;
  static CCriticalSection m_AppMutex;

  std::unique_ptr<CJNIXBMCAudioManagerOnAudioFocusChangeListener> m_audioFocusListener;
  std::unique_ptr<jni::CJNIXBMCBroadcastReceiver> m_broadcastReceiver;
  static std::unique_ptr<CJNIXBMCMainView> m_mainView;
  std::unique_ptr<jni::CJNIXBMCMediaSession> m_mediaSession;
  bool HasLaunchIntent(const std::string &package);
  std::string GetFilenameFromIntent(const CJNIIntent &intent);
  void run();
  void stop();
  void SetupEnv();
  static void SetRefreshRateCallback(CVariant *rate);
  static void SetDisplayModeCallback(CVariant *mode);
  static ANativeActivity *m_activity;
  static CJNIWakeLock *m_wakeLock;
  static int m_batteryLevel;
  static bool m_hasFocus;
  static bool m_isResumed;
  static bool m_headsetPlugged;
  static bool m_hdmiPlugged;
  IInputDeviceCallbacks* m_inputDeviceCallbacks;
  IInputDeviceEventHandler* m_inputDeviceEventHandler;
  static bool m_hasReqVisible;
  static bool m_hasPIP;
  bool m_videosurfaceInUse;
  bool m_firstrun;
  bool m_exiting;
  pthread_t m_thread;
  static CCriticalSection m_applicationsMutex;
  static std::vector<androidPackage> m_applications;
  static std::vector<CActivityResultEvent*> m_activityResultEvents;

  static CCriticalSection m_captureMutex;
  static CCaptureEvent m_captureEvent;
  static std::queue<jni::CJNIImage> m_captureQueue;

  static ANativeWindow* m_window;

  static CVideoSyncAndroid* m_syncImpl;
  static CEvent m_vsyncEvent;

  void XBMC_Pause(bool pause);
  void XBMC_Stop();
  bool XBMC_DestroyDisplay();
  bool XBMC_SetupDisplay();
  static CRect m_droid2guiRatio;

  static uint32_t m_playback_state;
  static CRect m_surface_rect;
};
