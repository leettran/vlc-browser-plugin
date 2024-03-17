/*****************************************************************************
 * plugin.h: ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2005-2010 the VideoLAN team
 *
 * Authors: Damien Fouilleul <Damien.Fouilleul@laposte.net>
 *          Jean-Paul Saman <jpsaman@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#include <olectl.h>

#include "../common/win32_fullscreen.h"
#include "../common/vlc_player.h"

extern "C" const GUID CLSID_VLCPlugin2;
extern "C" const GUID LIBID_AXVLC;
extern "C" const GUID DIID_DVLCEvents;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

class VLCPluginClass : public IClassFactory
{

public:

    VLCPluginClass(LONG *p_class_ref, HINSTANCE hInstance, REFCLSID rclsid);

    /* IUnknown methods */
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    /* IClassFactory methods */
    STDMETHODIMP CreateInstance(LPUNKNOWN pUnkOuter, REFIID riid, void **ppv);
    STDMETHODIMP LockServer(BOOL fLock);

    REFCLSID getClassID(void) { return (REFCLSID)_classid; };

    LPCTSTR getInPlaceWndClassName(void) const { return TEXT("VLC Plugin In-Place"); };
    HINSTANCE getHInstance(void) const { return _hinstance; };
    LPPICTURE getInPlacePict(void) const
        { if( NULL != _inplace_picture) _inplace_picture->AddRef(); return _inplace_picture; };

protected:

    virtual ~VLCPluginClass();

private:

    LPLONG      _p_class_ref;
    ULONG       _class_ref;
    HINSTANCE   _hinstance;
    CLSID       _classid;
    ATOM        _inplace_wndclass_atom;
    LPPICTURE   _inplace_picture;
};

