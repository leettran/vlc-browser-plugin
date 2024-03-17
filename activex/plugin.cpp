/*****************************************************************************
 * plugin.cpp: ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2006-2010 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "plugin.h"

#include "oleobject.h"
#include "olecontrol.h"
#include "oleinplaceobject.h"
#include "oleinplaceactiveobject.h"
#include "persistpropbag.h"
#include "persiststreaminit.h"
#include "persiststorage.h"
#include "provideclassinfo.h"
#include "connectioncontainer.h"
#include "objectsafety.h"
#include "vlccontrol2.h"
#include "viewobject.h"
#include "dataobject.h"
#include "supporterrorinfo.h"

#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <winreg.h>
#include <winuser.h>
#include <servprov.h>
#include <shlwapi.h>
#include <wininet.h>
#include <windowsx.h>

using namespace std;

#define LEFT_BUTTON     0x01
#define RIGHT_BUTTON    0x02
#define MIDDLE_BUTTON   0x04

#define SHIFT_MASK      0x01
#define CTRL_MASK       0x02
#define ALT_MASK        0x04

////////////////////////////////////////////////////////////////////////
//class factory

static LRESULT CALLBACK VLCInPlaceClassWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    VLCPlugin *p_instance = reinterpret_cast<VLCPlugin *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch( uMsg )
    {
        case WM_ERASEBKGND:
            return 1L;

        case WM_PAINT:
            PAINTSTRUCT ps;
            RECT pr;
            if( GetUpdateRect(hWnd, &pr, FALSE) )
            {
                RECT bounds;
                GetClientRect(hWnd, &bounds);
                BeginPaint(hWnd, &ps);
                p_instance->onPaint(ps.hdc, bounds, pr);
                EndPaint(hWnd, &ps);
            }
            return 0L;
        case WM_SIZE:{
            int new_client_width = LOWORD(lParam);
            int new_client_height = HIWORD(lParam);
            //first child will be resized to client area
            HWND hChildWnd = GetWindow(hWnd, GW_CHILD);
            if(hChildWnd){
                MoveWindow(hChildWnd, 0, 0, new_client_width, new_client_height, FALSE);
            }
            return 0L;
        }
        case WM_CHAR:
        case WM_SYSKEYUP:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            p_instance->OnKeyEvent(uMsg, wParam, lParam);
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
};

VLCPluginClass::VLCPluginClass(LONG *p_class_ref, HINSTANCE hInstance, REFCLSID rclsid) :
    _p_class_ref(p_class_ref),
    _class_ref(0),
    _hinstance(hInstance),
    _classid(rclsid),
    _inplace_picture(NULL)
{
    WNDCLASS wClass;

    if( ! GetClassInfo(hInstance, getInPlaceWndClassName(), &wClass) )
    {
        wClass.style          = CS_NOCLOSE|CS_DBLCLKS;
        wClass.lpfnWndProc    = VLCInPlaceClassWndProc;
        wClass.cbClsExtra     = 0;
        wClass.cbWndExtra     = 0;
        wClass.hInstance      = hInstance;
        wClass.hIcon          = NULL;
        wClass.hCursor        = LoadCursor(NULL, IDC_ARROW);
        wClass.hbrBackground  = NULL;
        wClass.lpszMenuName   = NULL;
        wClass.lpszClassName  = getInPlaceWndClassName();

        _inplace_wndclass_atom = RegisterClass(&wClass);
    }
    else
    {
        _inplace_wndclass_atom = 0;
    }

    HBITMAP hbitmap = (HBITMAP)LoadImage(getHInstance(), MAKEINTRESOURCE(2), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
    if( NULL != hbitmap )
    {
        PICTDESC pictDesc;

        pictDesc.cbSizeofstruct = sizeof(PICTDESC);
        pictDesc.picType        = PICTYPE_BITMAP;
        pictDesc.bmp.hbitmap    = hbitmap;
        pictDesc.bmp.hpal       = NULL;

        if( FAILED(OleCreatePictureIndirect(&pictDesc, IID_IPicture, TRUE, reinterpret_cast<LPVOID*>(&_inplace_picture))) )
            _inplace_picture = NULL;
    }
    AddRef();
};

VLCPluginClass::~VLCPluginClass()
{
    if( 0 != _inplace_wndclass_atom )
        UnregisterClass(MAKEINTATOM(_inplace_wndclass_atom), _hinstance);

    if( NULL != _inplace_picture )
        _inplace_picture->Release();
};

STDMETHODIMP VLCPluginClass::QueryInterface(REFIID riid, void **ppv)
{
    if( NULL == ppv )
        return E_INVALIDARG;

    if( (IID_IUnknown == riid)
     || (IID_IClassFactory == riid) )
    {
        AddRef();
        *ppv = reinterpret_cast<LPVOID>(this);

        return NOERROR;
    }

    *ppv = NULL;

    return E_NOINTERFACE;
};

STDMETHODIMP_(ULONG) VLCPluginClass::AddRef(void)
{
    InterlockedIncrement(_p_class_ref);
    return ++_class_ref;
};

STDMETHODIMP_(ULONG) VLCPluginClass::Release(void)
{
    InterlockedDecrement(_p_class_ref);
    if( 0 == (--_class_ref) )
    {
        delete this;
        return 0;
    }
    return _class_ref;
};

STDMETHODIMP VLCPluginClass::CreateInstance(LPUNKNOWN pUnkOuter, REFIID riid, void **ppv)
{
    if( NULL == ppv )
        return E_POINTER;

    *ppv = NULL;

    if( (NULL != pUnkOuter) && (IID_IUnknown != riid) ) {
        return CLASS_E_NOAGGREGATION;
    }

    VLCPlugin *plugin = new VLCPlugin(this, pUnkOuter);
    if( NULL != plugin )
    {
        HRESULT hr = plugin->QueryInterface(riid, ppv);
        // the following will destroy the object if QueryInterface() failed
        plugin->Release();
        return hr;
    }
    return E_OUTOFMEMORY;
};

STDMETHODIMP VLCPluginClass::LockServer(BOOL fLock)
{
    if( fLock )
        AddRef();
    else
        Release();

    return S_OK;
};

////////////////////////////////////////////////////////////////////////
extern HMODULE DllGetModule();

VLCPlugin::VLCPlugin(VLCPluginClass *p_class, LPUNKNOWN pUnkOuter) :
    _inplacewnd(NULL),
    _WindowsManager(DllGetModule(), _ViewRC, &m_player, this, this),
    _p_class(p_class),
    _i_ref(1UL),
    _i_codepage(CP_ACP),
    _b_usermode(TRUE)
{
    _ViewRC.hDeFullscreenBitmap =
        LoadImage(DllGetModule(), MAKEINTRESOURCE(3),
                  IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS);

    _ViewRC.hPlayBitmap =
        LoadImage(DllGetModule(), MAKEINTRESOURCE(4),
                  IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS);

    _ViewRC.hPauseBitmap =
        LoadImage(DllGetModule(), MAKEINTRESOURCE(5),
                  IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS);

    _ViewRC.hVolumeBitmap =
        LoadImage(DllGetModule(), MAKEINTRESOURCE(6),
                  IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS);

    _ViewRC.hVolumeMutedBitmap =
        LoadImage(DllGetModule(), MAKEINTRESOURCE(7),
                  IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS);

    _ViewRC.hBackgroundIcon =
        (HICON) LoadImage(DllGetModule(), MAKEINTRESOURCE(8),
                          IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);

    _ViewRC.hFullscreenBitmap =
        LoadImage(DllGetModule(), MAKEINTRESOURCE(9),
                  IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS);

    p_class->AddRef();

    vlcOleControl = new VLCOleControl(this);
    vlcOleInPlaceObject = new VLCOleInPlaceObject(this);
    vlcOleInPlaceActiveObject = new VLCOleInPlaceActiveObject(this);
    vlcPersistStorage = new VLCPersistStorage(this);
    vlcPersistStreamInit = new VLCPersistStreamInit(this);
    vlcPersistPropertyBag = new VLCPersistPropertyBag(this);
    vlcProvideClassInfo = new VLCProvideClassInfo(this);
    vlcConnectionPointContainer = new VLCConnectionPointContainer(this);
    vlcObjectSafety = new VLCObjectSafety(this);
    vlcControl2 = new VLCControl2(this);
    vlcViewObject = new VLCViewObject(this);
    vlcDataObject = new VLCDataObject(this);
    vlcOleObject = new VLCOleObject(this);
    vlcSupportErrorInfo = new VLCSupportErrorInfo(this);

    // configure controlling IUnknown interface for implemented interfaces
    this->pUnkOuter = (NULL != pUnkOuter) ? pUnkOuter : dynamic_cast<LPUNKNOWN>(this);

    // default picure
    _p_pict = p_class->getInPlacePict();

    // make sure that persistable properties are initialized
    onInit();

    initVLC();
}

VLCPlugin::~VLCPlugin()
{
    delete vlcSupportErrorInfo;
    delete vlcOleObject;
    delete vlcDataObject;
    delete vlcViewObject;
    delete vlcControl2;
    delete vlcConnectionPointContainer;
    delete vlcProvideClassInfo;
    delete vlcPersistPropertyBag;
    delete vlcPersistStreamInit;
    delete vlcPersistStorage;
    delete vlcOleInPlaceActiveObject;
    delete vlcOleInPlaceObject;
    delete vlcObjectSafety;

    delete vlcOleControl;
    if( _p_pict )
        _p_pict->Release();

    SysFreeString(_bstr_mrl);
    SysFreeString(_bstr_baseurl);

    _p_class->Release();
}

STDMETHODIMP VLCPlugin::QueryInterface(REFIID riid, void **ppv)
{
    if( NULL == ppv )
        return E_INVALIDARG;

    if( IID_IUnknown == riid )
        *ppv = reinterpret_cast<LPVOID>(this);
    else if( IID_IOleObject == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcOleObject);
    else if( IID_IOleControl == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcOleControl);
    else if( IID_IOleWindow == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcOleInPlaceObject);
    else if( IID_IOleInPlaceObject == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcOleInPlaceObject);
    else if( IID_IOleInPlaceActiveObject == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcOleInPlaceActiveObject);
    else if( IID_IPersist == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcPersistStreamInit);
    else if( IID_IPersistStreamInit == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcPersistStreamInit);
    else if( IID_IPersistStorage == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcPersistStorage);
    else if( IID_IPersistPropertyBag == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcPersistPropertyBag);
    else if( IID_IProvideClassInfo == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcProvideClassInfo);
    else if( IID_IProvideClassInfo2 == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcProvideClassInfo);
    else if( IID_IConnectionPointContainer == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcConnectionPointContainer);
    else if (IID_IObjectSafety == riid)
        *ppv = reinterpret_cast<LPVOID>(vlcObjectSafety);
    else if( IID_IVLCControl2 == riid || IID_IDispatch == riid)
        *ppv = reinterpret_cast<LPVOID>(vlcControl2);
    else if( IID_IViewObject == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcViewObject);
    else if( IID_IViewObject2 == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcViewObject);
    else if( IID_IDataObject == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcDataObject);
    else if( IID_ISupportErrorInfo == riid )
        *ppv = reinterpret_cast<LPVOID>(vlcSupportErrorInfo);
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    ((LPUNKNOWN)*ppv)->AddRef();
    return NOERROR;
};

STDMETHODIMP_(ULONG) VLCPlugin::AddRef(void)
{
    return InterlockedIncrement((LONG *)&_i_ref);
};

STDMETHODIMP_(ULONG) VLCPlugin::Release(void)
{
    if( ! InterlockedDecrement((LONG *)&_i_ref) )
    {
        delete this;
        return 0;
    }
    return _i_ref;
}

//////////////////////////////////////

HRESULT VLCPlugin::onInit(void)
{
    // initialize persistable properties
    set_autoplay(true);
    _b_autoloop   = FALSE;
    _bstr_baseurl = NULL;
    _bstr_mrl     = NULL;
    _b_visible    = TRUE;
    _b_mute       = FALSE;
    _i_volume     = 100;
    _i_time       = 0;
    _i_backcolor  = 0;
    // set default/preferred size (320x240) pixels in HIMETRIC
    HDC hDC = CreateDevDC(NULL);
    _extent.cx = 320;
    _extent.cy = 240;
    HimetricFromDP(hDC, (LPPOINT)&_extent, 1);
    DeleteDC(hDC);
    return S_OK;
}

HRESULT VLCPlugin::onLoad(void)
{
    if( SysStringLen(_bstr_baseurl) == 0 )
    {
        /*
        ** try to retreive the base URL using the client site moniker, which for Internet Explorer
        ** is the URL of the page the plugin is embedded into.
        */
        LPOLECLIENTSITE pClientSite;
        if( SUCCEEDED(vlcOleObject->GetClientSite(&pClientSite)) && (NULL != pClientSite) )
        {
            IBindCtx *pBC = 0;
            if( SUCCEEDED(CreateBindCtx(0, &pBC)) )
            {
                LPMONIKER pContMoniker = NULL;
                if( SUCCEEDED(pClientSite->GetMoniker(OLEGETMONIKER_ONLYIFTHERE,
                                OLEWHICHMK_CONTAINER, &pContMoniker)) )
                {
                    LPOLESTR base_url;
                    if( SUCCEEDED(pContMoniker->GetDisplayName(pBC, NULL, &base_url)) )
                    {
                        /*
                        ** check that the moniker name is a URL
                        */
                        if( UrlIsW(base_url, URLIS_URL) )
                        {
                            /* copy base URL */
                            _bstr_baseurl = SysAllocString(base_url);
                        }
                        CoTaskMemFree(base_url);
                    }
                }
            }
        }
    }
    setDirty(FALSE);
    return S_OK;
}

