/*****************************************************************************
 * vlccontrol.h: ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * Copyright (C) 2010 M2X BV
 *
 * Authors: Damien Fouilleul <Damien.Fouilleul@laposte.net>
 *          Jean-Paul Saman <jpsaman _at_ m2x _dot_ nl>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef _VLCCONTROL2_H_
#define _VLCCONTROL2_H_

#include "axvlc_idl.h"

#include <ole2.h>

class VLCInterfaceBase {
public:
    VLCInterfaceBase(VLCPlugin *p): _plug(p), _ti(NULL) { }
    virtual ~VLCInterfaceBase() { if( _ti ) _ti->Release(); }

protected:
    HRESULT loadTypeInfo(REFIID riid);
    ITypeInfo *TypeInfo() const { return _ti; }

    STDMETHODIMP_(ULONG) AddRef(void) { return _plug->pUnkOuter->AddRef(); };
    STDMETHODIMP_(ULONG) Release(void) { return _plug->pUnkOuter->Release(); };

    VLCPlugin *_plug;
private:
    ITypeInfo *_ti;
};

template<class T,class I>
class VLCInterface: public I, protected VLCInterfaceBase
{
private:
    typedef VLCInterfaceBase Base;
          T *This()       { return static_cast<      T *>(this); }
    const T *This() const { return static_cast<const T *>(this); }

    HRESULT loadTypeInfo()
    {
        return TypeInfo() ? NOERROR : Base::loadTypeInfo(_riid);
    }

public:
    VLCInterface(VLCPlugin *p): Base(p) { }

    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv ) return E_POINTER;

        if( (IID_IUnknown == riid)
         || (IID_IDispatch == riid)
         || (_riid == riid) )
        {
            This()->AddRef();
            *ppv = reinterpret_cast<LPVOID>(This());
            return NOERROR;
        }
        // behaves as a standalone object
        return E_NOINTERFACE;
    }

    STDMETHODIMP GetTypeInfoCount(UINT* pctInfo)
    {
        if( NULL == pctInfo )
            return E_INVALIDARG;
        *pctInfo = SUCCEEDED(loadTypeInfo()) ? 1 : 0;
        return NOERROR;
    }
    STDMETHODIMP GetTypeInfo(UINT , LCID , LPTYPEINFO* ppTInfo)
    {
        if( NULL == ppTInfo )
            return E_INVALIDARG;

        if( SUCCEEDED(loadTypeInfo()) )
        {
            (*ppTInfo = TypeInfo())->AddRef();
            return NOERROR;
        }
        *ppTInfo = NULL;
        return E_NOTIMPL;
    }

    STDMETHODIMP GetIDsOfNames(REFIID , LPOLESTR* rgszNames,
            UINT cNames, LCID , DISPID* rgDispID)
    {
        return FAILED(loadTypeInfo()) ? E_NOTIMPL :
            DispGetIDsOfNames(TypeInfo(), rgszNames, cNames, rgDispID);
    }

    STDMETHODIMP Invoke(DISPID dispIdMember, REFIID ,
        LCID , WORD wFlags, DISPPARAMS* pDispParams,
        VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr)
    {
        return FAILED(loadTypeInfo()) ? E_NOTIMPL :
            DispInvoke(This(), TypeInfo(), dispIdMember, wFlags,
                       pDispParams, pVarResult, pExcepInfo, puArgErr);
    }

    STDMETHODIMP_(ULONG) AddRef(void)  { return Base::AddRef(); };
    STDMETHODIMP_(ULONG) Release(void) { return Base::Release(); };
private:
    static REFIID _riid;
};

class VLCAudio : public VLCInterface<VLCAudio,IVLCAudio>
{
public:
    VLCAudio(VLCPlugin *p): VLCInterface<VLCAudio,IVLCAudio>(p) { }

    // IVLCAudio methods
    STDMETHODIMP get_mute(VARIANT_BOOL*);
    STDMETHODIMP put_mute(VARIANT_BOOL);
    STDMETHODIMP get_volume(long*);
    STDMETHODIMP put_volume(long);
    STDMETHODIMP get_track(long*);
    STDMETHODIMP put_track(long);
    STDMETHODIMP get_count(long*);
    STDMETHODIMP get_channel(long*);
    STDMETHODIMP put_channel(long);
    STDMETHODIMP toggleMute();
    STDMETHODIMP description(long trackId, BSTR*);
};

class VLCTitle : public VLCInterface<VLCTitle,IVLCTitle>
{
public:
    VLCTitle(VLCPlugin *p): VLCInterface<VLCTitle,IVLCTitle>(p) { }

    // IVLCTitle methods
    STDMETHODIMP get_count(long*);
    STDMETHODIMP get_track(long*);
    STDMETHODIMP put_track(long);
    STDMETHODIMP description(long, BSTR*);
};

class VLCChapter : public VLCInterface<VLCChapter,IVLCChapter>
{
public:
    VLCChapter(VLCPlugin *p): VLCInterface<VLCChapter,IVLCChapter>(p) { }

    // IVLCChapter methods
    STDMETHODIMP get_count(long*);
    STDMETHODIMP countForTitle(long, long*);
    STDMETHODIMP get_track(long*);
    STDMETHODIMP put_track(long);
    STDMETHODIMP description(long, long, BSTR*);
    STDMETHODIMP next();
    STDMETHODIMP prev();
};

class VLCInput: public VLCInterface<VLCInput,IVLCInput>
{
public:
    VLCInput(VLCPlugin *p): VLCInterface<VLCInput,IVLCInput>(p),
        _p_vlctitle(new VLCTitle(p)), _p_vlcchapter(new VLCChapter(p)) { }
    virtual ~VLCInput() {
        _p_vlctitle->Release();
        _p_vlcchapter->Release();
    }

    // IVLCInput methods
    STDMETHODIMP get_length(double*);
    STDMETHODIMP get_position(double*);
    STDMETHODIMP put_position(double);
    STDMETHODIMP get_time(double*);
    STDMETHODIMP put_time(double);
    STDMETHODIMP get_state(long*);
    STDMETHODIMP get_rate(double*);
    STDMETHODIMP put_rate(double);
    STDMETHODIMP get_fps(double*);
    STDMETHODIMP get_hasVout(VARIANT_BOOL*);
    STDMETHODIMP get_title(IVLCTitle**);
    STDMETHODIMP get_chapter(IVLCChapter**);

private:
    IVLCTitle       *_p_vlctitle;
    IVLCChapter     *_p_vlcchapter;
};

class VLCMarquee: public VLCInterface<VLCMarquee,IVLCMarquee>
{
public:
    VLCMarquee(VLCPlugin *p): VLCInterface<VLCMarquee,IVLCMarquee>(p) { }
    virtual ~VLCMarquee() { }

    // IVLCMarquee methods
    STDMETHODIMP enable()  { return do_put_int(libvlc_marquee_Enable, true); }
    STDMETHODIMP disable() { return do_put_int(libvlc_marquee_Enable, false); }

    STDMETHODIMP put_text(BSTR);
    STDMETHODIMP get_position(BSTR *);
    STDMETHODIMP put_position(BSTR);

#define PROP_INT( a, b ) \
        STDMETHODIMP get_##a(LONG *val) { return do_get_int(b,val); } \
        STDMETHODIMP put_##a(LONG val)  { return do_put_int(b,val); }

    PROP_INT( color,    libvlc_marquee_Color )
    PROP_INT( opacity,  libvlc_marquee_Opacity )
    PROP_INT( refresh,  libvlc_marquee_Refresh )
    PROP_INT( size,     libvlc_marquee_Size )
    PROP_INT( timeout,  libvlc_marquee_Timeout )
    PROP_INT( x,        libvlc_marquee_X )
    PROP_INT( y,        libvlc_marquee_Y )

#undef  PROP_INT

private:
    HRESULT do_put_int(unsigned idx, LONG val);
    HRESULT do_get_int(unsigned idx, LONG *val);
};

class VLCLogo: public VLCInterface<VLCLogo,IVLCLogo>
{
public:
    VLCLogo(VLCPlugin *p): VLCInterface<VLCLogo,IVLCLogo>(p) { }
    virtual ~VLCLogo() { }

    STDMETHODIMP enable()  { return do_put_int(libvlc_logo_enable, true); }
    STDMETHODIMP disable() { return do_put_int(libvlc_logo_enable, false); }

    STDMETHODIMP file(BSTR fname);

#define PROP_INT( a ) \
        STDMETHODIMP get_##a(LONG *val) \
            { return do_get_int(libvlc_logo_##a,val); } \
        STDMETHODIMP put_##a(LONG val) \
            { return do_put_int(libvlc_logo_##a,val); }

    PROP_INT( delay )
    PROP_INT( repeat )
    PROP_INT( opacity )
    PROP_INT( x )
    PROP_INT( y )

#undef  PROP_INT

    STDMETHODIMP get_position(BSTR* val);
    STDMETHODIMP put_position(BSTR val);

private:
    HRESULT do_put_int(unsigned idx, LONG val);
    HRESULT do_get_int(unsigned idx, LONG *val);
};

class VLCDeinterlace: public VLCInterface<VLCDeinterlace,IVLCDeinterlace>
{
public:
    VLCDeinterlace(VLCPlugin *p):
        VLCInterface<VLCDeinterlace,IVLCDeinterlace>(p) { }
    virtual ~VLCDeinterlace() { }

    STDMETHODIMP enable(BSTR val);
    STDMETHODIMP disable();
};

class VLCPlaylistItems: public VLCInterface<VLCPlaylistItems,IVLCPlaylistItems>
{
public:
    VLCPlaylistItems(VLCPlugin *p):
        VLCInterface<VLCPlaylistItems,IVLCPlaylistItems>(p) { }

    // IVLCPlaylistItems methods
    STDMETHODIMP get_count(long*);
    STDMETHODIMP clear();
    STDMETHODIMP remove(long);
};

class VLCPlaylist: public VLCInterface<VLCPlaylist,IVLCPlaylist>
{
public:
    VLCPlaylist(VLCPlugin *p);
    virtual ~VLCPlaylist();

    // IVLCPlaylist methods
    STDMETHODIMP get_itemCount(long*);
    STDMETHODIMP get_isPlaying(VARIANT_BOOL*);
    STDMETHODIMP get_currentItem(long*);
    STDMETHODIMP add(BSTR, VARIANT, VARIANT, long*);
    STDMETHODIMP play();
    STDMETHODIMP playItem(long);
    STDMETHODIMP pause();
    STDMETHODIMP togglePause();
    STDMETHODIMP stop();
    STDMETHODIMP stop_async();
    STDMETHODIMP next();
    STDMETHODIMP prev();
    STDMETHODIMP clear();
    STDMETHODIMP removeItem(long);
    STDMETHODIMP get_items(IVLCPlaylistItems**);
    STDMETHODIMP parse(long options, long timeout, long* status);

private:
    static void async_handler_cb(LPVOID obj);
    void async_handler();

private:
    VLCPlaylistItems*    _p_vlcplaylistitems;
    HANDLE               _async_thread;
    DWORD                _async_thread_id;
};

class VLCSubtitle: public VLCInterface<VLCSubtitle,IVLCSubtitle>
{
public:
    VLCSubtitle(VLCPlugin *p): VLCInterface<VLCSubtitle,IVLCSubtitle>(p) { }

    // IVLCSubtitle methods
    STDMETHODIMP get_track(long*);
    STDMETHODIMP put_track(long);
    STDMETHODIMP get_count(long*);
    STDMETHODIMP description(long, BSTR*);
};

class VLCVideo: public VLCInterface<VLCVideo,IVLCVideo>
{
public:
    VLCVideo(VLCPlugin *p): VLCInterface<VLCVideo,IVLCVideo>(p),
        _p_vlcmarquee(new VLCMarquee(p)), _p_vlclogo(new VLCLogo(p)),
        _p_vlcdeint(new VLCDeinterlace(p)) { }
    virtual ~VLCVideo() {
        _p_vlcmarquee->Release();
        _p_vlclogo->Release();
        _p_vlcdeint->Release();
    }

    // IVLCVideo methods
    STDMETHODIMP get_fullscreen(VARIANT_BOOL*);
    STDMETHODIMP put_fullscreen(VARIANT_BOOL);
    STDMETHODIMP get_width(long*);
    STDMETHODIMP get_height(long*);
    STDMETHODIMP get_aspectRatio(BSTR*);
    STDMETHODIMP put_aspectRatio(BSTR);
    STDMETHODIMP get_scale(float*);
    STDMETHODIMP put_scale(float);
    STDMETHODIMP get_subtitle(long*);
    STDMETHODIMP put_subtitle(long);
    STDMETHODIMP put_crop_ratio(ULONG, ULONG);
    STDMETHODIMP put_crop_window(ULONG, ULONG, ULONG, ULONG);
    STDMETHODIMP put_crop_border(ULONG, ULONG, ULONG, ULONG);
    STDMETHODIMP get_teletext(long*);
    STDMETHODIMP put_teletext(long);
    STDMETHODIMP get_marquee(IVLCMarquee**);
    STDMETHODIMP get_logo(IVLCLogo**);
    STDMETHODIMP get_deinterlace(IVLCDeinterlace**);
    STDMETHODIMP takeSnapshot(LPPICTUREDISP*);
    STDMETHODIMP toggleFullscreen();
    STDMETHODIMP toggleTeletext();
    STDMETHODIMP get_track(long*);
    STDMETHODIMP put_track(long);
    STDMETHODIMP get_count(long*);
    STDMETHODIMP description(long, BSTR*);

private:
    IVLCMarquee     *_p_vlcmarquee;
    IVLCLogo        *_p_vlclogo;
    IVLCDeinterlace *_p_vlcdeint;
};

class VLCMediaDescription:
    public VLCInterface<VLCMediaDescription, IVLCMediaDescription>
{
public:
    VLCMediaDescription(VLCPlugin *p)
        : VLCInterface<VLCMediaDescription, IVLCMediaDescription>(p) { }

    // IVLCMediaDescription methods
    STDMETHODIMP get_title(BSTR*);
    STDMETHODIMP get_artist(BSTR*);
    STDMETHODIMP get_genre(BSTR*);
    STDMETHODIMP get_copyright(BSTR*);
    STDMETHODIMP get_album(BSTR*);
    STDMETHODIMP get_trackNumber(BSTR*);
    STDMETHODIMP get_description(BSTR*);
    STDMETHODIMP get_rating(BSTR*);
    STDMETHODIMP get_date(BSTR*);
    STDMETHODIMP get_setting(BSTR*);
    STDMETHODIMP get_URL(BSTR*);
    STDMETHODIMP get_language(BSTR*);
    STDMETHODIMP get_nowPlaying(BSTR*);
    STDMETHODIMP get_publisher(BSTR*);
    STDMETHODIMP get_encodedBy(BSTR*);
    STDMETHODIMP get_artworkURL(BSTR*);
    STDMETHODIMP get_trackID(BSTR*);

private:
    STDMETHODIMP get_meta(BSTR*, libvlc_meta_t);
};

class VLCControl2 : public IVLCControl2
{
public:

    VLCControl2(VLCPlugin *p_instance);
    virtual ~VLCControl2();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override
    {
        if( NULL == ppv )
          return E_POINTER;
        if( (IID_IUnknown == riid)
         || (IID_IDispatch == riid)
         || (IID_IVLCControl2 == riid) )
        {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        return _p_instance->pUnkOuter->QueryInterface(riid, ppv);
    };

    STDMETHODIMP_(ULONG) AddRef(void) override { return _p_instance->pUnkOuter->AddRef(); }
    STDMETHODIMP_(ULONG) Release(void) override { return _p_instance->pUnkOuter->Release(); }

    // IDispatch methods
    STDMETHODIMP GetTypeInfoCount(UINT*) override;
    STDMETHODIMP GetTypeInfo(UINT, LCID, LPTYPEINFO*) override;
    STDMETHODIMP GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override;
    STDMETHODIMP Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*) override;

    // IVLCControl2 methods
    STDMETHODIMP get_AutoLoop(VARIANT_BOOL *autoloop) override;
    STDMETHODIMP put_AutoLoop(VARIANT_BOOL autoloop) override;
    STDMETHODIMP get_AutoPlay(VARIANT_BOOL *autoplay) override;
    STDMETHODIMP put_AutoPlay(VARIANT_BOOL autoplay) override;
    STDMETHODIMP get_BaseURL(BSTR *url) override;
    STDMETHODIMP put_BaseURL(BSTR url) override;
    STDMETHODIMP get_MRL(BSTR *mrl) override;
    STDMETHODIMP put_MRL(BSTR mrl) override;
    STDMETHODIMP get_Toolbar(VARIANT_BOOL *visible) override;
    STDMETHODIMP put_Toolbar(VARIANT_BOOL visible) override;
    STDMETHODIMP get_StartTime(long *seconds) override;
    STDMETHODIMP put_StartTime(long seconds) override;
    STDMETHODIMP get_VersionInfo(BSTR *version) override;
    STDMETHODIMP getVersionInfo(BSTR *version) override;
    STDMETHODIMP get_Visible(VARIANT_BOOL *visible) override;
    STDMETHODIMP put_Visible(VARIANT_BOOL visible) override;
    STDMETHODIMP get_Volume(long *volume) override;
    STDMETHODIMP put_Volume(long volume) override;
    STDMETHODIMP get_BackColor(OLE_COLOR *backcolor) override;
    STDMETHODIMP put_BackColor(OLE_COLOR backcolor) override;
    STDMETHODIMP get_FullscreenEnabled(VARIANT_BOOL* enabled) override;
    STDMETHODIMP put_FullscreenEnabled(VARIANT_BOOL enabled) override;
    STDMETHODIMP get_Branding(VARIANT_BOOL* visible) override;
    STDMETHODIMP put_Branding(VARIANT_BOOL visible) override;

    STDMETHODIMP get_audio(IVLCAudio**) override;
    STDMETHODIMP get_input(IVLCInput**) override;
    STDMETHODIMP get_playlist(IVLCPlaylist**) override;
    STDMETHODIMP get_subtitle(IVLCSubtitle**) override;
    STDMETHODIMP get_video(IVLCVideo**) override;
    STDMETHODIMP get_mediaDescription(IVLCMediaDescription**) override;

protected:
    HRESULT loadTypeInfo();

private:
    VLCPlugin    *_p_instance;
    ITypeInfo    *_p_typeinfo;

    IVLCAudio    *_p_vlcaudio;
    IVLCInput    *_p_vlcinput;
    IVLCPlaylist *_p_vlcplaylist;
    IVLCSubtitle *_p_vlcsubtitle;
    IVLCVideo    *_p_vlcvideo;

    VLCMediaDescription    _p_vlcmedia_desc;
};

#endif
