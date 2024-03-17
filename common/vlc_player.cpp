/*****************************************************************************
 * Copyright � 2002-2011 VideoLAN and VLC authors
 * $Id$
 *
 * Authors: Sergey Radionov <rsatom_gmail.com>
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

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <future>
#endif

#include "vlc_player.h"

bool vlc_player::open(VLC::Instance& inst)
{
    if( !inst )
        return false;

    _libvlc_instance = inst;

    try {
        _mp   = VLC::MediaPlayer(inst);
        _ml   = VLC::MediaList();
        _ml_p = VLC::MediaListPlayer(inst);

        _ml_p.setMediaList( _ml );
        _ml_p.setMediaPlayer( _mp );
    }
    catch (std::runtime_error&) {
        return false;
    }

    return true;
}

int vlc_player::add_item(const char * mrl, unsigned int optc, const char **optv)
{
    VLC::Media media;
    try {
        media = VLC::Media( mrl, VLC::Media::FromLocation );
    }
    catch ( std::runtime_error& ) {
        return -1;
    }

    for( unsigned int i = 0; i < optc; ++i )
        media.addOptionFlag( optv[i], libvlc_media_option_unique );

    VLC::MediaList::Lock lock( _ml );
    if( _ml.addMedia( media ) )
         return _ml.count() - 1;
    return -1;
}

int vlc_player::current_item()
{
    auto media = _mp.media();

    if( !media )
        return -1;
    return _ml.indexOfItem( *media );
}

int vlc_player::items_count()
{
    VLC::MediaList::Lock lock( _ml );
    return _ml.count();
}

bool vlc_player::delete_item(unsigned int idx)
{
    VLC::MediaList::Lock lock( _ml );
    return _ml.removeIndex( idx );
}

void vlc_player::clear_items()
{
    VLC::MediaList::Lock lock( _ml );
    for( int i = _ml.count(); i > 0; --i) {
        _ml.removeIndex( i - 1 );
    }
}

int vlc_player::preparse_item_sync(unsigned int idx, int options, unsigned int timeout)
{
    int retval = -1;

    VLC::MediaList::Lock lock( _ml );
    auto media = _ml.itemAtIndex( idx );
    if ( !media )
        return -1;
    auto em = media->eventManager();

#  if defined(_WIN32)
    HANDLE barrier = CreateEvent(nullptr, true,  false, nullptr);
    if ( barrier == nullptr )
        return -1;

    auto event = em.onParsedChanged(
        [&barrier, &retval](VLC::Media::ParsedStatus status )
    {
        retval = int( status );
        SetEvent( barrier );
    });

    media->parseRequest( _libvlc_instance, VLC::Media::ParseFlags( options ), timeout );

    DWORD waitResult = WaitForSingleObject( barrier, INFINITE );
    switch ( waitResult ) {
    case WAIT_OBJECT_0:
        break;
    default:
        retval = -1;
        break;
    }
    CloseHandle( barrier );
    event->unregister();
#  else
    std::promise<int> promise;
    std::future<int> future = promise.get_future();

    auto event = em.onParsedChanged(
        [&promise]( VLC::Media::ParsedStatus status )
    {
        promise.set_value( int( status ) );
    });

    media->parseWithOptions( VLC::Media::ParseFlags( options ), timeout );

    future.wait();
    retval = future.get();
    event->unregister();
#  endif

    return retval;
}

std::shared_ptr<VLC::Media> vlc_player::get_media(unsigned int idx)
{
    return _ml.itemAtIndex(idx);
}

void vlc_player::play()
{
    if( 0 == items_count() )
        return;
    else if( -1 == current_item() ) {
        _ml_p.playItemAtIndex( 0 );
    }
    else
        _ml_p.play();
}

int vlc_player::currentAudioTrack()
{
    auto tracks = _mp.tracks( VLC::MediaTrack::Type::Audio, true );
    return getCurrentTrack( tracks );
}

int vlc_player::currentSubtitleTrack()
{
    auto tracks = _mp.tracks( VLC::MediaTrack::Type::Subtitle, true );
    return getCurrentTrack( tracks );
}

int vlc_player::currentVideoTrack()
{
    auto tracks = _mp.tracks( VLC::MediaTrack::Type::Video, true );
    return getCurrentTrack( tracks );
}

int vlc_player::getCurrentTrack( const std::vector<VLC::MediaTrack>& tracks )
{
    for ( const auto& t : tracks )
    {
        if ( t.selected() )
            return t.id();
    }
    return -1;
}