void VLCPlugin::initVLC()
{
    try
    {
        static const char * const ppsz_argv[] = {
            "-vv",
            "--no-stats",
            "--intf=dummy",
            "--no-video-title-show",
        };

        auto instance = VLC::Instance( sizeof(ppsz_argv) / sizeof(*ppsz_argv), ppsz_argv );
        if( !m_player.open( instance ) )
            return;
    }
    catch (std::runtime_error&)
    {
        return;
    }

    // register player events
    player_register_events();

    if( !isInPlaceActive()  )
    {
        LPOLECLIENTSITE pClientSite;
        if( SUCCEEDED(vlcOleObject->GetClientSite(&pClientSite)) && (NULL != pClientSite) )
        {
            vlcOleObject->DoVerb(OLEIVERB_INPLACEACTIVATE, NULL, pClientSite, 0, NULL, NULL);
            pClientSite->Release();
        }
    }
};

void VLCPlugin::setErrorInfo(REFIID riid, const char *description)
{
    vlcSupportErrorInfo->setErrorInfo( getClassID() == CLSID_VLCPlugin2 ?
        OLESTR("VideoLAN.VLCPlugin.2") : OLESTR("VideoLAN.VLCPlugin.1"),
        riid, description );
};

HRESULT VLCPlugin::onAmbientChanged(LPUNKNOWN pContainer, DISPID dispID)
{
    VARIANT v;
    switch( dispID )
    {
        case DISPID_AMBIENT_BACKCOLOR:
            VariantInit(&v);
            V_VT(&v) = VT_I4;
            if( SUCCEEDED(GetObjectProperty(pContainer, dispID, v)) )
            {
                setBackColor(V_I4(&v));
            }
            break;
        case DISPID_AMBIENT_DISPLAYNAME:
            break;
        case DISPID_AMBIENT_FONT:
            break;
        case DISPID_AMBIENT_FORECOLOR:
            break;
        case DISPID_AMBIENT_LOCALEID:
            break;
        case DISPID_AMBIENT_MESSAGEREFLECT:
            break;
        case DISPID_AMBIENT_SCALEUNITS:
            break;
        case DISPID_AMBIENT_TEXTALIGN:
            break;
        case DISPID_AMBIENT_USERMODE:
            VariantInit(&v);
            V_VT(&v) = VT_BOOL;
            if( SUCCEEDED(GetObjectProperty(pContainer, dispID, v)) )
            {
                setUserMode(V_BOOL(&v) != VARIANT_FALSE);
            }
            break;
        case DISPID_AMBIENT_UIDEAD:
            break;
        case DISPID_AMBIENT_SHOWGRABHANDLES:
            break;
        case DISPID_AMBIENT_SHOWHATCHING:
            break;
        case DISPID_AMBIENT_DISPLAYASDEFAULT:
            break;
        case DISPID_AMBIENT_SUPPORTSMNEMONICS:
            break;
        case DISPID_AMBIENT_AUTOCLIP:
            break;
        case DISPID_AMBIENT_APPEARANCE:
            break;
        case DISPID_AMBIENT_CODEPAGE:
            VariantInit(&v);
            V_VT(&v) = VT_I4;
            if( SUCCEEDED(GetObjectProperty(pContainer, dispID, v)) )
            {
                setCodePage(V_I4(&v));
            }
            break;
        case DISPID_AMBIENT_PALETTE:
            break;
        case DISPID_AMBIENT_CHARSET:
            break;
        case DISPID_AMBIENT_RIGHTTOLEFT:
            break;
        case DISPID_AMBIENT_TOPTOBOTTOM:
            break;
        case DISPID_UNKNOWN:
            /*
            ** multiple property change, look up the ones we are interested in
            */
            VariantInit(&v);
            V_VT(&v) = VT_BOOL;
            if( SUCCEEDED(GetObjectProperty(pContainer, DISPID_AMBIENT_USERMODE, v)) )
            {
                setUserMode(V_BOOL(&v) != VARIANT_FALSE);
            }
            VariantInit(&v);
            V_VT(&v) = VT_I4;
            if( SUCCEEDED(GetObjectProperty(pContainer, DISPID_AMBIENT_CODEPAGE, v)) )
            {
                setCodePage(V_I4(&v));
            }
            break;
    }
    return S_OK;
}

