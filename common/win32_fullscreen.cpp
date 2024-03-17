/*****************************************************************************
 * vlc_win32_fullscreen.h: a VLC plugin for Mozilla
 *****************************************************************************
 * Copyright © 2002-2011 VideoLAN and VLC authors
 * $Id$
 *
 * Authors: Sergey Radionov <rsatom@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <math.h>

#include "win32_fullscreen.h"

////////////////////////////////////////////////////////////////////////////////
//VLCControlsWnd members
////////////////////////////////////////////////////////////////////////////////
VLCControlsWnd*
VLCControlsWnd::CreateControlsWindow(HINSTANCE hInstance,
                                     VLCWindowsManager* wm, HWND hWndParent)
{
    VLCControlsWnd* wnd = new VLCControlsWnd(hInstance, wm);
    if( wnd && wnd->Create(hWndParent) ) {
        return wnd;
    }
    delete wnd;
    return 0;
}

VLCControlsWnd::VLCControlsWnd(HINSTANCE hInstance, VLCWindowsManager* wm)
    :VLCWnd(hInstance), _wm(wm),
     hToolTipWnd(0), hFSButton(0), hPlayPauseButton(0),
     hVideoPosScroll(0), hMuteButton(0), hVolumeSlider(0)
{
}

VLCControlsWnd::~VLCControlsWnd()
{
    if(hToolTipWnd){
        ::DestroyWindow(hToolTipWnd);
        hToolTipWnd = 0;
    }
}

bool VLCControlsWnd::Create(HWND hWndParent)
{
    return VLCWnd::CreateEx(WS_EX_TOPMOST, TEXT("VLC Controls Window"),
                            WS_CHILD|WS_CLIPSIBLINGS,
                            0, 0, 0, 0, hWndParent, 0);
}

void VLCControlsWnd::PreRegisterWindowClass(WNDCLASS* wc)
{
    wc->lpszClassName = TEXT("VLC Controls Class");
    wc->hbrBackground = (HBRUSH)(COLOR_3DFACE+1);
};

void VLCControlsWnd::CreateToolTip()
{
    hToolTipWnd = CreateWindowEx(WS_EX_TOPMOST,
            TOOLTIPS_CLASS,
            NULL,
            WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            hWnd(),
            NULL,
            hInstance(),
            NULL);

    SetWindowPos(hToolTipWnd,
            HWND_TOPMOST,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);


    TOOLINFO ti;
    ti.cbSize = sizeof(TOOLINFO);
    ti.uFlags = TTF_SUBCLASS|TTF_IDISHWND;
    ti.hwnd   = hWnd();
    ti.hinst  = hInstance();

    TCHAR HintText[100];
    RECT ActivateTTRect;

    //end fullscreen button tooltip
    GetWindowRect(hFSButton, &ActivateTTRect);
    GetWindowText(hFSButton, HintText, sizeof(HintText));
    ti.uId = (UINT_PTR)hFSButton;
    ti.rect = ActivateTTRect;
    ti.lpszText = HintText;
    SendMessage(hToolTipWnd, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);

    //play/pause button tooltip
    GetWindowRect(hPlayPauseButton, &ActivateTTRect);
    GetWindowText(hPlayPauseButton, HintText, sizeof(HintText));
    ti.uId = (UINT_PTR)hPlayPauseButton;
    ti.rect = ActivateTTRect;
    ti.lpszText = HintText;
    SendMessage(hToolTipWnd, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);

    //mute button tooltip
    GetWindowRect(hMuteButton, &ActivateTTRect);
    GetWindowText(hMuteButton, HintText, sizeof(HintText));
    ti.uId = (UINT_PTR)hMuteButton;
    ti.rect = ActivateTTRect;
    ti.lpszText = HintText;
    SendMessage(hToolTipWnd, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);
}

LRESULT VLCControlsWnd::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg){
        case WM_CREATE:{
            const int ControlsHeight = 21+3;
            const int ButtonsWidth = ControlsHeight;

            int HorizontalOffset = xControlsSpace;
            int ControlWidth = ButtonsWidth;
            hPlayPauseButton =
                CreateWindow(TEXT("BUTTON"), TEXT("Play/Pause"),
                             WS_CHILD|WS_VISIBLE|BS_BITMAP|BS_FLAT,
                             HorizontalOffset, xControlsSpace,
                             ControlWidth, ControlsHeight, hWnd(),
                             (HMENU)ID_FS_PLAY_PAUSE, 0, 0);
            SendMessage(hPlayPauseButton, BM_SETIMAGE,
                        (WPARAM)IMAGE_BITMAP, (LPARAM)RC().hPlayBitmap);
            HorizontalOffset+=ControlWidth+xControlsSpace;

            ControlWidth = 200;
            int VideoPosControlHeight = 10;
            hVideoPosScroll =
                CreateWindow(PROGRESS_CLASS, TEXT("Video Position"),
                             WS_CHILD|WS_DISABLED|WS_VISIBLE|SBS_HORZ|SBS_TOPALIGN|PBS_SMOOTH,
                             HorizontalOffset, xControlsSpace+(ControlsHeight-VideoPosControlHeight)/2,
                             ControlWidth, VideoPosControlHeight, hWnd(),
                             (HMENU)ID_FS_VIDEO_POS_SCROLL, 0, 0);
            HMODULE hThModule = LoadLibrary(TEXT("UxTheme.dll"));
            if(hThModule){
                FARPROC proc = GetProcAddress(hThModule, "SetWindowTheme");
                typedef HRESULT (WINAPI* SetWindowThemeProc)(HWND, LPCWSTR, LPCWSTR);
                if(proc){
                    ((SetWindowThemeProc)proc)(hVideoPosScroll, L"", L"");
                }
                FreeLibrary(hThModule);
            }
            HorizontalOffset+=ControlWidth+xControlsSpace;
            SendMessage(hVideoPosScroll, (UINT)PBM_SETRANGE, 0, MAKELPARAM(0, 1000));

            ControlWidth = ButtonsWidth;
            hMuteButton =
                CreateWindow(TEXT("BUTTON"), TEXT("Mute"),
                             WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_PUSHLIKE|BS_BITMAP, //BS_FLAT
                             HorizontalOffset, xControlsSpace,
                             ControlWidth, ControlsHeight,
                             hWnd(), (HMENU)ID_FS_MUTE, 0, 0);
            SendMessage(hMuteButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP,
                        (LPARAM)RC().hVolumeBitmap);
            HorizontalOffset+=ControlWidth+xControlsSpace;

            ControlWidth = 100;
            hVolumeSlider =
                CreateWindow(TRACKBAR_CLASS, TEXT("Volume"),
                             WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_BOTTOM|TBS_AUTOTICKS,
                             HorizontalOffset, xControlsSpace,
                             ControlWidth, ControlsHeight - 4, hWnd(),
                             (HMENU)ID_FS_VOLUME, 0, 0);
            HorizontalOffset+=ControlWidth+xControlsSpace;
            SendMessage(hVolumeSlider, TBM_SETRANGE, FALSE, (LPARAM) MAKELONG (0, 100));
            SendMessage(hVolumeSlider, TBM_SETTICFREQ, (WPARAM) 10, 0);

            ControlWidth = ButtonsWidth;
            DWORD dwFSBtnStyle = WS_CHILD|BS_BITMAP|BS_FLAT;
            if( !PO() || PO()->get_enable_fs() ){
                dwFSBtnStyle |= WS_VISIBLE;
            }
            hFSButton =
                CreateWindow(TEXT("BUTTON"), TEXT("Toggle fullscreen"),
                             dwFSBtnStyle,
                             HorizontalOffset, xControlsSpace,
                             ControlWidth, ControlsHeight, hWnd(),
                             (HMENU)ID_FS_SWITCH_FS, 0, 0);
            SendMessage(hFSButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP,
                        (LPARAM)RC().hFullscreenBitmap);
            HorizontalOffset+=ControlWidth+xControlsSpace;

            RECT rect;
            GetClientRect(GetParent(hWnd()), &rect);

            int ControlWndWidth = HorizontalOffset;
            int ControlWndHeight = xControlsSpace+ControlsHeight+xControlsSpace;
            SetWindowPos(hWnd(), 0,
                         0, (rect.bottom - rect.top) - ControlWndWidth,
                         rect.right-rect.left, ControlWndHeight,
                         SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOACTIVATE);

            CreateToolTip();

            if( VP() ){
                RegisterToVLCEvents();
                UpdateVolumeSlider( VP()->get_mp().volume() );
                UpdateMuteButton( VP()->get_mp().mute() );
            }

            break;
        }
        case WM_LBUTTONUP:{
            POINT BtnUpPoint = {LOWORD(lParam), HIWORD(lParam)};
            RECT VideoPosRect;
            GetWindowRect(hVideoPosScroll, &VideoPosRect);
            ClientToScreen(hWnd(), &BtnUpPoint);
            if(PtInRect(&VideoPosRect, BtnUpPoint)){
                SetVideoPos(float(BtnUpPoint.x-VideoPosRect.left)/(VideoPosRect.right-VideoPosRect.left));
            }
            break;
        }
        case WM_TIMER:{
            POINT MousePoint;
            GetCursorPos(&MousePoint);
            RECT ControlWndRect;
            GetWindowRect(hWnd(), &ControlWndRect);
            if(PtInRect(&ControlWndRect, MousePoint)||GetCapture()==hVolumeSlider){
                //do not allow control window to close while mouse is within
                NeedShowControls();
            }
            else{
                NeedHideControls();
            }
            break;
        }
        case WM_SETCURSOR:{
            RECT VideoPosRect;
            GetWindowRect(hVideoPosScroll, &VideoPosRect);
            DWORD dwMsgPos = GetMessagePos();
            POINT MsgPosPoint = {LOWORD(dwMsgPos), HIWORD(dwMsgPos)};
            if(PtInRect(&VideoPosRect, MsgPosPoint)){
                SetCursor(LoadCursor(NULL, IDC_HAND));
                return TRUE;
            }
            else{
                return VLCWnd::WindowProc(uMsg, wParam, lParam);
            }
            break;
        }
        case WM_NCDESTROY:
            break;
        case WM_COMMAND:{
            WORD NCode = HIWORD(wParam);
            WORD Control = LOWORD(wParam);
            switch(NCode){
                case BN_CLICKED:{
                    switch(Control){
                        case ID_FS_SWITCH_FS:
                            WM().ToggleFullScreen();
                            break;
                        case ID_FS_PLAY_PAUSE:{
                            if( VP() ){
                                if( IsPlaying() )
                                    VP()->mlp().pause();
                                else
                                    VP()->play();
                            }
                            break;
                        }
                        case ID_FS_MUTE:{
                            if( VP() ){
                                bool newMutedState = IsDlgButtonChecked(hWnd(), ID_FS_MUTE) != FALSE;
                                VP()->get_mp().setMute( newMutedState );
                                UpdateMuteButton( newMutedState );
                            }
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
        case WM_SIZE:{
            if( GetWindowLong(hWnd(), GWL_STYLE) & WS_VISIBLE &&
                !WM().IsFullScreen() &&
                ( !PO() || !PO()->get_show_toolbar() ) )
            {
                //hide controls when they are not allowed
                NeedHideControls();
            }

            const int new_client_width = LOWORD(lParam);

            bool isFSBtnVisible =
                (GetWindowLong(hFSButton, GWL_STYLE) & WS_VISIBLE) != 0;

            HDWP hDwp = BeginDeferWindowPos(4);

            int VideoScrollWidth = new_client_width;

            POINT pt = {0, 0};
            RECT rect;
            GetWindowRect(hPlayPauseButton, &rect);
            pt.x = rect.right;
            ScreenToClient(hWnd(), &pt);
            VideoScrollWidth -= pt.x;
            VideoScrollWidth -= xControlsSpace;

            RECT VideoSrcollRect;
            GetWindowRect(hVideoPosScroll, &VideoSrcollRect);

            RECT MuteRect;
            GetWindowRect(hMuteButton, &MuteRect);
            VideoScrollWidth -= xControlsSpace;
            VideoScrollWidth -= (MuteRect.right - MuteRect.left);

            RECT VolumeRect;
            GetWindowRect(hVolumeSlider, &VolumeRect);
            VideoScrollWidth -= xControlsSpace;
            VideoScrollWidth -= (VolumeRect.right - VolumeRect.left);

            RECT FSRect = {0, 0, 0, 0};
            if( isFSBtnVisible ) {
                GetWindowRect(hFSButton, &FSRect);
                VideoScrollWidth -= xControlsSpace;
                VideoScrollWidth -= (FSRect.right - FSRect.left);
                VideoScrollWidth -= xControlsSpace;
            }

            pt.x = VideoSrcollRect.left;
            pt.y = VideoSrcollRect.top;
            ScreenToClient(hWnd(), &pt);
            hDwp = DeferWindowPos(hDwp, hVideoPosScroll, 0, pt.x, pt.y,
                                  VideoScrollWidth,
                                  VideoSrcollRect.bottom - VideoSrcollRect.top,
                                  SWP_NOACTIVATE|SWP_NOOWNERZORDER);

            int HorizontalOffset =
                pt.x + VideoScrollWidth + xControlsSpace;
            pt.x = 0;
            pt.y = MuteRect.top;
            ScreenToClient(hWnd(), &pt);
            hDwp = DeferWindowPos(hDwp, hMuteButton, 0,
                                  HorizontalOffset, pt.y, 0, 0,
                                  SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
            HorizontalOffset +=
                MuteRect.right - MuteRect.left + xControlsSpace;

            pt.x = 0;
            pt.y = VolumeRect.top;
            ScreenToClient(hWnd(), &pt);
            hDwp = DeferWindowPos(hDwp, hVolumeSlider, 0,
                                  HorizontalOffset, pt.y, 0, 0,
                                  SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
            HorizontalOffset +=
                VolumeRect.right - VolumeRect.left + xControlsSpace;

            if( isFSBtnVisible ) {
                pt.x = 0;
                pt.y = FSRect.top;
                ScreenToClient(hWnd(), &pt);
                hDwp = DeferWindowPos(hDwp, hFSButton, 0,
                                      HorizontalOffset, pt.y, 0, 0,
                                      SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
            }

            EndDeferWindowPos(hDwp);
            break;
        }
        case WM_HSCROLL:
        case WM_VSCROLL: {
            if( hVolumeSlider == (HWND)lParam ){
                if( VP() ){
                    LRESULT SliderPos = SendMessage(hVolumeSlider, (UINT) TBM_GETPOS, 0, 0);
                    VP()->get_mp().setVolume( SliderPos );
                }
            }
            break;
        }
        default:
            return VLCWnd::WindowProc(uMsg, wParam, lParam);
    }
    return 0L;
}

void VLCControlsWnd::RegisterToVLCEvents()
{
    VP()->get_mp().eventManager().onPositionChanged([this](float pos) {
        PostMessage(hVideoPosScroll, (UINT) PBM_SETPOS, (WPARAM)(pos * 1000), 0);
    });

    VP()->get_mp().eventManager().onPlaying([this] {
        PostMessage(hPlayPauseButton, BM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM) RC().hPauseBitmap);
    });

    VP()->get_mp().eventManager().onPaused([this] {
        PostMessage(hPlayPauseButton, BM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM) RC().hPlayBitmap);
    });

    VP()->get_mp().eventManager().onStopped([this] {
        PostMessage(hPlayPauseButton, BM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM) RC().hPlayBitmap);
        PostMessage(hVideoPosScroll, (UINT) PBM_SETPOS, (WPARAM)0, 0);
    });

    VP()->get_mp().eventManager().onStopping([this] {
        PostMessage(hPlayPauseButton, BM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM) RC().hPlayBitmap);
        PostMessage(hVideoPosScroll, (UINT) PBM_SETPOS, (WPARAM)0, 0);
    });

    VP()->get_mp().eventManager().onAudioVolume([this](float vol) {
        UpdateVolumeSlider( roundf(vol * 100) );
    });

    VP()->get_mp().eventManager().onMuted([this] {
        UpdateMuteButton(true);
    });

    VP()->get_mp().eventManager().onUnmuted([this] {
        UpdateMuteButton(false);
    });
}

void VLCControlsWnd::NeedShowControls()
{
    if ( WM().IsFullScreen() || (PO() && PO()->get_show_toolbar()) )
    {
        SetWindowPos( hWnd(),  HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
        ShowWindow( hWnd(), SW_SHOW );
    }
    SetTimer(hWnd(), 1, 2 * 1000, nullptr);
}

void VLCControlsWnd::NeedHideControls()
{
    ShowWindow( hWnd(), SW_HIDE );
    KillTimer(hWnd(), 1);
}

void VLCControlsWnd::SetVideoPos(float Pos) //0-start, 1-end
{
    if( VP() ){
        VP()->get_mp().setPosition( Pos, true );

        if( VP()->get_mp().length() > 0 )
            PostMessage(hVideoPosScroll, (UINT)PBM_SETPOS, (WPARAM) (Pos * 1000), 0);
    }
}

void VLCControlsWnd::UpdateVolumeSlider(unsigned int vol)
{
    const LRESULT SliderPos = SendMessage(hVolumeSlider, (UINT) TBM_GETPOS, 0, 0);
    if( (UINT)SliderPos != vol )
        PostMessage(hVolumeSlider, (UINT) TBM_SETPOS, (WPARAM) TRUE, (LPARAM) vol);
}

void VLCControlsWnd::UpdateMuteButton(bool muted)
{
    int MuteButtonState = SendMessage(hMuteButton, (UINT) BM_GETCHECK, 0, 0);
    if((muted&&(BST_UNCHECKED==MuteButtonState))||(!muted&&(BST_CHECKED==MuteButtonState))){
        PostMessage(hMuteButton, BM_SETCHECK, (WPARAM)(muted?BST_CHECKED:BST_UNCHECKED), 0);
    }
    LRESULT lResult = SendMessage(hMuteButton, BM_GETIMAGE, (WPARAM)IMAGE_BITMAP, 0);
    if( (muted && ((HANDLE)lResult == RC().hVolumeBitmap)) ||
        (!muted&&((HANDLE)lResult == RC().hVolumeMutedBitmap)) )
    {
        HANDLE hBmp = muted ? RC().hVolumeMutedBitmap : RC().hVolumeBitmap ;
        PostMessage(hMuteButton, BM_SETIMAGE,
             (WPARAM)IMAGE_BITMAP, (LPARAM)hBmp);
    }
}

void VLCControlsWnd::UpdateFullscreenButton(bool fullscreen)
{
    if (fullscreen)
        PostMessage( hFSButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM) RC().hDeFullscreenBitmap );
    else
        PostMessage( hFSButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM) RC().hFullscreenBitmap );
}

////////////////////////////////////////////////////////////////////////////////
//VLCHolderWnd members
////////////////////////////////////////////////////////////////////////////////

VLCHolderWnd*
VLCHolderWnd::CreateHolderWindow(HINSTANCE hInstance,
                                 HWND hParentWnd, VLCWindowsManager* WM)
{
    VLCHolderWnd* wnd = new VLCHolderWnd(hInstance, WM);
    if( wnd && wnd->Create(hParentWnd) ) {
        return wnd;
    }
    delete wnd;
    return 0;
}

VLCHolderWnd::~VLCHolderWnd()
{
    if(_hBgBrush) {
        DeleteObject(_hBgBrush);
        _hBgBrush = 0;
    }
}

bool VLCHolderWnd::Create(HWND hWndParent)
{
    return VLCWnd::Create(TEXT("Holder Window"),
                            WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS|WS_VISIBLE,
                            0, 0, 0, 0, hWndParent, 0);
}

void VLCHolderWnd::PreRegisterWindowClass(WNDCLASS* wc)
{
    if( !_hBgBrush){
        unsigned r = 0, g = 0, b = 0;
        HTMLColor2RGB(PO()->get_bg_color().c_str(), &r, &g, &b);
        _hBgBrush = CreateSolidBrush(RGB(r, g, b));
    }

    wc->hbrBackground = _hBgBrush;
    wc->lpszClassName = TEXT("Web Plugin VLC Window Holder Class");
}

LRESULT VLCHolderWnd::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch( uMsg )
    {
        case WM_CREATE:{
            CREATESTRUCT* CreateStruct = (CREATESTRUCT*)(lParam);

            RECT ParentClientRect;
            GetClientRect(CreateStruct->hwndParent, &ParentClientRect);
            MoveWindow(hWnd(), 0, 0,
                       (ParentClientRect.right-ParentClientRect.left),
                       (ParentClientRect.bottom-ParentClientRect.top), FALSE);
            _CtrlsWnd =
                VLCControlsWnd::CreateControlsWindow(hInstance(), _wm,
                                                     hWnd());
            // This needs to access the media player HWND, therefore we need
            // to wait for the vout to be created
            VP()->get_mp().eventManager().onVout([this](int nbVout) {
                if ( nbVout == 0 )
                    return;
                HWND hwnd = FindMP_hWnd();
                if ( hwnd == nullptr )
                    return;
                DWORD s = GetWindowLong(hwnd, GWL_STYLE);
                s |= WS_CLIPSIBLINGS;
                SetWindowLong(hwnd, GWL_STYLE, s);

                //libvlc events arrives from separate thread,
                //so we need post message to main thread, to notify it.
                PostMessage(hWnd(), WM_SET_MOUSE_HOOK, 0, 0);
            });
            break;
        }
        case WM_SET_MOUSE_HOOK:{
            MouseHook(true);
            KeyboardHook(true);
            break;
        }
        case WM_PAINT:{
            PAINTSTRUCT PaintStruct;
            HDC hDC = BeginPaint(hWnd(), &PaintStruct);
            if( PO() && PO()->get_enable_branding() )
            {
                RECT rect;
                GetClientRect(hWnd(), &rect);
                int IconX = ((rect.right - rect.left) - GetSystemMetrics(SM_CXICON))/2;
                int IconY = ((rect.bottom - rect.top) - GetSystemMetrics(SM_CYICON))/2;
                DrawIcon(hDC, IconX, IconY, RC().hBackgroundIcon);
            }
            EndPaint(hWnd(), &PaintStruct);
            break;
        }
        case WM_SHOWWINDOW:{
            if(FALSE!=wParam){ //showing
                _CtrlsWnd->NeedShowControls();
            }
            break;
        }
        case WM_SIZE:
            if(_CtrlsWnd){
                int new_client_width = LOWORD(lParam);
                int new_client_height = HIWORD(lParam);

                RECT rect;
                GetWindowRect(_CtrlsWnd->hWnd(), &rect);

                MoveWindow(_CtrlsWnd->hWnd(),
                           0, new_client_height - (rect.bottom - rect.top),
                           new_client_width, (rect.bottom-rect.top), TRUE);
            }
            break;
        case WM_MOUSEMOVE:
            WM().OnMouseEvent(uMsg, wParam, lParam);
            if (_CtrlsWnd && (_oldMouseCoords.x != GET_X_LPARAM(lParam) ||
                _oldMouseCoords.y != GET_Y_LPARAM(lParam)))
            {
                _CtrlsWnd->NeedShowControls();
                _oldMouseCoords = MAKEPOINTS(lParam);
            }
            break;

        case WM_MBUTTONDBLCLK:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
        case WM_XBUTTONDBLCLK:
        case WM_MBUTTONUP:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_XBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_XBUTTONDOWN:
            WM().OnMouseEvent(uMsg, wParam, lParam);
            return VLCWnd::WindowProc(uMsg, wParam, lParam);
            break;

        case WM_CHAR:
        case WM_KEYUP:
        case WM_KEYDOWN:
            WM().OnKeyEvent(uMsg, wParam, lParam);
            return VLCWnd::WindowProc(uMsg, wParam, lParam);
        case WM_MOUSE_EVENT_NOTIFY:{
            // This is called synchronously, though handling it directly from the mouse hook
            // deadlocks (quite likely because we're destroying the windows we're being called from)
            // Just repost this asynchronously, and let the called know we received the message.
            PostMessage(hWnd(), WM_MOUSE_EVENT_REPOST, wParam, lParam);
            return WM_MOUSE_EVENT_NOTIFY_SUCCESS;
        }
        case WM_MOUSE_EVENT_REPOST:
        {
            //on click set focus to the parent of MP to receice key events
            switch (wParam) {
                case WM_RBUTTONUP:
                case WM_MBUTTONUP:
                case WM_LBUTTONUP:
                case WM_XBUTTONUP:
                {
                    HWND mphwnd = FindMP_hWnd();
                    if (mphwnd)
                        SetFocus(GetParent(mphwnd));
                   break;
                }
            }
            WM().OnMouseEvent(uMsg, wParam, lParam);
            break;
        }
        case WM_KEYBOARD_EVENT_NOTIFY:{
            PostMessage(hWnd(), WM_KEYBOARD_EVENT_REPOST, wParam, lParam);
            return WM_KEYBOARD_EVENT_NOTIFY_SUCCESS;
        }
        case WM_KEYBOARD_EVENT_REPOST:
            if ((lParam & 0xA0000000) == 0)
                WM().OnKeyEvent(WM_KEYDOWN, wParam, lParam);
            else
                WM().OnKeyEvent(WM_KEYUP, wParam, lParam);
            break;
        default:
            return VLCWnd::WindowProc(uMsg, wParam, lParam);
    }
    return 0;
}

void VLCHolderWnd::DestroyWindow()
{
    if(_CtrlsWnd){
        delete _CtrlsWnd;
        _CtrlsWnd = 0;
    }

    if(hWnd())
        ::DestroyWindow(hWnd());
}

LRESULT CALLBACK VLCHolderWnd::MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if(nCode >= 0){
        switch(wParam){
            case WM_MOUSEMOVE:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDBLCLK:
            case WM_LBUTTONDBLCLK:
            case WM_XBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_LBUTTONDOWN:
            case WM_XBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
            case WM_LBUTTONUP:
            case WM_XBUTTONUP:
            {
                MOUSEHOOKSTRUCT* mhs = reinterpret_cast<MOUSEHOOKSTRUCT*>(lParam);

                //try to find HolderWnd and notify it
                HWND hNotifyWnd = mhs->hwnd;
                LRESULT SMRes = ::SendMessage(hNotifyWnd, WM_MOUSE_EVENT_NOTIFY, wParam, 0);
                while( hNotifyWnd && WM_MOUSE_EVENT_NOTIFY_SUCCESS != SMRes){
                    hNotifyWnd = GetParent(hNotifyWnd);
                    SMRes = ::SendMessage(hNotifyWnd, WM_MOUSE_EVENT_NOTIFY, wParam, 0);
                }
                break;
            }
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}


LRESULT CALLBACK VLCHolderWnd::KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode != HC_ACTION)
        return CallNextHookEx(NULL, nCode, wParam, lParam);

    //try to find HolderWnd and notify it
    HWND hNotifyWnd = ::GetFocus();
    LRESULT SMRes = ::SendMessage(hNotifyWnd, WM_KEYBOARD_EVENT_NOTIFY, wParam, lParam);
    while( hNotifyWnd && WM_KEYBOARD_EVENT_NOTIFY_SUCCESS != SMRes){
        hNotifyWnd = GetParent(hNotifyWnd);
        SMRes = ::SendMessage(hNotifyWnd, WM_KEYBOARD_EVENT_NOTIFY, wParam, lParam);
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}


void VLCHolderWnd::MouseHook(bool SetHook)
{
    if(SetHook){
        HWND hMPWnd = FindMP_hWnd();
        const DWORD WndThreadID = (hMPWnd) ? GetWindowThreadProcessId(hMPWnd, NULL) : 0;
        if( _hMouseHook &&( !hMPWnd || WndThreadID != _MouseHookThreadId) ){
            //unhook if something changed
            MouseHook(false);
        }

        if(!_hMouseHook && hMPWnd && WndThreadID){
            _MouseHookThreadId = WndThreadID;
            _hMouseHook =
                SetWindowsHookEx(WH_MOUSE, VLCHolderWnd::MouseHookProc,
                                 NULL, WndThreadID);
        }
    }
    else{
        if(_hMouseHook){
            UnhookWindowsHookEx(_hMouseHook);
            _MouseHookThreadId=0;
            _hMouseHook = 0;
        }
    }
}

void VLCHolderWnd::KeyboardHook(bool SetHook)
{
    if(SetHook){
        HWND hMPWnd = FindMP_hWnd();
        const DWORD WndThreadID = (hMPWnd) ? GetWindowThreadProcessId(hMPWnd, NULL) : 0;
        if( _hKeyboardHook &&( !hMPWnd || WndThreadID != _KeyboardHookThreadId) ){
            //unhook if something changed
            KeyboardHook(false);
        }

        if(!_hKeyboardHook && hMPWnd && WndThreadID){
            _KeyboardHookThreadId = WndThreadID;
            _hKeyboardHook =
                SetWindowsHookEx(WH_KEYBOARD, VLCHolderWnd::KeyboardHookProc,
                                 NULL, WndThreadID);
        }
    }
    else{
        if(_hKeyboardHook){
            UnhookWindowsHookEx(_hKeyboardHook);
            _KeyboardHookThreadId=0;
            _hKeyboardHook = 0;
        }
    }
}


HWND VLCHolderWnd::FindMP_hWnd()
{
    if(_CtrlsWnd)
        return GetWindow(_CtrlsWnd->hWnd(), GW_HWNDNEXT);
    else
        return GetWindow(hWnd(), GW_CHILD);
}

////////////////////////////////////////////////////////////////////////////////
//VLCFullScreenWnd members
////////////////////////////////////////////////////////////////////////////////
HINSTANCE VLCFullScreenWnd::_hinstance = 0;
ATOM VLCFullScreenWnd::_fullscreen_wndclass_atom = 0;

void VLCFullScreenWnd::RegisterWndClassName(HINSTANCE hInstance)
{
    //save hInstance for future use
    _hinstance = hInstance;

    WNDCLASS wClass;

    memset(&wClass, 0 , sizeof(wClass));
    if( ! GetClassInfo(_hinstance,  getClassName(), &wClass) )
    {
        wClass.style          = CS_NOCLOSE|CS_DBLCLKS;
        wClass.lpfnWndProc    = FSWndWindowProc;
        wClass.cbClsExtra     = 0;
        wClass.cbWndExtra     = 0;
        wClass.hInstance      = _hinstance;
        wClass.hIcon          = NULL;
        wClass.hCursor        = LoadCursor(NULL, IDC_ARROW);
        wClass.hbrBackground  = (HBRUSH)(COLOR_3DFACE+1);
        wClass.lpszMenuName   = NULL;
        wClass.lpszClassName  = getClassName();

        _fullscreen_wndclass_atom = RegisterClass(&wClass);
    }
    else
    {
        _fullscreen_wndclass_atom = 0;
    }

}
void VLCFullScreenWnd::UnRegisterWndClassName()
{
    if(0 != _fullscreen_wndclass_atom){
        UnregisterClass(MAKEINTATOM(_fullscreen_wndclass_atom), _hinstance);
        _fullscreen_wndclass_atom = 0;
    }
}

LRESULT CALLBACK VLCFullScreenWnd::FSWndWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    VLCFullScreenWnd* fs_data = reinterpret_cast<VLCFullScreenWnd *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch( uMsg )
    {
        case WM_CREATE:{
            CREATESTRUCT* CreateStruct = (CREATESTRUCT*)(lParam);
            VLCWindowsManager* WM = (VLCWindowsManager*)CreateStruct->lpCreateParams;

            fs_data = new VLCFullScreenWnd(hWnd, WM);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(fs_data));

            break;
        }
        case WM_NCDESTROY:
            delete fs_data;
            SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
            break;
        case WM_SHOWWINDOW:{
            if(FALSE==wParam){ //hiding
                break;
            }

            //simulate lParam for WM_SIZE
            RECT ClientRect;
            GetClientRect(hWnd, &ClientRect);
            lParam = MAKELPARAM(ClientRect.right, ClientRect.bottom);
        }
        case WM_SIZE:{
            if(fs_data->_WindowsManager->IsFullScreen()){
                int new_client_width = LOWORD(lParam);
                int new_client_height = HIWORD(lParam);
                VLCHolderWnd* HolderWnd =  fs_data->_WindowsManager->getHolderWnd();
                SetWindowPos(HolderWnd->hWnd(), HWND_BOTTOM, 0, 0,
                             new_client_width, new_client_height,
                             SWP_NOACTIVATE|SWP_NOOWNERZORDER);
            }
            break;
        }

        case WM_CHAR:
        case WM_SYSKEYUP:
        case WM_KEYUP:
            if (fs_data)
                fs_data->_WindowsManager->OnKeyEvent(uMsg, wParam, lParam);
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN: {
            if (fs_data) {
                fs_data->_WindowsManager->OnKeyEvent(uMsg, wParam, lParam);
                fs_data->_WindowsManager->OnKeyDownEvent(uMsg, wParam, lParam);
            }
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
        }
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0L;
};

VLCFullScreenWnd* VLCFullScreenWnd::CreateFSWindow(VLCWindowsManager* WM)
{
    HWND hWnd = CreateWindow(getClassName(),
                TEXT("VLC Full Screen Window"),
                WS_POPUP|WS_CLIPCHILDREN,
                0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
                0,
                0,
                VLCFullScreenWnd::_hinstance,
                (LPVOID)WM
                );
    if(hWnd)
        return reinterpret_cast<VLCFullScreenWnd*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    return 0;
}

///////////////////////
//VLCWindowsManager
///////////////////////
VLCWindowsManager::VLCWindowsManager(HMODULE hModule, const VLCViewResources& rc,
                                     vlc_player* player, const vlc_player_options* po,
                                     InputObserver* observer)
    :_rc(rc), _hModule(hModule), _po(po), _hWindowedParentWnd(0), _vp(player),
    _HolderWnd(0), _FSWnd(0), _InputObserver(observer), _b_new_messages_flag(false), Last_WM_MOUSEMOVE_Pos(0)
{
    VLCFullScreenWnd::RegisterWndClassName(hModule);
}

VLCWindowsManager::~VLCWindowsManager()
{
    VLCFullScreenWnd::UnRegisterWndClassName();
}

void VLCWindowsManager::CreateWindows(HWND hWindowedParentWnd)
{
    _hWindowedParentWnd = hWindowedParentWnd;

    if(!_HolderWnd){
        _HolderWnd =
            VLCHolderWnd::CreateHolderWindow(getHModule(),
                                             hWindowedParentWnd, this);
    }
}

void VLCWindowsManager::DestroyWindows()
{
    if(_HolderWnd){
        _HolderWnd->DestroyWindow();
        delete _HolderWnd;
        _HolderWnd = 0;
    }

    if(_FSWnd){
        _FSWnd->DestroyWindow();
    }
    _FSWnd = 0;
}

void VLCWindowsManager::StartFullScreen()
{
    if( !_HolderWnd || ( PO() && !PO()->get_enable_fs() ) )
        return;//VLCWindowsManager::CreateWindows was not called

    if( VP() && !IsFullScreen() ){
        if( !_FSWnd ){
            _FSWnd= VLCFullScreenWnd::CreateFSWindow(this);
        }

        RECT FSRect = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };

        HMONITOR hMonitor = MonitorFromWindow(_hWindowedParentWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO MonInfo;
        memset(&MonInfo, 0, sizeof(MonInfo));
        MonInfo.cbSize = sizeof(MonInfo);
        if( GetMonitorInfo(hMonitor, &MonInfo) ) {
            FSRect = MonInfo.rcMonitor;
        }

#ifdef _DEBUG
        //to simplify debugging in fullscreen mode
        UINT FSFlags = SWP_NOZORDER;
#else
        UINT FSFlags = 0;
#endif

        SetParent(_HolderWnd->hWnd(), _FSWnd->getHWND());
        SetWindowPos(_FSWnd->getHWND(), HWND_TOPMOST,
                     FSRect.left, FSRect.top,
                     FSRect.right - FSRect.left, FSRect.bottom - FSRect.top,
                     FSFlags);

        ShowWindow(_FSWnd->getHWND(), SW_SHOW);

        HWND controlWindow = _HolderWnd->ControlWindow()->hWnd();
        //parenting to NULL promotes window to WS_POPUP
        SetParent(controlWindow, NULL);
        //Ensure control window is on the right screen
        RECT controlRect;
        GetWindowRect(controlWindow, &controlRect);
        OffsetRect(&controlRect, FSRect.left - controlRect.left, FSRect.bottom - controlRect.bottom);
        SetWindowPos(controlWindow,  HWND_TOPMOST,
                     controlRect.left, controlRect.top,
                     controlRect.right - controlRect.left, controlRect.bottom - controlRect.top,
                     SWP_FRAMECHANGED);

    }
}

void VLCWindowsManager::EndFullScreen()
{
    if(!_HolderWnd)
        return;//VLCWindowsManager::CreateWindows was not called

    if(IsFullScreen()){
        SetParent(_HolderWnd->hWnd(), _hWindowedParentWnd);

        RECT WindowedParentRect;
        GetClientRect(_hWindowedParentWnd, &WindowedParentRect);
        MoveWindow(_HolderWnd->hWnd(), 0, 0,
                   WindowedParentRect.right, WindowedParentRect.bottom, FALSE);

        ShowWindow(_FSWnd->getHWND(), SW_HIDE);

        _FSWnd->DestroyWindow();
        _FSWnd = nullptr;

        HWND controlWindow = _HolderWnd->ControlWindow()->hWnd();
        SetParent(controlWindow, _HolderWnd->hWnd());
        SetWindowPos(controlWindow,  HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
   }
}

void VLCWindowsManager::ToggleFullScreen()
{
    auto isFullscreen = IsFullScreen();
    if ( isFullscreen ){
        EndFullScreen();
    }
    else{
        StartFullScreen();
    }
    _HolderWnd->ControlWindow()->UpdateFullscreenButton( !isFullscreen );
}

bool VLCWindowsManager::IsFullScreen()
{
    return 0!=_FSWnd && 0!=_HolderWnd && GetParent(_HolderWnd->hWnd())==_FSWnd->getHWND();
}

void VLCWindowsManager::OnKeyDownEvent(UINT, WPARAM wParam, LPARAM)
{
    switch(wParam){
        case VK_ESCAPE:
        case 'F':
            EndFullScreen();
            _HolderWnd->ControlWindow()->UpdateFullscreenButton( FALSE );
            break;
    }
}

void VLCWindowsManager::OnKeyEvent(UINT uKeyMsg, WPARAM wParam, LPARAM lParam)
{
    if (_InputObserver)
        this->_InputObserver->OnKeyEvent(uKeyMsg, wParam, lParam);
}

void VLCWindowsManager::OnMouseEvent(UINT uMouseMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMouseMsg == WM_MOUSE_EVENT_REPOST)
    {
        DWORD MsgPos = GetMessagePos();
        switch(wParam){
            case WM_LBUTTONDBLCLK:
                ToggleFullScreen();
                break;
            case WM_MOUSEMOVE:{
                DWORD MsgPos = GetMessagePos();
                if(Last_WM_MOUSEMOVE_Pos != MsgPos){
                    Last_WM_MOUSEMOVE_Pos = MsgPos;
                    _HolderWnd->ControlWindow()->NeedShowControls();
                }
                break;
            }
        }
        if (_InputObserver)
            this->_InputObserver->OnMouseEvent(wParam, lParam, MsgPos);
    }
    else if (_InputObserver)
        this->_InputObserver->OnMouseEvent(uMouseMsg, wParam, lParam);
}