class VLCPlugin
    : public IUnknown, private vlc_player_options,
      public VLCWindowsManager::InputObserver
{
public:
    VLCPlugin(VLCPluginClass *p_class, LPUNKNOWN pUnkOuter);
    VLCPlugin(const VLCPlugin&) = delete;
    VLCPlugin& operator=(const VLCPlugin&) = delete;

    /* IUnknown methods */
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
    STDMETHODIMP_(ULONG) AddRef(void) override;
    STDMETHODIMP_(ULONG) Release(void) override;

    /* custom methods */
    HRESULT getTypeLib(LCID lcid, ITypeLib **pTL) { return LoadRegTypeLib(LIBID_AXVLC, 1, 0, lcid, pTL); };
    REFCLSID getClassID(void) { return _p_class->getClassID(); };
    REFIID getDispEventID(void) { return (REFIID)DIID_DVLCEvents; };

    vlc_player& get_player()
    {
        return m_player;
    }

    vlc_player_options& get_options()
        { return *static_cast<vlc_player_options*>(this); }
    const vlc_player_options& get_options() const
        { return *static_cast<const vlc_player_options*>(this); }

    /*
    ** persistant properties
    */
    void setMRL(BSTR mrl)
    {
        SysFreeString(_bstr_mrl);
        _bstr_mrl = SysAllocStringLen(mrl, SysStringLen(mrl));
        setDirty(TRUE);
    };
    BSTR getMRL(void) { return _bstr_mrl; };

    inline void setAutoPlay(BOOL autoplay)
    {
        set_autoplay(autoplay != FALSE);
        setDirty(TRUE);
    };
    inline BOOL getAutoPlay(void) { return get_autoplay()? TRUE : FALSE; };

    inline void setAutoLoop(BOOL autoloop)
    {
        _b_autoloop = autoloop;
        get_player().mlp().setPlaybackMode( autoloop ? libvlc_playback_mode_loop :
                                            libvlc_playback_mode_default );
        setDirty(TRUE);
    };
    inline BOOL getAutoLoop(void) { return _b_autoloop;};

    inline void setShowToolbar(BOOL showtoolbar)
    {
        set_show_toolbar(showtoolbar != FALSE);
        setDirty(TRUE);
    };
    inline BOOL getShowToolbar(void) { return get_show_toolbar() ? TRUE : FALSE; };

    void setVolume(int volume);
    int getVolume(void) { return _i_volume; };

    void setBackColor(OLE_COLOR backcolor);
    OLE_COLOR getBackColor(void) { return _i_backcolor; };

    void setVisible(BOOL fVisible);
    BOOL getVisible(void) { return _b_visible; };
    BOOL isVisible(void) { return _b_visible || (! _b_usermode); };

    inline void setStartTime(int time)
    {
        _i_time = time;
        setDirty(TRUE);
    };
    inline int getStartTime(void) { return _i_time; };

    void setTime(int time);
    int  getTime(void) { return _i_time; };

    void setBaseURL(BSTR url)
    {
        SysFreeString(_bstr_baseurl);
        _bstr_baseurl = SysAllocStringLen(url, SysStringLen(url));
        setDirty(TRUE);
    };
    BSTR getBaseURL(void) { return _bstr_baseurl; };

    // control size in HIMETRIC
    inline void setExtent(const SIZEL& extent)
    {
        _extent = extent;
        setDirty(TRUE);
    };
    const SIZEL& getExtent(void) { return _extent; };

    // transient properties
    void setMute(BOOL mute);

    inline void setPicture(LPPICTURE pict)
    {
        if( NULL != _p_pict )
            _p_pict->Release();
        if( NULL != pict )
            pict->AddRef();
        _p_pict = pict;
    };

    inline LPPICTURE getPicture(void)
    {
        if( NULL != _p_pict )
            _p_pict->AddRef();
        return _p_pict;
    };

    BOOL hasFocus(void);
    void setFocus(BOOL fFocus);

    inline UINT getCodePage(void) { return _i_codepage; };
    inline void setCodePage(UINT cp)
    {
        // accept new codepage only if it works on this system
        size_t mblen = WideCharToMultiByte(cp,
                0, L"test", -1, NULL, 0, NULL, NULL);
        if( mblen > 0 )
            _i_codepage = cp;
    };

    inline BOOL isUserMode(void) { return _b_usermode; };
    inline void setUserMode(BOOL um) { _b_usermode = um; };

    inline BOOL isDirty(void) { return _b_dirty; };
    inline void setDirty(BOOL dirty) { _b_dirty = dirty; };

    void setErrorInfo(REFIID riid, const char *description);

    // control geometry within container
    RECT getPosRect(void) { return _posRect; };
    inline HWND getInPlaceWindow(void) const { return _inplacewnd; };
    void toggleFullscreen();
    void setFullscreen(BOOL yes);
    BOOL getFullscreen();

    BOOL isInPlaceActive(void);

    //VLCWindowsManager::InputObserver methods
    virtual void OnKeyEvent(UINT uKeyMsg, WPARAM wParam, LPARAM lParam) override;
    virtual void OnMouseEvent(UINT uMouseMsg, WPARAM wParam, LPARAM lParam) override;

    /*
    ** container events
    */
    HRESULT onInit(void);
    HRESULT onLoad(void);
    HRESULT onActivateInPlace(LPMSG lpMesg, HWND hwndParent, LPCRECT lprcPosRect, LPCRECT lprcClipRect);
    HRESULT onInPlaceDeactivate(void);
    HRESULT onAmbientChanged(LPUNKNOWN pContainer, DISPID dispID);
    HRESULT onClose(DWORD dwSaveOption);
    void onPositionChange(LPCRECT lprcPosRect, LPCRECT lprcClipRect);
    void onDraw(DVTARGETDEVICE * ptd, HDC hicTargetDev,
            HDC hdcDraw, LPCRECTL lprcBounds, LPCRECTL lprcWBounds);
    void onPaint(HDC hdc, const RECT &bounds, const RECT &pr);

    /*
    ** control events
    */
    void freezeEvents(BOOL freeze);
    void firePropChangedEvent(DISPID dispid);

    // async events;
    void fireOnMediaPlayerNothingSpecialEvent();
    void fireOnMediaPlayerOpeningEvent();
    void fireOnMediaPlayerBufferingEvent(float cache);
    void fireOnMediaPlayerPlayingEvent();
    void fireOnMediaPlayerPausedEvent();
    void fireOnMediaPlayerForwardEvent();
    void fireOnMediaPlayerBackwardEvent();
    void fireOnMediaPlayerEncounteredErrorEvent();
    void fireOnMediaPlayerEndReachedEvent();
    void fireOnMediaPlayerStoppedEvent();
    void fireOnMediaPlayerStopAsyncDoneEvent();

    void fireOnMediaPlayerTimeChangedEvent(libvlc_time_t time);
    void fireOnMediaPlayerPositionChangedEvent(float position);
    void fireOnMediaPlayerSeekableChangedEvent(VARIANT_BOOL seekable);
    void fireOnMediaPlayerPausableChangedEvent(VARIANT_BOOL pausable);
    void fireOnMediaPlayerMediaChangedEvent();
    void fireOnMediaPlayerTitleChangedEvent(int title);
    void fireOnMediaPlayerLengthChangedEvent(long length);
    void fireOnMediaPlayerChapterChangedEvent(int chapter);

    void fireOnMediaPlayerVoutEvent(int count);
    void fireOnMediaPlayerMutedEvent();
    void fireOnMediaPlayerUnmutedEvent();
    void fireOnMediaPlayerAudioVolumeEvent(float volume);

    void fireClickEvent();
    void fireDblClickEvent();
    void fireMouseDownEvent(short nButton, short nShiftState, int x, int y);
    void fireMouseMoveEvent(short nButton, short nShiftState, int x, int y);
    void fireMouseUpEvent(short nButton, short nShiftState, int x, int y);
    void fireKeyDownEvent(short nChar, short nShiftState);
    void fireKeyPressEvent(short nChar);
    void fireKeyUpEvent(short nchar, short shiftState);

    // controlling IUnknown interface
    LPUNKNOWN pUnkOuter;

protected:
    virtual ~VLCPlugin();

private:
    void initVLC();
    void set_player_window();
    void player_register_events();

    //implemented interfaces
    class VLCOleObject *vlcOleObject;
    class VLCOleControl *vlcOleControl;
    class VLCOleInPlaceObject *vlcOleInPlaceObject;
    class VLCOleInPlaceActiveObject *vlcOleInPlaceActiveObject;
    class VLCPersistStreamInit *vlcPersistStreamInit;
    class VLCPersistStorage *vlcPersistStorage;
    class VLCPersistPropertyBag *vlcPersistPropertyBag;
    class VLCProvideClassInfo *vlcProvideClassInfo;
    class VLCConnectionPointContainer *vlcConnectionPointContainer;
    class VLCObjectSafety *vlcObjectSafety;
    class VLCControl2 *vlcControl2;
    class VLCViewObject *vlcViewObject;
    class VLCDataObject *vlcDataObject;
    class VLCSupportErrorInfo *vlcSupportErrorInfo;

    // in place activated window (Plugin window)
    HWND _inplacewnd;
    VLCViewResources  _ViewRC;
    VLCWindowsManager _WindowsManager;

    VLCPluginClass* _p_class;
    ULONG _i_ref;

    vlc_player m_player;

    UINT _i_codepage;
    BOOL _b_usermode;
    RECT _posRect;
    LPPICTURE _p_pict;

    // persistable properties
    BSTR _bstr_baseurl;
    BSTR _bstr_mrl;
    BOOL _b_autoloop;
    BOOL _b_visible;
    BOOL _b_mute;
    int  _i_volume;
    int  _i_time;
    SIZEL _extent;
    OLE_COLOR _i_backcolor;
    // indicates whether properties needs persisting
    BOOL _b_dirty;
};

#endif