HRESULT VLCPlugin::onClose(DWORD)
{
    if( isInPlaceActive() )
    {
        onInPlaceDeactivate();
    }
    vlcDataObject->onClose();
    return S_OK;
}

BOOL VLCPlugin::isInPlaceActive(void)
{
    return (NULL != _inplacewnd);
}

static short _shiftState()
{
    short shift = (GetKeyState(VK_SHIFT) < 0) ? SHIFT_MASK : 0;
    short ctrl  = (GetKeyState(VK_CONTROL) < 0) ? CTRL_MASK : 0;
    short alt   = (GetKeyState(VK_MENU) < 0) ? ALT_MASK : 0;
    return (shift | ctrl | alt);
}

void VLCPlugin::OnKeyEvent(UINT uKeyMsg, WPARAM wParam, LPARAM lParam)
{
    USHORT nChar = (USHORT)wParam;
    USHORT nShiftState = _shiftState();
    switch (uKeyMsg) {
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            fireKeyDownEvent(nChar , nShiftState);
            break;
        case WM_CHAR:
            fireKeyPressEvent(nChar);
            break;
        case WM_SYSKEYUP:
        case WM_KEYUP:
            fireKeyUpEvent(nChar , nShiftState);
            break;
    }
}

void VLCPlugin::OnMouseEvent(UINT uMouseMsg, WPARAM wParam, LPARAM lParam)
{
    short state = _shiftState();
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);
    switch (uMouseMsg) {
        case WM_MOUSEMOVE:
        {
            short button = 0;
            button |= (wParam & MK_LBUTTON) ? LEFT_BUTTON : 0;
            button |= (wParam & MK_MBUTTON) ? MIDDLE_BUTTON : 0;
            button |= (wParam & MK_RBUTTON) ? RIGHT_BUTTON : 0;
            fireMouseMoveEvent( button, state, x, y );
            break;
        }

        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
        case WM_LBUTTONDBLCLK:
            fireDblClickEvent();
            break;

        case WM_RBUTTONDOWN:
            fireMouseDownEvent( RIGHT_BUTTON, state , x, y );
            break;
        case WM_MBUTTONDOWN:
            fireMouseDownEvent( MIDDLE_BUTTON, state , x, y );
            break;
        case WM_LBUTTONDOWN:
            fireMouseDownEvent( LEFT_BUTTON, state , x, y );
            break;

        case WM_RBUTTONUP:
            fireMouseUpEvent( RIGHT_BUTTON, state , x, y );
            fireClickEvent();
            break;
        case WM_MBUTTONUP:
            fireMouseUpEvent( MIDDLE_BUTTON, state , x, y );
            fireClickEvent();
            break;
        case WM_LBUTTONUP:
            fireMouseUpEvent( LEFT_BUTTON, state , x, y );
            fireClickEvent();
            break;

        default:
            break;
    }
}

