/*****************************************************************************
 * Copyright © 2002-2011 VideoLAN and VLC authors
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

#pragma once

#include <vlcpp/vlc.hpp>

enum vlc_player_action_e
{
    pa_play,
    pa_pause,
    pa_stop,
    pa_next,
    pa_prev
};

class vlc_player
{
public:
    bool open(VLC::Instance& inst);

    int add_item(const char * mrl, unsigned int optc, const char **optv);
    int add_item(const char * mrl)
        { return add_item( mrl, 0, nullptr ); }

    int  current_item();
    int  items_count();
    bool delete_item(unsigned int idx);
    void clear_items();

    void play();

    int preparse_item_sync(unsigned int idx, int options, unsigned int timeout);

    VLC::MediaPlayer& get_mp()
    {
        return _mp;
    }

    VLC::MediaListPlayer& mlp()
    {
        return _ml_p;
    }

    std::shared_ptr<VLC::Media> get_media( unsigned int idx );

    int currentAudioTrack();
    int currentSubtitleTrack();
    int currentVideoTrack();

private:
    // Returns a 0-based track index, instead of the internal libvlc one
    int getCurrentTrack( const std::vector<VLC::MediaTrack>& tracks );


private:
    VLC::Instance           _libvlc_instance;
    VLC::MediaPlayer        _mp;
    VLC::MediaList          _ml;
    VLC::MediaListPlayer    _ml_p;
};