HRESULT VLCPlugin::onActivateInPlace(LPMSG, HWND hwndParent, LPCRECT lprcPosRect, LPCRECT lprcClipRect)
{
    RECT clipRect = *lprcClipRect;

    /*
    ** record keeping of control geometry within container
    */
    _posRect = *lprcPosRect;

    /*
    ** Create a window for in place activated control.
    ** the window geometry matches the control viewport
    ** within container so that embedded video is always
    ** properly displayed.
    */
    _inplacewnd = CreateWindow(_p_class->getInPlaceWndClassName(),
            TEXT("VLC Plugin In-Place Window"),
            WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
            lprcPosRect->left,
            lprcPosRect->top,
            lprcPosRect->right-lprcPosRect->left,
            lprcPosRect->bottom-lprcPosRect->top,
            hwndParent,
            0,
            _p_class->getHInstance(),
            NULL
           );

    if( NULL == _inplacewnd )
        return E_FAIL;

    SetWindowLongPtr(_inplacewnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    /* change cliprect coordinates system relative to window bounding rect */
    OffsetRect(&clipRect, -lprcPosRect->left, -lprcPosRect->top);

    HRGN clipRgn = CreateRectRgnIndirect(&clipRect);
    SetWindowRgn(_inplacewnd, clipRgn, TRUE);

    if( isUserMode() )
    {
        _WindowsManager.CreateWindows(this->getInPlaceWindow());

        // initial playlist item
        if( SysStringLen(_bstr_mrl) > 0 )
        {
            char *psz_mrl = NULL;

            if( SysStringLen(_bstr_baseurl) > 0 )
            {
                /*
                ** if the MRL a relative URL, we should end up with an absolute URL
                */
                LPWSTR abs_url = CombineURL(_bstr_baseurl, _bstr_mrl);
                if( NULL != abs_url )
                {
                    psz_mrl = CStrFromWSTR(CP_UTF8, abs_url, wcslen(abs_url));
                    CoTaskMemFree(abs_url);
                }
                else
                {
                    psz_mrl = CStrFromBSTR(CP_UTF8, _bstr_mrl);
                }
            }
            else
            {
                /*
                ** baseURL is empty, assume MRL is absolute
                */
                psz_mrl = CStrFromBSTR(CP_UTF8, _bstr_mrl);
            }
            if( NULL != psz_mrl )
            {
                const char *options[1];
                int i_options = 0;

                char timeBuffer[32];
                if( _i_time )
                {
                    snprintf(timeBuffer, sizeof(timeBuffer), ":start-time=%d", _i_time);
                    options[i_options++] = timeBuffer;
                }
                // add default target to playlist
                m_player.add_item( psz_mrl, i_options, options);
                CoTaskMemFree(psz_mrl);
            }
        }

        if( get_autoplay() )
        {
            get_player().play();
        }
    }

    if( isVisible() )
        ShowWindow(_inplacewnd, SW_SHOW);

    set_player_window();

    return S_OK;
};

void VLCPlugin::toggleFullscreen()
{
    _WindowsManager.ToggleFullScreen();
}

void VLCPlugin::setFullscreen(BOOL yes)
{
    if( yes )
        _WindowsManager.StartFullScreen();
    else
        _WindowsManager.EndFullScreen();
}

BOOL VLCPlugin::getFullscreen()
{
    return _WindowsManager.IsFullScreen();
}

HRESULT VLCPlugin::onInPlaceDeactivate(void)
{
    if( m_player.mlp().isPlaying() )
    {
        m_player.mlp().stopAsync();
    }

    _WindowsManager.DestroyWindows();

    DestroyWindow(_inplacewnd);
    _inplacewnd = NULL;

    return S_OK;
};

void VLCPlugin::setVisible(BOOL fVisible)
{
    if( fVisible != _b_visible )
    {
        _b_visible = fVisible;
        if( isInPlaceActive() )
        {
            ShowWindow(_inplacewnd, fVisible ? SW_SHOW : SW_HIDE);
            if( fVisible )
                InvalidateRect(_inplacewnd, NULL, TRUE);
        }
        setDirty(TRUE);
        firePropChangedEvent(DISPID_Visible);
    }
};

void VLCPlugin::setVolume(int volume)
{
    if( volume < 0 )
        volume = 0;
    else if( volume > 200 )
        volume = 200;

    if( volume != _i_volume )
    {
        _i_volume = volume;
        if ( m_player.get_mp().setVolume( volume ) )
            setDirty(TRUE);
    }
}

void VLCPlugin::setBackColor(OLE_COLOR backcolor)
{
    if( _i_backcolor != backcolor )
    {
        _i_backcolor = backcolor;
        if( isInPlaceActive() )
        {

        }
        setDirty(TRUE);
    }
};

void VLCPlugin::setTime(int seconds)
{
    if( seconds < 0 )
        seconds = 0;

    if( seconds != _i_time )
    {
        setStartTime(_i_time);
        m_player.get_mp().setTime( _i_time, true );
    }
}

void VLCPlugin::setMute(BOOL mute)
{
    if( mute != _b_mute )
    {
        _b_mute = mute;
        m_player.get_mp().setMute( _b_mute );
    }
}

void VLCPlugin::setFocus(BOOL fFocus)
{
    if( fFocus )
        SetActiveWindow(_inplacewnd);
};

BOOL VLCPlugin::hasFocus(void)
{
    return GetActiveWindow() == _inplacewnd;
};

void VLCPlugin::onDraw(DVTARGETDEVICE * ptd, HDC hicTargetDev,
        HDC hdcDraw, LPCRECTL lprcBounds, LPCRECTL lprcWBounds)
{
    if( isVisible() )
    {
        long width = lprcBounds->right-lprcBounds->left;
        long height = lprcBounds->bottom-lprcBounds->top;

        RECT bounds = { lprcBounds->left, lprcBounds->top, lprcBounds->right, lprcBounds->bottom };

        if( isUserMode() )
        {
            /* VLC is in user mode, just draw background color */
            COLORREF colorref = RGB(0, 0, 0);
            OleTranslateColor(_i_backcolor, (HPALETTE)GetStockObject(DEFAULT_PALETTE), &colorref);
            if( colorref != RGB(0, 0, 0) )
            {
                /* custom background */
                HBRUSH colorbrush = CreateSolidBrush(colorref);
                FillRect(hdcDraw, &bounds, colorbrush);
                DeleteObject((HANDLE)colorbrush);
            }
            else
            {
                /* black background */
                FillRect(hdcDraw, &bounds, (HBRUSH)GetStockObject(BLACK_BRUSH));
            }
        }
        else
        {
            /* VLC is in design mode, draw the VLC logo */
            FillRect(hdcDraw, &bounds, (HBRUSH)GetStockObject(WHITE_BRUSH));

            LPPICTURE pict = getPicture();
            if( NULL != pict )
            {
                OLE_XSIZE_HIMETRIC picWidth;
                OLE_YSIZE_HIMETRIC picHeight;

                pict->get_Width(&picWidth);
                pict->get_Height(&picHeight);

                SIZEL picSize = { picWidth, picHeight };

                if( NULL != hicTargetDev )
                {
                    DPFromHimetric(hicTargetDev, (LPPOINT)&picSize, 1);
                }
                else if( NULL != (hicTargetDev = CreateDevDC(ptd)) )
                {
                    DPFromHimetric(hicTargetDev, (LPPOINT)&picSize, 1);
                    DeleteDC(hicTargetDev);
                }

                if( picSize.cx > width-4 )
                    picSize.cx = width-4;
                if( picSize.cy > height-4 )
                    picSize.cy = height-4;

                LONG dstX = lprcBounds->left+(width-picSize.cx)/2;
                LONG dstY = lprcBounds->top+(height-picSize.cy)/2;

                if( NULL != lprcWBounds )
                {
                    RECT wBounds = { lprcWBounds->left, lprcWBounds->top, lprcWBounds->right, lprcWBounds->bottom };
                    pict->Render(hdcDraw, dstX, dstY, picSize.cx, picSize.cy,
                            0L, picHeight, picWidth, -picHeight, &wBounds);
                }
                else
                    pict->Render(hdcDraw, dstX, dstY, picSize.cx, picSize.cy,
                            0L, picHeight, picWidth, -picHeight, NULL);

                pict->Release();
            }

            SelectObject(hdcDraw, GetStockObject(BLACK_BRUSH));

            MoveToEx(hdcDraw, bounds.left, bounds.top, NULL);
            LineTo(hdcDraw, bounds.left+width-1, bounds.top);
            LineTo(hdcDraw, bounds.left+width-1, bounds.top+height-1);
            LineTo(hdcDraw, bounds.left, bounds.top+height-1);
            LineTo(hdcDraw, bounds.left, bounds.top);
        }
    }
};

void VLCPlugin::onPaint(HDC hdc, const RECT &bounds, const RECT &)
{
    if( isVisible() )
    {
        /* if VLC is in design mode, draw control logo */
        HDC hdcDraw = CreateCompatibleDC(hdc);
        if( NULL != hdcDraw )
        {
            SIZEL size = getExtent();
            DPFromHimetric(hdc, (LPPOINT)&size, 1);
            RECTL posRect = { 0, 0, size.cx, size.cy };

            int width = bounds.right-bounds.left;
            int height = bounds.bottom-bounds.top;

            HBITMAP hBitmap = CreateCompatibleBitmap(hdc, width, height);
            if( NULL != hBitmap )
            {
                HBITMAP oldBmp = (HBITMAP)SelectObject(hdcDraw, hBitmap);

                if( (size.cx != width) || (size.cy != height) )
                {
                    // needs to scale canvas
                    SetMapMode(hdcDraw, MM_ANISOTROPIC);
                    SetWindowExtEx(hdcDraw, size.cx, size.cy, NULL);
                    SetViewportExtEx(hdcDraw, width, height, NULL);
                }

                onDraw(NULL, hdc, hdcDraw, &posRect, NULL);

                SetMapMode(hdcDraw, MM_TEXT);
                BitBlt(hdc, bounds.left, bounds.top,
                        width, height,
                        hdcDraw, 0, 0,
                        SRCCOPY);

                SelectObject(hdcDraw, oldBmp);
                DeleteObject(hBitmap);
            }
            DeleteDC(hdcDraw);
        }
    }
};

void VLCPlugin::onPositionChange(LPCRECT lprcPosRect, LPCRECT lprcClipRect)
{
    RECT clipRect = *lprcClipRect;

    //RedrawWindow(GetParent(_inplacewnd), &_posRect, NULL, RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);

    /*
    ** record keeping of control geometry within container
    */
    _posRect = *lprcPosRect;

    /*
    ** change in-place window geometry to match clipping region
    */
    SetWindowPos(_inplacewnd, NULL,
            lprcPosRect->left,
            lprcPosRect->top,
            lprcPosRect->right-lprcPosRect->left,
            lprcPosRect->bottom-lprcPosRect->top,
            SWP_NOACTIVATE|
            SWP_NOCOPYBITS|
            SWP_NOZORDER|
            SWP_NOOWNERZORDER );

    /* change cliprect coordinates system relative to window bounding rect */
    OffsetRect(&clipRect, -lprcPosRect->left, -lprcPosRect->top);
    HRGN clipRgn = CreateRectRgnIndirect(&clipRect);
    SetWindowRgn(_inplacewnd, clipRgn, FALSE);

    //RedrawWindow(_videownd, &posRect, NULL, RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
};

void VLCPlugin::freezeEvents(BOOL freeze)
{
    vlcConnectionPointContainer->freezeEvents(freeze);
};

void VLCPlugin::firePropChangedEvent(DISPID dispid)
{
    vlcConnectionPointContainer->firePropChangedEvent(dispid);
};

/*
 * Async events
 */
void VLCPlugin::fireOnMediaPlayerMediaChangedEvent()
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerMediaChangedEvent, &dispparamsNoArgs);
};

void VLCPlugin::fireOnMediaPlayerNothingSpecialEvent()
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerNothingSpecialEvent, &dispparamsNoArgs);
};

void VLCPlugin::fireOnMediaPlayerOpeningEvent()
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerOpeningEvent, &dispparamsNoArgs);
};

void VLCPlugin::fireOnMediaPlayerBufferingEvent(float cache)
{
    DISPPARAMS params;
    params.cArgs = 1;
    params.rgvarg = (VARIANTARG *) CoTaskMemAlloc(sizeof(VARIANTARG) * params.cArgs) ;
    memset(params.rgvarg, 0, sizeof(VARIANTARG) * params.cArgs);
    params.rgvarg[0].vt = VT_I4;
    params.rgvarg[0].lVal = static_cast<LONG>(cache);
    params.rgdispidNamedArgs = NULL;
    params.cNamedArgs = 0;
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerBufferingEvent, &params);
};

void VLCPlugin::fireOnMediaPlayerPlayingEvent()
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerPlayingEvent, &dispparamsNoArgs);
};

void VLCPlugin::fireOnMediaPlayerPausedEvent()
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerPausedEvent, &dispparamsNoArgs);
};

void VLCPlugin::fireOnMediaPlayerEncounteredErrorEvent()
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerEncounteredErrorEvent, &dispparamsNoArgs);
};

void VLCPlugin::fireOnMediaPlayerEndReachedEvent()
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerEndReachedEvent, &dispparamsNoArgs);
};

void VLCPlugin::fireOnMediaPlayerStoppedEvent()
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerStoppedEvent, &dispparamsNoArgs);
};

void VLCPlugin::fireOnMediaPlayerStopAsyncDoneEvent()
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerStopAsyncDoneEvent, &dispparamsNoArgs);
};


void VLCPlugin::fireOnMediaPlayerForwardEvent()
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerForwardEvent, &dispparamsNoArgs);
};

void VLCPlugin::fireOnMediaPlayerBackwardEvent()
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerBackwardEvent, &dispparamsNoArgs);
};

void VLCPlugin::fireOnMediaPlayerTimeChangedEvent(libvlc_time_t  time)
{
    DISPPARAMS params;
    params.cArgs = 1;
    params.rgvarg = (VARIANTARG *) CoTaskMemAlloc(sizeof(VARIANTARG) * params.cArgs) ;
    memset(params.rgvarg, 0, sizeof(VARIANTARG) * params.cArgs);
    params.rgvarg[0].vt = VT_I4;
    params.rgvarg[0].lVal = static_cast<LONG>(time);
    params.rgdispidNamedArgs = NULL;
    params.cNamedArgs = 0;
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerTimeChangedEvent, &params);
};

void VLCPlugin::fireOnMediaPlayerPositionChangedEvent(float position)
{
    DISPPARAMS params;
    params.cArgs = 1;
    params.rgvarg = (VARIANTARG *) CoTaskMemAlloc(sizeof(VARIANTARG) * params.cArgs) ;
    memset(params.rgvarg, 0, sizeof(VARIANTARG) * params.cArgs);
    params.rgvarg[0].vt = VT_R4;
    params.rgvarg[0].fltVal = position;
    params.rgdispidNamedArgs = NULL;
    params.cNamedArgs = 0;
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerPositionChangedEvent, &params);
};

void VLCPlugin::fireOnMediaPlayerSeekableChangedEvent(VARIANT_BOOL seekable)
{
    DISPPARAMS params;
    params.cArgs = 1;
    params.rgvarg = (VARIANTARG *) CoTaskMemAlloc(sizeof(VARIANTARG) * params.cArgs) ;
    memset(params.rgvarg, 0, sizeof(VARIANTARG) * params.cArgs);
    params.rgvarg[0].vt = VT_BOOL;
    params.rgvarg[0].boolVal = seekable;
    params.rgdispidNamedArgs = NULL;
    params.cNamedArgs = 0;
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerSeekableChangedEvent, &params);
}

void VLCPlugin::fireOnMediaPlayerPausableChangedEvent(VARIANT_BOOL pausable)
{
    DISPPARAMS params;
    params.cArgs = 1;
    params.rgvarg = (VARIANTARG *) CoTaskMemAlloc(sizeof(VARIANTARG) * params.cArgs) ;
    memset(params.rgvarg, 0, sizeof(VARIANTARG) * params.cArgs);
    params.rgvarg[0].vt = VT_BOOL;
    params.rgvarg[0].boolVal = pausable;
    params.rgdispidNamedArgs = NULL;
    params.cNamedArgs = 0;
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerPausableChangedEvent, &params);
};

void VLCPlugin::fireOnMediaPlayerTitleChangedEvent(int title)
{
    DISPPARAMS params;
    params.cArgs = 1;
    params.rgvarg = (VARIANTARG *) CoTaskMemAlloc(sizeof(VARIANTARG) * params.cArgs) ;
    memset(params.rgvarg, 0, sizeof(VARIANTARG) * params.cArgs);
    params.rgvarg[0].vt = VT_I2;
    params.rgvarg[0].iVal = title;
    params.rgdispidNamedArgs = NULL;
    params.cNamedArgs = 0;
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerTitleChangedEvent, &params);
};

void VLCPlugin::fireOnMediaPlayerLengthChangedEvent(long length)
{
    DISPPARAMS params;
    params.cArgs = 1;
    params.rgvarg = (VARIANTARG *) CoTaskMemAlloc(sizeof(VARIANTARG) * params.cArgs) ;
    memset(params.rgvarg, 0, sizeof(VARIANTARG) * params.cArgs);
    params.rgvarg[0].vt = VT_I4;
    params.rgvarg[0].lVal = length;
    params.rgdispidNamedArgs = NULL;
    params.cNamedArgs = 0;
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerLengthChangedEvent, &params);
}

void VLCPlugin::fireClickEvent()
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    vlcConnectionPointContainer->fireEvent(DISPID_CLICK, &dispparamsNoArgs);
}

void VLCPlugin::fireDblClickEvent()
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    vlcConnectionPointContainer->fireEvent(DISPID_DBLCLICK, &dispparamsNoArgs);
}

void VLCPlugin::fireMouseDownEvent(short nButton, short nShiftState, int x, int y)
{
    DISPPARAMS params;
    params.cArgs = 4;
    params.rgvarg = (VARIANTARG *) CoTaskMemAlloc(sizeof(VARIANTARG) * params.cArgs);
    memset(params.rgvarg, 0, sizeof(VARIANTARG) * params.cArgs);
    params.rgvarg[3].vt = VT_I2;
    params.rgvarg[3].iVal = nButton;
    params.rgvarg[2].vt = VT_I2;
    params.rgvarg[2].iVal = nShiftState;
    params.rgvarg[1].vt = VT_I4;
    params.rgvarg[1].lVal = x;
    params.rgvarg[0].vt = VT_I4;
    params.rgvarg[0].lVal = y;
    params.rgdispidNamedArgs = NULL;
    params.cNamedArgs = 0;
    vlcConnectionPointContainer->fireEvent(DISPID_MOUSEDOWN, &params);
}

void VLCPlugin::fireMouseMoveEvent(short nButton, short nShiftState, int x, int y)
{
    DISPPARAMS params;
    params.cArgs = 4;
    params.rgvarg = (VARIANTARG *) CoTaskMemAlloc(sizeof(VARIANTARG) * params.cArgs);
    memset(params.rgvarg, 0, sizeof(VARIANTARG) * params.cArgs);
    params.rgvarg[3].vt = VT_I2;
    params.rgvarg[3].iVal = nButton;
    params.rgvarg[2].vt = VT_I2;
    params.rgvarg[2].iVal = nShiftState;
    params.rgvarg[1].vt = VT_I4;
    params.rgvarg[1].lVal = x;
    params.rgvarg[0].vt = VT_I4;
    params.rgvarg[0].lVal = y;
    params.rgdispidNamedArgs = NULL;
    params.cNamedArgs = 0;
    vlcConnectionPointContainer->fireEvent(DISPID_MOUSEMOVE, &params);
}


void VLCPlugin::fireMouseUpEvent(short nButton, short nShiftState, int x, int y)
{
    DISPPARAMS params;
    params.cArgs = 4;
    params.rgvarg = (VARIANTARG *) CoTaskMemAlloc(sizeof(VARIANTARG) * params.cArgs);
    memset(params.rgvarg, 0, sizeof(VARIANTARG) * params.cArgs);
    params.rgvarg[3].vt = VT_I2;
    params.rgvarg[3].iVal = nButton;
    params.rgvarg[2].vt = VT_I2;
    params.rgvarg[2].iVal = nShiftState;
    params.rgvarg[1].vt = VT_I4;
    params.rgvarg[1].lVal = x;
    params.rgvarg[0].vt = VT_I4;
    params.rgvarg[0].lVal = y;
    params.rgdispidNamedArgs = NULL;
    params.cNamedArgs = 0;
    vlcConnectionPointContainer->fireEvent(DISPID_MOUSEUP, &params);
}

void VLCPlugin::fireKeyDownEvent(short nChar, short nShiftState)
{
    DISPPARAMS params;
    params.cArgs = 2;
    params.rgvarg = (VARIANTARG *) CoTaskMemAlloc(sizeof(VARIANTARG) * params.cArgs);
    memset(params.rgvarg, 0, sizeof(VARIANTARG) * params.cArgs);
    //freed by VLCDispatchEvent::~VLCDispatchEvent
    params.rgvarg[1].vt = VT_I2 | VT_BYREF;
    params.rgvarg[1].piVal = new short(nChar);
    params.rgvarg[0].vt = VT_I2;
    params.rgvarg[0].iVal = nShiftState;
    params.rgdispidNamedArgs = NULL;
    params.cNamedArgs = 0;
    vlcConnectionPointContainer->fireEvent(DISPID_KEYDOWN, &params);
}

void VLCPlugin::fireKeyPressEvent(short nChar)
{
    DISPPARAMS params;
    params.cArgs = 1;
    params.rgvarg = (VARIANTARG *) CoTaskMemAlloc(sizeof(VARIANTARG) * params.cArgs);
    memset(params.rgvarg, 0, sizeof(VARIANTARG) * params.cArgs);
    //freed by VLCDispatchEvent::~VLCDispatchEvent
    params.rgvarg[0].vt = VT_I2 | VT_BYREF;
    params.rgvarg[0].piVal = new short(nChar);
    params.rgdispidNamedArgs = NULL;
    params.cNamedArgs = 0;
    vlcConnectionPointContainer->fireEvent(DISPID_KEYPRESS, &params);
}

void VLCPlugin::fireKeyUpEvent(short nChar, short nShiftState)
{
    DISPPARAMS params;
    params.cArgs = 2;
    params.rgvarg = (VARIANTARG *) CoTaskMemAlloc(sizeof(VARIANTARG) * params.cArgs);
    memset(params.rgvarg, 0, sizeof(VARIANTARG) * params.cArgs);
    //freed by VLCDispatchEvent::~VLCDispatchEvent
    params.rgvarg[1].vt = VT_I2 | VT_BYREF;
    params.rgvarg[1].piVal = new short(nChar);
    params.rgvarg[0].vt = VT_I2;
    params.rgvarg[0].iVal = nShiftState;
    params.rgdispidNamedArgs = NULL;
    params.cNamedArgs = 0;
    vlcConnectionPointContainer->fireEvent(DISPID_KEYUP, &params);
}

void VLCPlugin::fireOnMediaPlayerVoutEvent(int count)
{
    DISPPARAMS params;
    params.cArgs = 1;
    params.rgvarg = (VARIANTARG *) CoTaskMemAlloc(sizeof(VARIANTARG) * params.cArgs) ;
    memset(params.rgvarg, 0, sizeof(VARIANTARG) * params.cArgs);
    params.rgvarg[0].vt = VT_I2;
    params.rgvarg[0].iVal = count;
    params.rgdispidNamedArgs = NULL;
    params.cNamedArgs = 0;
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerVoutEvent, &params);
}

void VLCPlugin::fireOnMediaPlayerMutedEvent()
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerMutedEvent, &dispparamsNoArgs);
}

void VLCPlugin::fireOnMediaPlayerUnmutedEvent()
{
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerUnmutedEvent, &dispparamsNoArgs);
}

void VLCPlugin::fireOnMediaPlayerAudioVolumeEvent(float volume)
{
    DISPPARAMS params;
    params.cArgs = 1;
    params.rgvarg = (VARIANTARG *) CoTaskMemAlloc(sizeof(VARIANTARG) * params.cArgs) ;
    memset(params.rgvarg, 0, sizeof(VARIANTARG) * params.cArgs);
    params.rgvarg[0].vt = VT_R4;
    params.rgvarg[0].fltVal = volume;
    params.rgdispidNamedArgs = NULL;
    params.cNamedArgs = 0;
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerAudioVolumeEvent, &params);
}

void VLCPlugin::fireOnMediaPlayerChapterChangedEvent(int chapter)
{
    DISPPARAMS params;
    params.cArgs = 1;
    params.rgvarg = (VARIANTARG *) CoTaskMemAlloc(sizeof(VARIANTARG) * params.cArgs) ;
    memset(params.rgvarg, 0, sizeof(VARIANTARG) * params.cArgs);
    params.rgvarg[0].vt = VT_I2;
    params.rgvarg[0].iVal = chapter;
    params.rgdispidNamedArgs = NULL;
    params.cNamedArgs = 0;
    vlcConnectionPointContainer->fireEvent(DISPID_MediaPlayerChapterChangedEvent, &params);
}

/* */

void VLCPlugin::set_player_window()
{
    if (_WindowsManager.getHolderWnd())
        m_player.get_mp().setHwnd( _WindowsManager.getHolderWnd()->hWnd() );
}

#define B(val) ((val) ? 0xFFFF : 0x0000)

void VLCPlugin::player_register_events()
{
    auto& em = m_player.get_mp().eventManager();
    em.onMediaChanged([this](VLC::MediaPtr) {
        fireOnMediaPlayerMediaChangedEvent();
    });
    em.onNothingSpecial([this] {
        fireOnMediaPlayerNothingSpecialEvent();
    });
    em.onOpening([this] {
        fireOnMediaPlayerOpeningEvent();
    });
    em.onBuffering([this](float b) {
        fireOnMediaPlayerBufferingEvent(b);
    });
    em.onPlaying([this] {
        fireOnMediaPlayerPlayingEvent();
    });
    em.onPaused([this] {
        fireOnMediaPlayerPausedEvent();
    });
    em.onStopped([this] {
        fireOnMediaPlayerStoppedEvent();
    });
    em.onForward([this] {
        fireOnMediaPlayerForwardEvent();
    });
    em.onBackward([this] {
        fireOnMediaPlayerBackwardEvent();
    });
    em.onStopping([this] {
        fireOnMediaPlayerEndReachedEvent();
    });
    em.onEncounteredError([this] {
        fireOnMediaPlayerEncounteredErrorEvent();
    });
    em.onTimeChanged([this] (int64_t time) {
        fireOnMediaPlayerTimeChangedEvent( time );
    });
    em.onPositionChanged([this](float pos) {
        fireOnMediaPlayerPositionChangedEvent( pos );
    });
    em.onSeekableChanged([this](bool b) {
        fireOnMediaPlayerSeekableChangedEvent( B( b ) );
    });
    em.onPausableChanged([this](bool b) {
        fireOnMediaPlayerPausableChangedEvent( B( b ) );
    });
    em.onTitleSelectionChanged([this](const VLC::TitleDescription&, int t) {
        fireOnMediaPlayerTitleChangedEvent( t );
    });
    em.onLengthChanged( [this]( int64_t length ) {
        fireOnMediaPlayerLengthChangedEvent( length );
    });
    em.onChapterChanged( [this]( int chapter ) {
        fireOnMediaPlayerChapterChangedEvent( chapter );
    });
    em.onVout( [this]( int count ) {
        fireOnMediaPlayerVoutEvent( count );
    });
    em.onMuted( [this] {
        fireOnMediaPlayerMutedEvent();
    });
    em.onUnmuted( [this] {
        fireOnMediaPlayerUnmutedEvent();
    });
    em.onAudioVolume( [this]( float volume ) {
        fireOnMediaPlayerAudioVolumeEvent( volume );
    });
}

#undef B
